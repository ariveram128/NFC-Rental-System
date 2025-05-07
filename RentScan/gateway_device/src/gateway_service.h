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

/* Gateway service status structure */
typedef struct {
    bool backend_connected;            /* Whether backend is connected */
    uint32_t error_count;              /* Count of errors */
    uint32_t queue_size;               /* Size of message queue */
    uint32_t rental_count;             /* Count of active rentals */
} gateway_service_status_t;

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
 * @brief Get gateway service status
 * 
 * @param status Pointer to store status
 * @return int 0 on success, negative error code otherwise
 */
int gateway_service_get_status(gateway_service_status_t *status);

/**
 * @brief Reset error count
 * 
 * @return int 0 on success, negative error code otherwise
 */
int gateway_service_reset_errors(void);

/**
 * @brief Get rental information by index
 * 
 * @param index Index of rental
 * @param rental Pointer to store rental information
 * @return int 0 on success, negative error code otherwise
 */
int gateway_service_get_rental(uint32_t index, rental_info_t *rental);

/**
 * @brief Connect to backend
 * 
 * @return int 0 on success, negative error code otherwise
 */
int gateway_service_connect_backend(void);

/**
 * @brief Disconnect from backend
 * 
 * @return int 0 on success, negative error code otherwise
 */
int gateway_service_disconnect_backend(void);

#endif /* GATEWAY_SERVICE_H */ 