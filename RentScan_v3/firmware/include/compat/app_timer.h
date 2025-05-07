/**
 * @file app_timer.h
 * @brief Compatibility layer for app_timer.h from nRF SDK
 */

#ifndef APP_TIMER_COMPAT_H__
#define APP_TIMER_COMPAT_H__

#include <zephyr/kernel.h>
#include "app_error.h"

/* Timer mode definitions */
#define APP_TIMER_MODE_SINGLE_SHOT 0
#define APP_TIMER_MODE_REPEATED    1

/* Timer ID type */
typedef struct k_timer* app_timer_id_t;

/* Missing type definition - moved up to fix the error */
typedef uint32_t app_timer_mode_t;

/* Timer timeout handler type */
typedef void (*app_timer_timeout_handler_t)(void * p_context);

/* Timer creation macro */
#define APP_TIMER_DEF(timer_id) \
    static struct k_timer CONCAT_2(timer_id, _data); \
    static app_timer_id_t const timer_id = &CONCAT_2(timer_id, _data)

/* Forward declarations for internal timer handling */
static void app_timer_callback_adapter(struct k_timer *timer);

/* Timer creation function */
static inline ret_code_t app_timer_create(app_timer_id_t *p_timer_id,
                                         app_timer_mode_t mode,
                                         app_timer_timeout_handler_t timeout_handler)
{
    if (p_timer_id == NULL || timeout_handler == NULL) {
        return NRF_ERROR_NULL;
    }

    /* Store user context in timer user data */
    k_timer_init(*p_timer_id, app_timer_callback_adapter, NULL);
    
    /* Store handler and mode in timer user data */
    (*p_timer_id)->user_data = (void*)timeout_handler;
    
    return NRF_SUCCESS;
}

/* Timer start function */
static inline ret_code_t app_timer_start(app_timer_id_t timer_id,
                                        uint32_t timeout_ticks,
                                        void *p_context)
{
    if (timer_id == NULL) {
        return NRF_ERROR_NULL;
    }

    /* Store context in timer expiry function */
    timer_id->user_data = p_context;
    
    /* Convert ticks to milliseconds (approximate) */
    k_timeout_t duration = K_MSEC(timeout_ticks);
    
    /* Start timer with or without period based on mode */
    k_timer_start(timer_id, duration, K_NO_WAIT);
    
    return NRF_SUCCESS;
}

/* Timer stop function */
static inline ret_code_t app_timer_stop(app_timer_id_t timer_id)
{
    if (timer_id == NULL) {
        return NRF_ERROR_NULL;
    }

    k_timer_stop(timer_id);
    return NRF_SUCCESS;
}

/* Timer initialization function */
static inline ret_code_t app_timer_init(void)
{
    /* Nothing to do here in Zephyr */
    return NRF_SUCCESS;
}

/* Timer callback adapter */
static void app_timer_callback_adapter(struct k_timer *timer)
{
    app_timer_timeout_handler_t handler = (app_timer_timeout_handler_t)timer->user_data;
    if (handler != NULL) {
        handler(timer->user_data);
    }
}

/* Tick conversion */
#define APP_TIMER_TICKS(MS) (MS)

#endif // APP_TIMER_COMPAT_H__