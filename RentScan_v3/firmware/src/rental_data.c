/**
 * @file rental_data.c
 * @brief Implementation of rental data operations for RentScan system
 *
 * This file contains functions for processing rental operations, such as
 * checkout, checkin, and status queries.
 */

 #include <stdint.h>
 #include <stdbool.h>
 #include <string.h>
 #include <time.h>
 #include "app_error.h"
 #include "nrf_log.h"
 #include "rental_data.h"
 #include "storage_manager.h"
 
 // Define a small rental database for example purposes
 // In a real system, this would be stored in flash memory or external storage
 #define MAX_RENTAL_ITEMS 10
 static rental_item_t m_rental_database[MAX_RENTAL_ITEMS];
 static uint8_t m_num_items = 0;
 
 // Simulated current time (seconds since epoch)
 // In a real system, this would be from an RTC or network time
 static uint32_t m_current_time = 0;
 
 /**
  * @brief Initialize the rental data module
  *
  * @return NRF_SUCCESS if successful, error code otherwise
  */
 ret_code_t rental_data_init(void)
 {
     // Clear the database
     memset(m_rental_database, 0, sizeof(m_rental_database));
     m_num_items = 0;
     
     // Initialize with some example items
     rental_item_t item1, item2, item3;
     
     // Item 1
     memset(&item1, 0, sizeof(rental_item_t));
     item1.item_id = 1001;
     strncpy(item1.item_name, "Drill Kit", sizeof(item1.item_name) - 1);
     item1.status = RENTAL_STATUS_AVAILABLE;
     item1.rental_duration = 24; // 24 hours
     item1.timestamp = 0;
     item1.tag_id_length = 7;
     uint8_t tag1[] = {0x04, 0xA3, 0x27, 0x5F, 0x12, 0x34, 0x56};
     memcpy(item1.tag_id, tag1, item1.tag_id_length);
     
     // Item 2
     memset(&item2, 0, sizeof(rental_item_t));
     item2.item_id = 1002;
     strncpy(item2.item_name, "Ladder", sizeof(item2.item_name) - 1);
     item2.status = RENTAL_STATUS_AVAILABLE;
     item2.rental_duration = 72; // 72 hours
     item2.timestamp = 0;
     item2.tag_id_length = 7;
     uint8_t tag2[] = {0x04, 0xB1, 0x45, 0x6D, 0xAB, 0xCD, 0xEF};
     memcpy(item2.tag_id, tag2, item2.tag_id_length);
     
     // Item 3
     memset(&item3, 0, sizeof(rental_item_t));
     item3.item_id = 1003;
     strncpy(item3.item_name, "Power Washer", sizeof(item3.item_name) - 1);
     item3.status = RENTAL_STATUS_MAINTENANCE;
     item3.rental_duration = 48; // 48 hours
     item3.timestamp = 0;
     item3.tag_id_length = 7;
     uint8_t tag3[] = {0x04, 0xC7, 0x89, 0x0A, 0x11, 0x22, 0x33};
     memcpy(item3.tag_id, tag3, item3.tag_id_length);
     
     // Add items to database
     m_rental_database[0] = item1;
     m_rental_database[1] = item2;
     m_rental_database[2] = item3;
     m_num_items = 3;
     
     // Try to load rental data from storage
     // This would replace the example items with real stored data
     storage_manager_load_rental_items(m_rental_database, MAX_RENTAL_ITEMS, &m_num_items);
     
     // Set a simulated current time
     m_current_time = 1651234567; // A fixed timestamp for example
     
     NRF_LOG_INFO("Rental data initialized with %d items", m_num_items);
     return NRF_SUCCESS;
 }
 
 /**
  * @brief Update the current system time
  * 
  * @param timestamp New timestamp (seconds since epoch)
  */
 void rental_data_update_time(uint32_t timestamp)
 {
     m_current_time = timestamp;
 }
 
 /**
  * @brief Get the current system time
  * 
  * @return Current timestamp (seconds since epoch)
  */
 uint32_t rental_data_get_time(void)
 {
     return m_current_time;
 }
 
 /**
  * @brief Find a rental item in the database by tag ID
  * 
  * @param[in] p_tag_id Tag ID to search for
  * @param[in] tag_id_length Length of tag ID
  * @param[out] p_index Index in database where item was found
  * @return true if item was found, false otherwise
  */
 static bool find_item_by_tag_id(const uint8_t * p_tag_id, uint8_t tag_id_length, uint8_t * p_index)
 {
     if (p_tag_id == NULL || tag_id_length == 0 || p_index == NULL)
     {
         return false;
     }
     
     for (uint8_t i = 0; i < m_num_items; i++)
     {
         if (m_rental_database[i].tag_id_length == tag_id_length &&
             memcmp(m_rental_database[i].tag_id, p_tag_id, tag_id_length) == 0)
         {
             *p_index = i;
             return true;
         }
     }
     
     return false;
 }
 
 /**
  * @brief Find a rental item in the database by item ID
  * 
  * @param[in] item_id Item ID to search for
  * @param[out] p_index Index in database where item was found
  * @return true if item was found, false otherwise
  */
 static bool find_item_by_item_id(uint32_t item_id, uint8_t * p_index)
 {
     if (p_index == NULL)
     {
         return false;
     }
     
     for (uint8_t i = 0; i < m_num_items; i++)
     {
         if (m_rental_database[i].item_id == item_id)
         {
             *p_index = i;
             return true;
         }
     }
     
     return false;
 }
 
 /**
  * @brief Determine the type of rental operation
  * 
  * @param[in] p_item Rental item to analyze
  * @return Type of rental operation
  */
 rental_operation_type_t rental_data_determine_operation(const rental_item_t * p_item)
 {
     // If item ID is 0, it's a status query
     if (p_item->item_id == 0)
     {
         return RENTAL_OP_STATUS;
     }
     
     // If timestamp is 0, it's a check-in
     if (p_item->timestamp == 0 && p_item->status == RENTAL_STATUS_AVAILABLE)
     {
         return RENTAL_OP_CHECKIN;
     }
     
     // If timestamp is non-zero, it's a checkout
     if (p_item->timestamp != 0 && p_item->status == RENTAL_STATUS_RENTED)
     {
         return RENTAL_OP_CHECKOUT;
     }
     
     // Otherwise, it's unknown
     return RENTAL_OP_UNKNOWN;
 }
 
 /**
  * @brief Process a rental checkout operation
  * 
  * @param[in,out] p_item Rental item to process (will be updated with current data)
  * @return true if checkout was successful, false otherwise
  */
 bool rental_data_process_checkout(rental_item_t * p_item)
 {
     uint8_t index;
     
     if (p_item == NULL)
     {
         return false;
     }
     
     // Find the item in the database
     bool found = false;
     
     // Try to find by tag ID first
     if (p_item->tag_id_length > 0)
     {
         found = find_item_by_tag_id(p_item->tag_id, p_item->tag_id_length, &index);
     }
     
     // If not found by tag ID, try by item ID
     if (!found && p_item->item_id != 0)
     {
         found = find_item_by_item_id(p_item->item_id, &index);
     }
     
     if (!found)
     {
         NRF_LOG_WARNING("Item not found in database for checkout");
         return false;
     }
     
     // Check if item is available
     if (m_rental_database[index].status != RENTAL_STATUS_AVAILABLE)
     {
         NRF_LOG_WARNING("Item is not available for checkout (status: %d)", m_rental_database[index].status);
         
         // Update the provided item with current data
         *p_item = m_rental_database[index];
         return false;
     }
     
     // Process checkout
     m_rental_database[index].status = RENTAL_STATUS_RENTED;
     m_rental_database[index].timestamp = m_current_time;
     
     // Update the provided item with the current data
     *p_item = m_rental_database[index];
     
     // Save the updated item to storage
     storage_manager_save_rental_item(&m_rental_database[index]);
     
     NRF_LOG_INFO("Item %d successfully checked out", p_item->item_id);
     return true;
 }
 
 /**
  * @brief Process a rental checkin operation
  * 
  * @param[in,out] p_item Rental item to process (will be updated with current data)
  * @return true if checkin was successful, false otherwise
  */
 bool rental_data_process_checkin(rental_item_t * p_item)
 {
     uint8_t index;
     
     if (p_item == NULL)
     {
         return false;
     }
     
     // Find the item in the database
     bool found = false;
     
     // Try to find by tag ID first
     if (p_item->tag_id_length > 0)
     {
         found = find_item_by_tag_id(p_item->tag_id, p_item->tag_id_length, &index);
     }
     
     // If not found by tag ID, try by item ID
     if (!found && p_item->item_id != 0)
     {
         found = find_item_by_item_id(p_item->item_id, &index);
     }
     
     if (!found)
     {
         NRF_LOG_WARNING("Item not found in database for checkin");
         return false;
     }
     
     // Check if item is currently rented
     if (m_rental_database[index].status != RENTAL_STATUS_RENTED)
     {
         NRF_LOG_WARNING("Item is not currently rented (status: %d)", m_rental_database[index].status);
         
         // Update the provided item with current data
         *p_item = m_rental_database[index];
         return false;
     }
     
     // Process checkin
     m_rental_database[index].status = RENTAL_STATUS_AVAILABLE;
     m_rental_database[index].timestamp = 0;
     
     // Update the provided item with the current data
     *p_item = m_rental_database[index];
     
     // Save the updated item to storage
     storage_manager_save_rental_item(&m_rental_database[index]);
     
     NRF_LOG_INFO("Item %d successfully checked in", p_item->item_id);
     return true;
 }
 
 /**
  * @brief Get the status of a rental item
  * 
  * @param[in,out] p_item Rental item to query (will be updated with current data)
  * @return true if status retrieved successfully, false otherwise
  */
 bool rental_data_get_status(rental_item_t * p_item)
 {
     uint8_t index;
     
     if (p_item == NULL)
     {
         return false;
     }
     
     // Find the item in the database
     bool found = false;
     
     // Try to find by tag ID first
     if (p_item->tag_id_length > 0)
     {
         found = find_item_by_tag_id(p_item->tag_id, p_item->tag_id_length, &index);
     }
     
     // If not found by tag ID, try by item ID
     if (!found && p_item->item_id != 0)
     {
         found = find_item_by_item_id(p_item->item_id, &index);
     }
     
     if (!found)
     {
         NRF_LOG_WARNING("Item not found in database for status query");
         return false;
     }
     
     // Return the current item data
     *p_item = m_rental_database[index];
     
     // If item is rented, check if it's overdue
     if (m_rental_database[index].status == RENTAL_STATUS_RENTED)
     {
         uint32_t rental_end_time = m_rental_database[index].timestamp + 
                                   (m_rental_database[index].rental_duration * 3600); // Convert hours to seconds
         
         if (m_current_time > rental_end_time)
         {
             NRF_LOG_INFO("Item %d is overdue", p_item->item_id);
             // We could set a flag in the item to indicate it's overdue
             // For now, just log it
         }
     }
     
     NRF_LOG_INFO("Item %d status retrieved: %d", p_item->item_id, p_item->status);
     return true;
 }
 
 /**
  * @brief Add a new rental item to the database
  * 
  * @param[in] p_item New rental item to add
  * @return true if item was added successfully, false otherwise
  */
 bool rental_data_add_item(const rental_item_t * p_item)
 {
     uint8_t index;
     
     if (p_item == NULL)
     {
         return false;
     }
     
     // Check if database is full
     if (m_num_items >= MAX_RENTAL_ITEMS)
     {
         NRF_LOG_WARNING("Rental database is full");
         return false;
     }
     
     // Check if item already exists by tag ID
     if (p_item->tag_id_length > 0 && 
         find_item_by_tag_id(p_item->tag_id, p_item->tag_id_length, &index))
     {
         NRF_LOG_WARNING("Item with same tag ID already exists");
         return false;
     }
     
     // Check if item already exists by item ID
     if (p_item->item_id != 0 && find_item_by_item_id(p_item->item_id, &index))
     {
         NRF_LOG_WARNING("Item with same item ID already exists");
         return false;
     }
     
     // Add the new item
     m_rental_database[m_num_items] = *p_item;
     m_num_items++;
     
     // Save the new item to storage
     storage_manager_save_rental_item(p_item);
     
     NRF_LOG_INFO("New item added to database: ID %d", p_item->item_id);
     return true;
 }
 
 /**
  * @brief Remove a rental item from the database
  * 
  * @param[in] item_id ID of item to remove
  * @return true if item was removed successfully, false otherwise
  */
 bool rental_data_remove_item(uint32_t item_id)
 {
     uint8_t index;
     
     // Find item by ID
     if (!find_item_by_item_id(item_id, &index))
     {
         NRF_LOG_WARNING("Item not found for removal");
         return false;
     }
     
     // Remove the item by shifting the array
     for (uint8_t i = index; i < m_num_items - 1; i++)
     {
         m_rental_database[i] = m_rental_database[i + 1];
     }
     
     m_num_items--;
     
     // Update storage (in a real system, this would involve marking the item as deleted)
     storage_manager_remove_rental_item(item_id);
     
     NRF_LOG_INFO("Item %d removed from database", item_id);
     return true;
 }
 
 /**
  * @brief Check for overdue rental items
  * 
  * @param[out] p_items Array to receive overdue items
  * @param[in] max_items Maximum number of items the array can hold
  * @param[out] p_num_items Number of overdue items found
  * @return true if operation was successful, false otherwise
  */
 bool rental_data_check_overdue(rental_item_t * p_items, uint8_t max_items, uint8_t * p_num_items)
 {
     if (p_items == NULL || p_num_items == NULL || max_items == 0)
     {
         return false;
     }
     
     uint8_t count = 0;
     
     for (uint8_t i = 0; i < m_num_items && count < max_items; i++)
     {
         // Check if the item is rented and overdue
         if (m_rental_database[i].status == RENTAL_STATUS_RENTED)
         {
             uint32_t rental_end_time = m_rental_database[i].timestamp + 
                                       (m_rental_database[i].rental_duration * 3600); // Convert hours to seconds
             
             if (m_current_time > rental_end_time)
             {
                 // This item is overdue
                 p_items[count] = m_rental_database[i];
                 count++;
             }
         }
     }
     
     *p_num_items = count;
     NRF_LOG_INFO("Found %d overdue items", count);
     return true;
 }