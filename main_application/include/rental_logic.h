#ifndef RENTAL_LOGIC_H
#define RENTAL_LOGIC_H

// typedef struct {
//     char tag_id[32];
//     uint32_t start_time;
//     uint32_t duration;
//     bool active;
// } rental_data_t;

// int process_rental(const char *tag_id);
// int check_rental_expiration(void);


#include <stddef.h> // For size_t

// Function called when an NFC tag scan provides an item ID
void rental_logic_process_scan(const uint8_t *item_id, size_t item_id_len);

// Maybe other functions later (e.g., rental_start, rental_end)

#endif /* RENTAL_LOGIC_H */




