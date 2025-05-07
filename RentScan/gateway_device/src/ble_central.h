/**
 * @file ble_central.h
 * @brief BLE central functionality for the gateway device
 */

#ifndef BLE_CENTRAL_H
#define BLE_CENTRAL_H

#include <zephyr/types.h>
#include <stdbool.h>
#include <stddef.h>
#include "../../common/include/rentscan_protocol.h"

/**
 * @brief Callback for received BLE messages
 * 
 * @param msg Pointer to RentScan message
 */
typedef void (*ble_msg_received_cb_t)(const rentscan_msg_t *msg);

/**
 * @brief Initialize the BLE central
 * 
 * @param msg_received_cb Callback function to be called when a message is received
 * @return int 0 on success, negative error code otherwise
 */
int ble_central_init(ble_msg_received_cb_t msg_received_cb);

/**
 * @brief Start scanning for RentScan devices
 * 
 * @return int 0 on success, negative error code otherwise
 */
int ble_central_start_scan(void);

/**
 * @brief Stop scanning for RentScan devices
 * 
 * @return int 0 on success, negative error code otherwise
 */
int ble_central_stop_scan(void);

/**
 * @brief Send a message to a connected RentScan device
 * 
 * @param msg Pointer to RentScan message
 * @return int 0 on success, negative error code otherwise
 */
int ble_central_send_message(const rentscan_msg_t *msg);

/**
 * @brief Disconnect from the current device
 * 
 * @return int 0 on success, negative error code otherwise
 */
int ble_central_disconnect(void);

/**
 * @brief Reset the BLE stack (in case of irrecoverable errors)
 * 
 * @return int 0 on success, negative error code otherwise
 */
int ble_central_reset(void);

/**
 * @brief Check if connected to a RentScan device
 * 
 * @return true if connected
 * @return false if not connected
 */
bool ble_central_is_connected(void);

/**
 * @brief Get connection quality statistics
 * 
 * @param rssi Pointer to store RSSI value
 * @param tx_power Pointer to store TX power
 * @param conn_interval Pointer to store connection interval
 * @return int 0 on success, negative error code otherwise
 */
int ble_central_get_conn_stats(int8_t *rssi, int8_t *tx_power, uint16_t *conn_interval);

/**
 * @brief Add a device address to the whitelist
 * 
 * @param addr_str String representation of the BLE address
 * @return int 0 on success, negative error code otherwise
 */
int ble_central_add_to_whitelist(const char *addr_str);

/**
 * @brief Clear the device whitelist
 * 
 * @return int 0 on success, negative error code otherwise
 */
int ble_central_clear_whitelist(void);

#endif /* BLE_CENTRAL_H */ 