#include "nfc_handler.h"
#include "rental_logic.h" // Include to call the processing function

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <string.h>

#include <nfc_t2t_lib.h>        // Core NFC Type 2 Tag library
#include <nfc/ndef/msg_parser.h> // NDEF Message parsing
#include <nfc/ndef/text_rec.h>   // NDEF Text Record specifics
#include <nfc/ndef/uri_rec.h>    // NDEF URI Record specifics

#include <nfc/t2t/parser.h>        // Correct T2T parser header
#include <nfc/ndef/msg.h>          // NDEF Message processing

LOG_MODULE_REGISTER(nfc_handler, LOG_LEVEL_INF); // Register module for logging

// --- NDEF Message Buffers ---
#define NDEF_MSG_BUF_SIZE 512 // Adjust as needed
static uint8_t ndef_msg_buf[NDEF_MSG_BUF_SIZE];

// Work queue for deferring NFC data processing
static struct k_work_delayable nfc_process_work;

// Buffer to temporarily store NFC data for processing
static uint8_t nfc_data_buf[NDEF_MSG_BUF_SIZE];
static size_t nfc_data_len = 0;

// Forward Declarations
static void nfc_callback(void *context, nfc_t2t_event_t event,
                         const uint8_t *data, size_t data_length);
static void nfc_process_work_handler(struct k_work *work);

// --- Initialization Function ---
int nfc_reader_init(void)
{
    int err;
    
    // Initialize workqueue for processing
    k_work_init_delayable(&nfc_process_work, nfc_process_work_handler);

    // Initialize and start the NFC T2T parser
    err = nfc_t2t_parse_setup();
    if (err) {
        LOG_ERR("Cannot setup NFC T2T parser (err: %d)", err);
        return err;
    }
    
    LOG_INF("NFC T2T parser initialized successfully.");
    return 0;
}

// --- Start NFC Polling ---
int nfc_reader_start(void)
{
    int err;
    
    // Start the NFC T2T polling
    err = nfc_t2t_polling_start();
    if (err) {
        LOG_ERR("Cannot start NFC T2T polling (err: %d)", err);
        return err;
    }
    
    LOG_INF("NFC T2T polling started successfully.");
    return 0;
}

// --- Helper Function to Parse and Process ---
static void parse_and_process_ndef(const uint8_t *data, size_t data_length)
{
    int err;
    struct nfc_ndef_msg_desc *ndef_msg;
    uint8_t desc_buf[NFC_NDEF_PARSER_REQUIRED_MEM(10)]; // Buffer for 10 records max
    uint32_t desc_buf_len = sizeof(desc_buf);

    LOG_INF("Attempting to parse NDEF message (%u bytes)", data_length);

    // Parse the NDEF message
    err = ndef_msg_parser_parse(data, data_length, &ndef_msg, desc_buf, &desc_buf_len);
    if (err) {
        LOG_ERR("Error during NDEF message parsing (err %d)", err);
        // Print raw data for debugging if needed
        LOG_DBG("Raw data dump:");
        for (size_t i = 0; i < data_length && i < 32; i++) {
            printk("%02x ", data[i]);
            if ((i + 1) % 8 == 0) printk(" ");
        }
        printk("\n");
        return;
    }

    LOG_INF("NDEF message parsed successfully. %u record(s) found.",
            ndef_msg->record_count);

    // --- Iterate through records to find Item ID ---
    bool item_found = false;
    for (uint32_t i = 0; i < ndef_msg->record_count; i++) {
        const struct nfc_ndef_record_desc *rec = ndef_msg->record[i];
        
        // Check for Text Record
        if (rec->tnf == TNF_WELL_KNOWN &&
            !memcmp(rec->type, NFC_NDEF_TEXT_RECORD_TYPE, rec->type_length)) {

            struct nfc_ndef_text_rec_payload text_payload;
            err = nfc_ndef_text_rec_parse(rec, &text_payload);

            if (err == 0) {
                LOG_INF("Found Text Record:");
                LOG_INF(" - Language Code: %.*s", 
                        text_payload.lang_len, 
                        text_payload.lang_code);
                LOG_INF(" - Data: %.*s", 
                        text_payload.data_len, 
                        text_payload.data);

                // Call Rental Logic
                rental_logic_process_scan(text_payload.data, text_payload.data_len);
                item_found = true;
                break;
            } else {
                LOG_ERR("Failed to parse text record payload (err %d)", err);
            }
        }
        
        // Check for URI Record if needed
        else if (rec->tnf == TNF_WELL_KNOWN &&
                 !memcmp(rec->type, NFC_NDEF_URI_RECORD_TYPE, rec->type_length)) {
                 
            struct nfc_ndef_uri_rec_payload uri_payload;
            err = nfc_ndef_uri_rec_parse(rec, &uri_payload);
            
            if (err == 0) {
                LOG_INF("Found URI Record: %.*s", 
                        uri_payload.uri_data_len, 
                        uri_payload.uri_data);
                
                // Process URI-based item ID 
                // (you might want to extract just part of the URI)
                rental_logic_process_scan(uri_payload.uri_data, 
                                         uri_payload.uri_data_len);
                item_found = true;
                break;
            } else {
                LOG_ERR("Failed to parse URI record (err %d)", err);
            }
        }
    }

    if (!item_found) {
        LOG_WRN("No relevant Item ID record found in NDEF message.");
    }
}

// --- Work Handler for Processing NFC Data ---
static void nfc_process_work_handler(struct k_work *work)
{
    // Process the data stored in the buffer
    if (nfc_data_len > 0) {
        parse_and_process_ndef(nfc_data_buf, nfc_data_len);
        nfc_data_len = 0; // Reset for next tag
    }
}

// --- NFC Callback Function ---
static void nfc_callback(void *context, nfc_t2t_event_t event,
                         const uint8_t *data, size_t data_length)
{
    ARG_UNUSED(context);

    switch (event) {
    case NFC_T2T_EVENT_FIELD_ON:
        LOG_INF("NFC field detected");
        break;
        
    case NFC_T2T_EVENT_FIELD_OFF:
        LOG_INF("NFC field lost");
        break;
        
    case NFC_T2T_EVENT_DATA_READ:
        LOG_INF("NFC Tag data read, %u bytes of data received", data_length);
        
        if (data && data_length > 0) {
            // Copy data to our buffer
            if (data_length <= sizeof(nfc_data_buf)) {
                memcpy(nfc_data_buf, data, data_length);
                nfc_data_len = data_length;
                
                // Schedule processing via workqueue
                k_work_schedule(&nfc_process_work, K_NO_WAIT);
            } else {
                LOG_WRN("NFC data too large for buffer!");
            }
        }
        break;
        
    case NFC_T2T_EVENT_STOPPED:
        LOG_INF("NFC Tag reading stopped");
        break;
        
    default:
        LOG_WRN("Unhandled NFC T2T event: %d", event);
        break;
    }
}