#include <zephyr/types.h>
#include <stddef.h>
#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/settings/settings.h>

// Debug macros for logging
#define DEBUG_LOG printk

// Nordic UART Service UUID
#define BT_UUID_NUS_VAL \
    BT_UUID_128_ENCODE(0x6E400001, 0xB5A3, 0xF393, 0xE0A9, 0xE50E24DCCA9E)
#define BT_UUID_NUS_RX_VAL \
    BT_UUID_128_ENCODE(0x6E400002, 0xB5A3, 0xF393, 0xE0A9, 0xE50E24DCCA9E)
#define BT_UUID_NUS_TX_VAL \
    BT_UUID_128_ENCODE(0x6E400003, 0xB5A3, 0xF393, 0xE0A9, 0xE50E24DCCA9E)

#define BT_UUID_NUS_SERVICE BT_UUID_DECLARE_128(BT_UUID_NUS_VAL)
#define BT_UUID_NUS_RX      BT_UUID_DECLARE_128(BT_UUID_NUS_RX_VAL)
#define BT_UUID_NUS_TX      BT_UUID_DECLARE_128(BT_UUID_NUS_TX_VAL)

// Forward declarations
static void start_scan(void);
static void bt_ready(int err);
static uint8_t discover_func(struct bt_conn *conn,
                         const struct bt_gatt_attr *attr,
                         struct bt_gatt_discover_params *params);
static void emergency_bt_reset(void);

static struct bt_conn *default_conn;
static struct bt_gatt_discover_params discover_params;
static struct bt_gatt_subscribe_params nus_tx_subscribe_params;

// Handles and service range to be discovered
static uint16_t nus_rx_handle;
static uint16_t nus_tx_handle; 
static uint16_t nus_tx_ccc_handle;
static uint16_t current_service_start_handle; // Attribute handle of the current service being processed
static uint16_t nus_service_end_handle;

static bool device_found_flag;

// Define work item for delayed scan start
static struct k_work_delayable start_scan_work;

static void start_scan_work_handler(struct k_work *work)
{
    start_scan();
}

// Callback for receiving notifications from the NUS TX characteristic
static uint8_t nus_notify_callback(struct bt_conn *conn,
                              struct bt_gatt_subscribe_params *params,
                              const void *data, uint16_t length)
{
    if (!data) {
        printk("Unsubscribed from NUS TX\n");
        params->value_handle = 0;
        return BT_GATT_ITER_STOP;
    }

    // Safely handle received data using a static buffer instead of dynamic allocation
    if (length > 0) {
        static char buffer[128]; // Static buffer large enough for expected messages
        size_t copy_len = MIN(length, sizeof(buffer) - 1);
        
        memcpy(buffer, data, copy_len);
        buffer[copy_len] = '\0'; // Null terminate
        
        printk("Received from peripheral: %s\n", buffer);
        
        // Process the data
        if (strstr(buffer, "RENTAL START") != NULL) {
            printk("Rental data detected!\n");
            // Process the rental data here
        }
    }

    return BT_GATT_ITER_CONTINUE;
}

// Function to send data to the peripheral via NUS RX characteristic
static int send_to_peripheral(const char *data, uint16_t len)
{
    if (!default_conn) {
        DEBUG_LOG("Not connected - cannot send\n");
        return -ENOTCONN;
    }
    
    if (!nus_rx_handle) {
        DEBUG_LOG("NUS RX handle not found - cannot send\n");
        return -EINVAL;
    }

    static struct bt_gatt_write_params write_params; 
    
    write_params.func = NULL; 
    write_params.handle = nus_rx_handle;
    write_params.offset = 0;
    write_params.data = data;
    write_params.length = len;

    int err = bt_gatt_write(default_conn, &write_params);
    if (err) {
        printk("Failed to send data (err %d)\n", err);
    } else {
        printk("Sent to peripheral: %s\n", data);
    }
    return err;
}

// Error recovery handler - restart scanning when things go wrong
static void error_recovery(void)
{
    printk("Starting error recovery...\n");
    
    // If we have a connection, disconnect it
    if (default_conn) {
        bt_conn_disconnect(default_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
        bt_conn_unref(default_conn);
        default_conn = NULL;
    }
    
    // Reset all handles
    nus_rx_handle = 0;
    nus_tx_handle = 0;
    nus_tx_ccc_handle = 0;
    current_service_start_handle = 0;
    nus_service_end_handle = 0;
    
    // Clear discovery params
    memset(&discover_params, 0, sizeof(discover_params));
    
    // Restart scanning after a delay
    k_work_schedule(&start_scan_work, K_MSEC(1000));
    
    printk("Error recovery complete\n");
}

// Update the connected callback to be simpler and more direct
static void connected(struct bt_conn *conn, uint8_t conn_err)
{
    char addr[BT_ADDR_LE_STR_LEN];
    int err;

    if (!conn) {
        printk("ERROR: Connected callback with NULL connection pointer\n");
        return;
    }

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    if (conn_err) {
        printk("Failed to connect to %s (err %u)\n", addr, conn_err);
        if (default_conn) {
            bt_conn_unref(default_conn);
            default_conn = NULL;
        }
        start_scan();
        return;
    }

    printk("Connected: %s\n", addr);
    default_conn = bt_conn_ref(conn);

    // Reset handles before discovery
    nus_rx_handle = 0;
    nus_tx_handle = 0;
    nus_tx_ccc_handle = 0;
    current_service_start_handle = 0;
    nus_service_end_handle = 0;
    
    // Allow time to stabilize before service discovery
    k_sleep(K_MSEC(500));

    printk("Starting service discovery...\n");
    
    // Initialize discovery parameters
    memset(&discover_params, 0, sizeof(discover_params));
    discover_params.func = discover_func;
    discover_params.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
    discover_params.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
    discover_params.type = BT_GATT_DISCOVER_PRIMARY;
    
    // Start with a general service discovery (no UUID filter)
    // This avoids issues with UUID comparison
    printk("Discovery params: func %p, searching for all services\n", 
           discover_params.func);

    // Start service discovery
    err = bt_gatt_discover(conn, &discover_params);
    if (err) {
        printk("Service discovery failed (err %d). Disconnecting.\n", err);
        bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
    } else {
        printk("Service discovery started successfully\n");
    }
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
    printk("Disconnected: %s (reason 0x%02x)\n", addr, reason);

    if (default_conn != conn) {
        return;
    }

    // Unsubscribe from notifications if we were subscribed
    if (nus_tx_subscribe_params.value_handle) {
        bt_gatt_unsubscribe(conn, &nus_tx_subscribe_params);
        printk("Unsubscribed from NUS notifications\n");
    }

    bt_conn_unref(default_conn);
    default_conn = NULL;

    // Reset handles and subscription state
    nus_rx_handle = 0;
    nus_tx_handle = 0;
    nus_tx_ccc_handle = 0;
    current_service_start_handle = 0;
    nus_service_end_handle = 0;
    memset(&nus_tx_subscribe_params, 0, sizeof(nus_tx_subscribe_params));

    start_scan();
}

// Define connection callbacks
BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

// Simplified and staged discover_func
static uint8_t discover_func(struct bt_conn *conn,
                         const struct bt_gatt_attr *attr,
                         struct bt_gatt_discover_params *params)
{
    int err;

    // Defensive check for NULL connection
    if (!conn) {
        printk("ERROR: NULL connection in discover_func\n");
        error_recovery();
        return BT_GATT_ITER_STOP;
    }

    // Defensive check for NULL params
    if (!params) {
        printk("ERROR: NULL discovery params\n");
        error_recovery();
        return BT_GATT_ITER_STOP;
    }

    // NOTE: We're intentionally not checking for NULL UUID here
    // because params->uuid can be NULL when doing general service discovery
    
    if (!attr) {
        printk("Discovery complete but target not found, type %u\n", params->type);
        
        // If we were looking for the NUS service and didn't find it, trigger recovery
        if (params->type == BT_GATT_DISCOVER_PRIMARY) {
            printk("ERROR: NUS service not found, attempting different approach\n");
            
            // Try to find the Nordic UART Service by handle range using read by group type
            memset(params, 0, sizeof(*params));
            params->uuid = NULL;  // Don't filter by UUID for initial discovery
            params->start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
            params->end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
            params->type = BT_GATT_DISCOVER_PRIMARY;
            params->func = discover_func;
            
            printk("Starting general service discovery...\n");
            err = bt_gatt_discover(conn, params);
            if (err) {
                printk("General service discovery failed (err %d)\n", err);
                error_recovery();
            }
        } else {
            error_recovery();
        }
        
        return BT_GATT_ITER_STOP;
    }

    printk("Discovery: attr handle 0x%04x, type %u\n", attr->handle, params->type);

    // Stage 1: Discovered Primary Service
    if (params->type == BT_GATT_DISCOVER_PRIMARY) {
        if (!attr->user_data) {
            printk("ERROR: NULL user_data for service\n");
            error_recovery();
            return BT_GATT_ITER_STOP;
        }
        
        // Get service value
        struct bt_gatt_service_val *service_val = attr->user_data;
        if (!service_val || !service_val->uuid) {
            printk("ERROR: Invalid service data\n");
            error_recovery();
            return BT_GATT_ITER_STOP;
        }
        
        // Debug print the UUID
        printk("Found service with UUID type %u\n", service_val->uuid->type);
        
        // Check if this is the NUS service
        // First approach: try direct UUID comparison if we have a specific target
        if (params->uuid) {
            if (bt_uuid_cmp(service_val->uuid, params->uuid) != 0) {
                printk("Not the target service, continuing search...\n");
                return BT_GATT_ITER_CONTINUE;
            }
        } 
        // Second approach: check for NUS UUID manually
        else {
            // For 128-bit UUIDs (which NUS uses)
            if (service_val->uuid->type == BT_UUID_TYPE_128) {
                struct bt_uuid_128 *uuid_128 = (struct bt_uuid_128 *)service_val->uuid;
                // Print UUID for debugging
                printk("Service UUID: ");
                for (int i = 0; i < 16; i++) {
                    printk("%02x", uuid_128->val[i]);
                    if (i == 3 || i == 5 || i == 7 || i == 9) printk("-");
                }
                printk("\n");
                
                // The NUS UUID is 6E400001-B5A3-F393-E0A9-E50E24DCCA9E
                // Manually check if this is the NUS service
                static const uint8_t nus_uuid_val[] = {
                    0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0, 
                    0x93, 0xF3, 0xA3, 0xB5, 0x01, 0x00, 0x40, 0x6E
                };
                
                bool is_nus = true;
                for (int i = 0; i < 16; i++) {
                    if (uuid_128->val[i] != nus_uuid_val[i]) {
                        is_nus = false;
                        break;
                    }
                }
                
                if (!is_nus) {
                    printk("Not the NUS service, continuing search...\n");
                    return BT_GATT_ITER_CONTINUE;
                }
            } else {
                // Not a 128-bit UUID, can't be NUS
                return BT_GATT_ITER_CONTINUE;
            }
        }
        
        // At this point, we've found the NUS service either through direct comparison
        // or manual UUID checking
        printk("Found NUS Service: attr_handle 0x%04x, end_handle 0x%04x\n",
               attr->handle, service_val->end_handle);
        
        current_service_start_handle = attr->handle;
        nus_service_end_handle = service_val->end_handle;

        // Now discover NUS RX Characteristic 
        memset(params, 0, sizeof(*params));
        params->uuid = BT_UUID_NUS_RX;
        params->start_handle = attr->handle + 1; 
        params->end_handle = service_val->end_handle;
        params->type = BT_GATT_DISCOVER_CHARACTERISTIC;
        params->func = discover_func;

        printk("Starting RX characteristic discovery...\n");
        err = bt_gatt_discover(conn, params);
        if (err) {
            printk("Characteristic discovery failed (err %d)\n", err);
            error_recovery();
        }
        return BT_GATT_ITER_STOP;
    }
    // Stage 2: Discovered Characteristic
    else if (params->type == BT_GATT_DISCOVER_CHARACTERISTIC) {
        if (!attr->user_data) {
            printk("ERROR: NULL user_data for characteristic\n");
            error_recovery();
            return BT_GATT_ITER_STOP;
        }
        
        struct bt_gatt_chrc *chrc = attr->user_data;
        if (!chrc) {
            printk("ERROR: NULL characteristic data\n");
            error_recovery();
            return BT_GATT_ITER_STOP;
        }
        
        // Print the UUID type for debugging
        printk("Found characteristic with UUID type %u\n", chrc->uuid->type);
        
        // Check if we're looking for a specific UUID
        if (params->uuid) {
            // Check if this is the RX or TX characteristic
            if (bt_uuid_cmp(chrc->uuid, BT_UUID_NUS_RX) == 0) {
                nus_rx_handle = chrc->value_handle;
                printk("Found RX characteristic: value_handle 0x%04x\n", nus_rx_handle);

                // Now look for the TX characteristic
                memset(params, 0, sizeof(*params));
                params->uuid = BT_UUID_NUS_TX;
                params->start_handle = attr->handle + 1;
                params->end_handle = nus_service_end_handle;
                params->type = BT_GATT_DISCOVER_CHARACTERISTIC;
                params->func = discover_func;

                err = bt_gatt_discover(conn, params);
                if (err) {
                    printk("TX characteristic discovery failed (err %d)\n", err);
                    error_recovery();
                }
            } else if (bt_uuid_cmp(chrc->uuid, BT_UUID_NUS_TX) == 0) {
                nus_tx_handle = chrc->value_handle;
                printk("Found TX characteristic: value_handle 0x%04x\n", nus_tx_handle);

                // Find the CCC descriptor for enabling notifications
                memset(params, 0, sizeof(*params));
                params->uuid = BT_UUID_GATT_CCC;
                params->start_handle = nus_tx_handle + 1;
                params->end_handle = nus_service_end_handle;
                params->type = BT_GATT_DISCOVER_DESCRIPTOR;
                params->func = discover_func;

                err = bt_gatt_discover(conn, params);
                if (err) {
                    printk("CCC descriptor discovery failed (err %d)\n", err);
                    error_recovery();
                }
            } else {
                printk("Unknown characteristic found, continuing search\n");
                return BT_GATT_ITER_CONTINUE;
            }
        }
        // We're doing a general scan for characteristics
        else {
            // Print the UUID for debugging
            if (chrc->uuid->type == BT_UUID_TYPE_128) {
                struct bt_uuid_128 *uuid_128 = (struct bt_uuid_128 *)chrc->uuid;
                printk("Char UUID: ");
                for (int i = 0; i < 16; i++) {
                    printk("%02x", uuid_128->val[i]);
                    if (i == 3 || i == 5 || i == 7 || i == 9) printk("-");
                }
                printk("\n");
                
                // Check for NUS RX (6E400002-B5A3-F393-E0A9-E50E24DCCA9E)
                static const uint8_t nus_rx_uuid_val[] = {
                    0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0, 
                    0x93, 0xF3, 0xA3, 0xB5, 0x02, 0x00, 0x40, 0x6E
                };
                
                // Check for NUS TX (6E400003-B5A3-F393-E0A9-E50E24DCCA9E)
                static const uint8_t nus_tx_uuid_val[] = {
                    0x9E, 0xCA, 0xDC, 0x24, 0x0E, 0xE5, 0xA9, 0xE0, 
                    0x93, 0xF3, 0xA3, 0xB5, 0x03, 0x00, 0x40, 0x6E
                };
                
                // Check if this is the RX characteristic
                bool is_rx = true;
                for (int i = 0; i < 16; i++) {
                    if (uuid_128->val[i] != nus_rx_uuid_val[i]) {
                        is_rx = false;
                        break;
                    }
                }
                
                if (is_rx) {
                    nus_rx_handle = chrc->value_handle;
                    printk("Found RX characteristic: value_handle 0x%04x\n", nus_rx_handle);
                    // Continue searching for TX
                    return BT_GATT_ITER_CONTINUE;
                }
                
                // Check if this is the TX characteristic
                bool is_tx = true;
                for (int i = 0; i < 16; i++) {
                    if (uuid_128->val[i] != nus_tx_uuid_val[i]) {
                        is_tx = false;
                        break;
                    }
                }
                
                if (is_tx) {
                    nus_tx_handle = chrc->value_handle;
                    printk("Found TX characteristic: value_handle 0x%04x\n", nus_tx_handle);
                    
                    // Check if we found both characteristics
                    if (nus_rx_handle != 0) {
                        // We have both RX and TX, now find the CCC descriptor
                        memset(params, 0, sizeof(*params));
                        params->uuid = BT_UUID_GATT_CCC;
                        params->start_handle = nus_tx_handle + 1;
                        params->end_handle = nus_service_end_handle;
                        params->type = BT_GATT_DISCOVER_DESCRIPTOR;
                        params->func = discover_func;

                        err = bt_gatt_discover(conn, params);
                        if (err) {
                            printk("CCC descriptor discovery failed (err %d)\n", err);
                            error_recovery();
                        }
                        return BT_GATT_ITER_STOP;
                    }
                    
                    // Keep looking for the RX characteristic
                    return BT_GATT_ITER_CONTINUE;
                }
            }
            
            // Not a characteristic we're interested in
            return BT_GATT_ITER_CONTINUE;
        }
        
        return BT_GATT_ITER_STOP;
    }
    // Stage 3: Found CCC descriptor
    else if (params->type == BT_GATT_DISCOVER_DESCRIPTOR) {
        if (!nus_tx_handle) {
            printk("ERROR: Found CCC but TX handle not set\n");
            error_recovery();
            return BT_GATT_ITER_STOP;
        }
        
        if (bt_uuid_cmp(params->uuid, BT_UUID_GATT_CCC) != 0) {
            printk("Not a CCC descriptor, continuing search\n");
            return BT_GATT_ITER_CONTINUE;
        }
    
        nus_tx_ccc_handle = attr->handle;
        printk("Found CCC descriptor: handle 0x%04x\n", nus_tx_ccc_handle);

        // Double-check that we have all necessary handles
        if (!nus_rx_handle || !nus_tx_handle || !nus_tx_ccc_handle) {
            printk("Missing handles: RX: 0x%04x, TX: 0x%04x, CCC: 0x%04x\n", 
                   nus_rx_handle, nus_tx_handle, nus_tx_ccc_handle);
            error_recovery();
            return BT_GATT_ITER_STOP;
        }

        // Setup subscribe parameters
        memset(&nus_tx_subscribe_params, 0, sizeof(nus_tx_subscribe_params));
        nus_tx_subscribe_params.value_handle = nus_tx_handle;
        nus_tx_subscribe_params.ccc_handle = nus_tx_ccc_handle;
        nus_tx_subscribe_params.value = BT_GATT_CCC_NOTIFY;
        nus_tx_subscribe_params.notify = nus_notify_callback;

        // Subscribe to notifications
        printk("Subscribing to notifications...\n");
        err = bt_gatt_subscribe(conn, &nus_tx_subscribe_params);
        if (err && err != -EALREADY) {
            printk("Subscribe failed (err %d)\n", err);
            error_recovery();
        } else {
            printk("Subscribed successfully\n");
            
            // Wait a moment before sending test data
            k_sleep(K_MSEC(300));
            
            // Send a test message
            const char *hello_msg = "Hello from Central!";
            err = send_to_peripheral(hello_msg, strlen(hello_msg));
            if (err) {
                printk("Failed to send test message (err %d)\n", err);
            }
        }
        
        // Discovery is complete
        memset(params, 0, sizeof(*params));
        return BT_GATT_ITER_STOP;
    }

    return BT_GATT_ITER_CONTINUE;
}

// Check if the device name "RentScan" is in the advertising data
static bool check_device_name(struct bt_data *data, void *user_data)
{
    if (data->type == BT_DATA_NAME_COMPLETE) {
        printk("Name found in adv data: len=%u\n", data->data_len);
        
        char name_buf[32];
        size_t name_len = MIN(data->data_len, sizeof(name_buf) - 1);
        memcpy(name_buf, data->data, name_len);
        name_buf[name_len] = '\0';
        printk("Device name: '%s'\n", name_buf);
        
        if (data->data_len == 8 && memcmp(data->data, "RentScan", 8) == 0) {
            printk("Found RentScan device!\n");
            device_found_flag = true;
            return false;
        }
    }
    
    return true;
}

// Complete Bluetooth reset function
static void complete_bt_reset(void)
{
    printk("Performing complete Bluetooth stack reset...\n");
    int err;
    
    // First stop any scanning
    bt_le_scan_stop();
    
    // Disconnect and clean up any connections
    if (default_conn) {
        printk("Disconnecting from existing connection...\n");
        bt_conn_disconnect(default_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
        k_sleep(K_MSEC(100)); // Give time for disconnect to initiate
        bt_conn_unref(default_conn);
        default_conn = NULL;
    }
    
    // Release all other resources
    // This will also terminate any active connections
    printk("Closing any active connections...\n");
    
    // Wait to ensure controller processed disconnection
    k_sleep(K_MSEC(500));
    
    // Reset all handles and state variables
    printk("Resetting internal state...\n");
    nus_rx_handle = 0;
    nus_tx_handle = 0;
    nus_tx_ccc_handle = 0;
    current_service_start_handle = 0;
    nus_service_end_handle = 0;
    memset(&discover_params, 0, sizeof(discover_params));
    memset(&nus_tx_subscribe_params, 0, sizeof(nus_tx_subscribe_params));
    
    // Allow time for cleanup before disabling
    k_sleep(K_MSEC(1000));
    
    // Disable Bluetooth completely - this is the key to resetting the stack
    printk("Disabling Bluetooth stack...\n");
    err = bt_disable();
    if (err) {
        printk("Failed to disable Bluetooth (err %d)\n", err);
    } else {
        printk("Bluetooth disabled successfully\n");
    }
    
    // Wait for controller to fully shut down - longer wait
    k_sleep(K_MSEC(3000));
    
    // Re-enable Bluetooth with our bt_ready callback
    printk("Re-enabling Bluetooth...\n");
    err = bt_enable(bt_ready);
    if (err) {
        printk("Failed to re-enable Bluetooth (err %d)\n", err);
        // Try one more time
        k_sleep(K_MSEC(2000));
        printk("Trying again to re-enable Bluetooth...\n");
        err = bt_enable(bt_ready);
        if (err) {
            printk("Second attempt to re-enable Bluetooth failed (err %d)\n", err);
        }
    }
    
    // Wait for BT stack to fully initialize
    k_sleep(K_MSEC(2000));
    
    printk("BT reset complete. Starting scan after delay...\n");
    k_work_schedule(&start_scan_work, K_MSEC(1000));
}

static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
                     struct net_buf_simple *ad)
{
    char dev[BT_ADDR_LE_STR_LEN];
    int err;
    static int retry_count = 0;
    static int total_retry_count = 0;
    static uint64_t last_reset_time = 0;
    uint64_t current_time = k_uptime_get();
    
    bt_addr_le_to_str(addr, dev, sizeof(dev));
    printk("[DEVICE]: %s, AD evt type %u, AD data len %u, RSSI %i\n",
           dev, type, ad->len, rssi);

    if (type == BT_GAP_ADV_TYPE_ADV_IND ||
        type == BT_GAP_ADV_TYPE_ADV_DIRECT_IND) {
        
        device_found_flag = false;
        
        bt_data_parse(ad, check_device_name, NULL);
        
        if (device_found_flag) {
            err = bt_le_scan_stop();
            if (err) {
                printk("Stop LE scan failed (err %d)\n", err);
                return;
            }
            
            // Aggressive approach: Always assume there might be a stale connection
            // and try to clean it up proactively
            printk("Force cleaning connection state for device...\n");
            bt_le_scan_stop();
            
            // Reset all connection-related variables
            if (default_conn) {
                bt_conn_disconnect(default_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
                bt_conn_unref(default_conn);
                default_conn = NULL;
            }
            
            // Reset all handles and parameters to ensure clean state
            nus_rx_handle = 0;
            nus_tx_handle = 0;
            nus_tx_ccc_handle = 0;
            current_service_start_handle = 0;
            nus_service_end_handle = 0;
            memset(&discover_params, 0, sizeof(discover_params));
            memset(&nus_tx_subscribe_params, 0, sizeof(nus_tx_subscribe_params));
            
            // Force a delay to allow the controller state to settle
            k_sleep(K_MSEC(1000));

            struct bt_le_conn_param *param = BT_LE_CONN_PARAM_DEFAULT;
            printk("Attempting to connect to %s\n", dev);
            err = bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN, param, &default_conn);
            if (err) {
                printk("Create conn failed (err %d)\n", err);
                
                // Check if we have the "Found valid connection in disconnected state" error
                if (err == -EINVAL) {
                    retry_count++;
                    total_retry_count++;
                    
                    // If we've exceeded retries or the error persists after a complete reset
                    // or if this is the first attempt but we have many recent failures
                    if (retry_count >= 2 || 
                        (current_time - last_reset_time < 60000 && total_retry_count > 4) ||
                        (retry_count == 1 && total_retry_count > 10)) {
                        printk("Connection state issues persist. Attempting complete BT reset...\n");
                        
                        // Record when we did the complete reset
                        last_reset_time = current_time;
                        retry_count = 0;
                        total_retry_count = 0; // Reset total count after a full reset
                        
                        // Perform a complete BT stack reset
                        complete_bt_reset();
                        return;
                    }
                    
                    printk("Connection state issue detected, resetting BT state (attempt %d/2)\n", retry_count);
                    
                    // Wait longer between attempts
                    k_sleep(K_MSEC(2000));
                    
                    // Then start a fresh scan after a delay
                    k_work_schedule(&start_scan_work, K_MSEC(3000));
                    return;
                }
                
                retry_count = 0;
                start_scan();
            } else {
                printk("Connection creation initiated.\n");
                retry_count = 0;
            }
        }
    }
}

static void start_scan(void)
{
    int err;

    if (default_conn) {
        // Check if the connection is valid
        struct bt_conn_info info;
        err = bt_conn_get_info(default_conn, &info);
        
        if (err || info.state != BT_CONN_STATE_CONNECTED) {
            printk("Connection in invalid state. Cleaning up...\n");
            bt_conn_disconnect(default_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
            bt_conn_unref(default_conn);
            default_conn = NULL;
            
            // Add a delay to allow cleanup
            k_sleep(K_MSEC(500));
        } else {
            printk("Scan not started: connection already exists\n");
            return;
        }
    }

    printk("Starting LE scan for RentScan device...\n");

    struct bt_le_scan_param scan_param = {
        .type       = BT_LE_SCAN_TYPE_ACTIVE,
        .options    = BT_LE_SCAN_OPT_NONE,
        .interval   = BT_GAP_SCAN_FAST_INTERVAL,
        .window     = BT_GAP_SCAN_FAST_WINDOW,
    };

    err = bt_le_scan_start(&scan_param, device_found);
    if (err) {
        if (err == -EALREADY) {
            printk("Scan already started\n");
        } else {
            printk("Scanning failed to start (err %d)\n", err);
        }
        return;
    }

    printk("Scanning for RentScan device...\n");
}

static void bt_ready(int err)
{
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        // Attempt recovery after a delay
        k_sleep(K_MSEC(1000));
        err = bt_enable(bt_ready);
        if (err) {
            printk("Bluetooth re-init failed again (err %d)\n", err);
        }
        return;
    }

    printk("Bluetooth initialized\n");
    
    // Make sure any existing connections are cleaned up
    if (default_conn) {
        printk("Clearing existing connection on initialization\n");
        bt_conn_disconnect(default_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
        bt_conn_unref(default_conn);
        default_conn = NULL;
    }
    
    if (IS_ENABLED(CONFIG_SETTINGS)) {
        settings_load();
        printk("Settings loaded\n");
    }
    
    // Reset all connection state
    nus_rx_handle = 0;
    nus_tx_handle = 0;
    nus_tx_ccc_handle = 0;
    current_service_start_handle = 0;
    nus_service_end_handle = 0;
    memset(&discover_params, 0, sizeof(discover_params));
    memset(&nus_tx_subscribe_params, 0, sizeof(nus_tx_subscribe_params));

    // Initialize scan work handler
    k_work_init_delayable(&start_scan_work, start_scan_work_handler);
    
    // Start scanning after a delay to ensure the BT stack is fully initialized
    k_work_schedule(&start_scan_work, K_MSEC(1000));
    printk("Scheduled scan start.\n");
}

// Emergency reset by directly accessing Bluetooth controller hardware 
// Note: This is only used in extreme cases and is hardware-specific
static void emergency_bt_reset(void)
{
    printk("EMERGENCY: Attempting hardware-level reset of Bluetooth controller\n");
    
    // First disable Bluetooth stack
    bt_disable();
    k_sleep(K_MSEC(1000));
    
#ifdef CONFIG_BT_CTLR_FORCE_RESET
    // This is a hardware-specific approach that might be available in some configs
    bt_ctlr_force_reset();
#endif

    // Wait for controller to fully reset
    k_sleep(K_MSEC(3000));
    
    // Re-enable Bluetooth
    int err = bt_enable(bt_ready);
    if (err) {
        printk("Failed to re-enable Bluetooth after emergency reset (err %d)\n", err);
        // Wait and try once more
        k_sleep(K_MSEC(2000));
        err = bt_enable(bt_ready);
        if (err) {
            printk("Second attempt to re-enable failed (err %d)\n", err);
        }
    } else {
        printk("Bluetooth re-enabled successfully after emergency reset\n");
    }
    
    // Wait for stack to fully initialize
    k_sleep(K_MSEC(3000));
}

int main(void)
{
    int err;
    printk("==== RentScan Central ====\n");
    
    // Initialize BLE stack
    err = bt_enable(bt_ready);
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        k_sleep(K_MSEC(1000));
        
        // Try once more
        printk("Retrying Bluetooth initialization...\n");
        err = bt_enable(bt_ready);
        if (err) {
            printk("Bluetooth re-init failed again (err %d)\n", err);
            return 0;
        }
    }
    
    uint8_t consecutive_errors = 0;
    uint8_t emergency_reset_count = 0;
    uint64_t last_connection_time = 0;
    
    while (1) {
        k_sleep(K_SECONDS(10));
        
        // Periodic check to recover from stuck states
        if (!default_conn) {
            // If we're not connected, try to restart scan
            // This is safe to call even if already scanning
            printk("Periodic check: ensuring scan is active\n");
            start_scan();
            
            // Check for persistent connection issues
            uint64_t current_time = k_uptime_get();
            if (current_time - last_connection_time > 120000) { // 2 minutes with no connection
                consecutive_errors++;
                
                if (consecutive_errors >= 5 && emergency_reset_count < 2) {
                    printk("Persistent connection issues detected! Triggering emergency reset...\n");
                    consecutive_errors = 0;
                    emergency_reset_count++;
                    
                    // Try emergency reset as a last resort
                    emergency_bt_reset();
                    
                    // Restart scanning after emergency reset
                    k_sleep(K_MSEC(2000));
                    start_scan();
                }
            }
        } else {
            printk("Periodic check: connected to device\n");
            consecutive_errors = 0;
            last_connection_time = k_uptime_get();
        }
    }
    
    return 0;
}