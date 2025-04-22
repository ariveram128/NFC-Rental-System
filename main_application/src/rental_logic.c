#include "rental_logic.h"
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <string.h> 

LOG_MODULE_REGISTER(rental_logic, LOG_LEVEL_INF);

// Function called by nfc_handler when an item ID is scanned
void rental_logic_process_scan(const uint8_t *item_id, size_t item_id_len)
{

    // For now, just log it. Use %.*s to print a non-null-terminated buffer.
    LOG_INF("Rental Logic: Item Scanned - ID: %.*s", item_id_len, item_id);

    // future implementation
    // - Check if item is already rented
    // - Record start/end time
    // - Update state variable that ble_handler can read
    // - Trigger BLE notification via ble_handler function (to be added)
}
