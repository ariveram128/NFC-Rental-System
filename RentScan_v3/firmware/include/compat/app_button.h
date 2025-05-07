/**
 * @file app_button.h
 * @brief Compatibility layer for app_button.h from nRF SDK
 */

#ifndef APP_BUTTON_COMPAT_H__
#define APP_BUTTON_COMPAT_H__

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include "app_error.h"

/* Button configuration structure */
typedef struct {
    uint8_t pin_no;                  /**< Pin to be used as a button. */
    uint8_t active_state;            /**< Active state of the button (0 = low, 1 = high). */
    uint8_t pull_cfg;                /**< Pull configuration of the button. */
    void *  p_context;               /**< Context for button handler. */
} app_button_cfg_t;

/* Button event handler type */
typedef void (*app_button_handler_t)(uint8_t pin_no, uint8_t button_action);

/* Button action types */
#define APP_BUTTON_PUSH        0     /**< Button pushed. */
#define APP_BUTTON_RELEASE     1     /**< Button released. */
#define APP_BUTTON_LONG_PUSH   2     /**< Button long pushed. */

/* Pull configuration */
#define APP_BUTTON_PULL_NONE   0     /**< No pull. */
#define APP_BUTTON_PULL_UP     1     /**< Pull-up. */
#define APP_BUTTON_PULL_DOWN   2     /**< Pull-down. */

/**
 * @brief Button initialization function
 */
static inline ret_code_t app_button_init(app_button_cfg_t const *  p_buttons,
                                        uint8_t                    button_count,
                                        uint32_t                   detection_delay)
{
    /* This would be implemented using Zephyr's GPIO API in a real application */
    /* For now, just return success */
    return NRF_SUCCESS;
}

/**
 * @brief Enable button detection
 */
static inline ret_code_t app_button_enable(void)
{
    return NRF_SUCCESS;
}

/**
 * @brief Disable button detection
 */
static inline ret_code_t app_button_disable(void)
{
    return NRF_SUCCESS;
}

/**
 * @brief Check if a button is pressed
 */
static inline bool app_button_is_pushed(uint8_t button_id)
{
    /* This would be implemented using Zephyr's GPIO API */
    return false;
}

#endif // APP_BUTTON_COMPAT_H__