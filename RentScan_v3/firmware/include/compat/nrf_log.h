/**
 * @file nrf_log.h
 * @brief Compatibility layer for nrf_log.h from nRF SDK
 */

#ifndef NRF_LOG_COMPAT_H__
#define NRF_LOG_COMPAT_H__

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

/* Define the module name for logging */
#define LOG_MODULE_NAME rentscan
LOG_MODULE_REGISTER(LOG_MODULE_NAME);

/* Logging macros */
#define NRF_LOG_ERROR(...)       LOG_ERR(__VA_ARGS__)
#define NRF_LOG_WARNING(...)     LOG_WRN(__VA_ARGS__)
#define NRF_LOG_INFO(...)        LOG_INF(__VA_ARGS__)
#define NRF_LOG_DEBUG(...)       LOG_DBG(__VA_ARGS__)

/* Hexdump macros */
#define NRF_LOG_HEXDUMP_ERROR(p_data, len)   LOG_HEXDUMP_ERR(p_data, len, "")
#define NRF_LOG_HEXDUMP_WARNING(p_data, len) LOG_HEXDUMP_WRN(p_data, len, "")
#define NRF_LOG_HEXDUMP_INFO(p_data, len)    LOG_HEXDUMP_INF(p_data, len, "")
#define NRF_LOG_HEXDUMP_DEBUG(p_data, len)   LOG_HEXDUMP_DBG(p_data, len, "")

/* Log initialization function */
#define NRF_LOG_INIT(timestamp_func)         /* Nothing to do here */
#define NRF_LOG_FINAL_FLUSH()                /* Nothing to do here */
#define NRF_LOG_DEFAULT_BACKENDS_INIT()      /* Nothing to do here */
#define NRF_LOG_PROCESS()                    /* Nothing to do here */

/* Extra definitions that might be used */
#define NRF_LOG_FLUSH()                      /* Nothing to do here */
#define NRF_LOG_MODULE_REGISTER()            /* Nothing to do here */

#endif // NRF_LOG_COMPAT_H__