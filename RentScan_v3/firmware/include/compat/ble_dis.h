/**
 * @file ble_dis.h
 * @brief Compatibility layer for Nordic SDK BLE Device Information Service APIs
 *
 * This file provides compatibility with Nordic SDK BLE DIS APIs
 * when using Zephyr-based nRF Connect SDK
 */

#ifndef BLE_DIS_H__
#define BLE_DIS_H__

#include "ble.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Helper structure for UTF-8 strings
 */
typedef struct {
    uint8_t const * p_str;  /**< Pointer to the UTF-8 string. */
    uint16_t        length; /**< Length of the UTF-8 string. */
} ble_srv_utf8_str_t;

/**
 * @brief Device Information Service init structure
 */
typedef struct {
    ble_srv_utf8_str_t manufact_name_str;    /**< Manufacturer Name String. */
    ble_srv_utf8_str_t model_num_str;         /**< Model Number String. */
    ble_srv_utf8_str_t serial_num_str;        /**< Serial Number String. */
    ble_srv_utf8_str_t hw_rev_str;            /**< Hardware Revision String. */
    ble_srv_utf8_str_t fw_rev_str;            /**< Firmware Revision String. */
    ble_srv_utf8_str_t sw_rev_str;            /**< Software Revision String. */
    uint8_t            dis_char_rd_sec;       /**< Security level for reading DIS characteristics. */
} ble_dis_init_t;

/**
 * @brief Helper function for encoding a UTF-8 string from an ASCII string
 *
 * @param[out] p_utf8_str   Pointer to the UTF-8 string structure to fill.
 * @param[in]  p_ascii_str  Pointer to the ASCII string.
 */
static inline void ble_srv_ascii_to_utf8(ble_srv_utf8_str_t * p_utf8_str, char * p_ascii_str)
{
    p_utf8_str->p_str = (uint8_t *)p_ascii_str;
    p_utf8_str->length = strlen(p_ascii_str);
}

/**
 * @brief Function for initializing the Device Information Service.
 *
 * @param[in] p_dis_init   Information needed to initialize the service.
 *
 * @return NRF_SUCCESS on success, otherwise an error code.
 */
static inline ret_code_t ble_dis_init(const ble_dis_init_t * p_dis_init)
{
    /* This is a stub implementation - would be implemented in a real system */
    return 0;
}

#ifdef __cplusplus
}
#endif

#endif // BLE_DIS_H__ 