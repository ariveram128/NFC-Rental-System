#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/logging/log.h>
#include <bluetooth/gatt_dm.h>
#include <bluetooth/scan.h>
#include "ble_central.h"
#include <zephyr/sys/byteorder.h>
#include <zephyr/bluetooth/hci_vs.h>
#include "../include/gateway_config.h"
#include "../../common/include/rentscan_protocol.h"

LOG_MODULE_REGISTER(ble_central, LOG_LEVEL_INF);

static struct bt_conn *current_conn;
static struct bt_gatt_discover_params discover_params;
static struct bt_gatt_subscribe_params subscribe_params;
static uint16_t nus_rx_handle;
static uint16_t nus_tx_handle;

static ble_msg_received_cb_t msg_callback;
static bool scanning = false;
static int consecutive_errors = 0;

static void start_scan(void);
static void error_recovery(void);

static uint8_t notify_handler(struct bt_conn *conn,
                          struct bt_gatt_subscribe_params *params,
                          const void *data, uint16_t length)
{
    if (!data) {
        LOG_INF("Unsubscribed from notifications");
        params->value_handle = 0;
        return BT_GATT_ITER_STOP;
    }

    if (msg_callback && length >= sizeof(rentscan_msg_t)) {
        msg_callback((const rentscan_msg_t *)data);
    }

    return BT_GATT_ITER_CONTINUE;
}

static bool check_device(struct bt_data *data, void *user_data)
{
    bool *found = (bool *)user_data;
    
    if (data->type == BT_DATA_NAME_COMPLETE) {
        if (data->data_len == strlen(RENTSCAN_DEVICE_NAME) &&
            memcmp(data->data, RENTSCAN_DEVICE_NAME, data->data_len) == 0) {
            *found = true;
        }
    } else if (data->type == BT_DATA_UUID128_ALL) {
        uint8_t uuid[] = {
            BT_UUID_128_ENCODE(0x18ee2ef5, 0x263d, 0x4559, 0x953c, 0xd66077c89ae6)
        };
        if (data->data_len >= 16 && memcmp(data->data, uuid, 16) == 0) {
            *found = true;
        }
    }
    return true;
}

static void device_found(const bt_addr_le_t *addr, int8_t rssi, uint8_t type,
                        struct net_buf_simple *ad)
{
    char addr_str[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(addr, addr_str, sizeof(addr_str));
    int err;

    // Only look for connectable advertising
    if (type != BT_GAP_ADV_TYPE_ADV_IND &&
        type != BT_GAP_ADV_TYPE_ADV_DIRECT_IND) {
        return;
    }

    // Check if this is a RentScan device
    bool found = false;
    bt_data_parse(ad, check_device, &found);
    
    if (!found) {
        return;
    }

    LOG_INF("Found RentScan device %s, RSSI %d", addr_str, rssi);

    // Stop scanning
    err = bt_le_scan_stop();
    if (err) {
        LOG_ERR("Stop scan failed (err %d)", err);
        return;
    }

    // Create connection parameters
    struct bt_conn_le_create_param create_param = BT_CONN_LE_CREATE_PARAM_INIT(
        BT_CONN_LE_OPT_NONE,
        BT_GAP_SCAN_FAST_INTERVAL,
        BT_GAP_SCAN_FAST_WINDOW);

    // Connection parameters
    struct bt_le_conn_param conn_param = {
        .interval_min = BT_GAP_INIT_CONN_INT_MIN,
        .interval_max = BT_GAP_INIT_CONN_INT_MAX,
        .latency = 0,
        .timeout = BLE_CONN_SUPERVISION_TIMEOUT
    };

    // Connect to device
    err = bt_conn_le_create(addr, &create_param, &conn_param, &current_conn);
    if (err) {
        LOG_ERR("Create connection failed (err %d)", err);
        scanning = false;
        start_scan();
        return;
    }

    LOG_INF("Connection pending");
}

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

    if (!bt_uuid_cmp(discover_params.uuid, BT_UUID_RENTSCAN)) {
        discover_params.uuid = BT_UUID_RENTSCAN_RX;
        discover_params.start_handle = attr->handle + 1;
        discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;

        err = bt_gatt_discover(conn, &discover_params);
        if (err) {
            LOG_ERR("Discover failed (err %d)", err);
        }
    } else if (!bt_uuid_cmp(discover_params.uuid, BT_UUID_RENTSCAN_RX)) {
        nus_rx_handle = bt_gatt_attr_value_handle(attr);
        discover_params.uuid = BT_UUID_RENTSCAN_TX;
        discover_params.start_handle = attr->handle + 1;
        discover_params.type = BT_GATT_DISCOVER_CHARACTERISTIC;

        err = bt_gatt_discover(conn, &discover_params);
        if (err) {
            LOG_ERR("Discover failed (err %d)", err);
        }
    } else if (!bt_uuid_cmp(discover_params.uuid, BT_UUID_RENTSCAN_TX)) {
        nus_tx_handle = bt_gatt_attr_value_handle(attr);

        subscribe_params.notify = notify_handler;
        subscribe_params.value = BT_GATT_CCC_NOTIFY;
        subscribe_params.value_handle = nus_tx_handle;
        subscribe_params.ccc_handle = attr->handle + 2;

        err = bt_gatt_subscribe(conn, &subscribe_params);
        if (err && err != -EALREADY) {
            LOG_ERR("Subscribe failed (err %d)", err);
        }

        return BT_GATT_ITER_STOP;
    }

    return BT_GATT_ITER_STOP;
}

static void connected(struct bt_conn *conn, uint8_t err)
{
    char addr[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    if (err) {
        LOG_ERR("Failed to connect to %s (%u)", addr, err);
        scanning = false;
        start_scan();
        return;
    }

    LOG_INF("Connected to device %s", addr);
    current_conn = bt_conn_ref(conn);
    consecutive_errors = 0;

    discover_params.uuid = BT_UUID_RENTSCAN;
    discover_params.func = discover_func;
    discover_params.start_handle = BT_ATT_FIRST_ATTRIBUTE_HANDLE;
    discover_params.end_handle = BT_ATT_LAST_ATTRIBUTE_HANDLE;
    discover_params.type = BT_GATT_DISCOVER_PRIMARY;

    err = bt_gatt_discover(conn, &discover_params);
    if (err) {
        LOG_ERR("Service discovery failed (err %d)", err);
        error_recovery();
    }
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    char addr[BT_ADDR_LE_STR_LEN];
    bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

    LOG_INF("Disconnected from %s (reason 0x%02x)", addr, reason);

    if (conn == current_conn) {
        bt_conn_unref(current_conn);
        current_conn = NULL;
        nus_rx_handle = 0;
        nus_tx_handle = 0;
    }

    scanning = false;
    start_scan();
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

static void start_scan(void)
{
    int err;
    int retry_count = 0;
    const int max_retries = 3;

    if (scanning) {
        LOG_INF("Scan already active");
        return;
    }

    while (retry_count < max_retries) {
        // Configure scan parameters for better discovery
        struct bt_le_scan_param scan_param = {
            .type = BT_LE_SCAN_TYPE_ACTIVE,
            .interval = BLE_SCAN_INTERVAL,
            .window = BLE_SCAN_WINDOW,
            .options = BT_LE_SCAN_OPT_FILTER_DUPLICATE
        };

        err = bt_le_scan_start(&scan_param, device_found);
        if (err == 0) {
            scanning = true;
            LOG_INF("Scanning started successfully");
            return;
        } else if (err == -EAGAIN) {
            retry_count++;
            LOG_WRN("Scan start failed with EAGAIN, retry %d/%d", retry_count, max_retries);
            k_sleep(K_MSEC(1000 * retry_count));
        } else {
            LOG_ERR("Starting scanning failed (err %d)", err);
            return;
        }
    }

    LOG_ERR("Failed to start scan after %d retries", max_retries);
}

static void error_recovery(void)
{
    consecutive_errors++;
    
    if (consecutive_errors >= GATEWAY_ERROR_RESET_THRESHOLD) {
        LOG_WRN("Too many consecutive errors, resetting BLE");
        consecutive_errors = 0;
        ble_central_reset();
    } else {
        if (current_conn) {
            bt_conn_disconnect(current_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
        }
        scanning = false;
        start_scan();
    }
}

int ble_central_init(ble_msg_received_cb_t msg_received_cb)
{
    int err;

    msg_callback = msg_received_cb;

    err = bt_enable(NULL);
    if (err) {
        LOG_ERR("Bluetooth init failed (err %d)", err);
        return err;
    }

    LOG_INF("Bluetooth initialized");
    return 0;
}

int ble_central_start_scan(void)
{
    if (!scanning) {
        start_scan();
    }
    return 0;
}

int ble_central_stop_scan(void)
{
    if (scanning) {
        int err = bt_le_scan_stop();
        if (err) {
            LOG_ERR("Stopping scanning failed (err %d)", err);
            return err;
        }
        scanning = false;
    }
    return 0;
}

int ble_central_send_message(const rentscan_msg_t *msg)
{
    if (!current_conn || !nus_rx_handle) {
        return -ENOTCONN;
    }

    return bt_gatt_write_without_response(current_conn, nus_rx_handle,
                                        msg, sizeof(*msg), false);
}

int ble_central_disconnect(void)
{
    if (current_conn) {
        return bt_conn_disconnect(current_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
    }
    return 0;
}

int ble_central_reset(void)
{
    ble_central_disconnect();
    bt_disable();
    k_sleep(K_MSEC(1000));
    return bt_enable(NULL);
}

bool ble_central_is_connected(void)
{
    return current_conn != NULL;
}

/* Function to read RSSI using HCI command */
int ble_central_get_conn_stats(int8_t *rssi, int8_t *tx_power, uint16_t *conn_interval)
{
    if (!current_conn) {
        return -ENOTCONN;
    }

    /* Get connection interval from connection info */
    struct bt_conn_info info;
    int err = bt_conn_get_info(current_conn, &info);
    if (err) {
        return err;
    }

    if (conn_interval) {
        *conn_interval = info.le.interval;
    }

    /* Use HCI Read RSSI command to get actual RSSI value */
    if (rssi) {
        struct net_buf *buf, *rsp = NULL;
        struct bt_hci_cp_read_rssi *cp;
        struct bt_hci_rp_read_rssi *rp;
        
        /* Get connection handle - use index instead of non-existent bt_conn_get_id() */
        uint16_t handle = bt_conn_index(current_conn);
        
        /* Create HCI command */
        buf = bt_hci_cmd_create(BT_HCI_OP_READ_RSSI, sizeof(*cp));
        if (!buf) {
            return -ENOBUFS;
        }
        
        /* Fill command parameters - use manual assignment instead of sys_cpu_to_le16 */
        cp = net_buf_add(buf, sizeof(*cp));
        cp->handle = handle; /* No endian conversion needed as this is a local variable */
        
        /* Send command and wait for response */
        err = bt_hci_cmd_send_sync(BT_HCI_OP_READ_RSSI, buf, &rsp);
        if (err) {
            return err;
        }
        
        /* Process response */
        rp = (void *)rsp->data;
        if (rp->status) {
            /* Command failed */
            err = -EIO;
        } else {
            /* The HCI returns RSSI as a signed 8-bit value in dBm */
            *rssi = rp->rssi;
            err = 0;
        }
        
        /* Release response buffer */
        net_buf_unref(rsp);
        
        if (err) {
            return err;
        }
    }

    /* Estimate TX power based on connection parameters */
    if (tx_power) {
        if (info.le.interval < 50) {
            *tx_power = 0;  /* Higher power for fast connections */
        } else {
            *tx_power = -6; /* Lower power for slow connections */
        }
    }

    return 0;
}

int ble_central_add_to_whitelist(const char *addr_str)
{
    // Replace whitelist with filter accept list
    bt_addr_le_t addr;
    int err = bt_addr_le_from_str(addr_str, "random", &addr);
    if (err) {
        return err;
    }
    return bt_le_filter_accept_list_add(&addr);
}

int ble_central_clear_whitelist(void)
{
    // Replace whitelist with filter accept list
    return bt_le_filter_accept_list_clear();
}