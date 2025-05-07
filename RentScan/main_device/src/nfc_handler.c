#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/device.h>
#include <nrfx_nfct.h>
#include <nfc_t2t_lib.h>
#include <nfc/t2t/parser.h>
#include <nfc/ndef/msg.h>
#include <nfc/ndef/text_rec.h>
#include "nfc_handler.h"

LOG_MODULE_REGISTER(nfc_handler, LOG_LEVEL_INF);

#define NFC_TAG_DATA_MAX_LEN 1024
#define NFC_POLL_INTERVAL K_MSEC(100)

static nfc_tag_callback_t tag_callback;
static bool is_polling = false;
static struct k_work_delayable poll_work;
static uint8_t tag_data_buf[NFC_TAG_DATA_MAX_LEN];

/* NDEF message buffer */
static uint8_t ndef_msg_buf[NFC_TAG_DATA_MAX_LEN];
static uint32_t len;

/* Default NDEF message */
static const uint8_t en_code[] = {'e', 'n'};
static const uint8_t en_payload[] = "RentScan Ready";

/* NFC callback from T2T lib */
static void nfc_callback(void *context,
                        nfc_t2t_event_t event,
                        const uint8_t *data,
                        size_t data_length)
{
    int err;
    
    switch (event) {
    case NFC_T2T_EVENT_FIELD_ON:
        LOG_INF("NFC field detected");
        break;

    case NFC_T2T_EVENT_FIELD_OFF:
        LOG_INF("NFC field lost");
        break;

    case NFC_T2T_EVENT_DATA_READ:
        if (tag_callback && data) {
            // For now, use first 8 bytes as tag ID
            tag_callback(data, 8, data + 8, data_length - 8);
        }
        break;

    default:
        break;
    }
}

static void poll_work_handler(struct k_work *work)
{
    if (is_polling) {
        // Reschedule polling work
        k_work_schedule(&poll_work, NFC_POLL_INTERVAL);
    }
}

int nfc_handler_init(nfc_tag_callback_t tag_detected_cb)
{
    int err;

    tag_callback = tag_detected_cb;

    // Initialize NFC Type 2 Tag library
    nfc_t2t_setup(nfc_callback, NULL);

    // Initialize default NDEF message
    NFC_NDEF_MSG_DEF(welcome_msg, 1);
    NFC_NDEF_TEXT_RECORD_DESC_DEF(welcome_text,
                                 UTF_8,
                                 en_code,
                                 sizeof(en_code),
                                 en_payload,
                                 sizeof(en_payload));

    err = nfc_ndef_msg_record_add(&NFC_NDEF_MSG(welcome_msg),
                                 &NFC_NDEF_TEXT_RECORD_DESC(welcome_text));
    if (err) {
        LOG_ERR("Cannot add NDEF record (err %d)", err);
        return err;
    }

    len = sizeof(ndef_msg_buf);
    err = nfc_ndef_msg_encode(&NFC_NDEF_MSG(welcome_msg),
                             ndef_msg_buf,
                             &len);
    if (err) {
        LOG_ERR("Cannot encode NDEF message (err %d)", err);
        return err;
    }

    // Set encoded message as the NFC payload
    err = nfc_t2t_payload_set(ndef_msg_buf, len);
    if (err) {
        LOG_ERR("Cannot set payload (err %d)", err);
        return err;
    }

    // Initialize polling work
    k_work_init_delayable(&poll_work, poll_work_handler);

    LOG_INF("NFC handler initialized");
    return 0;
}

int nfc_handler_start_polling(void)
{
    if (is_polling) {
        return 0;
    }

    int err = nfc_t2t_emulation_start();
    if (err) {
        LOG_ERR("Cannot start T2T emulation (err %d)", err);
        return err;
    }

    is_polling = true;
    k_work_schedule(&poll_work, K_NO_WAIT);
    LOG_INF("NFC polling started");
    return 0;
}

int nfc_handler_stop_polling(void)
{
    if (!is_polling) {
        return 0;
    }

    nfc_t2t_emulation_stop();
    is_polling = false;
    k_work_cancel_delayable(&poll_work);
    LOG_INF("NFC polling stopped");
    return 0;
}

int nfc_handler_write_tag(const uint8_t *data, size_t data_len)
{
    if (!data || data_len == 0 || data_len > NFC_TAG_DATA_MAX_LEN) {
        return -EINVAL;
    }

    int err;

    // Create NDEF message with text record
    NFC_NDEF_MSG_DEF(ndef_msg, 1);
    NFC_NDEF_TEXT_RECORD_DESC_DEF(text_rec,
                                 UTF_8,
                                 en_code,
                                 sizeof(en_code),
                                 data,
                                 data_len);

    err = nfc_ndef_msg_record_add(&NFC_NDEF_MSG(ndef_msg),
                                 &NFC_NDEF_TEXT_RECORD_DESC(text_rec));
    if (err) {
        LOG_ERR("Cannot add NDEF record (err %d)", err);
        return err;
    }

    len = sizeof(ndef_msg_buf);
    err = nfc_ndef_msg_encode(&NFC_NDEF_MSG(ndef_msg),
                             ndef_msg_buf,
                             &len);
    if (err) {
        LOG_ERR("Cannot encode message (err %d)", err);
        return err;
    }

    err = nfc_t2t_payload_set(ndef_msg_buf, len);
    if (err) {
        LOG_ERR("Cannot set payload (err %d)", err);
        return err;
    }

    LOG_INF("Tag write successful");
    return 0;
}

bool nfc_handler_is_active(void)
{
    return is_polling;
}