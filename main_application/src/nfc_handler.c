/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <nfc/ndef/msg.h>
#include <nfc/ndef/msg_parser.h>
#include <nfc/ndef/text_rec.h>
#include <nfc/t2t/parser.h>
#include <nrfx_nfct.h>
#include "nfc_handler.h"

LOG_MODULE_REGISTER(nfc_handler, LOG_LEVEL_INF);

#define MAX_NDEF_RECORDS 10
#define NDEF_MSG_BUF_SIZE 256

static uint8_t tag_buffer[NDEF_MSG_BUF_SIZE];

/* NFC tag detection flag */
static volatile bool field_detected;

/* Forward declarations */
static void nfct_callback(const nrfx_nfct_evt_t *event);

/* NFCT configuration with callback */
static const nrfx_nfct_config_t nfct_config = {
    .rxtx_int_mask  = NRFX_NFCT_EVT_FIELD_DETECTED | 
                       NRFX_NFCT_EVT_RX_FRAMEEND,
    .cb             = nfct_callback
};

/* NFCT event callback handler */
static void nfct_callback(const nrfx_nfct_evt_t *event)
{
    switch (event->evt_id) {
    case NRFX_NFCT_EVT_FIELD_DETECTED:
        LOG_INF("NFC field detected");
        field_detected = true;
        break;

    case NRFX_NFCT_EVT_FIELD_LOST:
        LOG_INF("NFC field lost");
        field_detected = false;
        break;

    case NRFX_NFCT_EVT_RX_FRAMEEND:
        LOG_INF("NFC frame received");
        /* Here you would process the received data */
        break;

    default:
        LOG_DBG("Unhandled NFCT event: %d", event->evt_id);
        break;
    }
}

static int handle_ndef_text_record(struct nfc_ndef_record_desc *ndef_record,
                                   char *item_id, size_t max_len)
{
    int err;
    struct nfc_ndef_text_rec_payload text_payload;
    uint8_t payload_buffer[NDEF_MSG_BUF_SIZE];
    uint32_t payload_len = sizeof(payload_buffer);

    /* Get the payload from the record */
    err = nfc_ndef_record_payload_get(ndef_record,
                                      payload_buffer,
                                      &payload_len);
    if (err) {
        LOG_ERR("Error getting NDEF record payload: %d", err);
        return err;
    }

    /* Parse the text record */
    err = nfc_ndef_text_rec_parse(payload_buffer, 
                                 payload_len,
                                 &text_payload);
    if (err) {
        LOG_ERR("Error parsing NDEF text record: %d", err);
        return err;
    }

    LOG_INF("NDEF text record: lang_code='%s', text='%s'",
            text_payload.lang_code, text_payload.data);

    size_t copy_len = text_payload.data_len;
    if (copy_len >= max_len) {
        copy_len = max_len - 1;
    }

    memcpy(item_id, text_payload.data, copy_len);
    item_id[copy_len] = '\0';

    return copy_len;
}

static int parse_ndef_message(uint8_t *buf, uint32_t len, 
                              char *item_id, size_t max_len)
{
    int err;
    struct nfc_ndef_msg_desc *ndef_msg;
    uint8_t desc_buf[NDEF_MSG_BUF_SIZE];
    uint32_t desc_buf_len = sizeof(desc_buf);

    err = nfc_ndef_msg_parse(desc_buf,
                           &desc_buf_len,
                           buf,
                           &len);
    if (err) {
        LOG_ERR("Error parsing NDEF message: %d", err);
        return err;
    }

    ndef_msg = (struct nfc_ndef_msg_desc *)desc_buf;
    LOG_INF("NDEF message contains %d records", ndef_msg->record_count);

    /* Look for a Text record to find the item ID */
    for (size_t i = 0; i < ndef_msg->record_count; i++) {
        struct nfc_ndef_record_desc *record = ndef_msg->record[i];
        enum nfc_ndef_record_tnf tnf = record->tnf;
        const uint8_t *type = record->type;
        size_t type_length = record->type_length;

        /* Check for TNF Well Known + Type 'T' (Text record) */
        if ((tnf == TNF_WELL_KNOWN) && 
            (type_length == 1) && 
            (type[0] == 'T')) {
            
            return handle_ndef_text_record(record, item_id, max_len);
        }
    }

    LOG_WRN("No text record found in NDEF message");
    return -ENOENT;
}

int nfc_reader_init(void)
{
    int err;
    
    LOG_INF("Initializing NFC reader");
    
    /* Initialize the core NFC subsystem */
    err = nfc_init();
    if (err) {
        LOG_ERR("NFC initialization failed: %d", err);
        return err;
    }
    
    /* Start polling for NFC tags */
    err = nfc_start_polling();
    if (err) {
        LOG_ERR("NFC polling start failed: %d", err);
        return err;
    }
    
    LOG_INF("NFC reader initialization complete");
    return 0;
}

int nfc_init(void)
{
    nrfx_err_t err;

    LOG_INF("Initializing NFC T2T subsystem");

    /* Initialize field detection flag */
    field_detected = false;
    
    /* Initialize the Type 2 Tag library with nrfxlib */
    err = nrfx_nfct_init(&nfct_config);
    if (err != NRFX_SUCCESS) {
        LOG_ERR("Error initializing NFC T2T library: 0x%x", err);
        return -EIO;
    }

    LOG_INF("NFC initialization successful");
    return 0;
}

int nfc_start_polling(void)
{
    LOG_INF("Starting NFC field polling");

    /* Enable the NFCT peripheral */
    nrfx_nfct_enable();
    
    /* Just enable the hardware and wait for callbacks */
    LOG_INF("NFC polling started - waiting for tag detection");
    
    return 0;
}

int nfc_stop_polling(void)
{
    LOG_INF("Stopping NFC field polling");
    
    /* Disable the NFCT peripheral */
    nrfx_nfct_disable();

    return 0;
}

int nfc_handle_tag_detected(char *item_id, size_t max_len)
{
    int err;
    uint32_t data_len = sizeof(tag_buffer);

    LOG_INF("NFC tag detected - attempting to read");

    /* This is a simplified implementation. In a real application, 
     * you would read the data from the tag using callback functions
     * from the NFCT driver events. For now, we'll just simulate
     * by parsing hardcoded NDEF data.
     */
    
    /* In a real implementation, you would read the tag here */
    /* For now we'll simulate NDEF data */
    uint8_t sample_ndef[] = {
        /* NDEF message with a text record containing "item123" */
        0xD1,           /* TNF=0x01 (Well Known), SR=1, ME=1, MB=1 */
        0x01,           /* Record type length */
        0x08,           /* Payload length */
        'T',            /* Record type: 'T' for TEXT */
        0x02, 'e', 'n', /* Language code: 'en' */
        'i', 't', 'e', 'm', '1', '2', '3' /* Text: "item123" */
    };
    
    /* Copy sample data to buffer */
    memcpy(tag_buffer, sample_ndef, sizeof(sample_ndef));
    data_len = sizeof(sample_ndef);

    LOG_INF("Successfully read %d bytes from NFC tag", data_len);

    /* Parse NDEF message to extract the item ID */
    err = parse_ndef_message(tag_buffer, data_len, item_id, max_len);
    if (err < 0) {
        LOG_ERR("Error parsing NDEF message for item ID: %d", err);
        return err;
    }

    LOG_INF("Successfully read item ID: %s", item_id);
    
    return err; /* Return length of item ID */
} 