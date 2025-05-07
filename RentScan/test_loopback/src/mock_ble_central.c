/**
 * @file mock_ble_central.c
 * @brief Mock implementation of BLE central functionality
 */

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/logging/log.h>

#include "../../common/include/rentscan_protocol.h"

LOG_MODULE_REGISTER(mock_ble_central, LOG_LEVEL_INF);

/* Callback function pointer */
static void (*data_callback)(const uint8_t *data, uint16_t len);

/* Connection state */
static struct bt_conn *current_conn;
static bool discovery_complete;

/* UUIDs */
static struct bt_uuid_128 rentscan_service_uuid = BT_UUID_INIT_128(BT_UUID_RENTSCAN_VAL);
static struct bt_uuid_128 rentscan_rx_uuid = BT_UUID_INIT_128(BT_UUID_RENTSCAN_RX_VAL);
static struct bt_uuid_128 rentscan_tx_uuid = BT_UUID_INIT_128(BT_UUID_RENTSCAN_TX_VAL);

/* GATT subscription */
static struct bt_gatt_subscribe_params tx_subscribe_params;

/* Handle storage */
static uint16_t rx_handle;
static uint16_t tx_handle;

/* Device found during scanning */
static bt_addr_le_t found_device;
static bool is_device_found = false;

/* Work for connecting to device */
static struct k_work_delayable connect_work;

/* Scan parameters */
static const struct bt_le_scan_param scan_param = {
    .type = BT_LE_SCAN_TYPE_ACTIVE,
    .options = BT_LE_SCAN_OPT_FILTER_DUPLICATE,
    .interval = BT_GAP_SCAN_FAST_INTERVAL,
    .window = BT_GAP_SCAN_FAST_WINDOW,
};

/* GATT service discovery */
static struct bt_gatt_discover_params discover_params;

/* Helper function to find UUID in advertisement data */
static bool find_uuid_in_advertising_data(const uint8_t *data, uint16_t data_len, 
                                         const uint8_t *uuid, uint8_t uuid_len)
{
    uint8_t ad_type;
    uint8_t ad_len;
    uint8_t *ad_data;
    uint8_t pos = 0;
    
    while (pos < data_len) {
        ad_len = data[pos++];
        if (ad_len == 0) {
            break;
        }
        
        if (pos + ad_len > data_len) {
            break;
        }
        
        ad_type = data[pos++];
        ad_data = (uint8_t *)&data[pos];
        
        /* Looking for UUID128 fields */
        if ((ad_type == BT_DATA_UUID128_ALL || ad_type == BT_DATA_UUID128_SOME) && 
            ad_len >= uuid_len + 1) {
            if (memcmp(ad_data, uuid, uuid_len) == 0) {
                return true;
            }
        }
        
        pos += (ad_len - 1);
    }
    
    return false;
}

/* Data received callback */
static uint8_t notify_func(struct bt_conn *conn,
                         struct bt_gatt_subscribe_params *params,
                         const void *data, uint16_t length)
{
    if (!data) {
        LOG_INF("Unsubscribed");
        params->value_handle = 0U;
        return BT_GATT_ITER_STOP;
    }

    LOG_INF("Central received notify, %u bytes", length);
    
    if (data_callback) {
        data_callback(data, length);
    }
    
    return BT_GATT_ITER_CONTINUE;
}

/* Find service and characteristic handlers */
static uint8_t discover_func(struct bt_conn *conn,
                            const struct bt_gatt_attr *attr,
                            struct bt_gatt_discover_params *params)
{
    int err;

    if (!attr) {
        LOG_INF("Discover complete");
        (void)memset(params, 0, sizeof(*params));
        return BT_GATT_ITER_STOP;
    }

    LOG_INF("[ATTRIBUTE] handle %u", attr->handle);

    if (bt_uuid_cmp(discover_params.uuid, &rentscan_service_uuid.uuid) == 0) {
        /* RentScan service found, now discover characteristics */
        discover_params.uuid = &rentscan_rx_uuid.uuid;
        discover_params.start_handle = attr->handle + 1;
        discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;

        err = bt_gatt_discover(conn, &discover_params);
        if (err) {
            LOG_ERR("Discover RX failed (err %d)", err);
        }
    } else if (bt_uuid_cmp(discover_params.uuid, &rentscan_rx_uuid.uuid) == 0) {
        /* RX characteristic found */
        rx_handle = bt_gatt_attr_value_handle(attr);
        LOG_INF("Found RX characteristic, handle %u", rx_handle);
        
        /* Now discover TX characteristic */
        discover_params.uuid = &rentscan_tx_uuid.uuid;
        discover_params.start_handle = attr->handle + 1;
        discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;

        err = bt_gatt_discover(conn, &discover_params);
        if (err) {
            LOG_ERR("Discover TX failed (err %d)", err);
        }
    } else if (bt_uuid_cmp(discover_params.uuid, &rentscan_tx_uuid.uuid) == 0) {
        /* TX characteristic found */
        tx_handle = bt_gatt_attr_value_handle(attr);
        LOG_INF("Found TX characteristic, handle %u", tx_handle);
        
        /* Subscribe to TX notifications */
        tx_subscribe_params.notify = notify_func;
        tx_subscribe_params.value = BT_GATT_CCC_NOTIFY;
        tx_subscribe_params.value_handle = tx_handle;
        tx_subscribe_params.ccc_handle = bt_gatt_attr_value_handle(attr) + 1;

        err = bt_gatt_subscribe(conn, &tx_subscribe_params);
        if (err && err != -EALREADY) {
            LOG_ERR("Subscribe failed (err %d)", err);
        } else {
            LOG_INF("Subscribed to notifications");
            discovery_complete = true;
        }

        return BT_GATT_ITER_STOP;
    }

    return BT_GATT_ITER_STOP;
}

/* Device found during scanning */
static void device_found_cb(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
                        struct net_buf_simple *ad)
{
    char addr_str[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));

    /* We're only interested in connectable devices */
    if (type != BT_GAP_ADV_TYPE_ADV_IND && 
        type != BT_GAP_ADV_TYPE_ADV_DIRECT_IND) {
        return;
    }

    /* Check if the device has the RentScan service UUID */
    if (find_uuid_in_advertising_data(ad->data, ad->len, 
                                     rentscan_service_uuid.val, 16)) {
        LOG_INF("Found RentScan device %s (RSSI %d)", addr_str, rssi);
        
        /* Stop scanning */
        bt_le_scan_stop();
        
        /* Save device address for connection */
        bt_addr_le_copy(&found_device, addr);
        is_device_found = true;
        
        /* Schedule connection attempt */
        k_work_schedule(&connect_work, K_MSEC(100));
    }
}

/* Connect work handler */
static void connect_work_handler(struct k_work *work)
{
    int err;
    
    if (!is_device_found) {
        return;
    }
    
    LOG_INF("Connecting to device...");
    
    /* Connect to device */
    err = bt_conn_le_create(&found_device, BT_CONN_LE_CREATE_CONN, 
                          BT_LE_CONN_PARAM_DEFAULT, &current_conn);
    if (err) {
        LOG_ERR("Create connection failed (err %d)", err);
        
        /* Restart scanning */
        is_device_found = false;
        mock_ble_central_start_scan();
    }
}

/* Connection callbacks */
static void connected(struct bt_conn *conn, uint8_t err)
{
    if (err) {
        LOG_ERR("Connection failed (err %u)", err);
        
        /* Restart scanning */
        is_device_found = false;
        mock_ble_central_start_scan();
        return;
    }

    /* Check if this is the device we were looking for */
    if (is_device_found) {
        LOG_INF("Central connected");
        current_conn = bt_conn_ref(conn);
        
        /* Start service discovery */
        discovery_complete = false;
        discover_params.uuid = &rentscan_service_uuid.uuid;
        discover_params.func = discover_func;
        discover_params.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
        discover_params.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
        discover_params.type = BT_GATT_DISCOVER_PRIMARY;

        err = bt_gatt_discover(conn, &discover_params);
        if (err) {
            LOG_ERR("Discover failed (err %d)", err);
            bt_conn_disconnect(conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
        }
    }
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    LOG_INF("Central disconnected (reason %u)", reason);

    if (current_conn) {
        bt_conn_unref(current_conn);
        current_conn = NULL;
    }

    /* Restart scanning */
    is_device_found = false;
    discovery_complete = false;
    mock_ble_central_start_scan();
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

/* Mock API functions */

int mock_ble_central_init(void (*cb)(const uint8_t *, uint16_t))
{
    data_callback = cb;
    k_work_init_delayable(&connect_work, connect_work_handler);
    LOG_INF("Mock BLE central initialized");
    return 0;
}

int mock_ble_central_start_scan(void)
{
    int err;

    err = bt_le_scan_start(&scan_param, device_found_cb);
    if (err) {
        LOG_ERR("Scanning failed to start (err %d)", err);
        return err;
    }

    LOG_INF("Central scanning started");
    return 0;
}

int mock_ble_central_connect_to_device(void)
{
    if (!is_device_found) {
        return -ENODEV;
    }

    k_work_schedule(&connect_work, K_NO_WAIT);
    return 0;
}

int mock_ble_central_send_data(const uint8_t *data, uint16_t len)
{
    int err;

    if (!current_conn || !discovery_complete) {
        return -ENOTCONN;
    }

    LOG_INF("Central sending write, %d bytes", len);
    
    err = bt_gatt_write_without_response(current_conn, rx_handle, data, len, false);
    if (err) {
        LOG_ERR("Write failed (err %d)", err);
    }
    
    return err;
}

bool mock_ble_central_is_connected(void)
{
    return current_conn != NULL && discovery_complete;
}