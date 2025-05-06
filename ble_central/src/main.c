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

static void start_scan(void);

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

// Forward declarations
static uint8_t discover_func(struct bt_conn *conn,
                         const struct bt_gatt_attr *attr,
                         struct bt_gatt_discover_params *params);

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
    k_sleep(K_MSEC(300));

    // Start GATT exchange (optional but recommended for better performance)
    // Disable MTU exchange for now as it may be causing issues
    /*
    struct bt_gatt_exchange_params exchange_params = {
        .func = NULL  // We don't need a callback for this
    };
    err = bt_gatt_exchange_mtu(conn, &exchange_params);
    if (err) {
        printk("MTU exchange failed (err %d)\n", err);
    }
    
    // Allow some time for MTU exchange to complete
    k_sleep(K_MSEC(100));
    */

    printk("Starting service discovery...\n");
    
    // Initialize discovery parameters
    memset(&discover_params, 0, sizeof(discover_params));
    discover_params.uuid = BT_UUID_NUS_SERVICE;
    discover_params.func = discover_func;
    discover_params.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
    discover_params.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
    discover_params.type = BT_GATT_DISCOVER_PRIMARY;
    
    printk("Discovery params: UUID ptr %p, func %p\n", 
           discover_params.uuid, discover_params.func);

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

    // Defensive check for NULL UUID
    if (!params->uuid) {
        printk("ERROR: NULL UUID in discovery params\n");
        error_recovery();
        return BT_GATT_ITER_STOP;
    }

    if (!attr) {
        printk("Discovery complete but target not found, type %u\n", params->type);
        
        // If we were looking for the NUS service and didn't find it, trigger recovery
        if (params->type == BT_GATT_DISCOVER_PRIMARY && 
            bt_uuid_cmp(params->uuid, BT_UUID_NUS_SERVICE) == 0) {
            printk("ERROR: NUS service not found\n");
            error_recovery();
        }
        
        // Clear the discovery parameters
        memset(params, 0, sizeof(*params));
        return BT_GATT_ITER_STOP;
    }

    printk("Discovery: attr handle 0x%04x, type %u\n", attr->handle, params->type);

    // Stage 1: Discovered Primary NUS Service
    if (params->type == BT_GATT_DISCOVER_PRIMARY) {
        if (!attr->user_data) {
            printk("ERROR: NULL user_data for service\n");
            error_recovery();
            return BT_GATT_ITER_STOP;
        }
        
        // Verify this is actually the NUS service
        struct bt_gatt_service_val *service_val = attr->user_data;
        if (bt_uuid_cmp(service_val->uuid, BT_UUID_NUS_SERVICE) != 0) {
            printk("ERROR: Found service is not NUS service\n");
            return BT_GATT_ITER_CONTINUE; // Continue searching for NUS
        }
        
        printk("Found NUS Service: attr_handle 0x%04x, end_handle 0x%04x\n",
               attr->handle, service_val->end_handle);
        
        current_service_start_handle = attr->handle;
        nus_service_end_handle = service_val->end_handle;

        // Discover NUS RX Characteristic within this service
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
    // Stage 2: Discovered NUS RX Characteristic
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
        
        // Check if this is the RX or TX characteristic
        if (params->uuid == BT_UUID_NUS_RX || 
            (params->uuid && bt_uuid_cmp(params->uuid, BT_UUID_NUS_RX) == 0)) {
            
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
        } else if (params->uuid == BT_UUID_NUS_TX || 
                  (params->uuid && bt_uuid_cmp(params->uuid, BT_UUID_NUS_TX) == 0)) {
            
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

static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
                     struct net_buf_simple *ad)
{
    char dev[BT_ADDR_LE_STR_LEN];
    int err;
    static int retry_count = 0;
    
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
            
            // Check if we might have a stale connection
            if (default_conn) {
                printk("Cleaning up possible stale connection...\n");
                bt_conn_disconnect(default_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
                bt_conn_unref(default_conn);
                default_conn = NULL;
                
                // Reset all handles and parameters
                nus_rx_handle = 0;
                nus_tx_handle = 0;
                nus_tx_ccc_handle = 0;
                current_service_start_handle = 0;
                nus_service_end_handle = 0;
                memset(&discover_params, 0, sizeof(discover_params));
                memset(&nus_tx_subscribe_params, 0, sizeof(nus_tx_subscribe_params));
                
                // Add a delay to allow the stack to clean up
                k_sleep(K_MSEC(500));
            }

            struct bt_le_conn_param *param = BT_LE_CONN_PARAM_DEFAULT;
            printk("Attempting to connect to %s\n", dev);
            err = bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN, param, &default_conn);
            if (err) {
                printk("Create conn failed (err %d)\n", err);
                
                if (err == -EINVAL && retry_count < 3) {
                    // Handle "Found valid connection in disconnected state" error
                    retry_count++;
                    printk("Connection state issue detected, resetting BT state (attempt %d/3)\n", retry_count);
                    
                    // When we get EINVAL on connection attempt, we need to reset the BT stack state
                    // This will call bt_le_scan_stop() internally
                    bt_le_scan_stop();
                    
                    // Wait a moment to ensure everything's stopped
                    k_sleep(K_MSEC(1000));
                    
                    // Then start a fresh scan after a delay
                    k_work_schedule(&start_scan_work, K_MSEC(2000));
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
    
    while (1) {
        k_sleep(K_SECONDS(10));
        
        // Periodic check to recover from stuck states
        if (!default_conn) {
            // If we're not connected, try to restart scan
            // This is safe to call even if already scanning
            printk("Periodic check: ensuring scan is active\n");
            start_scan();
        } else {
            printk("Periodic check: connected to device\n");
        }
    }
    
    return 0;
}