#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/settings/settings.h>
#include "rental_manager.h"

LOG_MODULE_REGISTER(rental_manager, LOG_LEVEL_INF);

#define MAX_ACTIVE_RENTALS 10
#define RENTAL_CHECK_INTERVAL K_SECONDS(60)
#define SETTINGS_PREFIX "rental/"

struct rental_entry {
    uint8_t tag_id[MAX_TAG_ID_LEN];
    uint8_t tag_id_len;
    rentscan_status_t status;
    uint32_t start_time;
    uint32_t duration;
};

static struct rental_entry rentals[MAX_ACTIVE_RENTALS];
static uint8_t num_rentals;
static rental_status_cb_t status_callback;
static struct k_work_delayable check_work;

static void send_status_update(const struct rental_entry *entry)
{
    if (!status_callback) {
        return;
    }

    rentscan_msg_t msg = {
        .cmd = CMD_STATUS_RESP,
        .status = entry->status,
        .timestamp = entry->start_time,
        .duration = entry->duration,
        .tag_id_len = entry->tag_id_len
    };
    memcpy(msg.tag_id, entry->tag_id, entry->tag_id_len);

    status_callback(&msg);
}

static struct rental_entry *find_rental(const uint8_t *tag_id, size_t tag_id_len)
{
    for (int i = 0; i < num_rentals; i++) {
        if (rentals[i].tag_id_len == tag_id_len &&
            memcmp(rentals[i].tag_id, tag_id, tag_id_len) == 0) {
            return &rentals[i];
        }
    }
    return NULL;
}

static void check_work_handler(struct k_work *work)
{
    uint32_t now = k_uptime_get_32() / 1000;
    int expired = 0;

    for (int i = 0; i < num_rentals; i++) {
        if (rentals[i].status == STATUS_RENTED) {
            uint32_t end_time = rentals[i].start_time + rentals[i].duration;
            if (now >= end_time) {
                rentals[i].status = STATUS_EXPIRED;
                send_status_update(&rentals[i]);
                expired++;
            }
        }
    }

    if (expired > 0) {
        LOG_INF("%d rentals expired", expired);
    }

    // Schedule next check
    k_work_schedule(&check_work, RENTAL_CHECK_INTERVAL);
}

int rental_manager_init(rental_status_cb_t status_changed_cb)
{
    status_callback = status_changed_cb;
    num_rentals = 0;

    // Initialize periodic check work
    k_work_init_delayable(&check_work, check_work_handler);
    k_work_schedule(&check_work, RENTAL_CHECK_INTERVAL);

    LOG_INF("Rental manager initialized");
    return 0;
}

int rental_manager_process_tag(const uint8_t *tag_id, size_t tag_id_len,
                              const uint8_t *tag_data, size_t tag_data_len)
{
    if (!tag_id || tag_id_len == 0 || tag_id_len > MAX_TAG_ID_LEN) {
        return -EINVAL;
    }

    struct rental_entry *entry = find_rental(tag_id, tag_id_len);
    
    if (!entry && num_rentals < MAX_ACTIVE_RENTALS) {
        // New tag - add to rentals list
        entry = &rentals[num_rentals++];
        memcpy(entry->tag_id, tag_id, tag_id_len);
        entry->tag_id_len = tag_id_len;
        entry->status = STATUS_AVAILABLE;
        entry->start_time = 0;
        entry->duration = 0;
        
        send_status_update(entry);
    }

    return 0;
}

int rental_manager_process_command(const uint8_t *data, uint16_t len)
{
    if (!data || len < sizeof(rentscan_msg_t)) {
        return -EINVAL;
    }

    const rentscan_msg_t *msg = (const rentscan_msg_t *)data;
    struct rental_entry *entry = find_rental(msg->tag_id, msg->tag_id_len);

    if (!entry) {
        LOG_ERR("Unknown tag ID");
        return -ENOENT;
    }

    switch (msg->cmd) {
    case CMD_RENTAL_START:
        if (entry->status != STATUS_AVAILABLE) {
            LOG_WRN("Item not available for rent");
            return -EBUSY;
        }
        entry->status = STATUS_RENTED;
        entry->start_time = msg->timestamp;
        entry->duration = msg->duration;
        break;

    case CMD_RENTAL_END:
        if (entry->status != STATUS_RENTED &&
            entry->status != STATUS_EXPIRED) {
            LOG_WRN("Item not currently rented");
            return -EINVAL;
        }
        entry->status = STATUS_AVAILABLE;
        entry->start_time = 0;
        entry->duration = 0;
        break;

    default:
        LOG_WRN("Unknown command %d", msg->cmd);
        return -EINVAL;
    }

    send_status_update(entry);
    return 0;
}

int rental_manager_check_expirations(void)
{
    uint32_t now = k_uptime_get_32() / 1000;
    int expired = 0;

    for (int i = 0; i < num_rentals; i++) {
        if (rentals[i].status == STATUS_RENTED) {
            uint32_t end_time = rentals[i].start_time + rentals[i].duration;
            if (now >= end_time) {
                rentals[i].status = STATUS_EXPIRED;
                send_status_update(&rentals[i]);
                expired++;
            }
        }
    }

    return expired;
}

int rental_manager_get_status(const uint8_t *tag_id, size_t tag_id_len,
                             rentscan_status_t *status)
{
    if (!tag_id || !status || tag_id_len == 0) {
        return -EINVAL;
    }

    struct rental_entry *entry = find_rental(tag_id, tag_id_len);
    if (!entry) {
        return -ENOENT;
    }

    *status = entry->status;
    return 0;
}