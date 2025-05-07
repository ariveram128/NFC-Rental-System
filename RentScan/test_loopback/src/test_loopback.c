/**
 * @file test_loopback.c
 * @brief RentScan loopback test application
 */

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>

#include "../../common/include/rentscan_protocol.h"

LOG_MODULE_REGISTER(test_loopback, LOG_LEVEL_INF);

/* Function declarations for mock implementations */
int mock_ble_service_init(void (*cb)(const uint8_t *, uint16_t));
int mock_ble_service_start_advertising(bool fast);
int mock_ble_service_send_data(const uint8_t *data, uint16_t len);
bool mock_ble_service_is_connected(void);

int mock_ble_central_init(void (*cb)(const uint8_t *, uint16_t));
int mock_ble_central_start_scan(void);
int mock_ble_central_send_data(const uint8_t *data, uint16_t len);
bool mock_ble_central_is_connected(void);

/* Max retry attempts */
#define MAX_RETRY_ATTEMPTS 5
#define RETRY_DELAY_MS 1000
#define BLE_ROLE_SWITCH_DELAY_MS 2000

/* Test statistics */
static struct {
    uint32_t peripheral_msgs_sent;
    uint32_t peripheral_msgs_received;
    uint32_t central_msgs_sent;
    uint32_t central_msgs_received;
    uint32_t test_sequence;
} test_stats;

/* Test message work */
static struct k_work_delayable test_msg_work;
static struct k_work_delayable start_scan_work;
static struct k_work_delayable retry_work;

/* Retry state */
static struct {
    int remaining_adv_attempts;
    int remaining_scan_attempts;
    bool adv_started;
    bool scan_started;
} retry_state;

/* Message handlers */
static void peripheral_data_received(const uint8_t *data, uint16_t len)
{
    LOG_INF("Peripheral received data (%u bytes)", len);
    test_stats.peripheral_msgs_received++;
    
    /* Log the message type if it's a RentScan protocol message */
    if (len >= 1) {
        LOG_INF("  Message type: %u", data[0]);
    }
}

static void central_data_received(const uint8_t *data, uint16_t len)
{
    LOG_INF("Central received data (%u bytes)", len);
    test_stats.central_msgs_received++;
    
    /* Log the message type if it's a RentScan protocol message */
    if (len >= 1) {
        LOG_INF("  Message type: %u", data[0]);
    }
}

/* Send a test message */
static void send_test_message(void)
{
    uint8_t test_data[8];
    
    test_stats.test_sequence++;
    
    /* Alternate between peripheral and central sending messages */
    if (test_stats.test_sequence % 2 == 0) {
        /* Peripheral sending to central */
        if (mock_ble_service_is_connected()) {
            /* Create a test status message */
            test_data[0] = CMD_STATUS_RESP;  /* Status response message */
            test_data[1] = STATUS_AVAILABLE; /* Status available */
            test_data[2] = test_stats.test_sequence; /* Test sequence number */
            
            if (mock_ble_service_send_data(test_data, 3) == 0) {
                test_stats.peripheral_msgs_sent++;
                LOG_INF("Peripheral sent test message %u", test_stats.test_sequence);
            }
        }
    } else {
        /* Central sending to peripheral */
        if (mock_ble_central_is_connected()) {
            /* Create a test status request message */
            test_data[0] = CMD_STATUS_REQ;  /* Status request message */
            test_data[1] = test_stats.test_sequence; /* Test sequence number */
            
            if (mock_ble_central_send_data(test_data, 2) == 0) {
                test_stats.central_msgs_sent++;
                LOG_INF("Central sent test message %u", test_stats.test_sequence);
            }
        }
    }
    
    /* Schedule next message */
    k_work_schedule(&test_msg_work, K_SECONDS(10));
}

/* Test message work handler */
static void test_msg_work_handler(struct k_work *work)
{
    send_test_message();
}

/* Forward declaration for retry work handler */
static void retry_work_handler(struct k_work *work);

/* Start advertising function */
static int start_advertising(void)
{
    int err = mock_ble_service_start_advertising(true);
    if (err) {
        if (err == -EAGAIN && retry_state.remaining_adv_attempts > 0) {
            LOG_WRN("Advertising failed with EAGAIN, will retry (%d attempts left)", 
                   retry_state.remaining_adv_attempts);
            retry_state.remaining_adv_attempts--;
            k_work_schedule(&retry_work, K_MSEC(RETRY_DELAY_MS));
            return 0;
        } else {
            LOG_ERR("Advertising failed to start (err %d), no retries left", err);
            return err;
        }
    } else {
        LOG_INF("Advertising started successfully");
        retry_state.adv_started = true;
        return 0;
    }
}

/* Start scanning function */
static void start_scan_work_handler(struct k_work *work)
{
    int err = mock_ble_central_start_scan();
    if (err) {
        if (err == -EAGAIN && retry_state.remaining_scan_attempts > 0) {
            LOG_WRN("Scanning failed with EAGAIN, will retry (%d attempts left)", 
                   retry_state.remaining_scan_attempts);
            retry_state.remaining_scan_attempts--;
            /* Schedule retry after a longer delay */
            k_work_schedule(&retry_work, K_MSEC(RETRY_DELAY_MS * 2));
        } else {
            LOG_ERR("Scanning failed to start (err %d), no retries left", err);
        }
    } else {
        LOG_INF("Scanning started successfully");
        retry_state.scan_started = true;
    }
}

/* Retry work handler */
static void retry_work_handler(struct k_work *work)
{
    /* First try advertising if not started */
    if (!retry_state.adv_started && retry_state.remaining_adv_attempts > 0) {
        start_advertising();
    } 
    /* Then try scanning if advertising started and scanning not started */
    else if (retry_state.adv_started && !retry_state.scan_started && 
             retry_state.remaining_scan_attempts > 0) {
        start_scan_work_handler(NULL);
    }
}

/* Reset BLE stack */
static int reset_ble_stack(void)
{
    int err;
    
    LOG_INF("Disabling Bluetooth...");
    err = bt_disable();
    if (err) {
        LOG_ERR("Failed to disable Bluetooth (err %d)", err);
        return err;
    }
    
    LOG_INF("Bluetooth disabled, waiting before re-enabling...");
    k_sleep(K_MSEC(3000));
    
    /* Re-enable Bluetooth */
    LOG_INF("Re-enabling Bluetooth...");
    err = bt_enable(NULL);
    if (err) {
        LOG_ERR("Failed to re-enable Bluetooth (err %d)", err);
        return err;
    }
    
    LOG_INF("Bluetooth re-enabled, waiting to stabilize...");
    k_sleep(K_MSEC(2000));
    
    return 0;
}

/* Initialize peripherals */
static int init_peripherals(void)
{
    int err;
    
    /* Initialize peripheral role */
    err = mock_ble_service_init(peripheral_data_received);
    if (err) {
        LOG_ERR("Peripheral init failed (err %d)", err);
        return err;
    }
    
    /* Initialize central role */
    err = mock_ble_central_init(central_data_received);
    if (err) {
        LOG_ERR("Central init failed (err %d)", err);
        return err;
    }
    
    return 0;
}

/* Shell commands */
static int cmd_test_status(const struct shell *sh, size_t argc, char *argv[])
{
    shell_print(sh, "RentScan Loopback Test Status:");
    shell_print(sh, "--------------------------");
    shell_print(sh, "Peripheral connected: %s", 
              mock_ble_service_is_connected() ? "yes" : "no");
    shell_print(sh, "Central connected:    %s", 
              mock_ble_central_is_connected() ? "yes" : "no");
    shell_print(sh, "Messages:");
    shell_print(sh, "  Peripheral sent:    %u", test_stats.peripheral_msgs_sent);
    shell_print(sh, "  Peripheral received: %u", test_stats.peripheral_msgs_received);
    shell_print(sh, "  Central sent:       %u", test_stats.central_msgs_sent);
    shell_print(sh, "  Central received:   %u", test_stats.central_msgs_received);
    shell_print(sh, "  Test sequence:      %u", test_stats.test_sequence);
    return 0;
}

static int cmd_test_send(const struct shell *sh, size_t argc, char *argv[])
{
    send_test_message();
    shell_print(sh, "Test message sent, sequence %u", test_stats.test_sequence);
    return 0;
}

/* Start advertising command */
static int cmd_start_adv(const struct shell *sh, size_t argc, char *argv[])
{
    retry_state.remaining_adv_attempts = MAX_RETRY_ATTEMPTS;
    retry_state.adv_started = false;
    
    int err = start_advertising();
    if (err) {
        shell_error(sh, "Advertising failed to start after multiple retries (err %d)", err);
        return -1;
    }
    
    shell_print(sh, "Advertising start initiated with retries");
    return 0;
}

/* Start scanning command */
static int cmd_start_scan(const struct shell *sh, size_t argc, char *argv[])
{
    retry_state.remaining_scan_attempts = MAX_RETRY_ATTEMPTS;
    retry_state.scan_started = false;
    
    k_work_schedule(&start_scan_work, K_NO_WAIT);
    
    shell_print(sh, "Scanning start initiated with retries");
    return 0;
}

/* Reset BLE stack command */
static int cmd_reset_ble(const struct shell *sh, size_t argc, char *argv[])
{
    int err;
    
    shell_print(sh, "Resetting Bluetooth stack...");
    
    err = reset_ble_stack();
    if (err) {
        shell_error(sh, "Failed to reset Bluetooth stack (err %d)", err);
        return -1;
    }
    
    err = init_peripherals();
    if (err) {
        shell_error(sh, "Failed to initialize peripherals (err %d)", err);
        return -1;
    }
    
    shell_print(sh, "Bluetooth stack reset complete, starting advertising and scanning...");
    
    /* Initialize retry state */
    retry_state.remaining_adv_attempts = MAX_RETRY_ATTEMPTS;
    retry_state.remaining_scan_attempts = MAX_RETRY_ATTEMPTS;
    retry_state.adv_started = false;
    retry_state.scan_started = false;
    
    /* Start advertising first */
    start_advertising();
    
    /* Schedule scanning after a delay */
    k_work_schedule(&start_scan_work, K_MSEC(BLE_ROLE_SWITCH_DELAY_MS));
    
    return 0;
}

/* Create subcmd set for scan subcommand */
SHELL_STATIC_SUBCMD_SET_CREATE(scan_cmds,
    SHELL_CMD(start, NULL, "Start scanning for devices", cmd_start_scan),
    SHELL_SUBCMD_SET_END
);

/* Create subcmd set for adv subcommand */
SHELL_STATIC_SUBCMD_SET_CREATE(adv_cmds,
    SHELL_CMD(start, NULL, "Start advertising", cmd_start_adv),
    SHELL_SUBCMD_SET_END
);

/* Create subcmd set for rentscan_test command */
SHELL_STATIC_SUBCMD_SET_CREATE(rentscan_test_cmds,
    SHELL_CMD(status, NULL, "Show test status", cmd_test_status),
    SHELL_CMD(send, NULL, "Send test message", cmd_test_send),
    SHELL_CMD(scan, &scan_cmds, "Scanning commands", NULL),
    SHELL_CMD(adv, &adv_cmds, "Advertising commands", NULL),
    SHELL_CMD(reset, NULL, "Reset Bluetooth stack and restart", cmd_reset_ble),
    SHELL_SUBCMD_SET_END
);

/* Register rentscan_test command */
SHELL_CMD_REGISTER(rentscan_test, &rentscan_test_cmds, "RentScan test commands", NULL);

/* Main application */
#if !defined(CONFIG_ZTEST)
int main(void)
{
    int err;
    
    LOG_INF("RentScan loopback test starting");
    
    /* Initialize BLE stack */
    LOG_INF("Enabling Bluetooth...");
    err = bt_enable(NULL);
    if (err) {
        LOG_ERR("Bluetooth init failed (err %d)", err);
        return 0;
    }
    
    LOG_INF("Bluetooth initialized");
    
    /* Add a longer delay to ensure the controller is fully ready */
    k_sleep(K_MSEC(3000));
    
    /* Initialize test statistics */
    memset(&test_stats, 0, sizeof(test_stats));
    
    /* Initialize work items */
    k_work_init_delayable(&test_msg_work, test_msg_work_handler);
    k_work_init_delayable(&start_scan_work, start_scan_work_handler);
    k_work_init_delayable(&retry_work, retry_work_handler);
    
    /* Initialize retry state */
    retry_state.remaining_adv_attempts = MAX_RETRY_ATTEMPTS;
    retry_state.remaining_scan_attempts = MAX_RETRY_ATTEMPTS;
    retry_state.adv_started = false;
    retry_state.scan_started = false;
    
    /* Initialize peripheral role first */
    err = mock_ble_service_init(peripheral_data_received);
    if (err) {
        LOG_ERR("Peripheral init failed (err %d)", err);
        return 0;
    }
    
    LOG_INF("Peripheral role initialized, starting advertising...");
    
    /* Start advertising with retry */
    err = start_advertising();
    if (err) {
        LOG_WRN("Initial advertising failed, will not retry");
    }
    
    /* Add a delay between peripheral and central initialization */
    LOG_INF("Waiting before initializing central role...");
    k_sleep(K_MSEC(BLE_ROLE_SWITCH_DELAY_MS));
    
    /* Initialize central role second */
    LOG_INF("Initializing central role...");
    err = mock_ble_central_init(central_data_received);
    if (err) {
        LOG_ERR("Central init failed (err %d)", err);
        return 0;
    }
    
    /* Start scanning with retry (scheduled) */
    LOG_INF("Central role initialized, scheduling scanning start...");
    k_work_schedule(&start_scan_work, K_MSEC(500));
    
    /* Schedule first test message */
    k_work_schedule(&test_msg_work, K_SECONDS(10));
    
    LOG_INF("Test initialized, waiting for devices to connect");
    
    return 0;
}
#endif