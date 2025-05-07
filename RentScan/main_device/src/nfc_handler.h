/**
 * @file nfc_handler.h
 * @brief NFC tag reading and writing functionality
 */

#ifndef NFC_HANDLER_H
#define NFC_HANDLER_H

#include <zephyr/kernel.h>
#include <stddef.h>
#include <stdbool.h>

/**
 * @brief Callback for NFC tag detection/reading
 * 
 * @param tag_id Pointer to tag ID buffer
 * @param tag_id_len Length of tag ID
 * @param tag_data Pointer to tag data buffer (can be NULL if no data)
 * @param tag_data_len Length of tag data
 */
typedef void (*nfc_tag_callback_t)(const uint8_t *tag_id, size_t tag_id_len,
                                   const uint8_t *tag_data, size_t tag_data_len);

/**
 * @brief Initialize the NFC subsystem
 * 
 * @param tag_detected_cb Callback function to be called when a tag is detected
 * @return int 0 on success, negative error code otherwise
 */
int nfc_handler_init(nfc_tag_callback_t tag_detected_cb);

/**
 * @brief Start NFC polling for tags
 * 
 * @return int 0 on success, negative error code otherwise
 */
int nfc_handler_start_polling(void);

/**
 * @brief Stop NFC polling
 * 
 * @return int 0 on success, negative error code otherwise
 */
int nfc_handler_stop_polling(void);

/**
 * @brief Write data to an NFC tag
 * 
 * @param data Pointer to data buffer
 * @param data_len Length of data
 * @return int 0 on success, negative error code otherwise
 */
int nfc_handler_write_tag(const uint8_t *data, size_t data_len);

/**
 * @brief Get the current NFC subsystem status
 * 
 * @return true if NFC is active and polling
 * @return false if NFC is inactive
 */
bool nfc_handler_is_active(void);

#endif /* NFC_HANDLER_H */ 