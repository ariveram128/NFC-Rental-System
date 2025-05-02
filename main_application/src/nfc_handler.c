/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <nfc_t2t_lib.h>
#include <nfc/ndef/msg.h>
#include <nfc/ndef/text_rec.h>
#include "nfc_handler.h"
#include "ble_handler.h"

LOG_MODULE_REGISTER(nfc_handler, LOG_LEVEL_INF);

#define MAX_NDEF_RECORDS 2
#define NDEF_MSG_BUF_SIZE 256

static uint8_t ndef_msg_buf[NDEF_MSG_BUF_SIZE];
static uint8_t default_item_id[] = "item123";
static const uint8_t en_code[] = "en";

static uint8_t current_item_id[MAX_ITEM_ID_LEN];
static size_t current_item_id_len = 0;

static void nfc_callback(void *context, nfc_t2t_event_t event, const uint8_t *data, size_t data_length)
{
    ARG_UNUSED(context);

    switch (event) {
    case NFC_T2T_EVENT_FIELD_ON:
        LOG_INF("📶 NFC field detected");
        break;

    case NFC_T2T_EVENT_FIELD_OFF:
        LOG_INF("❌ NFC field lost");
        break;

    case NFC_T2T_EVENT_DATA_READ:
        LOG_INF("📲 NFC tag read by phone - sent item ID: %s", current_item_id);
        
        // Send notification over BLE when tag is read
        char msg[64];
        snprintf(msg, sizeof(msg), "TAG_READ: %s", current_item_id);
        ble_send(msg);
        
        // Also send as rental data to demonstrate the function
        ble_send_rental_data((char *)current_item_id, k_uptime_get_32() / 1000);
        break;

    default:
        LOG_WRN("Unhandled NFC event: %d", event);
        break;
    }
}

static int create_item_id_ndef_msg(uint8_t *buffer, uint32_t *len)
{
    int err;

    const uint8_t *item_id_data = (current_item_id_len > 0) ? current_item_id : default_item_id;
    size_t item_id_len = (current_item_id_len > 0) ? current_item_id_len : sizeof(default_item_id) - 1;

    NFC_NDEF_TEXT_RECORD_DESC_DEF(item_id_rec,
                                   UTF_8,
                                   en_code,
                                   sizeof(en_code) - 1,
                                   item_id_data,
                                   item_id_len);

    NFC_NDEF_MSG_DEF(nfc_msg, MAX_NDEF_RECORDS);
    err = nfc_ndef_msg_record_add(&NFC_NDEF_MSG(nfc_msg),
                                   &NFC_NDEF_TEXT_RECORD_DESC(item_id_rec));
    if (err < 0) {
        LOG_ERR("Failed to add record to NDEF msg (err %d)", err);
        return err;
    }

    err = nfc_ndef_msg_encode(&NFC_NDEF_MSG(nfc_msg), buffer, len);
    if (err < 0) {
        LOG_ERR("Failed to encode NDEF msg (err %d)", err);
        return err;
    }

    return 0;
}

int nfc_reader_init(void)
{
    int err;

    LOG_INF("🚀 Initializing NFC tag (emulation mode)");

    memcpy(current_item_id, default_item_id, sizeof(default_item_id) - 1);
    current_item_id_len = sizeof(default_item_id) - 1;

    err = nfc_t2t_setup(nfc_callback, NULL);
    if (err) {
        LOG_ERR("Failed to setup NFC (err %d)", err);
        return err;
    }

    uint32_t len = sizeof(ndef_msg_buf);
    err = create_item_id_ndef_msg(ndef_msg_buf, &len);
    if (err) {
        return err;
    }

    err = nfc_t2t_payload_set(ndef_msg_buf, len);
    if (err) {
        LOG_ERR("Failed to set NFC payload (err %d)", err);
        return err;
    }

    err = nfc_t2t_emulation_start();
    if (err) {
        LOG_ERR("Failed to start NFC emulation (err %d)", err);
        return err;
    }

    LOG_INF("✅ NFC tag ready — item ID: %s", current_item_id);
    return 0;
}

int nfc_update_item_id(const char *new_id, size_t id_len)
{
    if (id_len >= MAX_ITEM_ID_LEN) {
        LOG_ERR("Item ID too long");
        return -EINVAL;
    }

    memcpy(current_item_id, new_id, id_len);
    current_item_id[id_len] = '\0';
    current_item_id_len = id_len;

    uint32_t len = sizeof(ndef_msg_buf);
    int err = create_item_id_ndef_msg(ndef_msg_buf, &len);
    if (err) return err;

    err = nfc_t2t_payload_set(ndef_msg_buf, len);
    if (err) {
        LOG_ERR("Failed to set new payload (err %d)", err);
        return err;
    }

    LOG_INF("🔄 NFC tag updated — new item ID: %s", current_item_id);
    return 0;
}