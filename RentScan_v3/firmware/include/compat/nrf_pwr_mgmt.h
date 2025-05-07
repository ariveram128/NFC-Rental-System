/**
 * @file nrf_pwr_mgmt.h
 * @brief Compatibility layer for nrf_pwr_mgmt.h from nRF SDK
 */

#ifndef NRF_PWR_MGMT_COMPAT_H__
#define NRF_PWR_MGMT_COMPAT_H__

#include <zephyr/kernel.h>
#include <zephyr/pm/pm.h>
#include "app_error.h"

/* Power management modes */
typedef enum {
    NRF_PWR_MGMT_EVT_PREPARE_WAKEUP = 0,
    NRF_PWR_MGMT_EVT_PREPARE_SYSOFF,
    NRF_PWR_MGMT_EVT_PREPARE_DFU,
    NRF_PWR_MGMT_EVT_PREPARE_RESET,
} nrf_pwr_mgmt_evt_t;

/* Power management shutdown type */
typedef enum {
    NRF_PWR_MGMT_SHUTDOWN_GOTO_SYSOFF,
    NRF_PWR_MGMT_SHUTDOWN_GOTO_DFU,
    NRF_PWR_MGMT_SHUTDOWN_RESET,
    NRF_PWR_MGMT_SHUTDOWN_CONTINUE,
} nrf_pwr_mgmt_shutdown_t;

/* Handler type for power management events */
typedef nrf_pwr_mgmt_shutdown_t (*nrf_pwr_mgmt_shutdown_handler_t)(nrf_pwr_mgmt_evt_t event);

/**
 * @brief Initialize power management module
 * 
 * @param handler_on_shutdown Handler function for shutdown events (can be NULL)
 * @return ret_code_t Success or error code
 */
static inline ret_code_t nrf_pwr_mgmt_init(nrf_pwr_mgmt_shutdown_handler_t handler_on_shutdown)
{
    /* This would be implemented using Zephyr's PM API */
    return NRF_SUCCESS;
}

/**
 * @brief Run power management processing
 * 
 * @return true If CPU should go to sleep
 * @return false If CPU should not go to sleep
 */
static inline bool nrf_pwr_mgmt_run(void)
{
    /* In Zephyr, power management runs automatically */
    k_yield();
    return true;
}

#endif // NRF_PWR_MGMT_COMPAT_H__