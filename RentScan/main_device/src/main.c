/**
 * @file main.c
 * @brief Main application for RentScan main device (NFC reader + BLE peripheral)
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include <zephyr/bluetooth/bluetooth.h>  /* Add this for bt_enable/disable functions */
#include <string.h>

#include "../include/main_device_config.h"
#include "nfc_handler.h"
#include "ble_service.h"
#include "rental_manager.h"
#include "../../common/include/rentscan_protocol.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/* Define work queue items for various tasks */
static struct k_work_delayable expiry_check_work;
static struct k_work nfc_process_work;

/* Buffer for NFC tag data */
static uint8_t tag_data_buf[MAX_MSG_PAYLOAD];
static size_t tag_data_len;
static uint8_t tag_id_buf[MAX_TAG_ID_LEN];
static size_t tag_id_len;

/* Indicate if a tag is being processed */
static bool tag_processing = false;

/**
 * @brief Handler for NFC tag detection events
 */
static void tag_detected_handler(const uint8_t *tag_id, size_t id_len,
                               const uint8_t *tag_data, size_t data_len)
{
    if (tag_processing) {
        LOG_WRN("Already processing a tag, ignoring new tag");
        return;
    }
    
    /* Copy the tag data to our buffer */
    if (id_len > MAX_TAG_ID_LEN) {
        id_len = MAX_TAG_ID_LEN;
    }
    memcpy(tag_id_buf, tag_id, id_len);
    tag_id_len = id_len;
    
    if (tag_data && data_len > 0) {
        if (data_len > MAX_MSG_PAYLOAD) {
            data_len = MAX_MSG_PAYLOAD;
        }
        memcpy(tag_data_buf, tag_data, data_len);
        tag_data_len = data_len;
    } else {
        tag_data_len = 0;
    }
    
    /* Process the tag in the work queue */
    tag_processing = true;
    k_work_submit(&nfc_process_work);
}

/**
 * @brief Handler for BLE data reception
 */
static void ble_data_received_handler(const uint8_t *data, uint16_t len)
{
    /* Process the received command */
    int err = rental_manager_process_command(data, len);
    if (err) {
        LOG_ERR("Failed to process command: %d", err);
    }
}

/**
 * @brief Handler for rental status changes
 */
static void rental_status_changed_handler(const rentscan_msg_t *msg)
{
    /* Send the status update via BLE */
    int err = ble_service_send_message(msg);
    if (err) {
        LOG_ERR("Failed to send status update: %d", err);
    }
    
    /* If we're currently processing a tag, we're done now */
    tag_processing = false;
}

/**
 * @brief NFC tag processing work handler
 */
static void nfc_process_work_handler(struct k_work *work)
{
    LOG_INF("Processing NFC tag with ID: %.*s", tag_id_len, tag_id_buf);
    
    // Create a message to send to the gateway
    rentscan_msg_t msg;
    memset(&msg, 0, sizeof(msg));
    
    // Set message fields
    msg.cmd = CMD_STATUS_REQ; // Request status to check if item is rented
    msg.tag_id_len = tag_id_len;
    memcpy(msg.tag_id, tag_id_buf, tag_id_len);
    msg.timestamp = k_uptime_get_32() / 1000; // Convert to seconds
    
    // Only send if we have a BLE connection
    if (ble_service_is_connected()) {
        int err = ble_service_send_message(&msg);
        if (err) {
            LOG_ERR("Failed to send tag data via BLE: %d", err);
        } else {
            LOG_INF("Tag data sent to gateway");
        }
    } else {
        LOG_WRN("BLE not connected, can't send tag data");
    }
    
    // Process the tag locally
    int err = rental_manager_process_tag(tag_id_buf, tag_id_len,
                                       tag_data_buf, tag_data_len);
    if (err) {
        LOG_ERR("Failed to process tag: %d", err);
    }
    
    // Clear the processing flag
    tag_processing = false;
}

/**
 * @brief Rental expiry check work handler
 */
static void expiry_check_work_handler(struct k_work *work)
{
    /* Check for expired rentals */
    int expired = rental_manager_check_expirations();
    if (expired > 0) {
        LOG_INF("Found %d expired rentals", expired);
    }
    
    /* Reschedule the work */
    k_work_schedule_for_queue(&k_sys_work_q, &expiry_check_work,
                            K_SECONDS(RENTAL_EXPIRY_CHECK_PERIOD));
}

int main(void)
{
    int err;
    
    LOG_INF("RentScan main device starting");
    
    /* Initialize work queue items */
    k_work_init(&nfc_process_work, nfc_process_work_handler);
    k_work_init_delayable(&expiry_check_work, expiry_check_work_handler);
    
    /* Initialize the rental manager */
    err = rental_manager_init(rental_status_changed_handler);
    if (err) {
        LOG_ERR("Failed to initialize rental manager: %d", err);
        return err;
    }
    
    /* Initialize the BLE service */
    err = ble_service_init(ble_data_received_handler);
    if (err) {
        LOG_ERR("Failed to initialize BLE service: %d", err);
        return err;
    }

    /* Load settings - required for BLE address */
    if (IS_ENABLED(CONFIG_SETTINGS)) {
        settings_load();
    }

    /* Wait for BLE stack to fully initialize */
    k_sleep(K_MSEC(100));
    
    /* Initialize the NFC handler */
    err = nfc_handler_init(tag_detected_handler);
    if (err) {
        LOG_ERR("Failed to initialize NFC handler: %d", err);
        return err;
    }
    
    /* Start BLE advertising with retry on error */
    for (int retry = 0; retry < 5; retry++) {
        err = ble_service_start_advertising(true);
        if (err == 0) {
            break;  // Success
        }
        
        LOG_WRN("BLE advertising start failed (attempt %d): %d", retry+1, err);
        k_sleep(K_MSEC(500 * (retry + 1)));  // Progressive backoff
        
        // For specific errors, try a Bluetooth system reset
        if (err == -EINVAL && retry == 2) {
            LOG_INF("Attempting Bluetooth subsystem reset");
            bt_disable();
            k_sleep(K_MSEC(1000));
            err = bt_enable(NULL);
            if (err) {
                LOG_ERR("Bluetooth re-init failed: %d", err);
                continue;
            }
            k_sleep(K_MSEC(100));
        }
    }
    
    if (err) {
        LOG_ERR("Failed to start advertising: %d", err);
        /* Continue anyway */
    }
    
    /* Start NFC polling */
    err = nfc_handler_start_polling();
    if (err) {
        LOG_ERR("Failed to start NFC polling: %d", err);
        /* Continue anyway */
    }
    
    /* Schedule rental expiry check */
    k_work_schedule_for_queue(&k_sys_work_q, &expiry_check_work, 
                            K_SECONDS(RENTAL_EXPIRY_CHECK_PERIOD));
    
    LOG_INF("RentScan main device initialized");
    
    /* Application is now running - Zephyr will handle the threads */
    return 0;
}