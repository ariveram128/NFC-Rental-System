#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include <stdio.h>
#include "rental_logic.h"
#include "ble_handler.h"  // assume you have ble_send(char )

LOG_MODULE_REGISTER(rental_logic, LOG_LEVEL_INF);

#define ITEM_ID_MAX_LEN 32
#define RENTAL_DURATION_MS 30000  // 30 seconds for demo

static int64_t rental_start_time = 0;
static bool rental_active = false;
static bool rental_expired_notified = false;
static char current_item_id[ITEM_ID_MAX_LEN] = {0};

void rental_logic_process_scan(const uint8_t *item_id, size_t item_id_len)
{
    if (item_id == NULL || item_id_len == 0 || item_id_len >= ITEM_ID_MAX_LEN) {
        LOG_ERR("Invalid item ID data");
        return;
    }

    strncpy(current_item_id, (const char *)item_id, item_id_len);
    current_item_id[item_id_len] = '\0';

    rental_start_time = k_uptime_get();
    rental_active = true;
    rental_expired_notified = false;

    LOG_INF("Rental started for item: %s", current_item_id);
}

void rental_logic_update_status(void)
{
    if (!rental_active && rental_expired_notified) {
        return;  // already notified, do nothing
    }

    int64_t now = k_uptime_get();
    int64_t elapsed = now - rental_start_time;

    if (elapsed >= RENTAL_DURATION_MS && rental_active) {
        rental_active = false;
        rental_expired_notified = true;

        char msg[64];
        snprintf(msg, sizeof(msg), "[%s] RENTAL EXPIRED", current_item_id);
        ble_send(msg);
        LOG_INF("%s", msg);

    } else if (rental_active) {
        char msg[64];
        snprintf(msg, sizeof(msg), "[%s] RENTAL ACTIVE", current_item_id);
        ble_send(msg);
        LOG_INF("%s", msg);
    }
}