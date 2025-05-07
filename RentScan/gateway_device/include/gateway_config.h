/**
 * @file gateway_config.h
 * @brief Configuration parameters for the gateway device (BLE central + backend connector)
 */

#ifndef GATEWAY_CONFIG_H
#define GATEWAY_CONFIG_H

/** BLE configuration */
#define BLE_SCAN_INTERVAL 0x0060      /**< Scan interval (60 ms) */
#define BLE_SCAN_WINDOW 0x0050        /**< Scan window (50 ms) - Increased for better discovery */
#define BLE_SCAN_TIMEOUT_MS 10000     /**< Scan timeout in milliseconds (10 seconds) - Increased */
#define BLE_CONN_RETRY_LIMIT 5        /**< Maximum connection retry attempts - Increased */
#define BLE_CONN_RETRY_DELAY_MS 2000  /**< Delay between connection attempts (ms) - Increased */
#define BLE_CONN_SUPERVISION_TIMEOUT 800 /**< Connection supervision timeout (8 seconds) - Increased */

/** Gateway behavior configuration */
#define GATEWAY_RECONNECT_DELAY_MS 3000 /**< Delay before reconnecting after disconnect (ms) */
#define GATEWAY_HEALTH_CHECK_PERIOD_MS 30000 /**< Period for health checks (ms) */
#define GATEWAY_ERROR_RESET_THRESHOLD 5  /**< Number of consecutive errors before reset */

#endif /* GATEWAY_CONFIG_H */