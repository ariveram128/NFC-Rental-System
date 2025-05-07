/**
 * @file rental_manager.h
 * @brief Rental management functionality
 */

#ifndef RENTAL_MANAGER_H
#define RENTAL_MANAGER_H

#include <zephyr/types.h>
#include <stdbool.h>
#include "../../common/include/rentscan_protocol.h"

/**
 * @brief Callback for rental status changes
 * 
 * @param msg Pointer to RentScan message containing status information
 */
typedef void (*rental_status_cb_t)(const rentscan_msg_t *msg);

/**
 * @brief Initialize the rental manager
 * 
 * @param status_changed_cb Callback function for rental status changes
 * @return int 0 on success, negative error code otherwise
 */
int rental_manager_init(rental_status_cb_t status_changed_cb);

/**
 * @brief Process a tag scan event
 * 
 * @param tag_id Pointer to tag ID
 * @param tag_id_len Length of tag ID
 * @param tag_data Pointer to tag data (can be NULL)
 * @param tag_data_len Length of tag data
 * @return int 0 on success, negative error code otherwise
 */
int rental_manager_process_tag(const uint8_t *tag_id, size_t tag_id_len,
                              const uint8_t *tag_data, size_t tag_data_len);

/**
 * @brief Process a command received from the gateway
 * 
 * @param data Pointer to command data
 * @param len Length of command data
 * @return int 0 on success, negative error code otherwise
 */
int rental_manager_process_command(const uint8_t *data, uint16_t len);

/**
 * @brief Check for rental expirations
 * 
 * This function should be called periodically to check for rental expirations
 * 
 * @return int Number of expired rentals found
 */
int rental_manager_check_expirations(void);

/**
 * @brief Get current rental status for a tag
 * 
 * @param tag_id Pointer to tag ID
 * @param tag_id_len Length of tag ID
 * @param status Pointer to status variable to be filled
 * @return int 0 on success, negative error code otherwise
 */
int rental_manager_get_status(const uint8_t *tag_id, size_t tag_id_len,
                             rentscan_status_t *status);

#endif /* RENTAL_MANAGER_H */ 