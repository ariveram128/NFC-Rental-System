#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include "gateway_service.h"

LOG_MODULE_REGISTER(gateway_service, LOG_LEVEL_INF);

#define CONFIG_PREFIX "gateway/"
#define MAX_CONFIG_KEY_LEN 32
#define MAX_CONFIG_VALUE_LEN 64

static bool backend_connected = false;

// Simple config storage using settings subsystem
static int settings_set(const char *name, size_t len, settings_read_cb read_cb, void *cb_arg)
{
    const char *next;
    size_t name_len;
    char value[MAX_CONFIG_VALUE_LEN];
    
    name_len = settings_name_next(name, &next);
    
    if (!next) {
        if (len > sizeof(value) - 1) {
            return -EINVAL;
        }

        if (read_cb(cb_arg, value, len) < 0) {
            return -EINVAL;
        }

        value[len] = '\0';
        LOG_DBG("Loaded setting %s = %s", name, value);
    }

    return 0;
}

SETTINGS_STATIC_HANDLER_DEFINE(gateway, CONFIG_PREFIX, NULL, settings_set, NULL, NULL);

int gateway_service_init(void)
{
    int err;

    // Initialize settings subsystem
    err = settings_subsys_init();
    if (err) {
        LOG_ERR("Failed to initialize settings subsystem (err %d)", err);
        return err;
    }

    // Load stored settings
    err = settings_load();
    if (err) {
        LOG_ERR("Failed to load settings (err %d)", err);
        return err;
    }

    LOG_INF("Gateway service initialized");
    return 0;
}

int gateway_service_process_message(const rentscan_msg_t *msg)
{
    if (!msg) {
        return -EINVAL;
    }

    // For now, just log the message
    // TODO: Implement actual backend communication
    LOG_INF("Processing message command %d", msg->cmd);
    return 0;
}

int gateway_service_request_status(const uint8_t *tag_id, size_t tag_id_len)
{
    if (!tag_id || tag_id_len == 0) {
        return -EINVAL;
    }

    // For now, just log the request
    // TODO: Implement actual backend status request
    LOG_INF("Requesting status for tag");
    return 0;
}

bool gateway_service_is_connected(void)
{
    return backend_connected;
}

int gateway_service_set_config(const char *config_key, const char *config_value)
{
    if (!config_key || !config_value) {
        return -EINVAL;
    }

    char key[MAX_CONFIG_KEY_LEN];
    snprintf(key, sizeof(key), CONFIG_PREFIX "%s", config_key);

    int err = settings_save_one(key, config_value, strlen(config_value));
    if (err) {
        LOG_ERR("Failed to save config %s (err %d)", config_key, err);
        return err;
    }

    LOG_INF("Config set: %s = %s", config_key, config_value);
    return 0;
}

int gateway_service_get_config(const char *config_key, char *config_value, size_t config_value_len)
{
    if (!config_key || !config_value || config_value_len == 0) {
        return -EINVAL;
    }

    char key[MAX_CONFIG_KEY_LEN];
    snprintf(key, sizeof(key), CONFIG_PREFIX "%s", config_key);

    // Instead of using settings_runtime_get, we'll store the value during settings_load
    // and return it from our local storage
    memset(config_value, 0, config_value_len);
    
    // For now, return empty value and indicate no data found
    return -ENOENT;
}