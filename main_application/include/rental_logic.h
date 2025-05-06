/* rental_logic.h - RentScan Rental System Logic */

#ifndef RENTAL_LOGICH
#define RENTAL_LOGICH

#include <stddef.h>
#include <stdint.h>

/*
 
@brief Process a scanned NFC tag containing an item ID
This function is called when an NFC tag with item information is read.
It handles the rental business logic, such as starting/ending rentals
or checking item status.
@param[in] item_id Pointer to item identifier data
@param[in] item_id_len Length of the item identifier data 
*/
void rental_logic_process_scan(const uint8_t *item_id, size_t item_id_len);

/*
@brief Periodically checks rental status and sends BLE notifications*
This function should be called in a loop or thread to monitor if
the rental is still active or has expired, and send status updates
over BLE accordingly.*/
void rental_logic_update_status(void);

#endif /* RENTAL_LOGICH */