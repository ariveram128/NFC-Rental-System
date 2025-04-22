#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "nfc_handler.h"
#include "ble_handler.h"
#include "rental_logic.h"

int main(void)
{
    int err;
    
    printk("RentScan System Starting...
");
    
    err = nfc_init();
    if (err) {
        printk("NFC initialization failed (err %d)
", err);
        return err;
    }
    
    err = ble_init();
    if (err) {
        printk("BLE initialization failed (err %d)
", err);
        return err;
    }
    
    printk("RentScan System Ready
");
    
    while (1) {
        // Main application loop
        // Handle NFC events and rental expiration checks
        k_sleep(K_MSEC(100));
    }
    
    return 0;
}
