/**
 * @file ble.h
 * @brief Compatibility layer for Nordic SDK BLE API
 *
 * This file provides compatibility with Nordic SDK BLE APIs
 * when using Zephyr-based nRF Connect SDK
 */

#ifndef BLE_H__
#define BLE_H__

#include <stdint.h>
#include <string.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/bluetooth/uuid.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief GATT characteristic handles
 */
typedef struct {
    uint16_t value_handle;   /**< Value handle */
    uint16_t cccd_handle;    /**< CCCD handle */
    uint16_t sccd_handle;    /**< SCCD handle */
} ble_gatts_char_handles_t;

/**
 * @brief Connection handle type
 */
typedef uint16_t ble_conn_handle_t;

/**
 * @brief Invalid connection handle value
 */
#define BLE_CONN_HANDLE_INVALID     0xFFFF

/**
 * @brief Security modes
 */
#define SEC_OPEN                    0  /**< Open link. */
#define SEC_JUST_WORKS              1  /**< Just Works pairing. */
#define SEC_MITM                    2  /**< MITM protected pairing. */

/**
 * @brief BLE UUID type
 */
typedef struct {
    uint16_t    uuid;       /**< 16-bit UUID value or octets. */
    uint8_t     type;       /**< UUID type, see @ref BLE_UUID_TYPES. */
} ble_uuid_t;

/**
 * @brief 128-bit UUID values
 */
typedef struct {
    uint8_t uuid128[16]; /**< 128-bit UUID value. */
} ble_uuid128_t;

/**
 * @brief GATT characteristic properties
 */
typedef struct {
    uint8_t broadcast      : 1; /**< Broadcasting of value permitted. */
    uint8_t read           : 1; /**< Reading value permitted. */
    uint8_t write_wo_resp  : 1; /**< Writing value with Write Command permitted. */
    uint8_t write          : 1; /**< Writing value with Write Request permitted. */
    uint8_t notify         : 1; /**< Notications of value permitted. */
    uint8_t indicate       : 1; /**< Indications of value permitted. */
    uint8_t auth_signed_wr : 1; /**< Authenticated signed writes permitted. */
} ble_gatt_char_props_t;

/**
 * @brief BLE GATTS service types
 */
#define BLE_GATTS_SRVC_TYPE_PRIMARY  0  /**< Primary Service. */
#define BLE_GATTS_SRVC_TYPE_SECONDARY 1 /**< Secondary Service. */

/**
 * @brief BLE GATTS write event
 */
typedef struct {
    uint16_t handle;   /**< Attribute handle. */
    uint16_t offset;   /**< Offset in bytes to write from. */
    uint16_t len;      /**< Length in bytes to write. */
    uint8_t  *data;    /**< Data to write. */
} ble_gatts_evt_write_t;

/**
 * @brief BLE GATTS HVX types
 */
#define BLE_GATT_HVX_NOTIFICATION    1  /**< Notification. */
#define BLE_GATT_HVX_INDICATION      2  /**< Indication. */

/**
 * @brief BLE GATTS HVX parameters
 */
typedef struct {
    uint16_t handle;   /**< Attribute handle. */
    uint8_t  type;     /**< Notification or Indication, see @ref BLE_GATT_HVX_TYPES. */
    uint16_t offset;   /**< Offset in bytes for the value. */
    uint16_t *p_len;   /**< Length in bytes of the value. */
    uint8_t  *p_data;  /**< Data to be written. */
} ble_gatts_hvx_params_t;

/**
 * @brief BLE characteristic add parameters
 */
typedef struct {
    uint16_t                    uuid;           /**< UUID of the characteristic. */
    uint8_t                     uuid_type;      /**< UUID type. */
    uint16_t                    max_len;        /**< Maximum length of the characteristic value. */
    uint16_t                    init_len;       /**< Initial length of the characteristic value. */
    uint8_t                    *p_init_value;   /**< Initial value of the characteristic */
    ble_gatt_char_props_t       char_props;     /**< Characteristic properties. */
    uint8_t                     read_access;    /**< Read access level. */
    uint8_t                     write_access;   /**< Write access level. */
    uint8_t                     cccd_write_access; /**< CCCD write access level. */
} ble_add_char_params_t;

/**
 * @brief BLE GATTS event types
 */
typedef enum {
    BLE_GATTS_EVT_WRITE = 2,    /**< Write operation performed. Changed to 2 to avoid conflict */
} ble_gatts_evt_type_t;

/**
 * @brief BLE GATTS event parameters
 */
typedef struct {
    union {
        ble_gatts_evt_write_t write;  /**< Write event parameters. */
        // Add other GATTS event params here if needed
    } params;                         /**< Event parameter union. */
    uint16_t conn_handle;             /**< Connection handle for GATTS events. */
} ble_gatts_evt_t;

/**
 * @brief BLE GAP event types
 */
typedef enum {
    BLE_GAP_EVT_CONNECTED = 1,       /**< Connection established. */
    BLE_GAP_EVT_DISCONNECTED = 3     /**< Connection terminated. Changed from 2 to 3 to avoid conflict with BLE_GATTS_EVT_WRITE */
} ble_gap_evt_type_t;

/**
 * @brief BLE GAP event parameters
 */
typedef struct {
    union {
        struct {
            uint16_t conn_handle;    /**< Connection handle */
        } connected;
        struct {
            uint16_t conn_handle;    /**< Connection handle */
            uint8_t  reason;         /**< Disconnection reason */
        } disconnected;
    };
} ble_gap_evt_t;

/**
 * @brief BLE event header
 */
typedef struct {
    uint16_t evt_id;  /**< Event ID. */
} ble_evt_hdr_t;

/**
 * @brief BLE event structure
 */
typedef struct {
    ble_evt_hdr_t header;            /**< Event header. */
    union {
        ble_gap_evt_t   gap_evt;     /**< GAP event parameters. */
        ble_gatts_evt_t gatts_evt;   /**< GATTS event parameters. */
    } evt;                           /**< Event parameters. */
} ble_evt_t;

/**
 * @brief Compatibility mapping for characteristic_add
 */
static inline ret_code_t characteristic_add(uint16_t service_handle,
                                            ble_add_char_params_t * p_char_params,
                                            ble_gatts_char_handles_t * p_handles)
{
    /* Implementation would be provided in an accompanying .c file */
    /* For now, just return success */
    return 0;
}

/**
 * @brief Dummy implementation for service UUID add
 */
static inline ret_code_t sd_ble_uuid_vs_add(ble_uuid128_t const *p_vs_uuid, uint8_t *p_uuid_type)
{
    /* Implementation would be provided in an accompanying .c file */
    /* For now, just return success */
    *p_uuid_type = 3; // Dummy value
    return 0;
}

/**
 * @brief Dummy implementation for service add
 */
static inline ret_code_t sd_ble_gatts_service_add(uint8_t type, ble_uuid_t const *p_uuid, uint16_t *p_handle)
{
    /* Implementation would be provided in an accompanying .c file */
    /* For now, just return success */
    *p_handle = 1; // Dummy value
    return 0;
}

/**
 * @brief Dummy implementation for system attributes set
 */
static inline ret_code_t sd_ble_gatts_sys_attr_set(uint16_t conn_handle, uint8_t const *p_sys_attr_data, uint16_t len, uint32_t flags)
{
    /* Implementation would be provided in an accompanying .c file */
    /* For now, just return success */
    return 0;
}

#ifdef __cplusplus
}
#endif

#endif // BLE_H__