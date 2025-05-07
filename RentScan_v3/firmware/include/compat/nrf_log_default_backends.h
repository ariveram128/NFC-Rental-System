/**
 * @file nrf_log_default_backends.h
 * @brief Compatibility layer for Nordic SDK logging backends
 *
 * This file provides compatibility with Nordic SDK logging backend APIs
 * when using Zephyr-based nRF Connect SDK
 */

#ifndef NRF_LOG_DEFAULT_BACKENDS_H__
#define NRF_LOG_DEFAULT_BACKENDS_H__

#include "nrf_log.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize default log backends
 */
void NRF_LOG_DEFAULT_BACKENDS_INIT(void)
{
    /* In Zephyr, backends are configured at build time */
}

#ifdef __cplusplus
}
#endif

#endif // NRF_LOG_DEFAULT_BACKENDS_H__ 