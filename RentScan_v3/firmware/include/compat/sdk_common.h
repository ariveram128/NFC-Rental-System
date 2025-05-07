/**
 * @file sdk_common.h
 * @brief Compatibility layer for sdk_common.h from nRF SDK
 */

#ifndef SDK_COMMON_COMPAT_H__
#define SDK_COMMON_COMPAT_H__

#include <zephyr/kernel.h>
#include "app_error.h"
#include "nordic_common.h"

/* SDK error check macros */
#define VERIFY_PARAM_NOT_NULL(param) \
    do { \
        if (param == NULL) { \
            return NRF_ERROR_NULL; \
        } \
    } while (0)

#define VERIFY_SUCCESS(err_code) \
    do { \
        ret_code_t _err_code = (err_code); \
        if (_err_code != NRF_SUCCESS) { \
            return _err_code; \
        } \
    } while (0)

#define VERIFY_TRUE(expr, err_code) \
    do { \
        if (!(expr)) { \
            return err_code; \
        } \
    } while (0)

/* SDK module init macros */
#define SDK_SUCCESS_INIT(module) \
    { \
        .p_reg = (module), \
        .initialized = true \
    }

#define SDK_ERROR_INIT(module, err_code) \
    { \
        .p_reg = (module), \
        .err_code = (err_code), \
        .initialized = false \
    }

#endif // SDK_COMMON_COMPAT_H__