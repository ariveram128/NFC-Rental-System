/**
 * @file nfc_ndef_text_rec.h
 * @brief Compatibility layer for NFC NDEF text record operations
 *
 * This file provides compatibility with Nordic SDK NFC NDEF text record APIs
 * when using Zephyr-based nRF Connect SDK
 */

#ifndef NFC_NDEF_TEXT_REC_H__
#define NFC_NDEF_TEXT_REC_H__

#include <zephyr/kernel.h>
#include "nfc_ndef_record.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Language code length for NDEF text records
 */
#define NFC_NDEF_TEXT_LANGUAGE_CODE_SIZE 2

/**
 * @brief NDEF text record language encoding options
 */
typedef enum {
    NFC_NDEF_TEXT_UTF8  = 0x00, /**< UTF-8 text encoding */
    NFC_NDEF_TEXT_UTF16 = 0x01  /**< UTF-16 text encoding */
} nfc_ndef_text_encoding_t;

/**
 * @brief Function for generating an NFC NDEF text record
 *
 * @param[in] p_text_data      Pointer to text data
 * @param[in] text_length      Length of text data
 * @param[in] p_language_code  Pointer to language code (ISO 639-1)
 * @param[in] encoding         Text encoding (UTF-8 or UTF-16)
 * @param[out] p_record_desc   Pointer to record descriptor
 *
 * @retval NRF_SUCCESS     If the function completed successfully
 * @retval Other           Other error codes returned by nfc_ndef_record_encode()
 */
ret_code_t nfc_ndef_text_record_create(uint8_t const * p_text_data,
                                       uint8_t text_length,
                                       uint8_t const * p_language_code,
                                       nfc_ndef_text_encoding_t encoding,
                                       nfc_ndef_record_desc_t * p_record_desc);

#ifdef __cplusplus
}
#endif

#endif // NFC_NDEF_TEXT_REC_H__ 