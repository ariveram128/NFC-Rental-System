/**
 * @file ble_srv_common.h
 * @brief Compatibility layer for ble_srv_common.h from nRF SDK
 */

#ifndef BLE_SRV_COMMON_COMPAT_H__
#define BLE_SRV_COMMON_COMPAT_H__

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include "app_error.h"

/* BLE service security modes */
typedef enum {
    SEC_OPEN        = 0,
    SEC_JUST_WORKS  = 1,
    SEC_MITM        = 2,
    SEC_OOB         = 3,
} ble_security_mode_t;

/* BLE service connection handle */
typedef uint16_t ble_conn_handle_t;
#define BLE_CONN_HANDLE_INVALID 0xFFFF

/* Service handles */
typedef struct {
    uint16_t service_handle;
    uint16_t char_value_handle;
} ble_srv_handles_t;

/* Service connection context */
typedef struct {
    ble_conn_handle_t conn_handle;
} ble_srv_conn_ctx_t;

/**
 * @brief Add a characteristic to a service
 */
static inline ret_code_t ble_srv_add_char(uint16_t                   service_handle,
                                         const struct bt_uuid     *  p_uuid,
                                         uint8_t                     perm,
                                         uint16_t                  * p_char_handle)
{
    /* This would be implemented using Zephyr's BT GATT API */
    return NRF_SUCCESS;
}

/**
 * @brief Add a descriptor to a characteristic
 */
static inline ret_code_t ble_srv_add_desc(uint16_t                   char_handle,
                                         const struct bt_uuid     *  p_uuid,
                                         uint8_t                  *  p_value,
                                         uint16_t                    value_len,
                                         uint16_t                  * p_desc_handle)
{
    /* This would be implemented using Zephyr's BT GATT API */
    return NRF_SUCCESS;
}

/**
 * @brief Notify a characteristic value
 */
static inline ret_code_t ble_srv_notify(const ble_srv_conn_ctx_t * p_ctx,
                                      uint16_t                   char_handle,
                                      uint8_t                  * p_value,
                                      uint16_t                   value_len)
{
    /* This would be implemented using Zephyr's BT GATT API */
    return NRF_SUCCESS;
}

/* CCCD (Client Characteristic Configuration Descriptor) handling */
typedef struct {
    uint8_t enabled;
} ble_srv_cccd_config_t;

/* Connection configuration */
typedef struct {
    ble_security_mode_t read_perm;
    ble_security_mode_t write_perm;
    uint8_t vlen:1;
    uint8_t vloc:2;
    uint8_t rd_auth:1;
    uint8_t wr_auth:1;
} ble_srv_conn_cfg_t;

#endif // BLE_SRV_COMMON_COMPAT_H__