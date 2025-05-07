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

#ifdef __cplusplus
}
#endif

#endif // BLE_SRV_COMMON_H__