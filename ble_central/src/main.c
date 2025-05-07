#include <zephyr/types.h>
#include <stddef.h>
#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <string.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/sys/byteorder.h>
#include <zephyr/settings/settings.h>
#include <zephyr/sys/util.h>


// Debug macros for logging
#define DEBUG_LOG printk

// NUS UUIDs definition - using the original 6E400001-B5A3-F393-E0A9-E50E24DCCA9E format
// These are defined in the reversed byte order as per Bluetooth specification
#define BT_UUID_NUS_VAL \
	BT_UUID_128_ENCODE(0x6e400001, 0xb5a3, 0xf393, 0xe0a9, 0xe50e24dcca9e)
#define BT_UUID_NUS_RX_VAL \
	BT_UUID_128_ENCODE(0x6e400002, 0xb5a3, 0xf393, 0xe0a9, 0xe50e24dcca9e)
#define BT_UUID_NUS_TX_VAL \
	BT_UUID_128_ENCODE(0x6e400003, 0xb5a3, 0xf393, 0xe0a9, 0xe50e24dcca9e)

#define BT_UUID_NUS        BT_UUID_DECLARE_128(BT_UUID_NUS_VAL)
#define BT_UUID_NUS_RX     BT_UUID_DECLARE_128(BT_UUID_NUS_RX_VAL)
#define BT_UUID_NUS_TX     BT_UUID_DECLARE_128(BT_UUID_NUS_TX_VAL)

// Global variables
static struct bt_uuid_128 nus_uuid = BT_UUID_INIT_128(BT_UUID_NUS_VAL);
static struct bt_uuid_128 nus_rx_uuid = BT_UUID_INIT_128(BT_UUID_NUS_RX_VAL);
static struct bt_uuid_128 nus_tx_uuid = BT_UUID_INIT_128(BT_UUID_NUS_TX_VAL);

// Forward declarations
static void start_scan(void);
static int complete_bt_reset(void);
static void bt_ready(int err);
static uint8_t discover_func(struct bt_conn *conn,
                         const struct bt_gatt_attr *attr,
                         struct bt_gatt_discover_params *params);
static void emergency_bt_reset(void);

// Error recovery tracking
static int consecutive_errors = 0;
static int emergency_reset_count = 0;
static int64_t last_connection_time = 0;

static struct bt_conn *current_conn;
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
    if (!current_conn) {
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

    int err = bt_gatt_write(current_conn, &write_params);
    if (err) {
        printk("Failed to send data (err %d)\n", err);
    } else {
        printk("Sent to peripheral: %s\n", data);
    }
    return err;
}

// Error recovery function - includes robust disconnect and clean-up
static void error_recovery(void)
{
    int err;
    static int recovery_attempts = 0;
    
    printk("*** Error recovery triggered (attempt %d) ***\n", ++recovery_attempts);
    
    // Clean up the discovery parameters
    memset(&discover_params, 0, sizeof(discover_params));
    
    // Reset all handles 
    nus_rx_handle = 0;
    nus_tx_handle = 0;
    nus_tx_ccc_handle = 0;
    nus_service_end_handle = 0;
    
    // Increment consecutive error counter for tracking persistent issues
    consecutive_errors++; 
    
    // If we're having persistent issues, try more aggressive measures
    if (consecutive_errors >= 3) {
        printk("Multiple consecutive errors detected, performing full BT reset\n");
        complete_bt_reset();
        return;
    }
    
    // If we have a connection, try to disconnect gracefully first
    if (current_conn) {
        // Try to unsubscribe if we were subscribed
        if (nus_tx_subscribe_params.value_handle != 0) {
            printk("Unsubscribing from notifications\n");
            err = bt_gatt_unsubscribe(current_conn, &nus_tx_subscribe_params);
            if (err) {
                printk("Unsubscribe failed (err %d)\n", err);
                // Continue anyway, we'll reset everything
            }
            
            // Reset subscription params
            memset(&nus_tx_subscribe_params, 0, sizeof(nus_tx_subscribe_params));
        }
        
        printk("Disconnecting from device\n");
        err = bt_conn_disconnect(current_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
        if (err) {
            printk("Disconnect failed (err %d)\n", err);
        }
        
        // Wait a moment for the disconnect to happen
        k_sleep(K_MSEC(200));
        
        // Release the connection reference
        bt_conn_unref(current_conn);
        current_conn = NULL;
    }
    
    // Wait a bit before restarting scan
    k_sleep(K_MSEC(1000));
    
    // Restart scanning
    printk("Restarting scan\n");
    start_scan();
}

// Update the connected callback to be simpler and more direct
static void connected(struct bt_conn *conn, uint8_t err)
{
    if (err) {
        printk("Connection failed (err %u)\n", err);
        // Attempt to start scanning again
        current_conn = NULL;
        start_scan();
        return;
    }

    printk("Connected\n");
    current_conn = bt_conn_ref(conn);
    last_connection_time = k_uptime_get();
    consecutive_errors = 0; // Reset error counter on successful connection

    // Reset handles before discovery
    nus_rx_handle = 0;
    nus_tx_handle = 0;
    nus_tx_ccc_handle = 0;
    nus_service_end_handle = 0;

    // Stop scanning, we're connected
    err = bt_le_scan_stop();
    if (err && err != -EALREADY) {
        printk("Stop LE scan failed (err %d)\n", err);
    }

    // Start service discovery
    memset(&discover_params, 0, sizeof(discover_params));
    discover_params.uuid = (struct bt_uuid *)&nus_uuid;
    discover_params.func = discover_func;
    discover_params.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
    discover_params.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
    discover_params.type = BT_GATT_DISCOVER_PRIMARY;

    err = bt_gatt_discover(conn, &discover_params);
    if (err) {
        printk("Discover failed (err %d)\n", err);
        error_recovery();
    }
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
    printk("Disconnected: %s (reason 0x%02x)\n", addr, reason);

    if (current_conn != conn) {
        return;
    }

    // Unsubscribe from notifications if we were subscribed
    if (nus_tx_subscribe_params.value_handle) {
        bt_gatt_unsubscribe(conn, &nus_tx_subscribe_params);
        printk("Unsubscribed from NUS notifications\n");
    }

    bt_conn_unref(current_conn);
    current_conn = NULL;

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

    if (!attr) {
        printk("Discovery complete but target not found, type %u\n", params->type);
        
        // If we were looking for a specific UUID but didn't find it, try a more general approach
        if (params->type == BT_GATT_DISCOVER_PRIMARY && params->uuid) {
            // Try general service discovery instead
            printk("Trying general service discovery without UUID filter\n");
            memset(params, 0, sizeof(*params));
            params->func = discover_func;
            params->start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
            params->end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
            params->type = BT_GATT_DISCOVER_PRIMARY;
            
            err = bt_gatt_discover(conn, params);
            if (err) {
                printk("General service discovery failed (err %d)\n", err);
                error_recovery();
            }
            return BT_GATT_ITER_STOP;
        }
        // If we were looking for characteristics but didn't find them, try a general char discovery
        else if (params->type == BT_GATT_DISCOVER_CHARACTERISTIC && 
                 (params->uuid == &nus_rx_uuid.uuid || params->uuid == &nus_tx_uuid.uuid)) {
            
            printk("Specific characteristic not found, trying general characteristic discovery\n");
            // Clear the UUID to find all characteristics in this service
        memset(params, 0, sizeof(*params));
            params->start_handle = current_service_start_handle;
            params->end_handle = nus_service_end_handle;
            params->type = BT_GATT_DISCOVER_CHARACTERISTIC;
            params->func = discover_func;
            
            err = bt_gatt_discover(conn, params);
            if (err) {
                printk("General characteristic discovery failed (err %d)\n", err);
                error_recovery();
            }
            return BT_GATT_ITER_STOP;
        }
        
        if (params->type == BT_GATT_DISCOVER_DESCRIPTOR) {
            printk("CCC descriptor not found, aborting\n");
            error_recovery();
            return BT_GATT_ITER_STOP;
        }
        
        // If no service found by UUID, start scanning again
        printk("No matching service found, restarting scan\n");
        error_recovery();
        return BT_GATT_ITER_STOP;
    }

    // Stage 1: Found NUS Service
    if (params->type == BT_GATT_DISCOVER_PRIMARY) {
        struct bt_gatt_service_val *service_val = (struct bt_gatt_service_val *)attr->user_data;
        if (!service_val || !service_val->uuid) {
            printk("ERROR: Invalid service data\n");
            error_recovery();
            return BT_GATT_ITER_STOP;
        }
        
        // Log the found service UUID for debugging
        if (service_val->uuid->type == BT_UUID_TYPE_128) {
            struct bt_uuid_128 *uuid_128 = (struct bt_uuid_128 *)service_val->uuid;
            printk("Service UUID: ");
            for (int i = 0; i < 16; i++) {
                printk("%02x", uuid_128->val[i]);
            }
            printk("\n");
        }
        
        // Check if this is the NUS service
        if (bt_uuid_cmp(service_val->uuid, (const struct bt_uuid *)&nus_uuid) == 0) {
            printk("NUS service found - start: 0x%04x, end: 0x%04x\n",
               attr->handle, service_val->end_handle);
        
            current_service_start_handle = attr->handle;
            nus_service_end_handle = service_val->end_handle;

            // Search for characteristics within this service
            memset(params, 0, sizeof(*params));
            params->start_handle = attr->handle + 1;
            params->end_handle = service_val->end_handle;
            params->type = BT_GATT_DISCOVER_CHARACTERISTIC;
            params->func = discover_func;
            
            // Search for RX characteristic first
            params->uuid = &nus_rx_uuid.uuid;
            
            err = bt_gatt_discover(conn, params);
        if (err) {
                printk("Characteristic discovery failed (err %d)\n", err);
                error_recovery();
            }
            return BT_GATT_ITER_STOP;
        }
        
        // Not the service we're looking for, continue searching
        return BT_GATT_ITER_CONTINUE;
    }
    // Stage 2: Found either RX or TX characteristic
    else if (params->type == BT_GATT_DISCOVER_CHARACTERISTIC) {
        if (!attr) {
            printk("No attribute found for characteristic discovery\n");
            return BT_GATT_ITER_CONTINUE;
        }
        
        if (!params->uuid) {
            printk("No UUID for characteristic discovery\n");
            return BT_GATT_ITER_CONTINUE;
        }
        
        struct bt_gatt_chrc *chrc = (struct bt_gatt_chrc *)attr->user_data;
        if (!chrc) {
            printk("No characteristic data found\n");
            return BT_GATT_ITER_CONTINUE;
        }
        
        if (!chrc->uuid) {
            printk("No UUID in characteristic data\n");
            return BT_GATT_ITER_CONTINUE;
        }

        // Debug logging of UUID bytes
        if (chrc->uuid->type == BT_UUID_TYPE_128) {
            struct bt_uuid_128 *uuid_128 = (struct bt_uuid_128 *)chrc->uuid;
            printk("Found characteristic with UUID-128: ");
            for (int i = 0; i < 16; i++) {
                printk("%02x", uuid_128->val[i]);
            }
            printk("\n");
            
            // Log the target UUIDs for comparison
            printk("Target RX UUID-128: ");
            for (int i = 0; i < 16; i++) {
                printk("%02x", nus_rx_uuid.val[i]);
            }
            printk("\n");
            
            printk("Target TX UUID-128: ");
            for (int i = 0; i < 16; i++) {
                printk("%02x", nus_tx_uuid.val[i]);
            }
            printk("\n");
        }
        
        // Check for RX characteristic UUID
        if (bt_uuid_cmp(chrc->uuid, (const struct bt_uuid *)&nus_rx_uuid) == 0) {
            printk("Found RX characteristic\n");
            nus_rx_handle = attr->handle + 1;  // +1 because handle is for declaration
            printk("RX handle: 0x%04x\n", nus_rx_handle);
            
            // If we've found both RX and TX, move on to discovering CCC
            if (nus_rx_handle && nus_tx_handle) {
                printk("Found both RX and TX characteristics, looking for CCC descriptor\n");
                
                memset(params, 0, sizeof(*params));
                params->uuid = BT_UUID_GATT_CCC;
                params->start_handle = nus_tx_handle + 1;
                params->end_handle = nus_service_end_handle;
                params->type = BT_GATT_DISCOVER_DESCRIPTOR;
                params->func = discover_func;
                
                err = bt_gatt_discover(conn, params);
                if (err) {
                    printk("Descriptor discovery failed (err %d)\n", err);
                    error_recovery();
                }
                return BT_GATT_ITER_STOP;
            }
            // Otherwise, continue looking for the other characteristic
            return BT_GATT_ITER_CONTINUE;
        }
        // Check for TX characteristic UUID
        else if (bt_uuid_cmp(chrc->uuid, (const struct bt_uuid *)&nus_tx_uuid) == 0) {
            printk("Found TX characteristic\n");
            nus_tx_handle = attr->handle + 1;  // +1 because handle is for declaration
            printk("TX handle: 0x%04x\n", nus_tx_handle);
            
            // If we've found both RX and TX, move on to discovering CCC
            if (nus_rx_handle && nus_tx_handle) {
                printk("Found both RX and TX characteristics, looking for CCC descriptor\n");
                
                memset(params, 0, sizeof(*params));
                params->uuid = BT_UUID_GATT_CCC;
                params->start_handle = nus_tx_handle + 1;
                params->end_handle = nus_service_end_handle;
                params->type = BT_GATT_DISCOVER_DESCRIPTOR;
                params->func = discover_func;
                
                err = bt_gatt_discover(conn, params);
                if (err) {
                    printk("Descriptor discovery failed (err %d)\n", err);
                    error_recovery();
                }
                return BT_GATT_ITER_STOP;
            }
            // Otherwise, continue looking for the other characteristic
            return BT_GATT_ITER_CONTINUE;
        }
        // Not the characteristic we're looking for
        else {
            printk("Found other characteristic, continuing search\n");
            return BT_GATT_ITER_CONTINUE;
        }
    }
    // Stage 3: Found CCC descriptor
    else if (params->type == BT_GATT_DISCOVER_DESCRIPTOR) {
        if (!nus_tx_handle) {
            printk("ERROR: Found CCC but TX handle not set\n");
            error_recovery();
            return BT_GATT_ITER_STOP;
        }
        
        // Check if this is indeed a CCC descriptor
        if (params->uuid && bt_uuid_cmp(params->uuid, BT_UUID_GATT_CCC) != 0) {
            // Not a CCC, keep looking
            printk("Not a CCC descriptor, continuing search\n");
            return BT_GATT_ITER_CONTINUE;
        }
        
        // Print the handle for debugging
        printk("Found descriptor with handle 0x%04x\n", attr->handle);
        
        // Double check that this is within range of the TX handle
        if (attr->handle <= nus_tx_handle || attr->handle > nus_service_end_handle) {
            printk("Descriptor handle out of expected range, continuing search\n");
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

        printk("All required handles found! Setting up notification subscription...\n");
        
        // Setup subscribe parameters
        memset(&nus_tx_subscribe_params, 0, sizeof(nus_tx_subscribe_params));
        nus_tx_subscribe_params.value_handle = nus_tx_handle;
        nus_tx_subscribe_params.ccc_handle = nus_tx_ccc_handle;
        nus_tx_subscribe_params.value = BT_GATT_CCC_NOTIFY;
        nus_tx_subscribe_params.notify = nus_notify_callback;

        // Subscribe to notifications with retry logic
        printk("Subscribing to notifications...\n");
        int subscribe_retries = 0;
        const int max_subscribe_retries = 5;
        
        while (subscribe_retries < max_subscribe_retries) {
        err = bt_gatt_subscribe(conn, &nus_tx_subscribe_params);
            if (err == 0) {
                printk("Subscribed successfully\n");
                
                // Wait a moment before sending test data
                k_sleep(K_MSEC(500));
                
                // Send a test message
                const char *hello_msg = "Hello from Central!";
                err = send_to_peripheral(hello_msg, strlen(hello_msg));
                if (err) {
                    printk("Failed to send test message (err %d)\n", err);
                    // Don't trigger error recovery just for send failure
                } else {
                    printk("Test message sent successfully!\n");
                }
                break; // Success, exit retry loop
            } else if (err == -EALREADY) {
                printk("Already subscribed\n");
                break; // Already subscribed, this is fine
        } else {
                subscribe_retries++;
                printk("Subscribe failed (err %d), retry %d/%d\n", 
                       err, subscribe_retries, max_subscribe_retries);
                
                if (subscribe_retries >= max_subscribe_retries) {
                    printk("Maximum subscribe retries reached, giving up\n");
                    error_recovery();
                    break;
                }
                
                // Wait before retrying
                k_sleep(K_MSEC(500 * subscribe_retries));
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
static int complete_bt_reset(void)
{
    printk("Performing complete Bluetooth stack reset...\n");
    int err;
    
    // Make sure scan is stopped first, with retries for EAGAIN
    for (int i = 0; i < 5; i++) {
        err = bt_le_scan_stop();
        if (err == 0) {
            printk("Successfully stopped scanning\n");
            break;
        } else if (err == -EAGAIN || err == -11) {
            printk("Scan stop got EAGAIN, retrying... (%d)\n", i+1);
            k_sleep(K_MSEC(1000));
        } else {
            printk("Scan stop failed with err %d, continuing reset\n", err);
            break;
        }
    }
    
    // Disconnect and clean up any connections
    if (current_conn) {
        printk("Disconnecting from existing connection...\n");
        bt_conn_disconnect(current_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
        k_sleep(K_MSEC(100)); // Give time for disconnect to initiate
        bt_conn_unref(current_conn);
        current_conn = NULL;
    }
    
    // Release all other resources
    // This will also terminate any active connections
    printk("Cleaning up resources...\n");
    
    // Wait to ensure controller processed disconnection
    k_sleep(K_MSEC(1000));
    
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
    
    // Try to disable BT with retries for EAGAIN
    for (int i = 0; i < 5; i++) {
        err = bt_disable();
        if (err == 0) {
            printk("Bluetooth disabled successfully\n");
            break;
        } else if (err == -EAGAIN || err == -11) {
            printk("BT disable got EAGAIN, retrying... (%d)\n", i+1);
            k_sleep(K_MSEC(1000));
        } else {
            printk("Failed to disable Bluetooth (err %d), continuing reset\n", err);
            break;
        }
    }
    
    // Wait for controller to fully shut down - longer wait
    k_sleep(K_MSEC(3000));
    
    // Re-enable Bluetooth with our bt_ready callback
    printk("Re-enabling Bluetooth...\n");
    
    // Try to re-enable with retries
    for (int i = 0; i < 5; i++) {
        err = bt_enable(bt_ready);
        if (err == 0) {
            printk("Bluetooth re-enabled successfully\n");
            break;
        } else if (err == -EAGAIN || err == -11) {
            printk("BT enable got EAGAIN, retrying... (%d)\n", i+1);
            k_sleep(K_MSEC(1000 * (i+1))); // Increasing backoff
        } else {
            printk("Failed to re-enable Bluetooth (err %d), trying once more\n", err);
            k_sleep(K_MSEC(2000));
            err = bt_enable(bt_ready);
            if (err) {
                printk("Second attempt to re-enable Bluetooth failed (err %d)\n", err);
            } else {
                printk("Second attempt to re-enable Bluetooth succeeded\n");
                break;
            }
            break;
        }
    }
    
    // Wait for BT stack to fully initialize
    k_sleep(K_MSEC(2000));
    
    printk("BT reset complete. Starting scan after delay...\n");
    k_work_schedule(&start_scan_work, K_MSEC(2000));
    return 0;
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
            if (current_conn) {
                bt_conn_disconnect(current_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
                bt_conn_unref(current_conn);
                current_conn = NULL;
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
            err = bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN, param, &current_conn);
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
    int retry = 0;
    const int max_retries = 5;

    if (current_conn) {
        // Check if the connection is valid
        struct bt_conn_info info;
        err = bt_conn_get_info(current_conn, &info);
        
        if (err || info.state != BT_CONN_STATE_CONNECTED) {
            printk("Connection in invalid state. Cleaning up...\n");
            bt_conn_disconnect(current_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
            bt_conn_unref(current_conn);
            current_conn = NULL;
            
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

    // Try multiple times if we get EAGAIN
    while (retry < max_retries) {
    err = bt_le_scan_start(&scan_param, device_found);
    if (err) {
            if (err == -EAGAIN || err == -11) { // -11 is EAGAIN
                retry++;
                printk("Scan start temporarily failed (EAGAIN), retry %d/%d\n", 
                      retry, max_retries);
                // Wait progressively longer between retries
                k_sleep(K_MSEC(500 * retry));
                continue;
            } else if (err == -EALREADY) {
                printk("Scan already started\n");
                break;
            } else {
        printk("Scanning failed to start (err %d)\n", err);
                // For other errors, back off and try again later
                k_work_schedule(&start_scan_work, K_MSEC(2000));
                return;
            }
        } else {
            // Success
            break;
        }
    }

    if (retry >= max_retries) {
        printk("WARNING: Failed to start scan after %d retries!\n", max_retries);
        // Try to recover by resetting Bluetooth
        complete_bt_reset();
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
    if (current_conn) {
        printk("Clearing existing connection on initialization\n");
        bt_conn_disconnect(current_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
        bt_conn_unref(current_conn);
        current_conn = NULL;
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

    printk("BLE Central Starting\n");
    
    // Initialize Bluetooth with enhanced callback-based approach
    err = bt_enable(bt_ready);
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return -1;
    }

    printk("Bluetooth initialized\n");
    
    // Make sure any existing connections are cleaned up
    if (current_conn) {
        printk("Clearing existing connection on initialization\n");
        bt_conn_disconnect(current_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
        bt_conn_unref(current_conn);
        current_conn = NULL;
    }
    
    // Initialize tracking variables
    consecutive_errors = 0;
    emergency_reset_count = 0;
    last_connection_time = k_uptime_get();
    
    // Main loop
    while (1) {
        // Sleep for a bit to allow BT stack to process
        k_sleep(K_SECONDS(10));
        
        // Periodic check to recover from stuck states
        if (!current_conn) {
            // If we're not connected, try to restart scan
            // This is safe to call even if already scanning
            int64_t time_since_last_conn = k_uptime_get() - last_connection_time;
            
            if (time_since_last_conn > (2 * 60 * MSEC_PER_SEC)) {  // 2 minutes
                consecutive_errors++;
                printk("No connection for %lld seconds, consecutive_errors: %d\n", 
                       time_since_last_conn / MSEC_PER_SEC, consecutive_errors);
                
                if (consecutive_errors >= 5 && emergency_reset_count < 2) {
                    printk("Too many consecutive failures - attempting emergency reset\n");
                    emergency_bt_reset();
                    emergency_reset_count++;
                    consecutive_errors = 0;
                    last_connection_time = k_uptime_get();
                } else {
                    // Try a regular reset
                    complete_bt_reset();
                }
            } else {
                // Just ensure we're scanning
                start_scan();
            }
        } else {
            // We have a connection, reset error counter
            consecutive_errors = 0;
            last_connection_time = k_uptime_get();
        }
    }
    
    return 0;
}