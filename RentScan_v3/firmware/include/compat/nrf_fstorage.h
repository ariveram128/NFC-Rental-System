/**
 * @file nrf_fstorage.h
 * @brief Compatibility layer for nrf_fstorage.h from nRF SDK
 */

#ifndef NRF_FSTORAGE_COMPAT_H__
#define NRF_FSTORAGE_COMPAT_H__

#include <zephyr/kernel.h>
/* Fix includes for Zephyr flash */
#include <zephyr/storage/flash_map.h>
#include <zephyr/fs/fs.h>
#include <zephyr/device.h>
#include <zephyr/drivers/flash.h>
#include "app_error.h"

/**
 * @brief Flash storage event types
 */
typedef enum {
    NRF_FSTORAGE_EVT_READ_COMPLETE,
    NRF_FSTORAGE_EVT_WRITE_COMPLETE,
    NRF_FSTORAGE_EVT_ERASE_COMPLETE,
} nrf_fstorage_evt_id_t;

/**
 * @brief Flash storage event
 */
typedef struct {
    nrf_fstorage_evt_id_t id;
    uint32_t addr;
    uint32_t len;
    void * p_param;
    ret_code_t result;  /* Added result field needed by storage_manager.c */
} nrf_fstorage_evt_t;

/**
 * @brief Flash storage callback
 */
typedef void (*nrf_fstorage_evt_handler_t)(nrf_fstorage_evt_t * p_evt);

/**
 * @brief Flash storage instance
 */
typedef struct {
    uint32_t start_addr;
    uint32_t end_addr;
    void * p_api;
    nrf_fstorage_evt_handler_t evt_handler;
} nrf_fstorage_t;

/* Define the NRF_FSTORAGE_DEF macro needed by storage_manager.c */
#define NRF_FSTORAGE_DEF(name) nrf_fstorage_t name

/* SoftDevice flash storage API definition - 
   moved from nrf_fstorage_sd.h to simplify */
struct nrf_fstorage_sd_api {
    const char * p_name;
};
extern const struct nrf_fstorage_sd_api nrf_fstorage_sd;

/* Declare a global fstorage instance that will be accessed 
   in storage_manager.c */
extern nrf_fstorage_t fstorage;

/**
 * @brief Initialize flash storage
 */
static inline ret_code_t nrf_fstorage_init(nrf_fstorage_t * p_fs, void * p_api, void * p_param)
{
    if (p_fs == NULL) {
        return NRF_ERROR_NULL;
    }
    
    /* In Zephyr, we would initialize flash storage differently
     * This is a placeholder - actual implementation would use Zephyr's flash API
     */
    return NRF_SUCCESS;
}

/**
 * @brief Read data from flash
 */
static inline ret_code_t nrf_fstorage_read(nrf_fstorage_t * p_fs, uint32_t addr, void * p_dest, uint32_t len)
{
    if (p_fs == NULL || p_dest == NULL || len == 0) {
        return NRF_ERROR_NULL;
    }
    
    /* In Zephyr, we would use the flash_area_read API
     * This is a placeholder - actual implementation would use Zephyr's flash API
     */
    return NRF_SUCCESS;
}

/**
 * @brief Write data to flash
 */
static inline ret_code_t nrf_fstorage_write(nrf_fstorage_t * p_fs, uint32_t addr, void const * p_src, uint32_t len, void * p_param)
{
    if (p_fs == NULL || p_src == NULL || len == 0) {
        return NRF_ERROR_NULL;
    }
    
    /* In Zephyr, we would use the flash_area_write API
     * This is a placeholder - actual implementation would use Zephyr's flash API
     */
    return NRF_SUCCESS;
}

/**
 * @brief Erase flash page
 */
static inline ret_code_t nrf_fstorage_erase(nrf_fstorage_t * p_fs, uint32_t addr, uint32_t len, void * p_param)
{
    if (p_fs == NULL || len == 0) {
        return NRF_ERROR_NULL;
    }
    
    /* In Zephyr, we would use the flash_area_erase API
     * This is a placeholder - actual implementation would use Zephyr's flash API
     */
    return NRF_SUCCESS;
}

#endif // NRF_FSTORAGE_COMPAT_H__