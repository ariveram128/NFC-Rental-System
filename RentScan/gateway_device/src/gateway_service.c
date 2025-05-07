#include <zephyr/kernel.h>
#include <zephyr/settings/settings.h>
#include <zephyr/logging/log.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <zephyr/random/rand32.h>
#include "gateway_service.h"

LOG_MODULE_REGISTER(gateway_service, LOG_LEVEL_INF);

#define CONFIG_PREFIX "gateway/"
#define MAX_CONFIG_KEY_LEN 32
#define MAX_CONFIG_VALUE_LEN 64

/* Backend simulation settings */
#define BACKEND_SIM_STORAGE_SIZE 16
#define BACKEND_SIM_CHECK_INTERVAL_MS 10000  /* 10 second interval for connection checks */
#define MAX_ACTIVE_RENTALS 8

/* Simulated backend connection state */
static bool backend_connected = false;
static int backend_error_count = 0;
static struct k_work_delayable backend_sim_check_work;

/* Simulated backend storage */
typedef struct {
    rentscan_msg_t messages[BACKEND_SIM_STORAGE_SIZE];
    uint8_t message_count;
    uint32_t last_sent_timestamp;
    uint8_t config_values[MAX_CONFIG_VALUE_LEN];
    rental_info_t active_rentals[MAX_ACTIVE_RENTALS];
    uint8_t rental_count;
    bool connected;
} backend_sim_t;

static backend_sim_t backend_sim = {
    .message_count = 0,
    .last_sent_timestamp = 0,
    .config_values = {0},
    .rental_count = 0
};

/* Forward declarations */
static void backend_sim_check_handler(struct k_work *work);

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

/* Check if a rental has expired based on current time */
static bool rental_is_expired(const rental_info_t *rental)
{
    if (!rental || !rental->active) {
        return false;
    }
    
    uint32_t current_time = k_uptime_get_32() / 1000;
    return (current_time > rental->start_time + rental->duration);
}

/* Find rental by item ID */
static rental_info_t *find_rental_by_item_id(const char *item_id)
{
    if (!item_id) {
        return NULL;
    }
    
    for (int i = 0; i < backend_sim.rental_count; i++) {
        if (strcmp(backend_sim.active_rentals[i].item_id, item_id) == 0 && 
            backend_sim.active_rentals[i].active) {
            return &backend_sim.active_rentals[i];
        }
    }
    
    return NULL;
}

/* Simulated backend connection check work handler */
static void backend_sim_check_handler(struct k_work *work)
{
    /* Simulate backend connection state changes with some randomness */
    int random_val = sys_rand32_get() % 10;
    
    if (!backend_connected && random_val >= 2) {
        /* 80% chance to connect if disconnected */
        backend_connected = true;
        LOG_INF("Backend connection established");
    } else if (backend_connected && random_val == 0) {
        /* 10% chance to disconnect if connected */
        backend_connected = false;
        LOG_WRN("Backend connection lost");
    }
    
    /* Periodically process stored messages to simulate backend sync */
    if (backend_connected && backend_sim.message_count > 0) {
        LOG_INF("Simulating backend message processing: %u messages in queue", 
                backend_sim.message_count);
        
        /* Clear the queue to simulate successful processing */
        backend_sim.message_count = 0;
        backend_sim.last_sent_timestamp = k_uptime_get_32();
    }
    
    /* Check for expired rentals */
    bool expired_found = false;
    for (int i = 0; i < backend_sim.rental_count; i++) {
        if (backend_sim.active_rentals[i].active && 
            rental_is_expired(&backend_sim.active_rentals[i])) {
            LOG_WRN("Rental for item %s has expired", 
                   backend_sim.active_rentals[i].item_id);
            expired_found = true;
        }
    }
    
    if (expired_found && backend_connected) {
        LOG_INF("Simulating notification of expired rentals to backend");
    }
    
    /* Reschedule the work */
    k_work_schedule(&backend_sim_check_work, K_MSEC(BACKEND_SIM_CHECK_INTERVAL_MS));
}

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

    /* Initialize backend simulation work */
    k_work_init_delayable(&backend_sim_check_work, backend_sim_check_handler);
    
    /* Start the backend simulation check */
    k_work_schedule(&backend_sim_check_work, K_MSEC(BACKEND_SIM_CHECK_INTERVAL_MS));

    /* Use random seed to initialize backend connection state */
    int random_val = sys_rand32_get() % 10;
    backend_connected = (random_val >= 3); /* 70% chance to start as connected */

    LOG_INF("Gateway service initialized (Backend %s)", 
           backend_connected ? "connected" : "disconnected");
    return 0;
}

int gateway_service_process_message(const rentscan_msg_t *msg)
{
    if (!msg) {
        return -EINVAL;
    }

    LOG_INF("Processing message command %d", msg->cmd);
    
    /* Simulate some backend processing */
    if (!backend_connected) {
        LOG_WRN("Backend not connected, buffering message");
        
        /* If we have space in our simulated buffer */
        if (backend_sim.message_count < BACKEND_SIM_STORAGE_SIZE) {
            /* Store the message */
            memcpy(&backend_sim.messages[backend_sim.message_count], 
                   msg, sizeof(rentscan_msg_t));
            backend_sim.message_count++;
            LOG_INF("Message queued for sending (%d in queue)", backend_sim.message_count);
            return 0;
        } else {
            LOG_ERR("Message queue full, dropping message");
            backend_error_count++;
            return -ENOSPC;
        }
    } else {
        /* Simulate successful immediate processing */
        char cmd_str[32];
        
        /* Convert the command to a string for logging */
        switch (msg->cmd) {
            case CMD_RENTAL_START:
                snprintf(cmd_str, sizeof(cmd_str), "rental start");
                break;
            case CMD_RENTAL_END:
                snprintf(cmd_str, sizeof(cmd_str), "rental end");
                break;
            case CMD_STATUS_REQ:
                snprintf(cmd_str, sizeof(cmd_str), "status request");
                break;
            case CMD_STATUS_RESP:
                snprintf(cmd_str, sizeof(cmd_str), "status response");
                break;
            case CMD_ERROR:
                snprintf(cmd_str, sizeof(cmd_str), "error");
                break;
            default:
                snprintf(cmd_str, sizeof(cmd_str), "unknown(%d)", msg->cmd);
                break;
        }
        
        LOG_INF("Sent %s to backend for tag ID length %d", cmd_str, msg->tag_id_len);
        
        /* Update our last sent timestamp */
        backend_sim.last_sent_timestamp = k_uptime_get_32();
        return 0;
    }
}

int gateway_service_start_rental(const char *item_id, const char *user_id, uint32_t duration)
{
    if (!item_id || !user_id || duration == 0) {
        return -EINVAL;
    }
    
    /* Check if item is already rented */
    rental_info_t *existing = find_rental_by_item_id(item_id);
    if (existing) {
        LOG_WRN("Item %s is already rented", item_id);
        return -EBUSY;
    }
    
    /* Check if we have space for a new rental */
    if (backend_sim.rental_count >= MAX_ACTIVE_RENTALS) {
        LOG_ERR("Maximum active rentals reached");
        return -ENOSPC;
    }
    
    /* Add the new rental */
    rental_info_t *rental = &backend_sim.active_rentals[backend_sim.rental_count];
    strncpy(rental->item_id, item_id, MAX_TAG_ID_LEN);
    rental->item_id[MAX_TAG_ID_LEN] = '\0';
    
    strncpy(rental->user_id, user_id, sizeof(rental->user_id) - 1);
    rental->user_id[sizeof(rental->user_id) - 1] = '\0';
    
    rental->start_time = k_uptime_get_32() / 1000;  /* Convert to seconds */
    rental->duration = duration;
    rental->active = true;
    
    backend_sim.rental_count++;
    
    LOG_INF("Rental started for item %s by user %s for %u seconds", 
           item_id, user_id, duration);
    
    /* Create and send a message to the backend */
    rentscan_msg_t msg = {
        .cmd = CMD_RENTAL_START,
        .status = STATUS_RENTED,
        .tag_id_len = strlen(item_id),
        .timestamp = rental->start_time,
        .duration = duration,
        .payload_len = strlen(user_id)
    };
    
    /* Copy the tag ID and user ID */
    memcpy(msg.tag_id, item_id, msg.tag_id_len);
    memcpy(msg.payload, user_id, msg.payload_len);
    
    /* Process the message */
    return gateway_service_process_message(&msg);
}

int gateway_service_end_rental(const char *item_id)
{
    if (!item_id) {
        return -EINVAL;
    }
    
    /* Find the rental */
    rental_info_t *rental = find_rental_by_item_id(item_id);
    if (!rental) {
        LOG_WRN("No active rental found for item %s", item_id);
        return -ENOENT;
    }
    
    /* Calculate actual duration */
    uint32_t end_time = k_uptime_get_32() / 1000;
    uint32_t actual_duration = end_time - rental->start_time;
    
    /* Update rental status */
    rental->active = false;
    
    LOG_INF("Rental ended for item %s (duration: %u seconds)", 
           item_id, actual_duration);
    
    /* Create and send a message to the backend */
    rentscan_msg_t msg = {
        .cmd = CMD_RENTAL_END,
        .status = STATUS_AVAILABLE,
        .tag_id_len = strlen(item_id),
        .timestamp = end_time,
        .duration = actual_duration,
        .payload_len = 0
    };
    
    /* Copy the tag ID */
    memcpy(msg.tag_id, item_id, msg.tag_id_len);
    
    /* Process the message */
    return gateway_service_process_message(&msg);
}

int gateway_service_get_rental_status(const char *item_id, rentscan_status_t *status)
{
    if (!item_id || !status) {
        return -EINVAL;
    }
    
    /* Find the rental */
    rental_info_t *rental = find_rental_by_item_id(item_id);
    if (!rental) {
        *status = STATUS_AVAILABLE;
        return 0;
    }
    
    /* Check if rental has expired */
    if (rental_is_expired(rental)) {
        *status = STATUS_EXPIRED;
    } else {
        *status = STATUS_RENTED;
    }
    
    return 0;
}

int gateway_service_get_active_rentals(rental_info_t *rentals, size_t max_count, size_t *count)
{
    if (!rentals || !count || max_count == 0) {
        return -EINVAL;
    }
    
    *count = 0;
    
    /* Copy active rentals */
    for (int i = 0; i < backend_sim.rental_count && *count < max_count; i++) {
        if (backend_sim.active_rentals[i].active) {
            memcpy(&rentals[*count], &backend_sim.active_rentals[i], 
                  sizeof(rental_info_t));
            (*count)++;
        }
    }
    
    return 0;
}

int gateway_service_request_status(const uint8_t *tag_id, size_t tag_id_len)
{
    if (!tag_id || tag_id_len == 0 || tag_id_len > MAX_TAG_ID_LEN) {
        return -EINVAL;
    }

    LOG_INF("Requesting status for tag");
    
    /* Create a status request message */
    rentscan_msg_t status_req = {
        .cmd = CMD_STATUS_REQ,
        .status = 0,
        .tag_id_len = tag_id_len,
        .timestamp = k_uptime_get_32() / 1000,  /* Convert to seconds */
        .duration = 0,
        .payload_len = 0
    };
    
    /* Copy the tag ID */
    memcpy(status_req.tag_id, tag_id, tag_id_len);
    
    /* Process the message using our normal message processing */
    return gateway_service_process_message(&status_req);
}

bool gateway_service_is_connected(void)
{
    /* Update backend status with a slight chance of temporary disconnection */
    if (backend_connected) {
        /* If connected, there's a small random chance of temporary disconnect */
        int random_val = sys_rand32_get() % 100;
        if (random_val < 3) {  /* 3% chance of temporary disconnection */
            LOG_DBG("Temporary backend connection interruption");
            return false;
        }
    }
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

    /* For specific config keys, handle special behavior */
    if (strcmp(config_key, "backend_connect") == 0) {
        if (strcmp(config_value, "1") == 0 || 
            strcmp(config_value, "true") == 0 || 
            strcmp(config_value, "yes") == 0) {
            backend_connected = true;
            LOG_INF("Backend connection manually enabled");
        } else if (strcmp(config_value, "0") == 0 || 
                  strcmp(config_value, "false") == 0 || 
                  strcmp(config_value, "no") == 0) {
            backend_connected = false;
            LOG_INF("Backend connection manually disabled");
        }
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

    /* Special case for requesting backend status */
    if (strcmp(config_key, "backend_status") == 0) {
        snprintf(config_value, config_value_len, "%s", backend_connected ? "connected" : "disconnected");
        return strlen(config_value);
    }
    
    /* Special case for queue stats */
    if (strcmp(config_key, "queue_count") == 0) {
        snprintf(config_value, config_value_len, "%d", backend_sim.message_count);
        return strlen(config_value);
    }
    
    /* Special case for active rental count */
    if (strcmp(config_key, "rental_count") == 0) {
        int active_count = 0;
        for (int i = 0; i < backend_sim.rental_count; i++) {
            if (backend_sim.active_rentals[i].active) {
                active_count++;
            }
        }
        snprintf(config_value, config_value_len, "%d", active_count);
        return strlen(config_value);
    }
    
    /* Otherwise, try to load from settings */
    /* Real implementation would load from settings system */
    memset(config_value, 0, config_value_len);
    return -ENOENT;
}

/* Get error count from the backend simulation */
int gateway_service_get_error_count(void)
{
    return backend_error_count;
}

/* Reset error count */
void gateway_service_reset_error_count(void)
{
    backend_error_count = 0;
}

int gateway_service_get_status(gateway_service_status_t *status)
{
    if (!status) {
        return -EINVAL;
    }
    
    status->backend_connected = backend_connected;
    status->error_count = backend_error_count;
    status->queue_size = backend_sim.message_count;
    status->rental_count = backend_sim.rental_count;
    
    return 0;
}

int gateway_service_reset_errors(void)
{
    backend_error_count = 0;
    return 0;
}

int gateway_service_connect_backend(void)
{
    backend_connected = true;
    backend_sim.connected = true;
    
    /* Process any queued messages immediately */
    if (backend_sim.message_count > 0) {
        LOG_INF("Processing %d queued messages", backend_sim.message_count);
        backend_sim.message_count = 0;
    }
    
    LOG_INF("Backend connection established (manual)");
    return 0;
}

int gateway_service_disconnect_backend(void)
{
    backend_connected = false;
    backend_sim.connected = false;
    LOG_WRN("Backend connection lost (manual)");
    return 0;
}

int gateway_service_get_rental(uint32_t index, rental_info_t *rental)
{
    if (!rental || index >= backend_sim.rental_count) {
        return -EINVAL;
    }
    
    memcpy(rental, &backend_sim.active_rentals[index], sizeof(rental_info_t));
    return 0;
}