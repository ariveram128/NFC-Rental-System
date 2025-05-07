/**
 * @file main_device_config.h
 * @brief Configuration parameters for the main device (NFC reader + BLE peripheral)
 */

#ifndef MAIN_DEVICE_CONFIG_H
#define MAIN_DEVICE_CONFIG_H

/** NFC configuration */
#define NFC_POLL_PERIOD_MS 500        /**< NFC polling period in milliseconds */
#define NFC_READ_RETRY_LIMIT 3        /**< Number of NFC read retries */
#define NFC_WRITE_RETRY_LIMIT 3       /**< Number of NFC write retries */

/** BLE configuration - using Zephyr BLE GAP definitions */
#define BLE_ADV_FAST_INT_MIN 0x0020   /**< Fast advertising interval minimum (20 ms) */
#define BLE_ADV_FAST_INT_MAX 0x0040   /**< Fast advertising interval maximum (40 ms) */
#define BLE_ADV_SLOW_INT_MIN 0x0640   /**< Slow advertising interval minimum (1 second) */
#define BLE_ADV_SLOW_INT_MAX 0x0780   /**< Slow advertising interval maximum (1.2 seconds) */

/** Rental configuration */
#define DEFAULT_RENTAL_DURATION 3600  /**< Default rental duration in seconds (1 hour) */
#define RENTAL_EXPIRY_CHECK_PERIOD 60 /**< How often to check for rental expiry (seconds) */

#endif /* MAIN_DEVICE_CONFIG_H */