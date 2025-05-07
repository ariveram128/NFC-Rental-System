/**
 * @file nrf_log.h
 * @brief Compatibility layer for Nordic SDK logging functionality
 *
 * This file provides compatibility with Nordic SDK logging APIs
 * when using Zephyr-based nRF Connect SDK
 */

#ifndef NRF_LOG_H__
#define NRF_LOG_H__

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#ifdef __cplusplus
extern "C" {
#endif

LOG_MODULE_REGISTER(rentscan, CONFIG_LOG_DEFAULT_LEVEL);

/**
 * @brief Log a message with info level
 */
#define NRF_LOG_INFO(...) LOG_INF(__VA_ARGS__)

/**
 * @brief Log a message with error level
 */
#define NRF_LOG_ERROR(...) LOG_ERR(__VA_ARGS__)

/**
 * @brief Log a message with warning level
 */
#define NRF_LOG_WARNING(...) LOG_WRN(__VA_ARGS__)

/**
 * @brief Log a message with debug level
 */
#define NRF_LOG_DEBUG(...) LOG_DBG(__VA_ARGS__)

/**
 * @brief Log a hexdump at info level
 */
#define NRF_LOG_HEXDUMP_INFO(p_data, len) LOG_HEXDUMP_INF(p_data, len, "Hexdump")

/**
 * @brief Log a hexdump at debug level
 */
#define NRF_LOG_HEXDUMP_DEBUG(p_data, len) LOG_HEXDUMP_DBG(p_data, len, "Hexdump")

/**
 * @brief Log a hexdump at warning level
 */
#define NRF_LOG_HEXDUMP_WARNING(p_data, len) LOG_HEXDUMP_WRN(p_data, len, "Hexdump")

/**
 * @brief Log a hexdump at error level
 */
#define NRF_LOG_HEXDUMP_ERROR(p_data, len) LOG_HEXDUMP_ERR(p_data, len, "Hexdump")

// Forward declarations to avoid redefinition
ret_code_t NRF_LOG_INIT(void *p_default_config);
bool NRF_LOG_PROCESS(void);
void NRF_LOG_FLUSH(void);
void NRF_LOG_DEFAULT_BACKENDS_INIT(void);

#ifdef __cplusplus
}
#endif

#endif // NRF_LOG_H__