/* rental_logic.c - RentScan Rental System Logic */

#include "rental_logic.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <string.h>

LOG_MODULE_REGISTER(rental_logic, LOG_LEVEL_INF);

/* Simplified rental state for initial implementation */
static bool item_is_rented = false;
static uint8_t last_item_id[32];
static size_t last_item_id_len;
static uint32_t rental_start_time;

void rental_logic_process_scan(const uint8_t *item_id, size_t item_id_len)
{
    char id_buf[33] = {0}; // Buffer for printing the ID as string
    
    if (item_id_len >= sizeof(id_buf)) {
        item_id_len = sizeof(id_buf) - 1;  // Truncate if too long
    }
    
    // Copy ID to temporary buffer for logging
    memcpy(id_buf, item_id, item_id_len);
    id_buf[item_id_len] = '\0';
    
    LOG_INF("Item scanned - ID: %s", id_buf);
    
    // Simple toggle rental state for now
    if (!item_is_rented) {
        // Start rental
        item_is_rented = true;
        memcpy(last_item_id, item_id, item_id_len);
        last_item_id_len = item_id_len;
        rental_start_time = k_uptime_get_32();
        
        LOG_INF("Rental STARTED for item %s", id_buf);
    } else {
        // Check if this is the same item
        if (last_item_id_len == item_id_len && 
            memcmp(last_item_id, item_id, item_id_len) == 0) {
            
            // End rental
            item_is_rented = false;
            uint32_t rental_duration = k_uptime_get_32() - rental_start_time;
            LOG_INF("Rental ENDED for item %s (duration: %u ms)", 
                   id_buf, rental_duration);
        } else {
            // Different item scanned while one is already rented
            LOG_WRN("Cannot rent item %s - item %.*s is already rented", 
                   id_buf, last_item_id_len, last_item_id);
        }
    }
    
    // In future phases, this would send data over BLE to the gateway
}