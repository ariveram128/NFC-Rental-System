/**
 * @file app_error.h
 * @brief Compatibility layer for Nordic SDK error handling
 *
 * This file provides compatibility with Nordic SDK error handling APIs
 * when using Zephyr-based nRF Connect SDK
 */

#ifndef APP_ERROR_H__
#define APP_ERROR_H__

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Return code type
 */
typedef uint32_t ret_code_t;

/**
 * @brief Success code
 */
#define NRF_SUCCESS                    0
#define NRF_ERROR_NULL                 1
#define NRF_ERROR_BUSY                 2
#define NRF_ERROR_INVALID_PARAM        3
#define NRF_ERROR_INVALID_STATE        4
#define NRF_ERROR_NOT_FOUND            5
#define NRF_ERROR_NOT_SUPPORTED        6
#define NRF_ERROR_TIMEOUT              7
#define NRF_ERROR_INVALID_LENGTH       8
#define NRF_ERROR_INTERNAL             9

/**
 * @brief Error handler function
 */
static inline void app_error_handler(ret_code_t error_code, uint32_t line_num, const uint8_t * p_file_name)
{
    printk("ERROR %d [%s:%d]\n", error_code, p_file_name, line_num);
    k_panic();
}

/**
 * @brief Error check macro
 */
#define APP_ERROR_CHECK(ERR_CODE)                       \
    do                                                  \
    {                                                   \
        const uint32_t LOCAL_ERR_CODE = (ERR_CODE);     \
        if (LOCAL_ERR_CODE != NRF_SUCCESS)              \
        {                                               \
            app_error_handler(LOCAL_ERR_CODE, __LINE__, (const uint8_t*) __FILE__); \
        }                                               \
    } while (0)

/**
 * @brief Error check with return macro
 */
#define VERIFY_SUCCESS(ERR_CODE)                        \
    do                                                  \
    {                                                   \
        const uint32_t LOCAL_ERR_CODE = (ERR_CODE);     \
        if (LOCAL_ERR_CODE != NRF_SUCCESS)              \
        {                                               \
            return LOCAL_ERR_CODE;                      \
        }                                               \
    } while (0)

#ifdef __cplusplus
}
#endif

#endif // APP_ERROR_H__