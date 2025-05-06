/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <bluetooth/services/nus.h>

#include "ble_handler.h"

LOG_MODULE_REGISTER(ble_handler, LOG_LEVEL_INF);

static struct bt_conn *current_conn = NULL;
static bool notifications_enabled = false;

static void connected(struct bt_conn *conn, uint8_t err)
{
    if (err) {
        LOG_ERR("Connection failed (err %u)", err);
        return;
    }
    current_conn = bt_conn_ref(conn);
    notifications_enabled = false;  // Reset notification status on new connection
    LOG_INF("Connected");
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    LOG_INF("Disconnected (reason %u)", reason);
    if (current_conn) {
        bt_conn_unref(current_conn);
        current_conn = NULL;
        notifications_enabled = false;
    }
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

static void nus_receive_cb(struct bt_conn *conn, const uint8_t *data, uint16_t len)
{
    char msg[64];
    memcpy(msg, data, len);
    msg[len] = '\0';
    LOG_INF("Received over BLE: %s", msg);
}

// Callback for when NUS notifications are enabled or disabled
static void nus_notify_cb(enum bt_nus_send_status status)
{
    if (status == BT_NUS_SEND_STATUS_ENABLED) {
        LOG_INF("NUS notifications enabled");
        notifications_enabled = true;
    } else if (status == BT_NUS_SEND_STATUS_DISABLED) {
        LOG_INF("NUS notifications disabled");
        notifications_enabled = false;
    }
}

static struct bt_nus_cb nus_cb = {
    .received = nus_receive_cb,
    .sent = nus_notify_cb,
};

int ble_handler_init(void)
{
    int err;

    err = bt_nus_init(&nus_cb);
    if (err) {
        LOG_ERR("Failed to init NUS (err %d)", err);
        return err;
    }

    const struct bt_data ad[] = {
        BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
        BT_DATA(BT_DATA_NAME_COMPLETE, "RentScan", 8),
    };

    struct bt_le_adv_param adv_param = BT_LE_ADV_PARAM_INIT(
        BT_LE_ADV_OPT_CONNECTABLE,
        BT_GAP_ADV_FAST_INT_MIN_2,
        BT_GAP_ADV_FAST_INT_MAX_2,
        NULL
    );

    err = bt_le_adv_start(&adv_param, ad, ARRAY_SIZE(ad), NULL, 0);
    if (err) {
        LOG_ERR("Advertising failed to start (err %d)", err);
        return err;
    }

    LOG_INF("Advertising as RentScan device");
    return 0;
}

int ble_send(const char *msg)
{
    if (!current_conn) {
        LOG_WRN("No BLE connection, cannot send");
        return -ENOTCONN;
    }
    
    if (!notifications_enabled) {
        LOG_WRN("BLE notifications not enabled by central device");
        return -EINVAL;
    }

    int err = bt_nus_send(current_conn, msg, strlen(msg));
    if (err) {
        LOG_ERR("Failed to send over NUS (err %d)", err);
    } else {
        LOG_INF("Sent over BLE: %s", msg);
    }

    return err;
}

int ble_send_rental_data(const char *tag_id, uint32_t timestamp)
{
    char msg[64];
    snprintf(msg, sizeof(msg), "[%s] RENTAL START: %u", tag_id, timestamp);
    return ble_send(msg);
}