#include <zephyr/ztest.h>
#include <zephyr/kernel.h>
#include <rental_logic.h>

/* Stub BLE send function */
static int ble_send_count;
void ble_send(const char *msg)
{
    ARG_UNUSED(msg);
    ble_send_count++;
}

ZTEST_SUITE(rental_logic_tests, NULL, NULL, NULL, NULL, NULL);

ZTEST(rental_logic_tests, test_invalid_scan)
{
    ble_send_count = 0;
    /* Invalid inputs should not trigger BLE send */
    rental_logic_process_scan(NULL, 0);
    zassert_equal(ble_send_count, 0, "ble_send should not be called on invalid scan");
}

ZTEST(rental_logic_tests, test_active_update)
{
    ble_send_count = 0;
    uint8_t id[] = "ITEM123";

    rental_logic_process_scan(id, sizeof(id) - 1);
    /* First update: rental active, should send one message */
    rental_logic_update_status();
    zassert_equal(ble_send_count, 1, "Expected one notification for active rental");
}
