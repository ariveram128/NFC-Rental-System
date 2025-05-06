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
static struct bt_uuid *search_service_uuid = BT_UUID_NUS_SERVICE;
static struct bt_gatt_discover_params discover_params;
static uint16_t nus_rx_handle;
static uint16_t nus_tx_handle;
static struct bt_gatt_subscribe_params nus_tx_subscribe_params;
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

    char msg[65];
    memcpy(msg, data, MIN(length, sizeof(msg) - 1));
    msg[MIN(length, sizeof(msg) - 1)] = '\0';
    
    printk("Received from peripheral: %s\n", msg);
    
    // Check if it's a rental data message
    if (strstr(msg, "RENTAL START") != NULL) {
        printk("Rental data detected!\n");
        // Process the rental data here
    }

    return BT_GATT_ITER_CONTINUE;
}

// Function to send data to the peripheral via NUS RX characteristic
static int send_to_peripheral(const char *data, uint16_t len)
{
    if (!default_conn || !nus_rx_handle) {
        printk("Not connected or NUS RX handle not found\n");
        return -EINVAL;
    }

    struct bt_gatt_write_params write_params = {
        .func = NULL,
        .handle = nus_rx_handle,
        .offset = 0,
        .data = data,
        .length = len,
    };

    return bt_gatt_write(default_conn, &write_params);
}

// Update the connected callback to be simpler and more direct
static void connected(struct bt_conn *conn, uint8_t conn_err)
{
    char addr[BT_ADDR_LE_STR_LEN];

    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    if (conn_err) {
        printk("Failed to connect to %s (%u)\n", addr, conn_err);

        bt_conn_unref(default_conn);
        default_conn = NULL;

        start_scan();
        return;
    }

    printk("Connected: %s\n", addr);

    if (conn == default_conn) {
        // Wait longer after connection before starting discovery (important)
        k_sleep(K_MSEC(1000));
        
        // Use the most direct approach - define handles based on common NUS implementation
        printk("Using direct handle approach for NUS service\n");
        
        // Common NUS handle values
        nus_rx_handle = 0x0011; // RX characteristic value handle
        nus_tx_handle = 0x0013; // TX characteristic value handle
        
        // Set up subscription parameters
        memset(&nus_tx_subscribe_params, 0, sizeof(nus_tx_subscribe_params));
        nus_tx_subscribe_params.value_handle = nus_tx_handle;
        nus_tx_subscribe_params.notify = nus_notify_callback;
        nus_tx_subscribe_params.value = BT_GATT_CCC_NOTIFY;
        nus_tx_subscribe_params.ccc_handle = 0x0014; // CCC descriptor handle
        
        printk("Subscribing directly to TX handle: 0x%04x, CCC: 0x%04x\n", 
                nus_tx_handle, nus_tx_subscribe_params.ccc_handle);
        
        int err = bt_gatt_subscribe(conn, &nus_tx_subscribe_params);
        if (err && err != -EALREADY) {
            printk("Direct subscribe failed (err %d)\n", err);
        } else {
            printk("Direct subscription successful!\n");
            
            // Send a test message after a delay
            k_sleep(K_MSEC(1000));
            const char *test_msg = "Hello from Central!";
            err = send_to_peripheral(test_msg, strlen(test_msg));
            if (err) {
                printk("Failed to send message (err %d)\n", err);
            } else {
                printk("Sent: %s\n", test_msg);
            }
        }
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

    bt_conn_unref(default_conn);
    default_conn = NULL;

    // Reset handles
    nus_rx_handle = 0;
    nus_tx_handle = 0;

    start_scan();
}

// Define connection callbacks
BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

static uint8_t discover_func(struct bt_conn *conn,
                         const struct bt_gatt_attr *attr,
                         struct bt_gatt_discover_params *params)
{
    int err;

    if (!attr) {
        printk("Discover complete for type %u\n", params->type);

        // If we were discovering the primary service
        if (params->type == BT_GATT_DISCOVER_PRIMARY) {
            // Now look specifically for the RX characteristic
            discover_params.uuid = BT_UUID_NUS_RX;
            discover_params.start_handle = params->start_handle;
            discover_params.end_handle = params->end_handle;
            discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;
            
            printk("Starting RX characteristic discovery\n");
            err = bt_gatt_discover(conn, &discover_params);
            if (err) {
                printk("RX characteristic discover failed (err %d)\n", err);
                return BT_GATT_ITER_STOP;
            }
            return BT_GATT_ITER_STOP;
        }
        
        // If we were discovering the RX characteristic
        else if (params->type == BT_GATT_DISCOVER_CHARACTERISTIC && 
                 params->uuid && bt_uuid_cmp(params->uuid, BT_UUID_NUS_RX) == 0) {
            
            // Now look for the TX characteristic
            discover_params.uuid = BT_UUID_NUS_TX;
            discover_params.start_handle = params->start_handle;
            discover_params.end_handle = params->end_handle;
            discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;
            
            printk("Starting TX characteristic discovery\n");
            err = bt_gatt_discover(conn, &discover_params);
            if (err) {
                printk("TX characteristic discover failed (err %d)\n", err);
                return BT_GATT_ITER_STOP;
            }
            return BT_GATT_ITER_STOP;
        }
        
        // If we were discovering the TX characteristic
        else if (params->type == BT_GATT_DISCOVER_CHARACTERISTIC && 
                params->uuid && bt_uuid_cmp(params->uuid, BT_UUID_NUS_TX) == 0) {
            
            // Check if we found the TX handle
            if (!nus_tx_handle) {
                printk("Did not find TX characteristic\n");
                
                // Try to discover TX as a specific characteristic
                printk("Trying direct TX characteristic discovery\n");
                discover_params.uuid = BT_UUID_NUS_TX;
                discover_params.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
                discover_params.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
                discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;
                
                err = bt_gatt_discover(conn, &discover_params);
                if (err) {
                    printk("Direct TX discover failed (err %d)\n", err);
                    return BT_GATT_ITER_STOP;
                }
                return BT_GATT_ITER_STOP;
            }
            
            // If we have TX handle, look for CCC descriptor
            printk("Found TX handle 0x%04x, looking for CCC descriptor\n", nus_tx_handle);
            discover_params.uuid = BT_UUID_GATT_CCC;
            discover_params.start_handle = nus_tx_handle;
            discover_params.end_handle = 0xffff;
            discover_params.type = BT_GATT_DISCOVER_DESCRIPTOR;
            
            err = bt_gatt_discover(conn, &discover_params);
            if (err) {
                printk("CCC discover failed (err %d), trying direct subscription\n", err);
                
                // Try to subscribe anyway with best guess for CCC
                memset(&nus_tx_subscribe_params, 0, sizeof(nus_tx_subscribe_params));
                nus_tx_subscribe_params.value_handle = nus_tx_handle;
                nus_tx_subscribe_params.notify = nus_notify_callback;
                nus_tx_subscribe_params.value = BT_GATT_CCC_NOTIFY;
                nus_tx_subscribe_params.ccc_handle = nus_tx_handle + 2; // Common offset
                
                err = bt_gatt_subscribe(conn, &nus_tx_subscribe_params);
                if (err && err != -EALREADY) {
                    printk("Subscribe failed (err %d), trying other offsets\n", err);
                    
                    // Try different offsets for CCC handle
                    const uint16_t offsets[] = { 1, 3, 4 };
                    for (int i = 0; i < sizeof(offsets)/sizeof(offsets[0]); i++) {
                        nus_tx_subscribe_params.ccc_handle = nus_tx_handle + offsets[i];
                        printk("Trying with CCC handle 0x%04x\n", nus_tx_subscribe_params.ccc_handle);
                        
                        err = bt_gatt_subscribe(conn, &nus_tx_subscribe_params);
                        if (err != -EINVAL) {
                            printk("Subscription successful with offset %u!\n", offsets[i]);
                            break;
                        }
                    }
                } else {
                    printk("Best guess subscription successful!\n");
                }
            }
            return BT_GATT_ITER_STOP;
        }
        
        // If we were discovering the CCC descriptor
        else if (params->type == BT_GATT_DISCOVER_DESCRIPTOR) {
            printk("CCC discovery complete\n");
            
            if (nus_tx_handle) {
                // Try to subscribe with our best knowledge
                memset(&nus_tx_subscribe_params, 0, sizeof(nus_tx_subscribe_params));
                nus_tx_subscribe_params.value_handle = nus_tx_handle;
                nus_tx_subscribe_params.notify = nus_notify_callback;
                nus_tx_subscribe_params.value = BT_GATT_CCC_NOTIFY;
                
                // Use best guess for CCC
                nus_tx_subscribe_params.ccc_handle = nus_tx_handle + 2; 
                
                printk("Subscribing with CCC handle 0x%04x\n", nus_tx_subscribe_params.ccc_handle);
                err = bt_gatt_subscribe(conn, &nus_tx_subscribe_params);
                if (err && err != -EALREADY) {
                    printk("Final subscribe attempt failed (err %d)\n", err);
                } else {
                    printk("Final subscription successful!\n");
                    
                    // Send a test message
                    k_sleep(K_MSEC(500));
                    const char *test_msg = "Hello from Central!";
                    err = send_to_peripheral(test_msg, strlen(test_msg));
                    if (err) {
                        printk("Failed to send message (err %d)\n", err);
                    } else {
                        printk("Sent: %s\n", test_msg);
                    }
                }
            }
            return BT_GATT_ITER_STOP;
        }
        
        return BT_GATT_ITER_STOP;
    }

    // Found a primary service
    if (params->type == BT_GATT_DISCOVER_PRIMARY) {
        struct bt_gatt_service_val *service_val = attr->user_data;
        
        if (service_val && bt_uuid_cmp(service_val->uuid, BT_UUID_NUS_SERVICE) == 0) {
            printk("Found NUS service at handle 0x%04x, end handle 0x%04x\n", 
                   attr->handle, service_val->end_handle);
            
            // Save the service range for characteristic discovery
            discover_params.start_handle = attr->handle + 1;
            discover_params.end_handle = service_val->end_handle;
        }
    } 
    // Found a characteristic
    else if (params->type == BT_GATT_DISCOVER_CHARACTERISTIC) {
        struct bt_gatt_chrc *chrc = attr->user_data;
        
        if (!chrc) {
            printk("Invalid characteristic data\n");
            return BT_GATT_ITER_CONTINUE;
        }
        
        // Print UUID for debugging
        if (chrc->uuid->type == BT_UUID_TYPE_16) {
            printk("Found characteristic with UUID 0x%04x\n", 
                   ((struct bt_uuid_16 *)chrc->uuid)->val);
        } else if (chrc->uuid->type == BT_UUID_TYPE_128) {
            uint8_t *uuid_data = ((struct bt_uuid_128 *)chrc->uuid)->val;
            printk("Found characteristic with UUID 128: ");
            for (int i = 0; i < 16; i++) {
                printk("%02x", uuid_data[i]);
            }
            printk("\n");
        }
        
        // Compare UUIDs
        printk("Comparing to RX UUID...\n");
        if (bt_uuid_cmp(chrc->uuid, BT_UUID_NUS_RX) == 0) {
            printk("Found NUS RX characteristic at handle 0x%04x\n", attr->handle);
            nus_rx_handle = attr->handle + 1; // Value handle is the next one
            printk("RX value handle: 0x%04x\n", nus_rx_handle);
        } else {
            printk("Comparing to TX UUID...\n");
            if (bt_uuid_cmp(chrc->uuid, BT_UUID_NUS_TX) == 0) {
                printk("Found NUS TX characteristic at handle 0x%04x\n", attr->handle);
                nus_tx_handle = attr->handle + 1; // Value handle is the next one
                printk("TX value handle: 0x%04x\n", nus_tx_handle);
            }
        }
    }
    // Found a descriptor
    else if (params->type == BT_GATT_DISCOVER_DESCRIPTOR) {
        printk("Found descriptor at handle 0x%04x\n", attr->handle);
        
        if (bt_uuid_cmp(attr->uuid, BT_UUID_GATT_CCC) == 0) {
            printk("Found CCC descriptor at handle 0x%04x\n", attr->handle);
            
            // Store the CCC handle for the subscription params
            memset(&nus_tx_subscribe_params, 0, sizeof(nus_tx_subscribe_params));
            nus_tx_subscribe_params.value_handle = nus_tx_handle;
            nus_tx_subscribe_params.notify = nus_notify_callback;
            nus_tx_subscribe_params.value = BT_GATT_CCC_NOTIFY;
            nus_tx_subscribe_params.ccc_handle = attr->handle;
            
            // Immediately subscribe once we find the CCC
            printk("Found CCC, subscribing with handle 0x%04x\n", attr->handle);
            
            err = bt_gatt_subscribe(conn, &nus_tx_subscribe_params);
            if (err && err != -EALREADY) {
                printk("Subscribe failed (err %d)\n", err);
            } else {
                printk("Subscription successful!\n");
                
                // Send a test message after a delay
                k_sleep(K_MSEC(500));
                const char *test_msg = "Hello from Central!";
                err = send_to_peripheral(test_msg, strlen(test_msg));
                if (err) {
                    printk("Failed to send message (err %d)\n", err);
                } else {
                    printk("Sent: %s\n", test_msg);
                }
            }
        }
    }

    return BT_GATT_ITER_CONTINUE;
}

// Check if the device name "RentScan" is in the advertising data
static bool check_device_name(struct bt_data *data, void *user_data)
{
    // Looking for the device name in the advertising data
    if (data->type == BT_DATA_NAME_COMPLETE) {
        printk("Name found in adv data: len=%u\n", data->data_len);
        
        // Print the name for debugging
        char name_buf[32];
        size_t name_len = MIN(data->data_len, sizeof(name_buf) - 1);
        memcpy(name_buf, data->data, name_len);
        name_buf[name_len] = '\0';
        printk("Device name: '%s'\n", name_buf);
        
        // Check if it's "RentScan"
        if (data->data_len == 8 && memcmp(data->data, "RentScan", 8) == 0) {
            // Found the RentScan device!
            printk("Found RentScan device!\n");
            device_found_flag = true;
            return false; // Stop parsing
        }
    }
    
    // Continue parsing other advertising data
    return true;
}

static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
                     struct net_buf_simple *ad)
{
    char dev[BT_ADDR_LE_STR_LEN];
    
    bt_addr_le_to_str(addr, dev, sizeof(dev));
    printk("[DEVICE]: %s, AD evt type %u, AD data len %u, RSSI %i\n",
           dev, type, ad->len, rssi);

    // We're only interested in connectable devices
    if (type == BT_GAP_ADV_TYPE_ADV_IND ||
        type == BT_GAP_ADV_TYPE_ADV_DIRECT_IND) {
        
        // Reset the found flag before parsing
        device_found_flag = false;
        
        // Parse the advertising data to find the device name
        bt_data_parse(ad, check_device_name, NULL);
        
        if (device_found_flag) {
            int err;
            
            err = bt_le_scan_stop();
            if (err) {
                printk("Stop LE scan failed (err %d)\n", err);
                return;
            }

            struct bt_le_conn_param *param = BT_LE_CONN_PARAM_DEFAULT;
            err = bt_conn_le_create(addr, BT_CONN_LE_CREATE_CONN, param, &default_conn);
            if (err) {
                printk("Create conn failed (err %d)\n", err);
                start_scan();
            }
        }
    }
}

static void start_scan(void)
{
    int err;

    struct bt_le_scan_param scan_param = {
        .type       = BT_LE_SCAN_TYPE_ACTIVE,
        .options    = BT_LE_SCAN_OPT_NONE,
        .interval   = BT_GAP_SCAN_FAST_INTERVAL,
        .window     = BT_GAP_SCAN_FAST_WINDOW,
    };

    err = bt_le_scan_start(&scan_param, device_found);
    if (err) {
        printk("Scanning failed to start (err %d)\n", err);
        return;
    }

    printk("Scanning for RentScan device...\n");
}

static void bt_ready(int err)
{
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return;
    }

    printk("Bluetooth initialized\n");
    
    // Make sure we load settings properly
    if (IS_ENABLED(CONFIG_SETTINGS)) {
        settings_load();
        printk("Settings loaded\n");
    }

    // Start scanning with a small delay to ensure BT is fully ready
    k_work_submit_to_queue(&k_sys_work_q, (struct k_work *)&start_scan_work);
}

int main(void)
{
    int err;
    
    printk("Starting RentScan Central\n");
    
    // Initialize the delayed work
    k_work_init_delayable(&start_scan_work, start_scan_work_handler);

    // Enable Bluetooth
    err = bt_enable(bt_ready);
    if (err) {
        printk("Bluetooth init failed (err %d)\n", err);
        return 0;
    }
    
    // Main application loop
    while (1) {
        k_sleep(K_SECONDS(1));
    }
    
    return 0;
}