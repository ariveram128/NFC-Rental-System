/**
 * @file ble_srv_common.h
 * @brief Compatibility layer for Nordic SDK BLE service common APIs
 *
 * This file provides compatibility with Nordic SDK BLE service common APIs
 * when using Zephyr-based nRF Connect SDK
 */

#ifndef BLE_SRV_COMMON_H__
#define BLE_SRV_COMMON_H__

#include "ble.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Check if notification is enabled in CCCD
 *
 * @param[in] p_data Pointer to the CCCD value
 *
 * @return true if notification is enabled, false otherwise
 */
static inline bool ble_srv_is_notification_enabled(uint8_t const * p_data)
{
    return (p_data[0] & 0x01);
}

/**
 * @brief Check if indication is enabled in CCCD
 *
 * @param[in] p_data Pointer to the CCCD value
 *
 * @return true if indication is enabled, false otherwise
 */
static inline bool ble_srv_is_indication_enabled(uint8_t const * p_data)
{
    return (p_data[0] & 0x02);
}

/**
 * @brief Add a characteristic to a service
 *
 * @param[in] service_handle Service handle
 * @param[in] p_char_params Characteristic parameters
 * @param[out] p_handles Characteristic handles
 *
 * @return NRF_SUCCESS on success, otherwise an error code
 */
static inline ret_code_t characteristic_add(uint16_t service_handle,
                                          ble_add_char_params_t * p_char_params,
                                          ble_gatts_char_handles_t * p_handles)
{
    /* This is a stub implementation - would be implemented in a real system */
    p_handles->value_handle = 1;
    p_handles->cccd_handle = 2;
    return NRF_SUCCESS;
}

#ifdef __cplusplus
}
#endif

#endif // BLE_SRV_COMMON_H__