/**
 * @file nrf_log_ctrl.h
 * @brief Compatibility layer for Nordic SDK logging control functions
 *
 * This file provides compatibility with Nordic SDK logging control APIs
 * when using Zephyr-based nRF Connect SDK
 */

#ifndef NRF_LOG_CTRL_H__
#define NRF_LOG_CTRL_H__

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include "nrf_log.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the logger module
 *
 * @param p_default_config Default configuration (unused in Zephyr)
 * @return NRF_SUCCESS on successful initialization
 */
static inline ret_code_t NRF_LOG_INIT(void *p_default_config)
{
    /* No-op in Zephyr as initialization is done during the build */
    return NRF_SUCCESS;
}

/**
 * @brief Process any pending log messages
 *
 * @return True if there were pending log messages, false otherwise
 */
static inline bool NRF_LOG_PROCESS(void)
{
    /* In Zephyr, log processing is handled differently */
    return false;
}

/**
 * @brief Flush log data
 */
static inline void NRF_LOG_FLUSH(void)
{
    log_panic();
}

#ifdef __cplusplus
}
#endif

#endif // NRF_LOG_CTRL_H__ 