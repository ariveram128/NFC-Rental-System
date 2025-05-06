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

static void connected(struct bt_conn *conn, uint8_t conn_err)
{
    char addr[BT_ADDR_LE_STR_LEN];
    int err;

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
        // Start service discovery right away
        discover_params.uuid = search_service_uuid;
        discover_params.func = discover_func;
        discover_params.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
        discover_params.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
        discover_params.type = BT_GATT_DISCOVER_PRIMARY;

        err = bt_gatt_discover(default_conn, &discover_params);
        if (err) {
            printk("Discover failed (err %d)\n", err);
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
        printk("Discover complete\n");
        (void)memset(params, 0, sizeof(*params));
        
        // If we've found the TX handle, subscribe to notifications
        if (nus_tx_handle) {
            memset(&nus_tx_subscribe_params, 0, sizeof(nus_tx_subscribe_params));
            nus_tx_subscribe_params.value_handle = nus_tx_handle;
            nus_tx_subscribe_params.notify = nus_notify_callback;
            nus_tx_subscribe_params.value = BT_GATT_CCC_NOTIFY;
            
            // Don't auto-discover CCC, we'll find the next handle manually
            // This improves compatibility across different devices
            nus_tx_subscribe_params.ccc_handle = nus_tx_handle + 1;
            
            err = bt_gatt_subscribe(conn, &nus_tx_subscribe_params);
            if (err && err != -EALREADY) {
                printk("Subscribe failed (err %d)\n", err);
            } else {
                printk("Subscribed to NUS TX characteristic\n");
                
                // Wait a moment before sending to make sure subscription is processed
                k_sleep(K_MSEC(500));
                
                // Send a test message
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

    // Found NUS service
    if (bt_uuid_cmp(discover_params.uuid, BT_UUID_NUS_SERVICE) == 0) {
        printk("Found NUS service\n");
        
        // Now look for the RX characteristic
        discover_params.uuid = BT_UUID_NUS_RX;
        discover_params.start_handle = attr->handle + 1;
        discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;

        err = bt_gatt_discover(conn, &discover_params);
        if (err) {
            printk("Discover failed (err %d)\n", err);
        }
    }
    // Found NUS RX characteristic
    else if (bt_uuid_cmp(discover_params.uuid, BT_UUID_NUS_RX) == 0) {
        printk("Found NUS RX characteristic\n");
        nus_rx_handle = bt_gatt_attr_value_handle(attr);
        
        // Now look for the TX characteristic
        discover_params.uuid = BT_UUID_NUS_TX;
        discover_params.start_handle = attr->handle + 1;

        err = bt_gatt_discover(conn, &discover_params);
        if (err) {
            printk("Discover failed (err %d)\n", err);
        }
    }
    // Found NUS TX characteristic
    else if (bt_uuid_cmp(discover_params.uuid, BT_UUID_NUS_TX) == 0) {
        printk("Found NUS TX characteristic\n");
        nus_tx_handle = bt_gatt_attr_value_handle(attr);
        
        // Now discover the CCC
        discover_params.uuid = NULL;
        discover_params.start_handle = attr->handle + 1;
        discover_params.type = BT_GATT_DISCOVER_DESCRIPTOR;
        
        err = bt_gatt_discover(conn, &discover_params);
        if (err) {
            printk("Discover failed (err %d)\n", err);
        }
    }

    return BT_GATT_ITER_STOP;
}

// Check if the device name "RentScan" is in the advertising data
static bool check_device_name(struct bt_data *data, void *user_data)
{
    // Looking for the device name in the advertising data
    if (data->type == BT_DATA_NAME_COMPLETE) {
        if (data->data_len == 8 && memcmp(data->data, "RentScan", 8) == 0) {
            // Found the RentScan device!
            printk("Found RentScan device\n");
            device_found_flag = true;
        }
    }
    
    // Always return true to continue parsing all advertising data
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
    start_scan();
}

int main(void)
{
    int err;

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