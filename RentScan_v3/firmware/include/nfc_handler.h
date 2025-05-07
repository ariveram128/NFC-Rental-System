/**
 * @file nfc_handler.h
 * @brief NFC tag handling declarations for RentScan system
 */

 #ifndef NFC_HANDLER_H__
 #define NFC_HANDLER_H__
 
 #include <stdint.h>
 #include <stdbool.h>
 #include "app_error.h"
 #include "rental_data.h"
 
 // Maximum buffer size for NDEF messages
 #define NFC_HANDLER_NDEF_BUFFER_SIZE  256
 
 /**
  * @brief Initialize the NFC handler
  * 
  * @return NRF_SUCCESS if successful, error code otherwise
  */
 ret_code_t nfc_handler_init(void);
 
 /**
  * @brief Read current NFC tag
  * 
  * @return true if a tag was read, false otherwise
  */
 bool nfc_handler_read_tag(void);
 
 /**
  * @brief Scan for NFC tags in the vicinity
  * 
  * @param[out] p_item If a tag is found, this will be populated with the item data
  * @return true if a tag was found, false otherwise
  */
 bool nfc_handler_scan_for_tag(rental_item_t * p_item);
 
 /**
  * @brief Parse rental data from NFC tag content
  * 
  * @param[in] p_data Raw data from NFC tag
  * @param[in] data_length Length of data
  * @param[out] p_item Rental item structure to populate
  * @return true if parsing was successful, false otherwise
  */
 bool nfc_handler_parse_rental_data(const uint8_t * p_data, size_t data_length, rental_item_t * p_item);
 
 /**
  * @brief Update NFC tag content with rental item data
  * 
  * @param[in] p_item Rental item to write to the tag
  * @return true if update was successful, false otherwise
  */
 bool nfc_handler_update_tag_content(const rental_item_t * p_item);
 
 /**
  * @brief Write rental data to an NFC tag
  * 
  * @param[in] p_item Rental item to write
  * @return true if write was successful, false otherwise
  */
 bool nfc_handler_write_tag(const rental_item_t * p_item);
 
 /**
  * @brief Get the current tag ID
  * 
  * @param[out] p_tag_id Buffer to receive tag ID
  * @param[in,out] p_tag_id_length In: Buffer size, Out: Actual tag ID length
  * @return true if tag ID was retrieved, false otherwise
  */
 bool nfc_handler_get_tag_id(uint8_t * p_tag_id, uint8_t * p_tag_id_length);
 
 #endif // NFC_HANDLER_H__