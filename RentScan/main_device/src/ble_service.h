/**
 * @file ble_service.h
 * @brief BLE service functionality for the main device
 */

#ifndef BLE_SERVICE_H
#define BLE_SERVICE_H

#include <zephyr/types.h>
#include <stdbool.h>
#include <stddef.h>
#include "../../common/include/rentscan_protocol.h"

/**
 * @brief Callback for received BLE data
 * 
 * @param data Pointer to data buffer
 * @param len Length of data
 */
typedef void (*ble_data_received_cb_t)(const uint8_t *data, uint16_t len);

/**
 * @brief Initialize the BLE service
 * 
 * @param data_received_cb Callback function to be called when data is received
 * @return int 0 on success, negative error code otherwise
 */
int ble_service_init(ble_data_received_cb_t data_received_cb);

/**
 * @brief Start BLE advertising
 * 
 * @param fast Use fast advertising interval if true, slow if false
 * @return int 0 on success, negative error code otherwise
 */
int ble_service_start_advertising(bool fast);

/**
 * @brief Stop BLE advertising
 * 
 * @return int 0 on success, negative error code otherwise
 */
int ble_service_stop_advertising(void);

/**
 * @brief Send data over BLE
 * 
 * @param data Pointer to data buffer
 * @param len Length of data
 * @return int 0 on success, negative error code otherwise
 */
int ble_service_send_data(const uint8_t *data, uint16_t len);

/**
 * @brief Send a RentScan message over BLE
 * 
 * @param msg Pointer to RentScan message
 * @return int 0 on success, negative error code otherwise
 */
int ble_service_send_message(const rentscan_msg_t *msg);

/**
 * @brief Get BLE connection status
 * 
 * @return true if connected
 * @return false if not connected
 */
bool ble_service_is_connected(void);

/**
 * @brief Disconnect from current BLE connection
 * 
 * @return int 0 on success, negative error code otherwise
 */
int ble_service_disconnect(void);

#endif /* BLE_SERVICE_H */ 