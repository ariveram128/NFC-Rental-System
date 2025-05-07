/**
 * @file nfc_ndef_msg.h
 * @brief Compatibility layer for nfc_ndef_msg.h from nRF SDK
 */

#ifndef NFC_NDEF_MSG_COMPAT_H__
#define NFC_NDEF_MSG_COMPAT_H__

#include <zephyr/kernel.h>
#include "app_error.h"

/* Maximum number of records in an NDEF message */
#define NFC_NDEF_MSG_MAX_RECORDS 10

/* NDEF record types */
#define NFC_NDEF_RECORD_TNF_EMPTY      0x00
#define NFC_NDEF_RECORD_TNF_WELL_KNOWN 0x01
#define NFC_NDEF_RECORD_TNF_MEDIA_TYPE 0x02
#define NFC_NDEF_RECORD_TNF_URI        0x03
#define NFC_NDEF_RECORD_TNF_EXT_TYPE   0x04
#define NFC_NDEF_RECORD_TNF_UNKNOWN    0x05

/* NDEF record descriptor */
typedef struct {
    uint8_t   tnf;                /**< Type name format. */
    uint8_t * p_type;             /**< Type string. */
    uint8_t   type_len;           /**< Type string length. */
    uint8_t * p_id;               /**< ID string. */
    uint8_t   id_len;             /**< ID string length. */
    uint8_t * p_payload;          /**< Payload data. */
    uint32_t  payload_len;        /**< Payload data length. */
} nfc_ndef_record_desc_t;

/* NDEF message descriptor */
typedef struct {
    nfc_ndef_record_desc_t * p_record;     /**< Array of record descriptors. */
    uint32_t                  record_count; /**< Number of records. */
} nfc_ndef_msg_desc_t;

/**
 * @brief Initialize an NDEF message descriptor
 */
static inline ret_code_t nfc_ndef_msg_init(nfc_ndef_msg_desc_t * p_msg)
{
    if (p_msg == NULL) {
        return NRF_ERROR_NULL;
    }
    
    p_msg->record_count = 0;
    return NRF_SUCCESS;
}

/**
 * @brief Add a record to an NDEF message
 */
static inline ret_code_t nfc_ndef_msg_record_add(nfc_ndef_msg_desc_t * p_msg,
                                                nfc_ndef_record_desc_t * p_record)
{
    if (p_msg == NULL || p_record == NULL) {
        return NRF_ERROR_NULL;
    }
    
    if (p_msg->record_count >= NFC_NDEF_MSG_MAX_RECORDS) {
        return NRF_ERROR_NO_MEM;
    }
    
    p_msg->p_record[p_msg->record_count++] = *p_record;
    return NRF_SUCCESS;
}

/**
 * @brief Encode an NDEF message
 */
static inline ret_code_t nfc_ndef_msg_encode(nfc_ndef_msg_desc_t * p_msg,
                                           uint8_t * p_buffer,
                                           uint32_t * p_len)
{
    if (p_msg == NULL || p_buffer == NULL || p_len == NULL) {
        return NRF_ERROR_NULL;
    }

    /* This would be implemented with actual NDEF encoding logic */
    
    return NRF_SUCCESS;
}

#endif // NFC_NDEF_MSG_COMPAT_H__