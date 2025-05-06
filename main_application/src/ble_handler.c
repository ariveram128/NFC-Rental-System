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
static volatile bool notifications_enabled = false; // Ensure volatile for ISR context

// Work item for periodic checks and status messages
static struct k_work_delayable status_work;

// Forward declarations
static void status_work_handler(struct k_work *work);
static void print_service_info(void);
static void nus_send_status_cb(enum bt_nus_send_status status); // Add forward declaration for the new callback

static void connected(struct bt_conn *conn, uint8_t err)
{
    if (err) {
        LOG_ERR("Connection failed (err %u)", err);
        return;
    }
    current_conn = bt_conn_ref(conn);
    LOG_INF("Connected");
    
    // Reset notification state on new connection
    notifications_enabled = false; 

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
    LOG_INF("Notifications flag reset."); // Added log
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
    LOG_INF("Received over BLE: %s (len %u)", msg, len);
    
    // If we received data from central, it's a good sign we're connected correctly
    // The central might send data before enabling notifications.
    // We should not assume notifications are enabled just by receiving data.
    // The nus_send_status_cb callback is the definitive source for notification status.
}

// Callback for when NUS sends a notification successfully
static void nus_sent_cb(struct bt_conn *conn)
{
    // This callback confirms that a notification was successfully sent to the peer.
    // It doesn't necessarily mean the peer *processed* it, but the transport layer did its job.
    LOG_INF("NUS data sent successfully to %p", (void *)conn); // Cast conn to void* for logging
    // We don't set notifications_enabled = true here because the CCCD callback is the source of truth.
}

// New callback for NUS send status (reflects CCCD changes)
static void nus_send_status_cb(enum bt_nus_send_status status)
{
    if (status == BT_NUS_SEND_STATUS_ENABLED) {
        LOG_INF("✅ NUS notifications enabled by central");
        notifications_enabled = true;
        
        // Send a welcome message to confirm the notification channel works
        char welcome_msg[60];
        snprintf(welcome_msg, sizeof(welcome_msg), "RentScan ready! Notifications ON. Uptime: %us", k_uptime_get_32() / 1000);
        // It's generally safe to call ble_send from this callback as NUS service calls it from a work queue context.
        ble_send(welcome_msg);
    } else if (status == BT_NUS_SEND_STATUS_DISABLED) {
        LOG_INF("❌ NUS notifications disabled by central");
        notifications_enabled = false;
    } else {
        LOG_WRN("NUS send status unknown: %d", status);
    }
}

static struct bt_nus_cb nus_cb = {
    .received = nus_receive_cb,
    .sent = nus_sent_cb,
    .send_enabled = nus_send_status_cb, // Correctly assign to send_enabled
};

// Periodic work handler to check status and try sending test messages
static void status_work_handler(struct k_work *work)
{
    static uint32_t counter = 0;
    counter++;
    
    if (current_conn && !notifications_enabled) {
        LOG_WRN("Connected but notifications not enabled by central (attempt %u)", counter);
        
        // The peripheral should not try to force notifications.
        // It should wait for the central to enable them by writing to the CCCD.
        // The error -22 (EINVAL) on the peripheral side when trying to send a notification
        // before the central has enabled them is expected behavior.

        // If we've been connected for a while (e.g., > 15 seconds) and still no notifications,
        // it might indicate an issue on the central side or a pairing problem.
        // For now, we just log. A more robust solution might involve disconnecting
        // to allow the central to re-initiate the connection and subscription process.
        if (counter > 15 && (counter % 5 == 0)) { // Log every 5 attempts after 15 initial attempts
            LOG_WRN("Still no notifications from central after %u checks. Central needs to subscribe.", counter);
        }

    } else if (current_conn && notifications_enabled) {
        // Optionally, send a periodic heartbeat if notifications are enabled
        // This can be useful for the central to know the peripheral is still alive.
        // For example, every 30 seconds:
        if (counter % 30 == 0) {
            char heartbeat_msg[30];
            snprintf(heartbeat_msg, sizeof(heartbeat_msg), "Peripheral Heartbeat %u", counter / 30);
            // ble_send(heartbeat_msg); // Uncomment to enable heartbeat
            LOG_INF("Sent heartbeat (if enabled in ble_send)");
        }
    }
    
    // Reschedule the work
    k_work_schedule(&status_work, K_MSEC(1000));  // Check every 1 second
}

int ble_handler_init(void)
{
    int err;
    
    // Initialize the status check work
    k_work_init_delayable(&status_work, status_work_handler);

    LOG_INF("Initializing Nordic UART Service (NUS)");
    err = bt_nus_init(&nus_cb);
    if (err) {
        LOG_ERR("Failed to init NUS (err %d)", err);
        return err;
    }
    
    // Log information about registered services
    print_service_info();

    // Use constant data for advertising flags and name
    static const uint8_t ad_flags = BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR;
    static const char ad_name[] = "RentScan";
    
    static const struct bt_data ad[] = {
        { BT_DATA_FLAGS, 1, &ad_flags },
        { BT_DATA_NAME_COMPLETE, 8, ad_name }, // "RentScan" is exactly 8 characters
    };

    // Use BT_LE_ADV_CONN for simplicity
    err = bt_le_adv_start(BT_LE_ADV_CONN, ad, ARRAY_SIZE(ad), NULL, 0);
    if (err) {
        LOG_ERR("Advertising failed to start (err %d)", err);
        return err;
    }

    LOG_INF("Advertising as RentScan device with NUS service");
    return 0;
}

// Helper function to verify registered services
static void print_service_info(void)
{
    LOG_INF("Registered BLE services and characteristics:");
    LOG_INF("NUS Service UUID: 6E400001-B5A3-F393-E0A9-E50E24DCCA9E");
    LOG_INF("  RX Char UUID:   6E400002-B5A3-F393-E0A9-E50E24DCCA9E (Write)");
    LOG_INF("  TX Char UUID:   6E400003-B5A3-F393-E0A9-E50E24DCCA9E (Notify)");
}

int ble_send(const char *msg)
{
    if (!current_conn) {
        LOG_WRN("No BLE connection, cannot send");
        return -ENOTCONN;
    }
    
    if (!notifications_enabled) {
        LOG_WRN("BLE notifications not enabled by central. Cannot send. Central must subscribe to TX char.");
        return -EPERM; // EPERM (Operation not permitted) is more fitting than EINVAL here
    }

    int err = bt_nus_send(current_conn, msg, strlen(msg));
    if (err) {
        LOG_ERR("Failed to send over NUS (err %d)", err);
        
        // If we get an error while notifications are enabled, they might actually be disabled
        // This case should ideally not happen if CCCD state is managed correctly.
        if (err == -EINVAL || err == -EPIPE) { // EPIPE (Broken pipe) can also occur
            LOG_WRN("NUS send failed (err %d) despite notifications_enabled=true. Resetting flag.", err);
            notifications_enabled = false; // Re-evaluate notification state
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