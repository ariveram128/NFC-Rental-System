/**
 * @file ble_conn_params.h
 * @brief Compatibility layer for Nordic SDK BLE connection parameters APIs
 *
 * This file provides compatibility with Nordic SDK BLE connection parameters APIs
 * when using Zephyr-based nRF Connect SDK
 */

#ifndef BLE_CONN_PARAMS_H__
#define BLE_CONN_PARAMS_H__

#include "ble.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Connection parameters module event type
 */
typedef enum {
    BLE_CONN_PARAMS_EVT_FAILED,   /**< Connection parameters update failed. */
    BLE_CONN_PARAMS_EVT_SUCCEEDED /**< Connection parameters update succeeded. */
} ble_conn_params_evt_type_t;

/**
 * @brief Connection parameters module event
 */
typedef struct {
    ble_conn_params_evt_type_t evt_type; /**< Event type. */
    uint16_t conn_handle;                /**< Connection handle. */
} ble_conn_params_evt_t;

/**
 * @brief Connection parameters module event handler type
 */
typedef void (*ble_conn_params_evt_handler_t) (ble_conn_params_evt_t * p_evt);

/**
 * @brief Connection parameters module error handler type
 */
typedef void (*ble_conn_params_error_handler_t) (uint32_t nrf_error);

/**
 * @brief Connection parameters module init structure
 */
typedef struct {
    ble_conn_params_evt_handler_t    evt_handler;                   /**< Event handler. */
    ble_conn_params_error_handler_t  error_handler;                 /**< Error handler. */
    bool                             first_conn_params_update_delay; /**< Time from initiating event to first connection parameters update. */
    bool                             next_conn_params_update_delay;  /**< Time between connection parameters updates. */
    uint8_t                          max_conn_params_update_count;   /**< Number of attempts before giving up the connection parameter negotiation. */
    uint16_t                         start_on_notify_cccd_handle;    /**< CCCD Handle of the characteristic for which a notification or indication enables connection parameter update. Set to BLE_GATT_HANDLE_INVALID if not used. */
    bool                             disconnect_on_fail;             /**< Set to true to disconnect the link if the connection parameters update fails. */
    uint16_t                         min_conn_interval;              /**< Minimum connection interval in 1.25 ms units. */
    uint16_t                         max_conn_interval;              /**< Maximum connection interval in 1.25 ms units. */
    uint16_t                         slave_latency;                  /**< Slave latency. */
    uint16_t                         conn_sup_timeout;               /**< Connection supervision timeout in 10 ms units. */
} ble_conn_params_init_t;

/**
 * @brief Function for initializing the Connection Parameters module.
 *
 * @param[in] p_init Information needed to initialize the module.
 *
 * @return NRF_SUCCESS on successful initialization, otherwise an error code.
 */
static inline ret_code_t ble_conn_params_init(const ble_conn_params_init_t * p_init)
{
    /* This is a stub implementation - would be implemented in a real system */
    return 0;
}

/**
 * @brief Function for stopping the Connection Parameters module.
 *
 * @return NRF_SUCCESS on successful initialization, otherwise an error code.
 */
static inline ret_code_t ble_conn_params_stop(void)
{
    /* This is a stub implementation - would be implemented in a real system */
    return 0;
}

/**
 * @brief Function for changing the connection parameters.
 *
 * @param[in] conn_handle     Connection handle.
 * @param[in] min_conn_int    Minimum connection interval in 1.25 ms units.
 * @param[in] max_conn_int    Maximum connection interval in 1.25 ms units.
 * @param[in] slave_latency   Slave latency.
 * @param[in] conn_sup_timeout Connection supervision timeout in 10 ms units.
 *
 * @return NRF_SUCCESS on success, otherwise an error code.
 */
static inline ret_code_t ble_conn_params_change_conn_params(uint16_t conn_handle,
                                                          uint16_t min_conn_int,
                                                          uint16_t max_conn_int,
                                                          uint16_t slave_latency,
                                                          uint16_t conn_sup_timeout)
{
    /* This is a stub implementation - would be implemented in a real system */
    return 0;
}

#ifdef __cplusplus
}
#endif

#endif // BLE_CONN_PARAMS_H__ 