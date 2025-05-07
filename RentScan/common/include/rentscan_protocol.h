/**
 * @file rentscan_protocol.h
 * @brief Common protocol definitions for RentScan system
 */

#ifndef RENTSCAN_PROTOCOL_H
#define RENTSCAN_PROTOCOL_H

#include <zephyr/types.h>
#include <zephyr/bluetooth/uuid.h>

/**
 * @brief RentScan service UUIDs
 * 
 * Custom UUID: 18ee2ef5-263d-4559-953c-d66077c89ae6
 * RX UUID:     18ee2ef5-263d-4559-953c-d66077c89ae7
 * TX UUID:     18ee2ef5-263d-4559-953c-d66077c89ae8
 */
#define BT_UUID_RENTSCAN_VAL \
    BT_UUID_128_ENCODE(0x18ee2ef5, 0x263d, 0x4559, 0x953c, 0xd66077c89ae6)
#define BT_UUID_RENTSCAN_RX_VAL \
    BT_UUID_128_ENCODE(0x18ee2ef5, 0x263d, 0x4559, 0x953c, 0xd66077c89ae7)
#define BT_UUID_RENTSCAN_TX_VAL \
    BT_UUID_128_ENCODE(0x18ee2ef5, 0x263d, 0x4559, 0x953c, 0xd66077c89ae8)

#define BT_UUID_RENTSCAN      BT_UUID_DECLARE_128(BT_UUID_RENTSCAN_VAL)
#define BT_UUID_RENTSCAN_RX   BT_UUID_DECLARE_128(BT_UUID_RENTSCAN_RX_VAL)
#define BT_UUID_RENTSCAN_TX   BT_UUID_DECLARE_128(BT_UUID_RENTSCAN_TX_VAL)

/** Device name for BLE advertising */
#define RENTSCAN_DEVICE_NAME "RentScan"

/** Maximum tag ID length */
#define MAX_TAG_ID_LEN 16

/** Maximum message payload size */
#define MAX_MSG_PAYLOAD 128

/** Command types */
typedef enum {
    CMD_RENTAL_START = 1,  /**< Start a rental */
    CMD_RENTAL_END = 2,    /**< End a rental */
    CMD_STATUS_REQ = 3,    /**< Request status */
    CMD_STATUS_RESP = 4,   /**< Status response */
    CMD_ERROR = 0xFF       /**< Error message */
} rentscan_cmd_type_t;

/** Rental status */
typedef enum {
    STATUS_AVAILABLE = 0,  /**< Item available for rent */
    STATUS_RENTED = 1,     /**< Item currently rented */
    STATUS_EXPIRED = 2,    /**< Rental expired */
    STATUS_ERROR = 0xFF    /**< Error state */
} rentscan_status_t;

/** RentScan message structure */
typedef struct {
    uint8_t cmd;                    /**< Command type */
    uint8_t status;                 /**< Status code */
    uint8_t tag_id[MAX_TAG_ID_LEN]; /**< NFC Tag ID */
    uint8_t tag_id_len;             /**< Length of NFC Tag ID */
    uint32_t timestamp;             /**< Unix timestamp */
    uint32_t duration;              /**< Rental duration in seconds */
    uint8_t payload[MAX_MSG_PAYLOAD]; /**< Additional data */
    uint8_t payload_len;            /**< Length of payload */
} rentscan_msg_t;

#endif /* RENTSCAN_PROTOCOL_H */ 