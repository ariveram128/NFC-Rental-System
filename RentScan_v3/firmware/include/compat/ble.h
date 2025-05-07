/**
 * @file ble.h
 * @brief Compatibility layer for ble.h from nRF SDK
 */

#ifndef BLE_COMPAT_H__
#define BLE_COMPAT_H__

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/conn.h>
#include "app_error.h"

/* Bluetooth addresses */
typedef bt_addr_le_t ble_gap_addr_t;

/* Connection handles */
typedef uint16_t ble_conn_handle_t;
#define BLE_CONN_HANDLE_INVALID 0xFFFF

/* Characteristic properties */
#define BLE_GATT_CHAR_PROP_BROADCAST       0x01
#define BLE_GATT_CHAR_PROP_READ            0x02
#define BLE_GATT_CHAR_PROP_WRITE_WO_RESP   0x04
#define BLE_GATT_CHAR_PROP_WRITE           0x08
#define BLE_GATT_CHAR_PROP_NOTIFY          0x10
#define BLE_GATT_CHAR_PROP_INDICATE        0x20
#define BLE_GATT_CHAR_PROP_AUTH_SIGNED_WR  0x40
#define BLE_GATT_CHAR_PROP_EXT_PROP        0x80

/* BLE events */
typedef enum {
    BLE_GAP_EVT_CONNECTED,
    BLE_GAP_EVT_DISCONNECTED,
    BLE_GAP_EVT_CONN_PARAM_UPDATE,
    BLE_GATTC_EVT_WRITE_RSP,
    BLE_GATTS_EVT_WRITE,
    BLE_GATTS_EVT_SYS_ATTR_MISSING,
} ble_evt_type_t;

/* BLE parameter types */
typedef struct {
    uint8_t uuid_type;
    uint16_t uuid;
    uint8_t char_props;
} ble_gatts_char_md_t;

typedef struct {
    uint8_t read_perm;
    uint8_t write_perm;
    uint8_t vlen:1;
} ble_gatts_attr_md_t;

typedef struct {
    const uint8_t *p_value;
    uint16_t len;
    uint16_t init_len;
    uint16_t max_len;
    uint8_t *p_uuid;
} ble_gatts_attr_t;

typedef struct {
    uint16_t handle;
    uint16_t cccd_handle;
    uint16_t value_handle;
} ble_gatts_char_handles_t;

/* BLE event structure */
typedef struct {
    uint16_t conn_handle;
    ble_evt_type_t event_type;
} ble_evt_t;

/**
 * @brief Initialize the BLE stack
 */
static inline ret_code_t ble_stack_init(void)
{
    /* In Zephyr, BT stack initialization is done by bt_enable() */
    /* This would be implemented using Zephyr BT API */
    return NRF_SUCCESS;
}

/* Characteristic definition */
static inline ret_code_t sd_ble_gatts_characteristic_add(
                                        uint16_t                   service_handle,
                                        ble_gatts_char_md_t const *p_char_md,
                                        ble_gatts_attr_t const    *p_attr,
                                        ble_gatts_char_handles_t  *p_handles)
{
    /* Would be implemented using bt_gatt_characteristic_add() */
    return NRF_SUCCESS;
}

/* Notification/Indication */
static inline ret_code_t sd_ble_gatts_hvx(
                                uint16_t conn_handle, 
                                void *p_hvx_params)
{
    /* Would be implemented using bt_gatt_notify() or bt_gatt_indicate() */
    return NRF_SUCCESS;
}

/**
 * @brief BLE advertising parameters
 */
typedef struct {
    bool connectable;
    uint16_t interval;
} ble_adv_params_t;

/**
 * @brief Start advertising
 */
static inline ret_code_t ble_advertising_start(ble_adv_params_t const *p_adv_params)
{
    /* Would be implemented using bt_le_adv_start() */
    return NRF_SUCCESS;
}

#endif // BLE_COMPAT_H__