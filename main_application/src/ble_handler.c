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

// Work item for periodic checks and status messages
static struct k_work_delayable status_work;
static void status_work_handler(struct k_work *work);

static void connected(struct bt_conn *conn, uint8_t err)
{
    if (err) {
        LOG_ERR("Connection failed (err %u)", err);
        return;
    }
    current_conn = bt_conn_ref(conn);
    LOG_INF("Connected");
    
    // Start periodic status checks
    k_work_schedule(&status_work, K_MSEC(1000));
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    LOG_INF("Disconnected (reason %u)", reason);
    
    // Cancel periodic status checks
    k_work_cancel_delayable(&status_work);
    
    if (current_conn) {
        bt_conn_unref(current_conn);
        current_conn = NULL;
    }
    notifications_enabled = false;
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
    .connected = connected,
    .disconnected = disconnected,
};

static void nus_receive_cb(struct bt_conn *conn, const uint8_t *data, uint16_t len)
{
    char msg[64];
    memcpy(msg, data, MIN(len, sizeof(msg) - 1));
    msg[MIN(len, sizeof(msg) - 1)] = '\0';
    LOG_INF("Received over BLE: %s", msg);
    
    // If we received data from central, it's a good sign we're connected correctly
    if (!notifications_enabled) {
        LOG_INF("Received data from central, enabling notifications");
        notifications_enabled = true;
        
        // Send a welcome message
        char welcome_msg[50];
        snprintf(welcome_msg, sizeof(welcome_msg), "RentScan ready for scanning at %u", k_uptime_get_32() / 1000);
        ble_send(welcome_msg);
    }
}

// Callback for when NUS sends a notification successfully
static void nus_sent_cb(struct bt_conn *conn)
{
    // We know notifications are enabled if we can send data successfully
    if (!notifications_enabled) {
        LOG_INF("✅ NUS notifications enabled by central");
        notifications_enabled = true;
        
        // Send a welcome message to confirm the notification channel works
        char welcome_msg[50];
        snprintf(welcome_msg, sizeof(welcome_msg), "RentScan ready for scanning at %u", k_uptime_get_32() / 1000);
        ble_send(welcome_msg);
    }
}

// Callback for when NUS notify states change (Nordic BLE UART Service)
// This is called when client enables or disables notifications
static void on_cccd_changed(const struct bt_gatt_attr *attr, uint16_t value)
{
    LOG_INF("CCCD changed: value %u", value);
    
    if (value == BT_GATT_CCC_NOTIFY) {
        LOG_INF("✅ NUS notifications enabled explicitly by central");
        notifications_enabled = true;
        
        // Send a welcome message to confirm the notification channel works
        char welcome_msg[50];
        snprintf(welcome_msg, sizeof(welcome_msg), "RentScan ready at %u", k_uptime_get_32() / 1000);
        ble_send(welcome_msg);
    } else {
        LOG_INF("❌ NUS notifications disabled by central");
        notifications_enabled = false;
    }
}

static struct bt_nus_cb nus_cb = {
    .received = nus_receive_cb,
    .sent = nus_sent_cb,
};

// Periodic work handler to check status and try sending test messages
static void status_work_handler(struct k_work *work)
{
    static uint32_t counter = 0;
    counter++;
    
    if (current_conn && !notifications_enabled) {
        LOG_WRN("Connected but notifications not enabled (attempt %u)", counter);
        
        // Every 5 attempts, try forcing notification status
        if (counter % 5 == 0) {
            LOG_INF("Attempting to force notification status");
            notifications_enabled = true;
            
            // Try sending a test message
            char test_msg[40];
            snprintf(test_msg, sizeof(test_msg), "Test notification %u", counter);
            int err = bt_nus_send(current_conn, test_msg, strlen(test_msg));
            
            if (err) {
                LOG_ERR("Failed to send test notification (err %d)", err);
                notifications_enabled = false;
            } else {
                LOG_INF("Test notification sent successfully");
            }
        }
    }
    
    // Reschedule the work
    k_work_schedule(&status_work, K_MSEC(2000));
}

int ble_handler_init(void)
{
    int err;
    
    // Initialize the status check work
    k_work_init_delayable(&status_work, status_work_handler);

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
        LOG_WRN("BLE notifications not enabled by central");
        return -EINVAL;
    }

    int err = bt_nus_send(current_conn, msg, strlen(msg));
    if (err) {
        LOG_ERR("Failed to send over NUS (err %d)", err);
        
        // If we get an error while notifications are enabled, they might actually be disabled
        if (err == -EINVAL) {
            LOG_WRN("Got EINVAL, disabling notifications flag");
            notifications_enabled = false;
        }
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