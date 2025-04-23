/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "nfc_handler.h"
#include "ble_handler.h"
#include "rental_logic.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

#define MAX_ITEM_ID_LEN 64

int main(void)
{
    int err;

    LOG_INF("RentScan application started");

    /* Initialize NFC reader */
    err = nfc_reader_init();
    if (err) {
        LOG_ERR("Failed to initialize NFC reader: %d", err);
        return -1;
    }

    /* Initialize BLE subsystem (placeholder for future) */
    err = ble_init();
    if (err) {
        LOG_ERR("Failed to initialize BLE: %d", err);
        /* Continue without BLE for now */
    }

    LOG_INF("RentScan initialized and ready for NFC tags");

    /* Main application loop */
    while (1) {
        /* Process any pending events and sleep */
        k_sleep(K_MSEC(1000));
    }

    return 0;
} 