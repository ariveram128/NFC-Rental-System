/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef BLE_HANDLER_H
#define BLE_HANDLER_H

/**
 * @brief Initialize the BLE subsystem
 *
 * @return 0 on success, negative error code otherwise
 */
int ble_init(void);

/**
 * @brief Send rental data over BLE
 *
 * @param tag_id The identifier of the NFC tag
 * @param timestamp The timestamp when the rental was initiated
 * @return 0 on success, negative error code otherwise
 */
int ble_send_rental_data(const char *tag_id, uint32_t timestamp);

#endif /* BLE_HANDLER_H */
