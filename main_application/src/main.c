#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include "nfc_handler.h" // Include your module header
// Include ble_handler.h and rental_logic.h if main needs them directly

LOG_MODULE_REGISTER(main_app, LOG_LEVEL_INF);

int main(void)
{
    int err;
    LOG_INF("Starting RentScan NFC Reader Application");

    // --- Initialize NFC Reader ---
    err = nfc_reader_init();
    if (err) {
        LOG_ERR("NFC Reader initialization failed (err %d). Halting.", err);
        return err; // Or enter error state
    }

    // --- Initialize BLE Handler (Add in Phase 2/3) ---
    // err = ble_peripheral_init();
    // if (err) { ... }

    // --- Start NFC Polling ---
    // The nfc/reader sample might call nfc_reader_start() here,
    // or it might be handled internally after setup. Check sample.
    // If manual start is needed:
    /*
    err = nfc_reader_start(); // Check specific API name in sample/docs
    if (err) {
        LOG_ERR("Cannot start NFC reader (err %d)", err);
        // Handle error
    } else {
        LOG_INF("NFC Reader started polling...");
    }
    */

    // --- Main loop (can be empty if event-driven) ---
    while (1) {
        k_sleep(K_SECONDS(5)); // Sleep periodically, handlers do the work
        // LOG_DBG("Main loop tick"); // Optional debug print
    }

    return 0; // Should not be reached
}