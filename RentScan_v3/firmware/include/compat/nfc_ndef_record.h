/**
 * @file nfc_ndef_record.h
 * @brief Compatibility layer for nfc_ndef_record.h from nRF SDK
 */

#ifndef NFC_NDEF_RECORD_COMPAT_H__
#define NFC_NDEF_RECORD_COMPAT_H__

#include <zephyr/kernel.h>
#include "app_error.h"
#include "nfc_ndef_msg.h"  // Include our existing NDEF message compatibility layer

/* NFC record types */
#define NFC_NDEF_RECORD_TEXT_TYPE_ID "T"
#define NFC_NDEF_RECORD_URI_TYPE_ID  "U"

/**
 * @brief Initialize a URI record descriptor
 */
static inline ret_code_t nfc_ndef_uri_record_init(
    nfc_ndef_record_desc_t * p_uri_rec_desc,
    uint8_t                   uri_id_code,
    uint8_t                 * p_uri_data,
    uint32_t                  uri_data_len)
{
    if (p_uri_rec_desc == NULL || p_uri_data == NULL || uri_data_len == 0) {
        return NRF_ERROR_NULL;
    }
    
    p_uri_rec_desc->tnf        = NFC_NDEF_RECORD_TNF_WELL_KNOWN;
    p_uri_rec_desc->p_type     = (uint8_t*)NFC_NDEF_RECORD_URI_TYPE_ID;
    p_uri_rec_desc->type_len   = sizeof(NFC_NDEF_RECORD_URI_TYPE_ID) - 1;
    p_uri_rec_desc->p_id       = NULL;
    p_uri_rec_desc->id_len     = 0;
    p_uri_rec_desc->p_payload  = p_uri_data;
    p_uri_rec_desc->payload_len = uri_data_len;
    
    return NRF_SUCCESS;
}

/**
 * @brief Initialize a text record descriptor
 */
static inline ret_code_t nfc_ndef_text_record_init(
    nfc_ndef_record_desc_t * p_text_rec_desc,
    uint8_t                   utf8_flag,
    uint8_t                 * p_language_code,
    uint32_t                  language_code_len,
    uint8_t                 * p_text_data,
    uint32_t                  text_len)
{
    if (p_text_rec_desc == NULL || p_language_code == NULL || 
        language_code_len == 0 || p_text_data == NULL || text_len == 0) {
        return NRF_ERROR_NULL;
    }
    
    p_text_rec_desc->tnf       = NFC_NDEF_RECORD_TNF_WELL_KNOWN;
    p_text_rec_desc->p_type    = (uint8_t*)NFC_NDEF_RECORD_TEXT_TYPE_ID;
    p_text_rec_desc->type_len  = sizeof(NFC_NDEF_RECORD_TEXT_TYPE_ID) - 1;
    p_text_rec_desc->p_id      = NULL;
    p_text_rec_desc->id_len    = 0;
    p_text_rec_desc->p_payload = p_text_data;
    p_text_rec_desc->payload_len = text_len;
    
    return NRF_SUCCESS;
}

#endif // NFC_NDEF_RECORD_COMPAT_H__