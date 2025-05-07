/**
 * @file nfc_handler.c
 * @brief NFC tag handling functionality for RentScan system
 * 
 * This file contains functions for handling NFC tag reading, writing,
 * and data parsing for the RentScan rental system.
 */

 #include <string.h>
 #include "nfc_handler.h"
 #include "nrf_log.h"
 #include "app_error.h"
 #include "nfc_t4t_lib.h"
 #include "nfc_ndef_msg.h"
 #include "nfc_ndef_record.h"
 #include "nfc_ndef_text_rec.h"
 
 // Buffer for NDEF message
 static uint8_t m_ndef_buffer[NFC_HANDLER_NDEF_BUFFER_SIZE];
 static uint32_t m_ndef_buffer_len;
 
 // Current tag ID
 static uint8_t m_current_tag_id[NFC_TAG_ID_MAX_LENGTH];
 static uint8_t m_current_tag_id_length = 0;
 
 /**
  * @brief Initialize the NFC handler
  * 
  * @return NRF_SUCCESS if successful, error code otherwise
  */
 ret_code_t nfc_handler_init(void)
 {
     // Initialize buffers
     memset(m_ndef_buffer, 0, NFC_HANDLER_NDEF_BUFFER_SIZE);
     memset(m_current_tag_id, 0, NFC_TAG_ID_MAX_LENGTH);
     m_current_tag_id_length = 0;
     
     return NRF_SUCCESS;
 }
 
 /**
  * @brief Read current NFC tag
  * 
  * @return true if a tag was read, false otherwise
  */
 bool nfc_handler_read_tag(void)
 {
     bool tag_read = false;
     
     // This would normally be implemented with specific code for the NFC reader
     // Since nRF52840 doesn't have a direct NFC reader, this would likely involve:
     // 1. Using an external NFC reader connected via SPI/I2C
     // 2. Communicating with the reader to get tag data
     
     // For this example, we'll simulate tag reading with dummy data
     // In a real implementation, this would read from the actual NFC hardware
     
     // Simulate successful tag reading
     uint8_t dummy_tag_id[] = {0x04, 0xEB, 0x71, 0x3A, 0x4C, 0x84, 0x80};
     m_current_tag_id_length = sizeof(dummy_tag_id);
     memcpy(m_current_tag_id, dummy_tag_id, m_current_tag_id_length);
     
     NRF_LOG_INFO("NFC tag read: ID length %d", m_current_tag_id_length);
     NRF_LOG_HEXDUMP_INFO(m_current_tag_id, m_current_tag_id_length);
     
     tag_read = true;
     
     return tag_read;
 }
 
 /**
  * @brief Scan for NFC tags in the vicinity
  * 
  * @param[out] p_item If a tag is found, this will be populated with the item data
  * @return true if a tag was found, false otherwise
  */
 bool nfc_handler_scan_for_tag(rental_item_t * p_item)
 {
     bool tag_found = false;
     
     // This would normally scan for NFC tags within range
     // Since the nRF52840 doesn't have tag scanning capability directly,
     // this would typically involve an external NFC reader module
     
     // For this example, we'll simulate tag finding with dummy data
     // In a real implementation, this would scan using actual NFC hardware
     
     // 1 in 5 chance of "finding" a tag during scan for demonstration purposes
     if (rand() % 5 == 0)
     {
         // Simulate a found tag
         uint8_t dummy_tag_id[] = {0x04, 0xDA, 0x43, 0x2B, 0x65, 0x92, 0xF0};
         m_current_tag_id_length = sizeof(dummy_tag_id);
         memcpy(m_current_tag_id, dummy_tag_id, m_current_tag_id_length);
         
         // Create a dummy rental item
         if (p_item != NULL)
         {
             memset(p_item, 0, sizeof(rental_item_t));
             memcpy(p_item->tag_id, m_current_tag_id, m_current_tag_id_length);
             p_item->tag_id_length = m_current_tag_id_length;
             
             // Set some dummy data
             p_item->item_id = 12345;
             strncpy(p_item->item_name, "Demo Item", sizeof(p_item->item_name) - 1);
             p_item->status = RENTAL_STATUS_AVAILABLE;
             p_item->rental_duration = 24; // 24 hours
             p_item->timestamp = 0; // No active rental
         }
         
         NRF_LOG_INFO("NFC tag found during scan: ID length %d", m_current_tag_id_length);
         NRF_LOG_HEXDUMP_INFO(m_current_tag_id, m_current_tag_id_length);
         
         tag_found = true;
     }
     
     return tag_found;
 }
 
 /**
  * @brief Parse rental data from NFC tag content
  * 
  * @param[in] p_data Raw data from NFC tag
  * @param[in] data_length Length of data
  * @param[out] p_item Rental item structure to populate
  * @return true if parsing was successful, false otherwise
  */
 bool nfc_handler_parse_rental_data(const uint8_t * p_data, size_t data_length, rental_item_t * p_item)
 {
     if (p_data == NULL || p_item == NULL || data_length == 0)
     {
         return false;
     }
     
     // This function would parse an NDEF message from the NFC tag
     // and extract rental information from it
     
     // In a real implementation, this would parse NDEF records
     // For this example, we'll use a simplified format
     
     // Check if the data is large enough to be valid
     if (data_length < sizeof(rental_item_t))
     {
         NRF_LOG_WARNING("Data too short to be a valid rental item");
         return false;
     }
     
     // Simple case: assume the data is a directly encoded rental_item_t
     // In a real implementation, this would involve proper NDEF parsing
     memcpy(p_item, p_data, sizeof(rental_item_t));
     
     // Validate the parsed data
     if (p_item->tag_id_length > NFC_TAG_ID_MAX_LENGTH)
     {
         NRF_LOG_WARNING("Invalid tag ID length in parsed data");
         return false;
     }
     
     NRF_LOG_INFO("Parsed rental data: Item ID %d, Status %d", p_item->item_id, p_item->status);
     
     return true;
 }
 
 /**
  * @brief Update NFC tag content with rental item data
  * 
  * @param[in] p_item Rental item to write to the tag
  * @return true if update was successful, false otherwise
  */
 bool nfc_handler_update_tag_content(const rental_item_t * p_item)
 {
     ret_code_t err_code;
     
     if (p_item == NULL)
     {
         return false;
     }
     
     // Create an NDEF message with rental information
     // This would normally be a more complex NDEF message creation
     // For simplicity, we'll use a text record with some basic info
     
     // Create a text description of the rental status
     char status_text[128];
     const char* status_str;
     
     switch (p_item->status)
     {
         case RENTAL_STATUS_AVAILABLE:
             status_str = "Available";
             break;
         case RENTAL_STATUS_RENTED:
             status_str = "Rented";
             break;
         case RENTAL_STATUS_MAINTENANCE:
             status_str = "Maintenance";
             break;
         case RENTAL_STATUS_LOST:
             status_str = "Lost";
             break;
         default:
             status_str = "Unknown";
             break;
     }
     
     // Format a status message
     snprintf(status_text, sizeof(status_text), 
              "Item: %s\nID: %d\nStatus: %s\nDuration: %d hours",
              p_item->item_name, p_item->item_id, status_str, p_item->rental_duration);
     
     // Prepare text record
     nfc_ndef_record_desc_t text_rec;
     uint8_t language[] = {'e', 'n'};
     
     // Encode the record
     err_code = nfc_ndef_text_rec_encode((uint8_t*)language, sizeof(language),
                                      (uint8_t*)status_text, strlen(status_text),
                                      m_ndef_buffer, &m_ndef_buffer_len);
     if (err_code != NRF_SUCCESS)
     {
         NRF_LOG_ERROR("Error encoding NDEF text record: %d", err_code);
         return false;
     }
     
     // Update the NFC tag content
     // In this example, this simply updates the emulated tag
     err_code = nfc_t4t_ndef_file_set(m_ndef_buffer, m_ndef_buffer_len);
     if (err_code != NRF_SUCCESS)
     {
         NRF_LOG_ERROR("Error updating NFC tag content: %d", err_code);
         return false;
     }
     
     NRF_LOG_INFO("NFC tag content updated with rental information");
     return true;
 }
 
 /**
  * @brief Write rental data to an NFC tag
  * 
  * @param[in] p_item Rental item to write
  * @return true if write was successful, false otherwise
  */
 bool nfc_handler_write_tag(const rental_item_t * p_item)
 {
     if (p_item == NULL)
     {
         return false;
     }
     
     // This would normally handle the process of writing to a physical NFC tag
     // Since we're using tag emulation in this example, we'll just use the update function
     
     return nfc_handler_update_tag_content(p_item);
 }
 
 /**
  * @brief Get the current tag ID
  * 
  * @param[out] p_tag_id Buffer to receive tag ID
  * @param[in,out] p_tag_id_length In: Buffer size, Out: Actual tag ID length
  * @return true if tag ID was retrieved, false otherwise
  */
 bool nfc_handler_get_tag_id(uint8_t * p_tag_id, uint8_t * p_tag_id_length)
 {
     if (p_tag_id == NULL || p_tag_id_length == NULL || *p_tag_id_length < m_current_tag_id_length)
     {
         return false;
     }
     
     // Copy the current tag ID
     memcpy(p_tag_id, m_current_tag_id, m_current_tag_id_length);
     *p_tag_id_length = m_current_tag_id_length;
     
     return (m_current_tag_id_length > 0);
 }