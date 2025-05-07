#include <shell/shell.h>
#include <sys/byteorder.h>
#include <bluetooth/gatt.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/uuid.h>
#include "gateway_service.h"
#include "ble_central.h"
#include <logging/log.h>

LOG_MODULE_REGISTER(shell_commands, LOG_LEVEL_INF);

// Add external declaration for direct access to the subscription parameters
extern struct bt_gatt_subscribe_params subscribe_params;
extern struct bt_conn *current_conn;
extern uint16_t nus_tx_handle;
extern uint16_t nus_rx_handle;

static uint8_t notify_handler_manual(struct bt_conn *conn,
                          struct bt_gatt_subscribe_params *params,
                          const void *data, uint16_t length)
{
    LOG_INF("Manual subscribe notification received!");
    return BT_GATT_ITER_CONTINUE;
}

static int cmd_manual_subscribe(const struct shell *shell, size_t argc, char **argv)
{
    int err;

    if (argc < 3) {
        shell_error(shell, "Usage: rentscan manual_sub <tx_handle> <ccc_handle>");
        return -EINVAL;
    }

    if (!current_conn) {
        shell_error(shell, "Not connected to any device");
        return -ENOTCONN;
    }

    uint16_t tx_handle = (uint16_t)strtol(argv[1], NULL, 10);
    uint16_t ccc_handle = (uint16_t)strtol(argv[2], NULL, 10);

    shell_print(shell, "Attempting manual subscription with TX handle %u and CCC handle %u", 
                tx_handle, ccc_handle);

    // Store the TX handle for future use
    nus_tx_handle = tx_handle;

    // Set up a temporary subscribe parameters structure
    static struct bt_gatt_subscribe_params temp_params = {
        .notify = notify_handler_manual,
        .value = BT_GATT_CCC_NOTIFY,
    };
    
    temp_params.value_handle = tx_handle;
    temp_params.ccc_handle = ccc_handle;

    err = bt_gatt_subscribe(current_conn, &temp_params);
    if (err && err != -EALREADY) {
        shell_error(shell, "Subscribe failed (err %d)", err);
        return err;
    }

    shell_print(shell, "Manual subscription successful");
    return 0;
}

static int cmd_show_handles(const struct shell *shell, size_t argc, char **argv)
{
    if (!current_conn) {
        shell_error(shell, "Not connected to any device");
        return -ENOTCONN;
    }

    shell_print(shell, "Current RX handle: %u", nus_rx_handle);
    shell_print(shell, "Current TX handle: %u", nus_tx_handle);
    shell_print(shell, "Current CCC handle (if known): %u", subscribe_params.ccc_handle);

    return 0;
}

/* BLE central commands */
static int cmd_scan_start(const struct shell *shell, size_t argc, char **argv)
{
    int err = ble_central_start_scan();
    if (err) {
        shell_error(shell, "Failed to start scanning (err %d)", err);
        return err;
    }
    
    shell_print(shell, "Scanning started");
    return 0;
}

static int cmd_scan_stop(const struct shell *shell, size_t argc, char **argv)
{
    int err = ble_central_stop_scan();
    if (err) {
        shell_error(shell, "Failed to stop scanning (err %d)", err);
        return err;
    }
    
    shell_print(shell, "Scanning stopped");
    return 0;
}

static int cmd_disconnect(const struct shell *shell, size_t argc, char **argv)
{
    int err = ble_central_disconnect();
    if (err) {
        shell_error(shell, "Failed to disconnect (err %d)", err);
        return err;
    }
    
    shell_print(shell, "Disconnected");
    return 0;
}

static int cmd_reset(const struct shell *shell, size_t argc, char **argv)
{
    int err = ble_central_reset();
    if (err) {
        shell_error(shell, "Failed to reset BLE (err %d)", err);
        return err;
    }
    
    shell_print(shell, "BLE reset");
    return 0;
}

static int cmd_bt_status(const struct shell *shell, size_t argc, char **argv)
{
    int8_t rssi;
    int8_t tx_power;
    uint16_t conn_interval;
    int err;
    
    bool connected = ble_central_is_connected();
    
    shell_print(shell, "BLE Central Status:");
    shell_print(shell, "  Connected: %s", connected ? "yes" : "no");
    
    if (connected) {
        err = ble_central_get_conn_stats(&rssi, &tx_power, &conn_interval);
        if (err) {
            shell_error(shell, "Failed to get connection stats (err %d)", err);
        } else {
            shell_print(shell, "  RSSI: %d dBm", rssi);
            shell_print(shell, "  TX Power: %d dBm", tx_power);
            shell_print(shell, "  Conn Interval: %u.%u ms", 
                      conn_interval * 5 / 4,
                      (conn_interval * 5) % 4 * 25);
        }
    }
    
    return 0;
}

/* Backend connection commands */
static int cmd_backend_connect(const struct shell *shell, size_t argc, char **argv)
{
    gateway_service_connect_backend();
    shell_print(shell, "Backend connection requested");
    return 0;
}

static int cmd_backend_disconnect(const struct shell *shell, size_t argc, char **argv)
{
    gateway_service_disconnect_backend();
    shell_print(shell, "Backend disconnection requested");
    return 0;
}

/* Gateway service commands */
static int cmd_gw_status(const struct shell *shell, size_t argc, char **argv)
{
    gateway_service_status_t status;
    gateway_service_get_status(&status);
    
    shell_print(shell, "Gateway Service Status:");
    shell_print(shell, "  Backend Connected: %s", status.backend_connected ? "yes" : "no");
    shell_print(shell, "  Error Count: %u", status.error_count);
    shell_print(shell, "  Message Queue: %u", status.queue_size);
    shell_print(shell, "  Active Rentals: %u", status.rental_count);
    
    return 0;
}

static int cmd_reset_errors(const struct shell *shell, size_t argc, char **argv)
{
    gateway_service_reset_errors();
    shell_print(shell, "Error count reset");
    return 0;
}

/* Rental management commands */
static int cmd_rental_start(const struct shell *shell, size_t argc, char **argv)
{
    if (argc < 4) {
        shell_error(shell, "Usage: rentscan rental start <item_id> <user_id> <duration>");
        return -EINVAL;
    }
    
    char *item_id = argv[1];
    char *user_id = argv[2];
    uint32_t duration = (uint32_t)strtoul(argv[3], NULL, 10);
    
    int err = gateway_service_start_rental(item_id, user_id, duration);
    if (err) {
        shell_error(shell, "Failed to start rental (err %d)", err);
        return err;
    }
    
    shell_print(shell, "Rental started for item %s by user %s for %u seconds", 
               item_id, user_id, duration);
    return 0;
}

static int cmd_rental_end(const struct shell *shell, size_t argc, char **argv)
{
    if (argc < 2) {
        shell_error(shell, "Usage: rentscan rental end <item_id>");
        return -EINVAL;
    }
    
    char *item_id = argv[1];
    int err = gateway_service_end_rental(item_id);
    if (err) {
        shell_error(shell, "Failed to end rental (err %d)", err);
        return err;
    }
    
    shell_print(shell, "Rental ended for item %s", item_id);
    return 0;
}

static int cmd_rental_list(const struct shell *shell, size_t argc, char **argv)
{
    gateway_service_status_t status;
    gateway_service_get_status(&status);
    
    shell_print(shell, "Active Rentals (%u):", status.rental_count);
    
    for (int i = 0; i < status.rental_count; i++) {
        rental_info_t rental;
        gateway_service_get_rental(i, &rental);
        
        uint32_t now = k_uptime_get_32() / 1000;
        uint32_t elapsed = now - rental.start_time;
        uint32_t remaining = rental.duration > elapsed ? rental.duration - elapsed : 0;
        
        shell_print(shell, "  Item: %s", rental.item_id);
        shell_print(shell, "    User: %s", rental.user_id);
        shell_print(shell, "    Elapsed: %u seconds", elapsed);
        shell_print(shell, "    Remaining: %u seconds", remaining);
        shell_print(shell, "    Status: %s", rental.active ? "Active" : "Expired");
    }
    
    return 0;
}

static int cmd_whitelist_add(const struct shell *shell, size_t argc, char **argv)
{
    if (argc < 2) {
        shell_error(shell, "Usage: rentscan whitelist add <addr>");
        return -EINVAL;
    }
    
    char *addr = argv[1];
    int err = ble_central_add_to_whitelist(addr);
    if (err) {
        shell_error(shell, "Failed to add to whitelist (err %d)", err);
        return err;
    }
    
    shell_print(shell, "Address %s added to whitelist", addr);
    return 0;
}

static int cmd_whitelist_clear(const struct shell *shell, size_t argc, char **argv)
{
    int err = ble_central_clear_whitelist();
    if (err) {
        shell_error(shell, "Failed to clear whitelist (err %d)", err);
        return err;
    }
    
    shell_print(shell, "Whitelist cleared");
    return 0;
}

static int cmd_status(const struct shell *shell, size_t argc, char **argv)
{
    cmd_bt_status(shell, argc, argv);
    shell_print(shell, "");
    cmd_gw_status(shell, argc, argv);
    
    return 0;
}

/* Rental subcommands */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_rental,
    SHELL_CMD(start, NULL, "Start a rental", cmd_rental_start),
    SHELL_CMD(end, NULL, "End a rental", cmd_rental_end),
    SHELL_CMD(list, NULL, "List active rentals", cmd_rental_list),
    SHELL_SUBCMD_SET_END
);

/* Scan subcommands */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_scan,
    SHELL_CMD(start, NULL, "Start scanning", cmd_scan_start),
    SHELL_CMD(stop, NULL, "Stop scanning", cmd_scan_stop),
    SHELL_SUBCMD_SET_END
);

/* Backend subcommands */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_backend,
    SHELL_CMD(connect, NULL, "Connect to backend", cmd_backend_connect),
    SHELL_CMD(disconnect, NULL, "Disconnect from backend", cmd_backend_disconnect),
    SHELL_SUBCMD_SET_END
);

/* Whitelist subcommands */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_whitelist,
    SHELL_CMD(add, NULL, "Add device to whitelist", cmd_whitelist_add),
    SHELL_CMD(clear, NULL, "Clear whitelist", cmd_whitelist_clear),
    SHELL_SUBCMD_SET_END
);

/* Main command set */
SHELL_STATIC_SUBCMD_SET_CREATE(sub_rentscan,
    SHELL_CMD(whitelist, &sub_whitelist, "Manage whitelist", NULL),
    SHELL_CMD(scan, &sub_scan, "Control scanning", NULL),
    SHELL_CMD(disconnect, NULL, "Disconnect from device", cmd_disconnect),
    SHELL_CMD(reset, NULL, "Reset BLE stack", cmd_reset),
    SHELL_CMD(status, NULL, "Show status", cmd_status),
    SHELL_CMD(backend, &sub_backend, "Control backend connection", NULL),
    SHELL_CMD(reset_errors, NULL, "Reset error count", cmd_reset_errors),
    SHELL_CMD(rental, &sub_rental, "Manage rentals", NULL),
    SHELL_CMD(manual_sub, NULL, "Manual subscribe with handles", cmd_manual_subscribe),
    SHELL_CMD(show_handles, NULL, "Show current GATT handles", cmd_show_handles),
    SHELL_SUBCMD_SET_END
);

SHELL_CMD_REGISTER(rentscan, &sub_rentscan, "RentScan Gateway Commands", NULL); 