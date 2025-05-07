/**
 * @file nrf_fstorage_sd.h
 * @brief Compatibility layer for nrf_fstorage_sd.h from nRF SDK
 */

#ifndef NRF_FSTORAGE_SD_COMPAT_H__
#define NRF_FSTORAGE_SD_COMPAT_H__

#include "nrf_fstorage.h"

/* Initialize SoftDevice flash storage */
static inline ret_code_t nrf_fstorage_sd_init(void)
{
    /* In Zephyr, we would use the Flash API directly */
    return NRF_SUCCESS;
}

#endif // NRF_FSTORAGE_SD_COMPAT_H__