/**
 * @file main.c
 * @brief Main application for RentScan gateway device (BLE central + backend connector)
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/shell/shell.h>
#include <string.h>
#include <stdio.h>

#include "../include/gateway_config.h"
#include "ble_central.h"
#include "gateway_service.h"
#include "../../common/include/rentscan_protocol.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/* Define work queue items for various tasks */
static struct k_work_delayable health_check_work;
static struct k_work message_process_work;

/* Buffer for message processing */
static rentscan_msg_t current_msg;
static bool message_ready = false;

/* Error tracking */
static int consecutive_errors = 0;

/**
 * @brief Handler for BLE messages
 */
static void ble_message_handler(const rentscan_msg_t *msg)
{
    /* Copy the message to our buffer */
    memcpy(&current_msg, msg, sizeof(rentscan_msg_t));
    
    /* Signal that we have a message ready */
    message_ready = true;
    
    /* Process the message in the work queue */
    k_work_submit(&message_process_work);
}

/**
 * @brief Message processing work handler
 */
static void message_process_work_handler(struct k_work *work)
{
    int err;
    
    if (!message_ready) {
        return;
    }
    
    /* Process the message */
    err = gateway_service_process_message(&current_msg);
    if (err) {
        LOG_ERR("Failed to process message: %d", err);
        consecutive_errors++;
        
        if (consecutive_errors >= GATEWAY_ERROR_RESET_THRESHOLD) {
            LOG_WRN("Too many consecutive errors, resetting BLE");
            consecutive_errors = 0;
            ble_central_reset();
        }
    } else {
        consecutive_errors = 0;
    }
    
    /* Mark the message as processed */
    message_ready = false;
}

/**
 * @brief Health check work handler
 */
static void health_check_work_handler(struct k_work *work)
{
    /* Check connection state */
    if (!ble_central_is_connected()) {
        LOG_INF("Not connected to any device, starting scan");
        ble_central_start_scan();
    } else {
        /* Log connection statistics */
        int8_t rssi = 0;
        int8_t tx_power = 0;
        uint16_t conn_interval = 0;
        
        if (ble_central_get_conn_stats(&rssi, &tx_power, &conn_interval) == 0) {
            LOG_INF("Connection stats: RSSI=%d dBm, TX=%d dBm, Interval=%.2f ms",
                   rssi, tx_power, conn_interval * 1.25);
        }
    }
    
    /* Reschedule the work */
    k_work_schedule_for_queue(&k_sys_work_q, &health_check_work,
                            K_MSEC(GATEWAY_HEALTH_CHECK_PERIOD_MS));
}

/* Shell command to add a device to the whitelist */
static int cmd_whitelist_add(const struct shell *shell, size_t argc, char *argv[])
{
    if (argc != 2) {
        shell_print(shell, "Usage: whitelist add <device_address>");
        return -EINVAL;
    }
    
    int err = ble_central_add_to_whitelist(argv[1]);
    if (err) {
        shell_error(shell, "Failed to add device: %d", err);
        return err;
    }
    
    shell_print(shell, "Device added to whitelist: %s", argv[1]);
    return 0;
}

/* Shell command to clear the whitelist */
static int cmd_whitelist_clear(const struct shell *shell, size_t argc, char *argv[])
{
    int err = ble_central_clear_whitelist();
    if (err) {
        shell_error(shell, "Failed to clear whitelist: %d", err);
        return err;
    }
    
    shell_print(shell, "Whitelist cleared");
    return 0;
}

/* Shell command to start scanning */
static int cmd_scan_start(const struct shell *shell, size_t argc, char *argv[])
{
    int err = ble_central_start_scan();
    if (err) {
        shell_error(shell, "Failed to start scan: %d", err);
        return err;
    }
    
    shell_print(shell, "Scan started");
    return 0;
}

/* Shell command to stop scanning */
static int cmd_scan_stop(const struct shell *shell, size_t argc, char *argv[])
{
    int err = ble_central_stop_scan();
    if (err) {
        shell_error(shell, "Failed to stop scan: %d", err);
        return err;
    }
    
    shell_print(shell, "Scan stopped");
    return 0;
}

/* Shell command to disconnect */
static int cmd_disconnect(const struct shell *shell, size_t argc, char *argv[])
{
    int err = ble_central_disconnect();
    if (err) {
        shell_error(shell, "Failed to disconnect: %d", err);
        return err;
    }
    
    shell_print(shell, "Disconnected");
    return 0;
}

/* Shell command to reset the BLE stack */
static int cmd_ble_reset(const struct shell *shell, size_t argc, char *argv[])
{
    int err = ble_central_reset();
    if (err) {
        shell_error(shell, "Failed to reset BLE: %d", err);
        return err;
    }
    
    shell_print(shell, "BLE reset complete");
    return 0;
}

/* Shell command to set configuration */
static int cmd_config_set(const struct shell *shell, size_t argc, char *argv[])
{
    if (argc != 3) {
        shell_print(shell, "Usage: config set <key> <value>");
        return -EINVAL;
    }
    
    int err = gateway_service_set_config(argv[1], argv[2]);
    if (err) {
        shell_error(shell, "Failed to set config: %d", err);
        return err;
    }
    
    shell_print(shell, "Config set: %s=%s", argv[1], argv[2]);
    return 0;
}

/* Shell command to get configuration */
static int cmd_config_get(const struct shell *shell, size_t argc, char *argv[])
{
    if (argc != 2) {
        shell_print(shell, "Usage: config get <key>");
        return -EINVAL;
    }
    
    char value[64] = {0};
    int err = gateway_service_get_config(argv[1], value, sizeof(value));
    if (err < 0) {
        shell_error(shell, "Failed to get config: %d", err);
        return err;
    }
    
    shell_print(shell, "Config: %s=%s", argv[1], value);
    return 0;
}

/* Shell command to show status */
static int cmd_status(const struct shell *shell, size_t argc, char *argv[])
{
    shell_print(shell, "BLE Central Status:");
    shell_print(shell, "  Connected: %s", ble_central_is_connected() ? "yes" : "no");
    
    if (ble_central_is_connected()) {
        int8_t rssi = 0;
        int8_t tx_power = 0;
        uint16_t conn_interval = 0;
        
        if (ble_central_get_conn_stats(&rssi, &tx_power, &conn_interval) == 0) {
            shell_print(shell, "  RSSI: %d dBm", rssi);
            shell_print(shell, "  TX Power: %d dBm", tx_power);
            shell_print(shell, "  Conn Interval: %.2f ms", conn_interval * 1.25);
        }
    }
    
    shell_print(shell, "Gateway Service Status:");
    shell_print(shell, "  Backend Connected: %s", gateway_service_is_connected() ? "yes" : "no");
    shell_print(shell, "  Error Count: %d", consecutive_errors);
    
    return 0;
}

/* Define shell commands */
SHELL_STATIC_SUBCMD_SET_CREATE(whitelist_cmds,
    SHELL_CMD(add, NULL, "Add device to whitelist", cmd_whitelist_add),
    SHELL_CMD(clear, NULL, "Clear whitelist", cmd_whitelist_clear),
    SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(scan_cmds,
    SHELL_CMD(start, NULL, "Start scanning", cmd_scan_start),
    SHELL_CMD(stop, NULL, "Stop scanning", cmd_scan_stop),
    SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(config_cmds,
    SHELL_CMD(set, NULL, "Set configuration", cmd_config_set),
    SHELL_CMD(get, NULL, "Get configuration", cmd_config_get),
    SHELL_SUBCMD_SET_END
);

SHELL_STATIC_SUBCMD_SET_CREATE(sub_cmds,
    SHELL_CMD(whitelist, &whitelist_cmds, "Manage whitelist", NULL),
    SHELL_CMD(scan, &scan_cmds, "Control scanning", NULL),
    SHELL_CMD(disconnect, NULL, "Disconnect from device", cmd_disconnect),
    SHELL_CMD(reset, NULL, "Reset BLE stack", cmd_ble_reset),
    SHELL_CMD(config, &config_cmds, "Manage configuration", NULL),
    SHELL_CMD(status, NULL, "Show status", cmd_status),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(rentscan, &sub_cmds, "RentScan Gateway Commands", NULL);

int main(void)
{
    int err;
    
    LOG_INF("RentScan gateway starting");
    
    /* Initialize work queue items */
    k_work_init(&message_process_work, message_process_work_handler);
    k_work_init_delayable(&health_check_work, health_check_work_handler);
    
    /* Initialize the gateway service */
    err = gateway_service_init();
    if (err) {
        LOG_ERR("Failed to initialize gateway service: %d", err);
        return -1;
    }
    
    /* Initialize the BLE central */
    err = ble_central_init(ble_message_handler);
    if (err) {
        LOG_ERR("Failed to initialize BLE central: %d", err);
        return -1;
    }

    /* Load settings - required for BLE address */
    if (IS_ENABLED(CONFIG_SETTINGS)) {
        settings_load();
        LOG_INF("Settings loaded");
    }
    
    /* Wait for BLE stack to fully initialize */
    k_sleep(K_MSEC(100));
    
    /* Start scanning for devices */
    err = ble_central_start_scan();
    if (err) {
        if (err == -EAGAIN) {
            LOG_WRN("BLE stack busy, retrying scan in 2s...");
            k_sleep(K_MSEC(2000));
            err = ble_central_start_scan();
            if (err) {
                LOG_ERR("Second scan attempt failed: %d", err);
            }
        } else {
            LOG_ERR("Failed to start scanning: %d", err);
        }
        /* Continue execution regardless of scan error */
    }
    
    /* Schedule health check */
    k_work_schedule_for_queue(&k_sys_work_q, &health_check_work,
                            K_MSEC(GATEWAY_HEALTH_CHECK_PERIOD_MS));
    
    LOG_INF("RentScan gateway initialized");
    
    return 0;
}