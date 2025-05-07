/**
 * @file mock_ble_service.c
 * @brief Mock implementation of BLE peripheral service
 */

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/logging/log.h>

#include "../../common/include/rentscan_protocol.h"

LOG_MODULE_REGISTER(mock_ble_service, LOG_LEVEL_INF);

/* Callback function pointer */
static void (*data_callback)(const uint8_t *data, uint16_t len);

/* Connection state */
static struct bt_conn *current_conn;
static bool is_advertising = false;

/* Define the RentScan service and characteristic UUIDs */
static struct bt_uuid_128 rentscan_service_uuid = BT_UUID_INIT_128(BT_UUID_RENTSCAN_VAL);
static struct bt_uuid_128 rentscan_rx_uuid = BT_UUID_INIT_128(BT_UUID_RENTSCAN_RX_VAL);
static struct bt_uuid_128 rentscan_tx_uuid = BT_UUID_INIT_128(BT_UUID_RENTSCAN_TX_VAL);

/* Advertising data */
static const struct bt_data ad[] = {
    BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
    BT_DATA(BT_DATA_NAME_COMPLETE, RENTSCAN_DEVICE_NAME, sizeof(RENTSCAN_DEVICE_NAME) - 1),
    BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_RENTSCAN_VAL),
};

/* Function declarations */
static ssize_t on_receive(struct bt_conn *conn,
                         const struct bt_gatt_attr *attr,
                         const void *buf,
                         uint16_t len,
                         uint16_t offset,
                         uint8_t flags);

static void tx_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value);

/* Service Declaration */
BT_GATT_SERVICE_DEFINE(rentscan_svc,
    BT_GATT_PRIMARY_SERVICE(&rentscan_service_uuid),
    BT_GATT_CHARACTERISTIC(&rentscan_rx_uuid.uuid,
                        BT_GATT_CHRC_WRITE | BT_GATT_CHRC_WRITE_WITHOUT_RESP,
                        BT_GATT_PERM_WRITE,
                        NULL, on_receive, NULL),
    BT_GATT_CHARACTERISTIC(&rentscan_tx_uuid.uuid,
                        BT_GATT_CHRC_NOTIFY,
                        BT_GATT_PERM_NONE,
                        NULL, NULL, NULL),
    BT_GATT_CCC(tx_ccc_changed, BT_GATT_PERM_READ | BT_GATT_PERM_WRITE),
);

/* Connection callbacks */
static void connected(struct bt_conn *conn, uint8_t err)
{
    if (err) {
        LOG_ERR("Connection failed (err %u)", err);
        return;
    }

    current_conn = bt_conn_ref(conn);
    LOG_INF("Peripheral connected");
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    LOG_INF("Peripheral disconnected (reason %u)", reason);

    if (current_conn) {
        bt_conn_unref(current_conn);
        current_conn = NULL;
    }
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

/* Client characteristic configuration changed */
static void tx_ccc_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    bool notifications_enabled = (value == BT_GATT_CCC_NOTIFY);
    LOG_INF("Notifications %s", notifications_enabled ? "enabled" : "disabled");
}

/* Data received handler */
static ssize_t on_receive(struct bt_conn *conn,
                         const struct bt_gatt_attr *attr,
                         const void *buf,
                         uint16_t len,
                         uint16_t offset,
                         uint8_t flags)
{
    LOG_INF("Peripheral received data, len %u", len);
    
    if (data_callback) {
        data_callback(buf, len);
    }
    
    return len;
}

/* Mock API functions */

int mock_ble_service_init(void (*cb)(const uint8_t *, uint16_t))
{
    data_callback = cb;
    LOG_INF("Mock BLE service initialized");
    return 0;
}

int mock_ble_service_start_advertising(bool fast)
{
    if (is_advertising) {
        return 0;
    }

    int err;
    
    if (fast) {
        err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), NULL, 0);
    } else {
        /* Use slower advertising parameters for power saving */
        static const struct bt_le_adv_param slow_adv_param = {
            .id = BT_ID_DEFAULT,
            .sid = 0,
            .secondary_max_skip = 0,
            .options = BT_LE_ADV_OPT_CONNECTABLE | BT_LE_ADV_OPT_USE_NAME,
            .interval_min = BT_GAP_ADV_SLOW_INT_MIN,
            .interval_max = BT_GAP_ADV_SLOW_INT_MAX,
            .peer = NULL,
        };
        err = bt_le_adv_start(&slow_adv_param, ad, ARRAY_SIZE(ad), NULL, 0);
    }

    if (!err) {
        is_advertising = true;
        LOG_INF("Peripheral advertising started (%s mode)", fast ? "fast" : "slow");
    } else {
        LOG_ERR("Peripheral advertising failed to start (err %d)", err);
    }
    return err;
}

int mock_ble_service_send_data(const uint8_t *data, uint16_t len)
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

    LOG_INF("Peripheral sending data, %d bytes", len);
    return bt_gatt_notify_cb(current_conn, &params);
}

bool mock_ble_service_is_connected(void)
{
    return current_conn != NULL;
}