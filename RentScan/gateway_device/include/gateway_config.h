/**
 * @file gateway_config.h
 * @brief Configuration parameters for the gateway device (BLE central + backend connector)
 */

#ifndef GATEWAY_CONFIG_H
#define GATEWAY_CONFIG_H

/** BLE configuration */
#define BLE_SCAN_INTERVAL 0x0050      /**< Scan interval (50 ms) */
#define BLE_SCAN_WINDOW 0x0030        /**< Scan window (30 ms) */
#define BLE_SCAN_TIMEOUT_MS 5000      /**< Scan timeout in milliseconds (5 seconds) */
#define BLE_CONN_RETRY_LIMIT 3        /**< Maximum connection retry attempts */
#define BLE_CONN_RETRY_DELAY_MS 1000  /**< Delay between connection attempts (ms) */
#define BLE_CONN_SUPERVISION_TIMEOUT 400 /**< Connection supervision timeout (4 seconds) */

/** Gateway behavior configuration */
#define GATEWAY_RECONNECT_DELAY_MS 5000 /**< Delay before reconnecting after disconnect (ms) */
#define GATEWAY_HEALTH_CHECK_PERIOD_MS 30000 /**< Period for health checks (ms) */
#define GATEWAY_ERROR_RESET_THRESHOLD 5  /**< Number of consecutive errors before reset */

#endif /* GATEWAY_CONFIG_H */ 