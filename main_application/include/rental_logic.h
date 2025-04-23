/* rental_logic.h - RentScan Rental System Logic */

#ifndef RENTAL_LOGIC_H_
#define RENTAL_LOGIC_H_

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Process a scanned NFC tag containing an item ID
 *
 * This function is called when an NFC tag with item information is read.
 * It handles the rental business logic, such as starting/ending rentals
 * or checking item status.
 *
 * @param[in] item_id Pointer to item identifier data
 * @param[in] item_id_len Length of the item identifier data
 */
void rental_logic_process_scan(const uint8_t *item_id, size_t item_id_len);

#endif /* RENTAL_LOGIC_H_ */