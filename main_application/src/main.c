/* main.c - RentScan NFC Tag Reader Application */

/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 * Copyright (c) 2025 RentScan Project Team
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/settings/settings.h>

#include "ble_handler.h"
#include "nfc_handler.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/* Simple status check function to send periodic BLE messages */
void status_loop(void)
{
    int counter = 0;
    while (1) {
        char msg[32];
        snprintf(msg, sizeof(msg), "STATUS UPDATE: %d", counter++);
        ble_send(msg);
        k_sleep(K_SECONDS(10));  // Check every 10 seconds
    }
}

int main(void)
{
    int err;

    LOG_INF("=== RentScan Application Booting ===");

    /* Enable Bluetooth */
    err = bt_enable(NULL);
    if (err) {
        LOG_ERR("Bluetooth initialization failed (err %d)", err);
        return 0;
    }
    LOG_INF("Bluetooth initialized");

    /* Load saved BLE settings if enabled */
    if (IS_ENABLED(CONFIG_SETTINGS)) {
        settings_load();
    }

    /* Initialize BLE advertising + NUS service */
    err = ble_handler_init();
    if (err) {
        LOG_ERR("BLE handler initialization failed (err %d)", err);
        return 0;
    }
    
    /* Initialize NFC reader */
    err = nfc_reader_init();
    if (err) {
        LOG_ERR("NFC reader initialization failed (err %d)", err);
        return 0;
    }

    LOG_INF("RentScan initialized and ready for NFC tags");

    /* Start periodic status updates via BLE */
    status_loop();

    return 0;
}