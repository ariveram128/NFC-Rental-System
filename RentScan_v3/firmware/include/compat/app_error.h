/**
 * @file app_error.h
 * @brief Compatibility layer for app_error.h from nRF SDK
 */

#ifndef APP_ERROR_COMPAT_H__
#define APP_ERROR_COMPAT_H__

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>

/**
 * @brief Error type
 */
typedef uint32_t ret_code_t;

/**
 * @brief Success code
 */
#define NRF_SUCCESS 0

/**
 * @brief Error codes
 */
#define NRF_ERROR_BASE_NUM  0x0
#define NRF_ERROR_SDM_BASE_NUM  0x1000
#define NRF_ERROR_SOC_BASE_NUM  0x2000
#define NRF_ERROR_STK_BASE_NUM  0x3000

#define NRF_ERROR_INVALID_PARAM   (NRF_ERROR_BASE_NUM + 1)
#define NRF_ERROR_INVALID_STATE   (NRF_ERROR_BASE_NUM + 2)
#define NRF_ERROR_NOT_FOUND       (NRF_ERROR_BASE_NUM + 3)
#define NRF_ERROR_NO_MEM          (NRF_ERROR_BASE_NUM + 4)
#define NRF_ERROR_INTERNAL        (NRF_ERROR_BASE_NUM + 5)
#define NRF_ERROR_BUSY            (NRF_ERROR_BASE_NUM + 6)
#define NRF_ERROR_TIMEOUT         (NRF_ERROR_BASE_NUM + 7)
#define NRF_ERROR_NULL            (NRF_ERROR_BASE_NUM + 8)

/**
 * @brief Check for error and handle it
 */
#define APP_ERROR_CHECK(err_code) \
    do { \
        if ((err_code) != NRF_SUCCESS) { \
            printk("Error %d at %s:%d\n", (err_code), __FILE__, __LINE__); \
            k_panic(); \
        } \
    } while (0)

/**
 * @brief Handle error with handler function
 */
#define APP_ERROR_HANDLER(err_code) APP_ERROR_CHECK(err_code)

#endif // APP_ERROR_COMPAT_H__