/**
 * @file nrf_fstorage_impl.c
 * @brief Implementation of the Nordic SDK flash storage compatibility layer
 */

#include "nrf_fstorage.h"
#include "nrf_fstorage_sd.h"
#include <string.h>

/* Global fstorage instance */
nrf_fstorage_t fstorage;

/* Flash storage API implementation that uses Zephyr's flash API */
const nrf_fstorage_api_t nrf_fstorage_sd = {
    .init = NULL,
    .read = NULL,
    .write = NULL,
    .erase = NULL
};

/**
 * @brief Initialize a flash storage instance
 */
ret_code_t nrf_fstorage_init(nrf_fstorage_t * p_fs, const nrf_fstorage_api_t * p_api, void * p_param)
{
    if (p_fs == NULL || p_api == NULL) {
        return NRF_ERROR_INVALID_PARAM;
    }
    
    p_fs->p_api = p_api;
    
    // In a real implementation, this would initialize the flash driver
    return NRF_SUCCESS;
}

/**
 * @brief Read data from flash
 */
ret_code_t nrf_fstorage_read(nrf_fstorage_t * p_fs, uint32_t src, void * p_dest, uint32_t len)
{
    if (p_fs == NULL || p_dest == NULL || len == 0) {
        return NRF_ERROR_INVALID_PARAM;
    }
    
    // In a real implementation, this would use the Zephyr flash API
    // For now, simply copy memory assuming it's already in RAM (which it isn't in reality)
    memcpy(p_dest, (void*)src, len);
    
    return NRF_SUCCESS;
}

/**
 * @brief Write data to flash
 */
ret_code_t nrf_fstorage_write(nrf_fstorage_t * p_fs, uint32_t dest, void const * p_src, uint32_t len, void * p_param)
{
    if (p_fs == NULL || p_src == NULL || len == 0) {
        return NRF_ERROR_INVALID_PARAM;
    }
    
    // In a real implementation, this would use the Zephyr flash API
    // For now, just create an event to simulate success
    
    if (p_fs->evt_handler != NULL) {
        nrf_fstorage_evt_t evt = {
            .p_fstorage = p_fs,
            .addr = dest,
            .len = len,
            .result = NRF_SUCCESS,
            .p_param = p_param
        };
        
        p_fs->evt_handler(&evt);
    }
    
    return NRF_SUCCESS;
}

/**
 * @brief Erase flash pages
 */
ret_code_t nrf_fstorage_erase(nrf_fstorage_t * p_fs, uint32_t page_addr, uint32_t len, void * p_param)
{
    if (p_fs == NULL || len == 0) {
        return NRF_ERROR_INVALID_PARAM;
    }
    
    // In a real implementation, this would use the Zephyr flash API
    // For now, just create an event to simulate success
    
    if (p_fs->evt_handler != NULL) {
        nrf_fstorage_evt_t evt = {
            .p_fstorage = p_fs,
            .addr = page_addr,
            .len = len,
            .result = NRF_SUCCESS,
            .p_param = p_param
        };
        
        p_fs->evt_handler(&evt);
    }
    
    return NRF_SUCCESS;
}