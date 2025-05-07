/**
 * @file nrf_fstorage_impl.c
 * @brief Implementation for nrf_fstorage.h compatibility layer
 */

#include "nrf_fstorage.h"

/* Define the global fstorage instance needed by storage_manager.c */
nrf_fstorage_t fstorage;

/* Define the SoftDevice flash storage API instance */
const struct nrf_fstorage_sd_api nrf_fstorage_sd = {
    .p_name = "ZEPHYR_FLASH"
};