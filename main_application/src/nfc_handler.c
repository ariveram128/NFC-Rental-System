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
 #include "rental_logic.h"
 
 LOG_MODULE_REGISTER(nfc_handler, LOG_LEVEL_INF);
 
 #define MAX_NDEF_RECORDS 2
 #define NDEF_MSG_BUF_SIZE 256
 #define MAX_ITEM_ID_LEN 64
 
 /* Buffer used to hold an NFC NDEF message */
 static uint8_t ndef_msg_buf[NDEF_MSG_BUF_SIZE];
 
 /* Text content for the NFC tag */
 static uint8_t default_item_id[] = {'i', 't', 'e', 'm', '1', '2', '3'};
 static const uint8_t en_code[] = {'e', 'n'};
 
 /* Current item ID stored on the tag */
 static uint8_t current_item_id[MAX_ITEM_ID_LEN];
 static size_t current_item_id_len = 0;
 
/**
 * @brief Callback for NFC events
 */
static void nfc_callback(void *context,
                         nfc_t2t_event_t event,
                         const uint8_t *data,
                         size_t data_length)
{
    ARG_UNUSED(context);

    switch (event) {
    case NFC_T2T_EVENT_FIELD_ON:
        LOG_INF("NFC field detected");
        break;

    case NFC_T2T_EVENT_FIELD_OFF:
        LOG_INF("NFC field lost");
        break;
        
    /* Handle data read events */
    case NFC_T2T_EVENT_DATA_READ:
        LOG_INF("NFC tag read by external reader");
        break;
        
    /* The device doesn't provide a specific write event, so we need to 
     * rely on the application to update the tag content when needed 
     */
        
    default:
        LOG_DBG("Unhandled NFC event: %d", event);
        break;
    }
}
 /**
  * @brief Extract item ID from NDEF data
  */
 static int extract_item_id_from_ndef(const uint8_t *ndef_data, size_t ndef_len, 
                                     char *item_id, size_t max_len)
 {
     /* Try to find text content */
     for (size_t i = 0; i < ndef_len - 5; i++) {
         /* Look for TNF=1 (Well Known) and type = 'T' (Text) */
         if ((ndef_data[i] & 0x07) == 0x01 &&    /* TNF = 0x01 */
             ndef_data[i+1] == 0x01 &&           /* Type length = 1 */
             ndef_data[i+3] == 'T') {            /* Type = 'T' */
             
             uint8_t payload_len = ndef_data[i+2];
             if (i + 4 + payload_len > ndef_len) {
                 continue;  /* Not enough data */
             }
             
             /* Text record format: [status byte], [language code], [text] */
             uint8_t status_byte = ndef_data[i+4];
             uint8_t lang_code_len = status_byte & 0x3F;  /* Lower 6 bits */
             uint8_t text_start = i + 4 + 1 + lang_code_len;
             
             /* Extract the text */
             uint8_t text_len = payload_len - 1 - lang_code_len;
             if (text_len >= max_len) {
                 text_len = max_len - 1;
             }
             
             memcpy(item_id, &ndef_data[text_start], text_len);
             item_id[text_len] = '\0';
             
             LOG_INF("Extracted item ID from NDEF text record");
             return text_len;
         }
     }
     
     /* If we couldn't find a text record, use a simplified approach - extract ASCII */
     size_t id_len = 0;
     
     for (size_t i = 0; i < ndef_len && id_len < max_len - 1; i++) {
         if (ndef_data[i] >= 32 && ndef_data[i] <= 126) {
             item_id[id_len++] = ndef_data[i];
         }
     }
     
     if (id_len > 0) {
         item_id[id_len] = '\0';
         LOG_INF("Extracted ASCII text as item ID");
         return id_len;
     }
     
     LOG_ERR("No valid data found in NFC tag");
     return -EINVAL;
 }
 
 /**
  * @brief Create NDEF text record with item ID
  */
 static int create_item_id_ndef_msg(uint8_t *buffer, uint32_t *len)
 {
     int err;
     
     /* Use stored item ID if available, otherwise use default */
     const uint8_t *item_id_data;
     size_t item_id_len;
     
     if (current_item_id_len > 0) {
         item_id_data = current_item_id;
         item_id_len = current_item_id_len;
     } else {
         item_id_data = default_item_id;
         item_id_len = sizeof(default_item_id);
     }
 
     /* Create NFC NDEF text record with the item ID */
     NFC_NDEF_TEXT_RECORD_DESC_DEF(item_id_rec,
                                   UTF_8,
                                   en_code,
                                   sizeof(en_code),
                                   item_id_data,
                                   item_id_len);
 
     /* Create NFC NDEF message description */
     NFC_NDEF_MSG_DEF(nfc_item_msg, MAX_NDEF_RECORDS);
 
     /* Add text record to the message */
     err = nfc_ndef_msg_record_add(&NFC_NDEF_MSG(nfc_item_msg),
                                   &NFC_NDEF_TEXT_RECORD_DESC(item_id_rec));
     if (err < 0) {
         LOG_ERR("Cannot add item ID record: %d", err);
         return err;
     }
 
     /* Encode the message */
     err = nfc_ndef_msg_encode(&NFC_NDEF_MSG(nfc_item_msg),
                               buffer,
                               len);
     if (err < 0) {
         LOG_ERR("Cannot encode message: %d", err);
     }
 
     return err;
 }
 
 int nfc_reader_init(void)
 {
     int err;
     
     LOG_INF("Initializing NFC tag");
     
     /* Initialize the default item ID */
     memcpy(current_item_id, default_item_id, sizeof(default_item_id));
     current_item_id_len = sizeof(default_item_id);
     
     /* Initialize NFC */
     err = nfc_t2t_setup(nfc_callback, NULL);
     if (err) {
         LOG_ERR("Cannot setup NFC T2T library: %d", err);
         return err;
     }
     
     /* Create the NDEF message */
     uint32_t len = sizeof(ndef_msg_buf);
     err = create_item_id_ndef_msg(ndef_msg_buf, &len);
     if (err) {
         LOG_ERR("Cannot create NDEF message: %d", err);
         return err;
     }
     
     /* Set the message as NFC payload */
     err = nfc_t2t_payload_set(ndef_msg_buf, len);
     if (err) {
         LOG_ERR("Cannot set NFC payload: %d", err);
         return err;
     }
     
     /* Start NFC emulation */
     err = nfc_t2t_emulation_start();
     if (err) {
         LOG_ERR("Cannot start NFC emulation: %d", err);
         return err;
     }
     
     LOG_INF("NFC tag initialized with item ID: %s", current_item_id);
     return 0;
 }
 
 /* The following functions are kept for API compatibility
  * but modified to work with tag emulation mode
  */
 int nfc_init(void)
 {
     /* This function is now handled by nfc_reader_init */
     return 0;
 }
 
 int nfc_start_polling(void)
 {
     /* No polling in tag mode */
     return 0;
 }
 
 int nfc_stop_polling(void)
 {
     /* No polling in tag mode */
     return 0;
 }
 
 int nfc_handle_tag_detected(char *item_id, size_t max_len)
 {
     /* Not used in tag mode - handled by callback */
     return 0;
 }
 
 /* Add a function to update the tag content */
 int nfc_update_item_id(const char *new_id, size_t id_len)
 {
     int err;
     
     if (id_len >= MAX_ITEM_ID_LEN) {
         LOG_ERR("Item ID too long");
         return -EINVAL;
     }
     
     /* Update stored item ID */
     memcpy(current_item_id, new_id, id_len);
     current_item_id_len = id_len;
     
     /* Re-create NDEF message */
     uint32_t len = sizeof(ndef_msg_buf);
     err = create_item_id_ndef_msg(ndef_msg_buf, &len);
     if (err) {
         LOG_ERR("Cannot create NDEF message: %d", err);
         return err;
     }
     
     /* Update NFC payload */
     err = nfc_t2t_payload_set(ndef_msg_buf, len);
     if (err) {
         LOG_ERR("Cannot set NFC payload: %d", err);
         return err;
     }
     
     LOG_INF("NFC tag updated with new item ID: %s", current_item_id);
     return 0;
 }