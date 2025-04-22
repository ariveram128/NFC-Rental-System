#ifndef BLE_HANDLER_H
#define BLE_HANDLER_H

int ble_init(void);
int ble_send_rental_data(const char *tag_id, uint32_t timestamp);

#endif /* BLE_HANDLER_H */
