/* 
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: Apache-2.0
 */

 #ifndef NFC_HANDLER_H
 #define NFC_HANDLER_H
 #define MAX_ITEM_ID_LEN 64
 
 #include <zephyr/kernel.h>
 #include <stddef.h>
 #include <stdint.h>
 
 /**
  * @brief Initialize the NFC reader
  *
  * This function initializes the NFC reader functionality for tag polling.
  *
  * @return 0 on success, otherwise a negative error code
  */
 int nfc_reader_init(void);
 
 /**
  * @brief Initialize the NFC subsystem
  *
  * This function initializes the NFC Type 4 Tag emulation and
  * prepares for tag reading.
  *
  * @return 0 on success, otherwise a negative error code
  */
 int nfc_init(void);
 
 /**
  * @brief Start NFC field polling
  *
  * This function starts the polling for NFC tags.
  *
  * @return 0 on success, otherwise a negative error code
  */
 int nfc_start_polling(void);
 
 /**
  * @brief Stop NFC field polling
  *
  * This function stops the polling for NFC tags.
  *
  * @return 0 on success, otherwise a negative error code
  */
 int nfc_stop_polling(void);
 
 /**
  * @brief Callback function when an NFC tag is detected
  *
  * This function handles an NFC tag when detected, reading and parsing
  * the NDEF message to extract the Item ID.
  *
  * @param item_id Buffer to store the extracted item ID text
  * @param max_len Maximum length of the item_id buffer
  * @return Length of the item ID on success, otherwise a negative error code
  */
 int nfc_handle_tag_detected(char *item_id, size_t max_len);
 
 #endif /* NFC_HANDLER_H */
 