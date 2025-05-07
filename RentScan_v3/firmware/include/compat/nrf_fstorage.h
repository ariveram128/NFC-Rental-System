/**
 * @file nrf_fstorage.h
 * @brief Compatibility layer for Nordic SDK flash storage module
 *
 * This file provides compatibility with Nordic SDK flash storage APIs
 * when using Zephyr-based nRF Connect SDK
 */

#ifndef NRF_FSTORAGE_H__
#define NRF_FSTORAGE_H__

#include <stdint.h>
#include <stdbool.h>
#include <kernel.h>
#include <storage/flash_map.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Flash operation success code
 */
#define NRF_SUCCESS              0

/**
 * @brief Error code for busy state
 */
#define NRF_ERROR_BUSY           1

/**
 * @brief Error code for invalid parameters
 */
#define NRF_ERROR_INVALID_PARAM  2

/**
 * @brief Generic error code
 */
#define NRF_ERROR_INTERNAL       3

/**
 * @brief Return code type
 */
typedef uint32_t ret_code_t;

/**
 * @brief Forward declaration of fstorage API type
 */
typedef struct nrf_fstorage_api_s nrf_fstorage_api_t;

/**
 * @brief Forward declaration of fstorage event type
 */
typedef struct nrf_fstorage_evt_s nrf_fstorage_evt_t;

/**
 * @brief Fstorage event handler function type
 */
typedef void (*nrf_fstorage_evt_handler_t)(nrf_fstorage_evt_t * p_evt);

/**
 * @brief Fstorage instance
 */
typedef struct {
    nrf_fstorage_evt_handler_t evt_handler;  /**< Event handler. */
    uint32_t start_addr;                     /**< Start address of the flash area. */
    uint32_t end_addr;                       /**< End address of the flash area. */
    const nrf_fstorage_api_t * p_api;        /**< API implementation. */
    void * p_flash_info;                     /**< Flash-specific information. */
} nrf_fstorage_t;

/**
 * @brief Fstorage event
 */
struct nrf_fstorage_evt_s {
    nrf_fstorage_t * p_fstorage;          /**< Pointer to the fstorage instance. */
    uint32_t addr;                        /**< Address at which the operation was performed. */
    uint32_t len;                         /**< Length of the operation. */
    ret_code_t result;                    /**< Result of the operation. */
    void * p_param;                       /**< User-defined parameter passed to the event handler. */
};

/**
 * @brief Fstorage API structure
 */
struct nrf_fstorage_api_s {
    ret_code_t (*init)   (nrf_fstorage_t * p_fs, void * p_param);
    ret_code_t (*read)   (nrf_fstorage_t * p_fs, uint32_t src, void * p_dest, uint32_t len);
    ret_code_t (*write)  (nrf_fstorage_t * p_fs, uint32_t dest, void const * p_src, uint32_t len, void * p_param);
    ret_code_t (*erase)  (nrf_fstorage_t * p_fs, uint32_t page_addr, uint32_t len, void * p_param);
};

/**
 * @brief Fstorage instance definition macro
 */
#define NRF_FSTORAGE_DEF(_name) nrf_fstorage_t _name

/**
 * @brief Initialize a flash storage instance
 */
ret_code_t nrf_fstorage_init(nrf_fstorage_t * p_fs, const nrf_fstorage_api_t * p_api, void * p_param);

/**
 * @brief Read data from flash
 */
ret_code_t nrf_fstorage_read(nrf_fstorage_t * p_fs, uint32_t src, void * p_dest, uint32_t len);

/**
 * @brief Write data to flash
 */
ret_code_t nrf_fstorage_write(nrf_fstorage_t * p_fs, uint32_t dest, void const * p_src, uint32_t len, void * p_param);

/**
 * @brief Erase flash pages
 */
ret_code_t nrf_fstorage_erase(nrf_fstorage_t * p_fs, uint32_t page_addr, uint32_t len, void * p_param);

#ifdef __cplusplus
}
#endif

#endif // NRF_FSTORAGE_H__