/**
 * @file nrf_fstorage_sd.h
 * @brief Compatibility layer for Nordic SDK flash storage module with SoftDevice
 *
 * This file provides compatibility with Nordic SDK flash storage APIs
 * when using Zephyr-based nRF Connect SDK
 */

#ifndef NRF_FSTORAGE_SD_H__
#define NRF_FSTORAGE_SD_H__

#include "nrf_fstorage.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief fstorage SoftDevice implementation
 */
extern const nrf_fstorage_api_t nrf_fstorage_sd;

#ifdef __cplusplus
}
#endif

#endif // NRF_FSTORAGE_SD_H__