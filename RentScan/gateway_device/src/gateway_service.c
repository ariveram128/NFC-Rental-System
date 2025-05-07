#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>
#include <zephyr/logging/log.h>
#include <zephyr/uart.h>
#include <string.h>
#include <stdio.h>
#include "gateway_service.h"

LOG_MODULE_REGISTER(gateway_service, LOG_LEVEL_INF);

#define CONFIG_PREFIX "gateway/"
#define MAX_CONFIG_KEY_LEN 32
#define MAX_CONFIG_VALUE_LEN 64

/* UART device for backend communication */
#define UART_DEVICE_NODE DT_CHOSEN(zephyr_shell_uart)
static const struct device *uart_dev;

/* RX buffer for UART */
static uint8_t rx_buf[256];
static uint8_t rx_pos;

/* Backend connection status and error counter */
static bool backend_connected = false;
static uint32_t error_count = 0;

/* Settings subsystem */
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

/* UART interrupt callback */
static void uart_cb(const struct device *dev, void *user_data)
{
    uint8_t c;
    
    if (!uart_irq_update(dev)) {
        return;
    }
    
    if (uart_irq_rx_ready(dev)) {
        /* Read all available data */
        while (uart_fifo_read(dev, &c, 1) == 1) {
            /* Simple line buffering */
            if (rx_pos < sizeof(rx_buf) - 1) {
                rx_buf[rx_pos++] = c;
                
                /* If we received a newline, we have a complete message */
                if (c == '\n') {
                    rx_buf[rx_pos] = '\0';  /* Null terminate */
                    
                    /* Process the received message */
                    LOG_DBG("Received from backend: %s", rx_buf);
                    
                    /* TODO: Parse JSON response */
                    if (strstr((char *)rx_buf, "\"cmd\":4")) {
                        /* This is a status response */
                        LOG_INF("Received status response from backend");
                        backend_connected = true;
                    }
                    
                    /* Reset buffer for next message */
                    rx_pos = 0;
                }
            } else {
                /* Buffer overflow, reset */
                LOG_WRN("RX buffer overflow");
                rx_pos = 0;
                error_count++;
            }
        }
    }
    
    if (uart_irq_tx_ready(dev)) {
        /* TX is ready but we're not using it here */
        uart_irq_tx_disable(dev);
    }
}

SETTINGS_STATIC_HANDLER_DEFINE(gateway, CONFIG_PREFIX, NULL, settings_set, NULL, NULL);

int gateway_service_init(void)
{
    int err;

    /* Initialize settings subsystem */
    err = settings_subsys_init();
    if (err) {
        LOG_ERR("Failed to initialize settings subsystem (err %d)", err);
        return err;
    }

    /* Load stored settings */
    err = settings_load();
    if (err) {
        LOG_ERR("Failed to load settings (err %d)", err);
        return err;
    }
    
    /* Initialize UART for backend communication */
    uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE);
    if (!device_is_ready(uart_dev)) {
        LOG_ERR("UART device not ready");
        return -ENODEV;
    }
    
    /* Configure UART */
    err = uart_configure(uart_dev, 
                        115200,    /* Baud rate */
                        UART_CFG_PARITY_NONE,   /* No parity */
                        UART_CFG_STOP_BITS_1,   /* 1 stop bit */
                        UART_CFG_DATA_BITS_8);  /* 8 data bits */
    if (err) {
        LOG_ERR("Failed to configure UART (err %d)", err);
        return err;
    }
    
    /* Set up UART interrupt */
    uart_irq_callback_set(uart_dev, uart_cb);
    uart_irq_rx_enable(uart_dev);
    
    /* Reset RX buffer */
    rx_pos = 0;
    
    /* Send a hello message to check if backend is alive */
    char hello_msg[] = "{\"cmd\":3,\"tag_id\":\"0000\",\"tag_id_len\":2}\n";
    for (int i = 0; i < strlen(hello_msg); i++) {
        uart_poll_out(uart_dev, hello_msg[i]);
    }

    LOG_INF("Gateway service initialized");
    return 0;
}

int gateway_service_process_message(const rentscan_msg_t *msg)
{
    if (!msg) {
        return -EINVAL;
    }

    /* Create JSON message to send to backend */
    char json_buf[256];
    char tag_id_hex[MAX_TAG_ID_LEN * 2 + 1];
    
    /* Convert tag ID to hex string */
    for (int i = 0; i < msg->tag_id_len; i++) {
        sprintf(&tag_id_hex[i * 2], "%02x", msg->tag_id[i]);
    }
    tag_id_hex[msg->tag_id_len * 2] = '\0';
    
    /* Format JSON message */
    snprintf(json_buf, sizeof(json_buf),
             "{\"cmd\":%d,\"status\":%d,\"tag_id\":\"%s\",\"tag_id_len\":%d,"
             "\"timestamp\":%lu,\"duration\":%lu}\n",
             msg->cmd, msg->status, tag_id_hex, msg->tag_id_len,
             (unsigned long)msg->timestamp, (unsigned long)msg->duration);
    
    LOG_DBG("Sending to backend: %s", json_buf);
    
    /* Send message over UART */
    for (int i = 0; i < strlen(json_buf); i++) {
        uart_poll_out(uart_dev, json_buf[i]);
    }
    
    return 0;
}

int gateway_service_request_status(const uint8_t *tag_id, size_t tag_id_len)
{
    if (!tag_id || tag_id_len == 0) {
        return -EINVAL;
    }

    /* Create JSON message to send to backend */
    char json_buf[256];
    char tag_id_hex[MAX_TAG_ID_LEN * 2 + 1];
    
    /* Convert tag ID to hex string */
    for (int i = 0; i < tag_id_len; i++) {
        sprintf(&tag_id_hex[i * 2], "%02x", tag_id[i]);
    }
    tag_id_hex[tag_id_len * 2] = '\0';
    
    /* Format JSON message for status request */
    snprintf(json_buf, sizeof(json_buf),
             "{\"cmd\":%d,\"tag_id\":\"%s\",\"tag_id_len\":%d}\n",
             CMD_STATUS_REQ, tag_id_hex, (int)tag_id_len);
    
    LOG_DBG("Requesting status from backend: %s", json_buf);
    
    /* Send message over UART */
    for (int i = 0; i < strlen(json_buf); i++) {
        uart_poll_out(uart_dev, json_buf[i]);
    }
    
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

    /* Instead of using settings_runtime_get, we'll store the value during settings_load
     * and return it from our local storage. For now, return empty value.
     */
    memset(config_value, 0, config_value_len);
    
    /* For now, return no data found */
    return -ENOENT;
}

uint32_t gateway_service_get_error_count(void)
{
    return error_count;
}