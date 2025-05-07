#include <zephyr/types.h>
#include <stddef.h>
#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/settings/settings.h>

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/sys/byteorder.h>

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
static uint16_t nus_rx_handle;
static uint16_t nus_tx_handle;
static uint16_t nus_tx_ccc_handle;
static struct bt_gatt_subscribe_params nus_tx_subscribe_params;
static bool device_found_flag;
static bool is_scanning = false;
static volatile bool connection_stable = false;
static volatile bool subscription_active = false;

// Work queue for delayed operations
static struct k_work_q work_q;
K_THREAD_STACK_DEFINE(work_q_stack, 2048);

// Work items for delayed operations
static struct k_work_delayable start_scan_work;
static struct k_work_delayable send_test_message_work;

// Callback for receiving notifications from the NUS TX characteristic
static uint8_t nus_notify_callback(struct bt_conn *conn,
                              struct bt_gatt_subscribe_params *params,
                              const void *data, uint16_t length)
{
    if (!data) {
        printk("Unsubscribed from NUS TX\n");
        subscription_active = false;
        // Don't change params->value_handle here as it may unsubscribe
        return BT_GATT_ITER_CONTINUE;  // Return CONTINUE to keep subscription
    }

    // Set flags to indicate successful notification received
    connection_stable = true;
    subscription_active = true;

    char msg[65];
    memcpy(msg, data, MIN(length, sizeof(msg) - 1));
    msg[MIN(length, sizeof(msg) - 1)] = '\0';
    
    printk("Received from peripheral: %s\n", msg);
    
    // Check if it's a rental data message
    if (strstr(msg, "RENTAL START") != NULL) {
        printk("Rental data detected!\n");
        // Process the rental data here
    }

    return BT_GATT_ITER_CONTINUE;  // Keep subscription
}

// Function to send data to the peripheral via NUS RX characteristic
static int send_to_peripheral(const char *data, uint16_t len)
{
    int err;
    
    if (!default_conn || !nus_rx_handle) {
        printk("Not connected or NUS RX handle not found\n");
        return -EINVAL;
    }

    // Use simple GATT write without response for reliability
    err = bt_gatt_write_without_response(default_conn, nus_rx_handle, 
                                        data, len, false);
    if (err) {
        printk("Failed to send message (err %d)\n", err);
    } else {
        printk("Sent to peripheral: %s\n", data);
    }
    
    return err;
}

// Handler for sending a test message
static void send_test_message_handler(struct k_work *work)
{
    if (default_conn && nus_rx_handle) {
        const char *test_msg = "Hello from Central!";
        int err = send_to_peripheral(test_msg, strlen(test_msg));
        if (err) {
            printk("Failed to send test message (err %d)\n", err);
            // Try again later
            k_work_schedule_for_queue(&work_q, &send_test_message_work, K_MSEC(2000));
        } else {
            printk("Test message sent successfully\n");
        }
    } else {
        printk("Not connected, delaying test message\n");
        // Try again later
        k_work_schedule_for_queue(&work_q, &send_test_message_work, K_MSEC(2000));
    }
}

// Descriptor discovery callback
static uint8_t descriptor_discover_func(struct bt_conn *conn,
                                   const struct bt_gatt_attr *attr,
                                   struct bt_gatt_discover_params *params)
{
    int err;
    
    if (!attr) {
        printk("Descriptor Discovery complete\n");
        
        // Try to subscribe even if CCC isn't explicitly found
        if (nus_tx_handle) {
            printk("Setting up subscription to NUS TX characteristic\n");
            
            // Make sure we clear the structure first
            memset(&nus_tx_subscribe_params, 0, sizeof(nus_tx_subscribe_params));
            
            nus_tx_subscribe_params.value_handle = nus_tx_handle;
            nus_tx_subscribe_params.notify = nus_notify_callback;
            nus_tx_subscribe_params.value = BT_GATT_CCC_NOTIFY;
            
            // Auto-discover CCC handle if not explicitly found
            if (nus_tx_ccc_handle) {
                nus_tx_subscribe_params.ccc_handle = nus_tx_ccc_handle;
            } else {
                nus_tx_subscribe_params.ccc_handle = 0; 
            }
            
            err = bt_gatt_subscribe(conn, &nus_tx_subscribe_params);
            if (err && err != -EALREADY) {
                printk("Subscribe failed (err %d)\n", err);
                
                // Try a direct CCC write as fallback
                if (nus_tx_ccc_handle) {
                    uint16_t ccc_value = BT_GATT_CCC_NOTIFY;
                    err = bt_gatt_write_without_response(conn, nus_tx_ccc_handle,
                                                        &ccc_value, sizeof(ccc_value), false);
                    if (err) {
                        printk("Failed to write CCC (err %d)\n", err);
                    } else {
                        printk("Wrote CCC directly\n");
                        subscription_active = true;
                    }
                }
            } else {
                printk("Subscribed to NUS TX characteristic\n");
                subscription_active = true;
                
                // Schedule sending a test message after a delay
                k_work_schedule_for_queue(&work_q, &send_test_message_work, K_MSEC(1000));
            }
        }
        
        return BT_GATT_ITER_STOP;
    }
    
    printk("Descriptor found, handle: %u\n", attr->handle);
    
    // Check if it's a CCC descriptor
    if (bt_uuid_cmp(attr->uuid, BT_UUID_GATT_CCC) == 0) {
        printk("CCC descriptor found, handle: %u\n", attr->handle);
        nus_tx_ccc_handle = attr->handle;
    }
    
    return BT_GATT_ITER_CONTINUE;
}

// Characteristic discovery callback
static uint8_t characteristic_discover_func(struct bt_conn *conn,
                                      const struct bt_gatt_attr *attr,
                                      struct bt_gatt_discover_params *params)
{
    int err;
    
    if (!attr) {
        printk("Characteristic Discovery complete\n");
        
        // If both RX and TX are found, start descriptor discovery
        if (nus_rx_handle && nus_tx_handle) {
            printk("Both RX and TX characteristics found, discovering descriptors\n");
            
            discover_params.uuid = BT_UUID_GATT_CCC;
            discover_params.start_handle = nus_tx_handle;
            discover_params.end_handle = 0xffff;
            discover_params.type = BT_GATT_DISCOVER_DESCRIPTOR;
            discover_params.func = descriptor_discover_func;
            
            err = bt_gatt_discover(conn, &discover_params);
            if (err) {
                printk("Descriptor discover failed (err %d)\n", err);
                
                // Try subscribing without explicit CCC discovery
                memset(&nus_tx_subscribe_params, 0, sizeof(nus_tx_subscribe_params));
                nus_tx_subscribe_params.value_handle = nus_tx_handle;
                nus_tx_subscribe_params.notify = nus_notify_callback;
                nus_tx_subscribe_params.value = BT_GATT_CCC_NOTIFY;
                nus_tx_subscribe_params.ccc_handle = 0; // Auto-discover
                
                err = bt_gatt_subscribe(conn, &nus_tx_subscribe_params);
                if (err && err != -EALREADY) {
                    printk("Subscribe failed (err %d)\n", err);
                } else {
                    printk("Subscribed to NUS TX characteristic\n");
                    subscription_active = true;
                    
                    // Schedule sending a test message after a delay
                    k_work_schedule_for_queue(&work_q, &send_test_message_work, K_MSEC(1000));
                }
            }
        } 
        // If only RX or TX is found, try general characteristic discovery
        else {
            printk("Missing RX or TX characteristic, trying general discovery\n");
            
            discover_params.uuid = NULL;
            discover_params.start_handle = params->start_handle;
            discover_params.end_handle = params->end_handle;
            discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;
            discover_params.func = characteristic_discover_func;
            
            err = bt_gatt_discover(conn, &discover_params);
            if (err) {
                printk("General characteristic discover failed (err %d)\n", err);
            }
        }
        
        return BT_GATT_ITER_STOP;
    }
    
    // Check if this is a characteristic declaration
    if (params->type == BT_GATT_DISCOVER_CHARACTERISTIC) {
        struct bt_gatt_chrc *chrc = (struct bt_gatt_chrc *)attr->user_data;
        
        if (chrc && chrc->uuid) {
            // Is this the RX characteristic?
            if (bt_uuid_cmp(chrc->uuid, BT_UUID_NUS_RX) == 0) {
                printk("NUS RX characteristic found, handle: %u\n", attr->handle);
                nus_rx_handle = attr->handle + 1;  // Value handle is the next handle
            }
            // Is this the TX characteristic?
            else if (bt_uuid_cmp(chrc->uuid, BT_UUID_NUS_TX) == 0) {
                printk("NUS TX characteristic found, handle: %u\n", attr->handle);
                nus_tx_handle = attr->handle + 1;  // Value handle is the next handle
            }
        }
    }
    
    return BT_GATT_ITER_CONTINUE;
}

// Service discovery callback
static uint8_t service_discover_func(struct bt_conn *conn,
                                const struct bt_gatt_attr *attr,
                                struct bt_gatt_discover_params *params)
{
    int err;
    
    if (!attr) {
        printk("Service Discovery complete, NUS service not found\n");
        return BT_GATT_ITER_STOP;
    }
    
    // We found the NUS service
    printk("NUS service found, handle range: %u to %u\n", 
           attr->handle, params->end_handle);
    
    // Now discover characteristics within the service
    discover_params.uuid = NULL; // Discover all characteristics
    discover_params.start_handle = attr->handle + 1;
    discover_params.end_handle = params->end_handle;
    discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;
    discover_params.func = characteristic_discover_func;
    
    err = bt_gatt_discover(conn, &discover_params);
    if (err) {
        printk("Characteristic discover failed (err %d)\n", err);
    }
    
    return BT_GATT_ITER_STOP;
}

static void connected(struct bt_conn *conn, uint8_t conn_err)
{
    char addr[BT_ADDR_LE_STR_LEN];
    int err;

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    if (conn_err) {
        printk("Failed to connect to %s (err %u)\n", addr, conn_err);

        if (default_conn) {
            bt_conn_unref(default_conn);
            default_conn = NULL;
        }

        // Mark scanning as stopped and connection as unstable
        is_scanning = false;
        connection_stable = false;
        
        // Restart scan with a delay
        k_work_schedule_for_queue(&work_q, &start_scan_work, K_MSEC(1000));
        return;
    }

    printk("Connected to RentScan device: %s\n", addr);

    if (conn == default_conn) {
        // Reset handles for new connection
        nus_rx_handle = 0;
        nus_tx_handle = 0;
        nus_tx_ccc_handle = 0;
        subscription_active = false;
        
        // Slight delay before starting service discovery
        k_sleep(K_MSEC(200));
        
        // Start service discovery
        memset(&discover_params, 0, sizeof(discover_params));
        discover_params.uuid = BT_UUID_NUS_SERVICE;
        discover_params.func = service_discover_func;
        discover_params.start_handle = 0x0001;
        discover_params.end_handle = 0xffff;
        discover_params.type = BT_GATT_DISCOVER_PRIMARY;

        err = bt_gatt_discover(default_conn, &discover_params);
        if (err) {
            printk("Service discover failed (err %d)\n", err);
            return;
        }
    }
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    printk("Disconnected from %s (reason 0x%02x)\n", addr, reason);

    if (default_conn != conn) {
        return;
    }

    bt_conn_unref(default_conn);
    default_conn = NULL;

    // Reset handles and state
    nus_rx_handle = 0;
    nus_tx_handle = 0;
    nus_tx_ccc_handle = 0;
    connection_stable = false;
    subscription_active = false;
    
    // Only restart scanning if disconnected unexpectedly
    if (reason != BT_HCI_ERR_REMOTE_USER_TERM_CONN && 
        reason != BT_HCI_ERR_LOCALHOST_TERM_CONN) {
        printk("Unexpected disconnection, restarting scan...\n");
        is_scanning = false;
        k_work_schedule_for_queue(&work_q, &start_scan_work, K_MSEC(2000));
    }
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

// Check if the device name "RentScan" is in the advertising data
static bool check_device_name(struct bt_data *data, void *user_data)
{
    // Looking for the device name in the advertising data
    if (data->type == BT_DATA_NAME_COMPLETE || data->type == BT_DATA_NAME_SHORTENED) {
        printk("Found device with name: ");
        for (size_t i = 0; i < data->data_len; i++) {
            printk("%c", data->data[i]);
        }
        printk("\n");
        
        if (data->data_len == 8 && memcmp(data->data, "RentScan", 8) == 0) {
            // Found the RentScan device!
            printk("Found RentScan device!\n");
            device_found_flag = true;
            return false; // Stop parsing, we found what we need
        }
    }
    
    return true; // Continue parsing all advertising data
}

static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
                     struct net_buf_simple *ad)
{
    char dev[BT_ADDR_LE_STR_LEN];
    
    bt_addr_le_to_str(addr, dev, sizeof(dev));
    printk("[DEVICE]: %s, AD evt type %u, AD data len %u, RSSI %i\n",
           dev, type, ad->len, rssi);

    // We're only interested in connectable devices with sufficient advertising data
    if ((type == BT_GAP_ADV_TYPE_ADV_IND ||
         type == BT_GAP_ADV_TYPE_ADV_DIRECT_IND) && ad->len > 0) {
        
        // Reset the found flag before parsing
        device_found_flag = false;
        
        // Parse the advertising data to find the device name
        bt_data_parse(ad, check_device_name, NULL);
        
        if (device_found_flag) {
            int err;
            
            printk("Stopping scan to connect...\n");
            err = bt_le_scan_stop();
            if (err) {
                printk("Stop LE scan failed (err %d)\n", err);
                return;
            }
            
            // Mark scanning as stopped
            is_scanning = false;

            printk("Initiating connection to RentScan device...\n");
            
            // Use more conservative connection parameters
            struct bt_le_conn_param *param = BT_LE_CONN_PARAM_DEFAULT;
            err = bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN, param, &default_conn);
            if (err) {
                printk("Create conn failed (err %d)\n", err);
                // Restart scanning after a delay
                k_work_schedule_for_queue(&work_q, &start_scan_work, K_MSEC(1000));
            }
        }
    }
}

static void start_scan_handler(struct k_work *work)
{
    int err;

    // Check if we're already scanning
    if (is_scanning) {
        printk("Already scanning, skipping scan start\n");
        return;
    }

    // Check if we're already connected
    if (default_conn) {
        printk("Already connected, skipping scan start\n");
        return;
    }

    printk("Starting BLE scan with scan_type=active...\n");

    // Try to use a simpler scan parameter setup
    struct bt_le_scan_param scan_param = {
        .type       = BT_LE_SCAN_TYPE_ACTIVE,
        .options    = BT_LE_SCAN_OPT_NONE,
        .interval   = BT_GAP_SCAN_FAST_INTERVAL,
        .window     = BT_GAP_SCAN_FAST_WINDOW,
    };

    err = bt_le_scan_start(&scan_param, device_found);
    if (err) {
        if (err == -EAGAIN) {
            // EAGAIN means resource temporarily unavailable
            printk("Scan start failed with EAGAIN, trying again in 2 seconds\n");
            k_work_schedule_for_queue(&work_q, &start_scan_work, K_MSEC(2000));
        } else {
            printk("Scanning failed to start (err %d), retrying in 3 seconds\n", err);
            k_work_schedule_for_queue(&work_q, &start_scan_work, K_MSEC(3000));
        }
        return;
    }

    // Mark scanning as started
    is_scanning = true;
    printk("Scanning for RentScan device started successfully\n");
}

static void start_scan(void)
{
    k_work_schedule_for_queue(&work_q, &start_scan_work, K_NO_WAIT);
}

static void init_bt(void)
{
    printk("Initializing BT subsystem...\n");
    
    int err = bt_enable(NULL);
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return;
    }

    printk("Bluetooth initialized successfully\n");
    
    // Load persistent settings (addresses, bonding info)
    err = settings_load();
    if (err) {
        printk("Failed to load settings (err %d)\n", err);
    }
    
    // Delay before starting scan to ensure controller is ready
    k_sleep(K_MSEC(500));
    
    // Start scanning
    start_scan();
}

void main(void)
{
    printk("RentScan Central Application\n");
    
    // Initialize the work queue
    k_work_queue_start(&work_q, work_q_stack,
                      K_THREAD_STACK_SIZEOF(work_q_stack),
                      CONFIG_SYSTEM_WORKQUEUE_PRIORITY - 1, NULL);
    
    // Initialize delayed work items
    k_work_init_delayable(&start_scan_work, start_scan_handler);
    k_work_init_delayable(&send_test_message_work, send_test_message_handler);
    
    // Start the initialization process
    init_bt();
    
    // Main application loop - now just wait for NFC events
    printk("Waiting for NFC events from peripheral...\n");
    
    while (1) {
        // Periodically check connection status
        if (default_conn) {
            if (subscription_active) {
                printk("Connection active with notifications enabled\n");
            } else {
                printk("Connected but notifications not enabled. Trying to send a test message...\n");
                // Try to send a message - this may help the peripheral detect the connection
                const char *hello_msg = "Hello from Central";
                send_to_peripheral(hello_msg, strlen(hello_msg));
                
                // Re-subscribe if needed
                if (nus_tx_handle && !subscription_active) {
                    printk("Re-subscribing to notifications...\n");
                    memset(&nus_tx_subscribe_params, 0, sizeof(nus_tx_subscribe_params));
                    nus_tx_subscribe_params.value_handle = nus_tx_handle;
                    nus_tx_subscribe_params.notify = nus_notify_callback;
                    nus_tx_subscribe_params.value = BT_GATT_CCC_NOTIFY;
                    nus_tx_subscribe_params.ccc_handle = 0; // Auto-discover
                    
                    int err = bt_gatt_subscribe(default_conn, &nus_tx_subscribe_params);
                    if (err && err != -EALREADY) {
                        printk("Subscribe failed (err %d)\n", err);
                    } else {
                        printk("Subscription attempt made\n");
                    }
                }
            }
        }
        
        k_sleep(K_SECONDS(5));  // Check every 5 seconds
    }
}