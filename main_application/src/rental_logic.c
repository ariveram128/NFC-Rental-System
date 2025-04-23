/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "rental_logic.h"

LOG_MODULE_REGISTER(rental_logic, LOG_LEVEL_INF);

/**
 * @brief Process a scanned NFC tag
 *
 * This is a basic implementation that just logs the item ID.
 * In a real application, this would handle rental state transitions.
 */
void rental_logic_process_scan(const uint8_t *item_id, size_t item_id_len)
{
    if (item_id == NULL || item_id_len == 0) {
        LOG_ERR("Invalid item ID data");
        return;
    }

    LOG_INF("Rental logic processing item ID: %s (length: %d)", (const char *)item_id, item_id_len);
    
    /* In a full implementation, this would:
     * 1. Check if the item is available for rental
     * 2. Record the rental in the system
     * 3. Potentially notify a backend server
     * 4. Update any local state
     */
    
    LOG_INF("Rental logic successfully processed item");
}

// Rental logic implementation will go here 