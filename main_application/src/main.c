/* main.c - RentScan NFC Tag Reader Application */

/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 * Copyright (c) 2025 RentScan Project Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

 #include <zephyr/types.h>
 #include <stddef.h>
 #include <stdbool.h>
 #include <string.h>
 #include <zephyr/kernel.h>
 #include <zephyr/sys/printk.h>
 #include <st25r3911b_nfca.h>
 #include <nfc/ndef/msg_parser.h>
 #include <nfc/t2t/parser.h>
 #include <zephyr/sys/byteorder.h>
 #include <zephyr/logging/log.h>
 
 // Include our custom rental logic header
 #include "rental_logic.h"
 
 LOG_MODULE_REGISTER(main_app, LOG_LEVEL_INF);
 
 #define NFCA_BD 128
 #define BITS_IN_BYTE 8
 #define MAX_TLV_BLOCKS 10
 #define MAX_NDEF_RECORDS 10
 #define NFCA_T2T_BUFFER_SIZE 1024
 #define NFCA_LAST_BIT_MASK 0x80
 #define NFCA_FDT_ALIGN_84 84
 #define NFCA_FDT_ALIGN_20 20
 
 #define NFC_T2T_READ_CMD 0x30
 #define NFC_T2T_READ_CMD_LEN 0x02
 
 #define NFC_TX_DATA_LEN 16
 #define NFC_RX_DATA_LEN 64
 
 #define T2T_MAX_DATA_EXCHANGE 16
 #define TAG_TYPE_2_DATA_AREA_MULTIPLICATOR 8
 #define TAG_TYPE_2_DATA_AREA_SIZE_OFFSET (NFC_T2T_CC_BLOCK_OFFSET + 2)
 #define TAG_TYPE_2_BLOCKS_PER_EXCHANGE (T2T_MAX_DATA_EXCHANGE / NFC_T2T_BLOCK_SIZE)
 
 #define TRANSMIT_DELAY 3000
 #define ALL_REQ_DELAY 2000
 
 static uint8_t tx_data[NFC_TX_DATA_LEN];
 static uint8_t rx_data[NFC_RX_DATA_LEN];
 
 static struct k_poll_event events[ST25R3911B_NFCA_EVENT_CNT];
 static struct k_work_delayable transmit_work;
 
 static struct st25r3911b_nfca_buf tx_buf = {
     .data = tx_data,
     .len = sizeof(tx_data)
 };
 
 static const struct st25r3911b_nfca_buf rx_buf = {
     .data = rx_data,
     .len = sizeof(rx_data)
 };
 
 enum nfc_tag_type {
     NFC_TAG_TYPE_UNSUPPORTED = 0,
     NFC_TAG_TYPE_T2T,
 };
 
 enum t2t_state {
     T2T_IDLE,
     T2T_HEADER_READ,
     T2T_DATA_READ
 };
 
 struct t2t_tag {
     enum t2t_state state;
     uint16_t data_bytes;
     uint8_t data[NFCA_T2T_BUFFER_SIZE];
 };
 
 static enum nfc_tag_type tag_type;
 static struct t2t_tag t2t;
 
 static void nfc_tag_detect(bool all_request)
 {
     int err;
     enum st25r3911b_nfca_detect_cmd cmd;
 
     tag_type = NFC_TAG_TYPE_UNSUPPORTED;
 
     cmd = all_request ? ST25R3911B_NFCA_DETECT_CMD_ALL_REQ :
         ST25R3911B_NFCA_DETECT_CMD_SENS_REQ;
 
     err = st25r3911b_nfca_tag_detect(cmd);
     if (err) {
         LOG_ERR("Tag detect error: %d", err);
     }
 }
 
 static int ftd_calculate(uint8_t *data, size_t len)
 {
     uint8_t ftd_align;
 
     ftd_align = (data[len - 1] & NFCA_LAST_BIT_MASK) ?
         NFCA_FDT_ALIGN_84 : NFCA_FDT_ALIGN_20;
 
     return len * NFCA_BD * BITS_IN_BYTE + ftd_align;
 }
 
 static int nfc_t2t_read_block_cmd_make(uint8_t *tx_data,
                        size_t tx_data_size,
                        uint8_t block_num)
 {
     if (!tx_data) {
         return -EINVAL;
     }
 
     if (tx_data_size < NFC_T2T_READ_CMD_LEN) {
         return -ENOMEM;
     }
 
     tx_data[0] = NFC_T2T_READ_CMD;
     tx_data[1] = block_num;
 
     return 0;
 }
 
 static int t2t_header_read(void)
 {
     int err;
     int ftd;
 
     err = nfc_t2t_read_block_cmd_make(tx_data, sizeof(tx_data), 0);
     if (err) {
         return err;
     }
 
     tx_buf.data = tx_data;
     tx_buf.len = NFC_T2T_READ_CMD_LEN;
 
     ftd = ftd_calculate(tx_data, NFC_T2T_READ_CMD_LEN);
 
     t2t.state = T2T_HEADER_READ;
 
     err = st25r3911b_nfca_transfer_with_crc(&tx_buf, &rx_buf, ftd);
 
     return err;
 }
 
 static void process_tag_data(const uint8_t *data, size_t data_len)
 {
     // Process NDEF data for rental logic
     int err;
     uint8_t desc_buf[NFC_NDEF_PARSER_REQUIRED_MEM(MAX_NDEF_RECORDS)];
     size_t desc_buf_len = sizeof(desc_buf);
     
     // Extract item ID from tag
     struct nfc_ndef_msg_desc *ndef_msg_desc;
     
     err = nfc_ndef_msg_parse(desc_buf,
                 &desc_buf_len,
                 data,
                 &data_len);
     if (err) {
         LOG_ERR("Error during parsing a NDEF message, err: %d", err);
         return;
     }
 
     ndef_msg_desc = (struct nfc_ndef_msg_desc *) desc_buf;
     LOG_INF("NDEF message contains %u record(s)", ndef_msg_desc->record_count);
 
     // Look for text records to use as item IDs
     for (size_t i = 0; i < ndef_msg_desc->record_count; i++) {
         const struct nfc_ndef_record_desc *record = ndef_msg_desc->record[i];
         
         // Check for Text Record Type
         if (record->tnf == TNF_WELL_KNOWN) {
             uint8_t nfc_text_rec_type[1] = {0x54}; // 'T' for TEXT record type
             
             if ((record->type_length == 1) && 
                 (memcmp(record->type, nfc_text_rec_type, 1) == 0)) {
                 
                 // Get payload data (skip TNF byte)
                 uint8_t *text_data = (uint8_t *)record->payload_descriptor;
                 uint8_t lang_code_len = text_data[0] & 0x3F;
                 uint8_t *item_id = text_data + 1 + lang_code_len;
                 size_t item_id_len = record->payload_length - 1 - lang_code_len;
                 
                 LOG_INF("Found Text Record with Item ID: %.*s", item_id_len, item_id);
                 
                 // Call the rental logic with the item ID
                 rental_logic_process_scan(item_id, item_id_len);
                 break;
             }
         }
     }
 }
 
 static void t2t_data_read_complete(uint8_t *data)
 {
     int err;
 
     if (!data) {
         LOG_ERR("No T2T data read");
         return;
     }
 
     // Declaration of Type 2 Tag structure.
     NFC_T2T_DESC_DEF(tag_data, MAX_TLV_BLOCKS);
     struct nfc_t2t *t2t = &NFC_T2T_DESC(tag_data);
 
     err = nfc_t2t_parse(t2t, data);
     if (err) {
         LOG_ERR("Not enough memory to read whole tag. Printing what has been read");
     }
 
     LOG_INF("Tag content parsed successfully");
 
     struct nfc_t2t_tlv_block *tlv_block = t2t->tlv_block_array;
 
     for (size_t i = 0; i < t2t->tlv_count; i++) {
         if (tlv_block->tag == NFC_T2T_TLV_NDEF_MESSAGE) {
             LOG_INF("NDEF message TLV found - processing for rental system");
             process_tag_data(tlv_block->value, tlv_block->length);
             tlv_block++;
         }
     }
 
     st25r3911b_nfca_tag_sleep();
 
     k_work_reschedule(&transmit_work, K_MSEC(TRANSMIT_DELAY));
 }
 
 static int t2t_on_data_read(const uint8_t *data, size_t data_len,
             void (*t2t_read_complete)(uint8_t *))
 {
     int err;
     int ftd;
     uint8_t block_to_read;
     uint16_t offset = 0;
     static uint8_t block_num;
 
     block_to_read = t2t.data_bytes / NFC_T2T_BLOCK_SIZE;
     offset = block_num * NFC_T2T_BLOCK_SIZE;
 
     memcpy(t2t.data + offset, data, data_len);
 
     block_num += TAG_TYPE_2_BLOCKS_PER_EXCHANGE;
 
     if (block_num > block_to_read) {
         block_num = 0;
         t2t.state = T2T_IDLE;
 
         if (t2t_read_complete) {
             t2t_read_complete(t2t.data);
         }
 
         return 0;
     }
 
     err = nfc_t2t_read_block_cmd_make(tx_data, sizeof(tx_data), block_num);
     if (err) {
         return err;
     }
 
     tx_buf.data = tx_data;
     tx_buf.len = NFC_T2T_READ_CMD_LEN;
 
     ftd = ftd_calculate(tx_data, NFC_T2T_READ_CMD_LEN);
 
     err = st25r3911b_nfca_transfer_with_crc(&tx_buf, &rx_buf, ftd);
 
     return err;
 }
 
 static int on_t2t_transfer_complete(const uint8_t *data, size_t len)
 {
     switch (t2t.state) {
     case T2T_HEADER_READ:
         t2t.data_bytes = TAG_TYPE_2_DATA_AREA_MULTIPLICATOR *
                  data[TAG_TYPE_2_DATA_AREA_SIZE_OFFSET];
 
         if ((t2t.data_bytes + NFC_T2T_FIRST_DATA_BLOCK_OFFSET) > sizeof(t2t.data)) {
             return -ENOMEM;
         }
 
         t2t.state = T2T_DATA_READ;
 
         return t2t_on_data_read(data, len, t2t_data_read_complete);
 
     case T2T_DATA_READ:
         return t2t_on_data_read(data, len, t2t_data_read_complete);
 
     default:
         return -EFAULT;
     }
 }
 
 static void transfer_handler(struct k_work *work)
 {
     nfc_tag_detect(false);
 }
 
 static void nfc_field_on(void)
 {
     LOG_INF("NFC field on");
     nfc_tag_detect(false);
 }
 
 static void nfc_timeout(bool tag_sleep)
 {
     if (tag_sleep) {
         LOG_INF("Tag sleep or no tag detected, sending ALL Request");
     } else {
         // Do nothing
     }
 
     // Sleep will block processing loop. Accepted as it is short.
     k_sleep(K_MSEC(ALL_REQ_DELAY));
 
     nfc_tag_detect(true);
 }
 
 static void nfc_field_off(void)
 {
     LOG_INF("NFC field off");
 }
 
 static void tag_detected(const struct st25r3911b_nfca_sens_resp *sens_resp)
 {
     LOG_INF("Anticollision: 0x%x Platform: 0x%x", 
         sens_resp->anticollison,
         sens_resp->platform_info);
 
     int err = st25r3911b_nfca_anticollision_start();
 
     if (err) {
         LOG_ERR("Anticollision error: %d", err);
     }
 }
 
 static void anticollision_completed(const struct st25r3911b_nfca_tag_info *tag_info,
                     int err)
 {
     if (err) {
         LOG_ERR("Error during anticollision avoidance");
         nfc_tag_detect(false);
         return;
     }
 
     LOG_INF("Tag info, type: %d", tag_info->type);
 
     if (tag_info->type == ST25R3911B_NFCA_TAG_TYPE_T2T) {
         LOG_INF("Type 2 Tag detected");
         tag_type = NFC_TAG_TYPE_T2T;
 
         err = t2t_header_read();
         if (err) {
             LOG_ERR("Type 2 Tag data read error %d", err);
         }
     } else {
         LOG_ERR("Unsupported tag type");
         tag_type = NFC_TAG_TYPE_UNSUPPORTED;
         nfc_tag_detect(false);
         return;
     }
 }
 
 static void transfer_completed(const uint8_t *data, size_t len, int err)
 {
     if (err) {
         LOG_ERR("NFC Transfer error: %d", err);
         return;
     }
 
     if (tag_type == NFC_TAG_TYPE_T2T) {
         err = on_t2t_transfer_complete(data, len);
         if (err) {
             LOG_ERR("NFC-A T2T read error: %d", err);
         }
     }
 }
 
 static void tag_sleep(void)
 {
     LOG_INF("Tag entered Sleep state");
 }
 
 static const struct st25r3911b_nfca_cb cb = {
     .field_on = nfc_field_on,
     .field_off = nfc_field_off,
     .tag_detected = tag_detected,
     .anticollision_completed = anticollision_completed,
     .rx_timeout = nfc_timeout,
     .transfer_completed = transfer_completed,
     .tag_sleep = tag_sleep
 };
 
 int main(void)
 {
     int err;
 
     LOG_INF("Starting RentScan NFC Tag Reader Application");
     
     // Initialize the tag reading module
     k_work_init_delayable(&transmit_work, transfer_handler);
 
     err = st25r3911b_nfca_init(events, ARRAY_SIZE(events), &cb);
     if (err) {
         LOG_ERR("NFCA initialization failed err: %d", err);
         return 0;
     }
 
     err = st25r3911b_nfca_field_on();
     if (err) {
         LOG_ERR("Field on error %d", err);
         return 0;
     }
 
     LOG_INF("NFC Tag Reader started - ready to scan rental items");
 
     while (true) {
         k_poll(events, ARRAY_SIZE(events), K_FOREVER);
         err = st25r3911b_nfca_process();
         if (err) {
             LOG_ERR("NFC-A process failed, err: %d", err);
             return 0;
         }
     }
 }