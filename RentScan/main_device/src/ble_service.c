#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/logging/log.h>
#include "ble_service.h"
#include "../../common/include/rentscan_protocol.h"

LOG_MODULE_REGISTER(ble_service, LOG_LEVEL_INF);

static ble_data_received_cb_t data_callback;
static struct bt_conn *current_conn;
static bool is_advertising = false;

/* Buffer for outgoing data */
static uint8_t tx_buffer[sizeof(rentscan_msg_t)];
static uint8_t tx_buffer_len;

/* Function declarations */
static void tx_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value);
static ssize_t on_receive(struct bt_conn *conn,
                         const struct bt_gatt_attr *attr,
                         const void *buf,
                         uint16_t len,
                         uint16_t offset,
                         uint8_t flags);
static void send_response(void);

/* Advertising data */
static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, RENTSCAN_DEVICE_NAME, sizeof(RENTSCAN_DEVICE_NAME) - 1),
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_RENTSCAN_VAL),
};

/* Service Declaration */
BT_GATT_SERVICE_DEFINE(rentscan_svc,
    BT_GATT_PRIMARY_SERVICE(BT_UUID_RENTSCAN),
    BT_GATT_CHARACTERISTIC(BT_UUID_RENTSCAN_RX,
                        BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
                        BT_GATT_PERM_WRITE,
                        NULL, on_receive, NULL),
    BT_GATT_CHARACTERISTIC(BT_UUID_RENTSCAN_TX,
                        BT_GATT_CHRC_NOTIFY,
                        BT_GATT_PERM_NONE,
                        NULL, NULL, NULL),
    BT_GATT_CCC(tx_ccc_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

static void connected(struct bt_conn *conn, uint8_t err)
{
    if (err) {
        LOG_ERR("Connection failed (err %u)", err);
        return;
    }

    current_conn = bt_conn_ref(conn);
    LOG_INF("Connected");
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    LOG_INF("Disconnected (reason %u)", reason);

    if (current_conn) {
        bt_conn_unref(current_conn);
        current_conn = NULL;
    }
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

static void tx_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    bool notifications_enabled = (value == BT_GATT_CCC_NOTIFY);
    LOG_INF("Notifications %s", notifications_enabled ? "enabled" : "disabled");
}

static void send_response(void)
{
    if (tx_buffer_len > 0) {
        bt_gatt_notify(NULL, &rentscan_svc.attrs[2], tx_buffer, tx_buffer_len);
        tx_buffer_len = 0;
    }
}

static ssize_t on_receive(struct bt_conn *conn,
                         const struct bt_gatt_attr *attr,
                         const void *buf,
                         uint16_t len,
                         uint16_t offset,
                         uint8_t flags)
{
    LOG_INF("Received data, len %u", len);
    
    if (data_callback) {
        data_callback(buf, len);
    }
    
    return len;
}

int ble_service_init(ble_data_received_cb_t data_received_cb)
{
    int err;

    data_callback = data_received_cb;

    err = bt_enable(NULL);
    if (err) {
        LOG_ERR("Bluetooth init failed (err %d)", err);
        return err;
    }
    
    // Small delay to ensure Bluetooth stack is ready
    k_sleep(K_MSEC(100));

    LOG_INF("Bluetooth initialized");
    return 0;
}

int ble_service_start_advertising(bool fast)
{
    if (is_advertising) {
        return 0;
    }

    struct bt_le_adv_param *param;
    if (fast) {
        static const struct bt_le_adv_param fast_param = BT_LE_ADV_PARAM_INIT(
            BT_LE_ADV_OPT_CONNECTABLE | BT_LE_ADV_OPT_USE_NAME,
            BT_GAP_ADV_FAST_INT_MIN_2,  /* 100ms */
            BT_GAP_ADV_FAST_INT_MAX_2,  /* 150ms */
            NULL);
        param = (struct bt_le_adv_param *)&fast_param;
    } else {
        static const struct bt_le_adv_param slow_param = BT_LE_ADV_PARAM_INIT(
            BT_LE_ADV_OPT_CONNECTABLE | BT_LE_ADV_OPT_USE_NAME,
            BT_GAP_ADV_SLOW_INT_MIN,  /* 1s */
            BT_GAP_ADV_SLOW_INT_MAX,  /* 2.5s */
            NULL);
        param = (struct bt_le_adv_param *)&slow_param;
    }

    int err = bt_le_adv_start(param, ad, ARRAY_SIZE(ad), NULL, 0);
    if (!err) {
        is_advertising = true;
        LOG_INF("Advertising started (%s mode)", fast ? "fast" : "slow");
    } else {
        LOG_ERR("Advertising failed to start (err %d)", err);
    }
    return err;
}

int ble_service_stop_advertising(void)
{
    if (!is_advertising) {
        return 0;
    }

    int err = bt_le_adv_stop();
    if (!err) {
        is_advertising = false;
        LOG_INF("Advertising stopped");
    }
    return err;
}

int ble_service_send_data(const uint8_t *data, uint16_t len)
{
    if (!current_conn) {
        return -ENOTCONN;
    }

    struct bt_gatt_notify_params params = {0};
    params.uuid = NULL;
    params.attr = &rentscan_svc.attrs[3];  // TX characteristic
    params.data = data;
    params.len = len;
    params.func = NULL;

    return bt_gatt_notify_cb(current_conn, &params);
}

int ble_service_send_message(const rentscan_msg_t *msg)
{
    if (!msg) {
        return -EINVAL;
    }

    return ble_service_send_data((const uint8_t *)msg, sizeof(rentscan_msg_t));
}

bool ble_service_is_connected(void)
{
    return current_conn != NULL;
}

int ble_service_disconnect(void)
{
    if (!current_conn) {
        return -ENOTCONN;
    }

    return bt_conn_disconnect(current_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
}