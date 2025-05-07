/**
 * @file gateway_service.h
 * @brief Gateway service for forwarding data to backend
 */

#ifndef GATEWAY_SERVICE_H
#define GATEWAY_SERVICE_H

#include <zephyr/types.h>
#include "../../common/include/rentscan_protocol.h"

/* Rental information structure */
typedef struct {
    char item_id[MAX_TAG_ID_LEN + 1];  /* Item ID string */
    uint32_t start_time;               /* Start time in seconds (system uptime) */
    uint32_t duration;                 /* Duration in seconds */
    char user_id[16];                  /* User ID string */
    bool active;                       /* Whether rental is active */
} rental_info_t;

/**
 * @brief Initialize the gateway service
 * 
 * @return int 0 on success, negative error code otherwise
 */
int gateway_service_init(void);

/**
 * @brief Process a received message
 * 
 * This function will forward the message to the backend
 * 
 * @param msg Pointer to RentScan message
 * @return int 0 on success, negative error code otherwise
 */
int gateway_service_process_message(const rentscan_msg_t *msg);

/**
 * @brief Start a new rental
 * 
 * @param item_id Item ID string
 * @param user_id User ID string
 * @param duration Duration in seconds
 * @return int 0 on success, negative error code otherwise
 */
int gateway_service_start_rental(const char *item_id, const char *user_id, uint32_t duration);

/**
 * @brief End an active rental
 * 
 * @param item_id Item ID string
 * @return int 0 on success, negative error code otherwise
 */
int gateway_service_end_rental(const char *item_id);

/**
 * @brief Get rental status for an item
 * 
 * @param item_id Item ID string
 * @param status Pointer to store status
 * @return int 0 on success, negative error code otherwise
 */
int gateway_service_get_rental_status(const char *item_id, rentscan_status_t *status);

/**
 * @brief Get all active rentals
 * 
 * @param rentals Array to store rental info
 * @param max_count Maximum number of rentals to store
 * @param count Pointer to store actual number of rentals
 * @return int 0 on success, negative error code otherwise
 */
int gateway_service_get_active_rentals(rental_info_t *rentals, size_t max_count, size_t *count);

/**
 * @brief Request status for a tag from the backend
 * 
 * @param tag_id Pointer to tag ID
 * @param tag_id_len Length of tag ID
 * @return int 0 on success, negative error code otherwise
 */
int gateway_service_request_status(const uint8_t *tag_id, size_t tag_id_len);

/**
 * @brief Check connection to backend
 * 
 * @return true if connected
 * @return false if not connected
 */
bool gateway_service_is_connected(void);

/**
 * @brief Set gateway configuration
 * 
 * @param config_key Configuration key
 * @param config_value Configuration value
 * @return int 0 on success, negative error code otherwise
 */
int gateway_service_set_config(const char *config_key, const char *config_value);

/**
 * @brief Get gateway configuration
 * 
 * @param config_key Configuration key
 * @param config_value Buffer to store configuration value
 * @param config_value_len Length of buffer
 * @return int Length of configuration value or negative error code
 */
int gateway_service_get_config(const char *config_key, char *config_value, size_t config_value_len);

/**
 * @brief Get the backend error count
 * 
 * @return int The number of errors encountered
 */
int gateway_service_get_error_count(void);

/**
 * @brief Reset the backend error count
 */
void gateway_service_reset_error_count(void);

#endif /* GATEWAY_SERVICE_H */ 