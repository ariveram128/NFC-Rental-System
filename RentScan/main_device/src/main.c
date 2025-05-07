/**
 * @file main.c
 * @brief Main application for RentScan main device (NFC reader + BLE peripheral)
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
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
    int err = rental_manager_process_tag(tag_id_buf, tag_id_len,
                                       tag_data_buf, tag_data_len);
    if (err) {
        LOG_ERR("Failed to process tag: %d", err);
        tag_processing = false;
    }
    /* Note: tag_processing will be cleared in rental_status_changed_handler */
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

void main(void)
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
        return;
    }
    
    /* Initialize the BLE service */
    err = ble_service_init(ble_data_received_handler);
    if (err) {
        LOG_ERR("Failed to initialize BLE service: %d", err);
        return;
    }
    
    /* Initialize the NFC handler */
    err = nfc_handler_init(tag_detected_handler);
    if (err) {
        LOG_ERR("Failed to initialize NFC handler: %d", err);
        return;
    }
    
    /* Start BLE advertising */
    err = ble_service_start_advertising(true);
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
} 