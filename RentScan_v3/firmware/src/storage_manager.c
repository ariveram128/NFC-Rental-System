/**
 * @file storage_manager.c
 * @brief Implementation of persistent storage for rental items
 *
 * This file provides functions for saving and loading rental data to/from
 * non-volatile memory (flash).
 */

 #include <string.h>
 #include "nrf_fstorage.h"
 #include "nrf_fstorage_sd.h"
 #include "nrf_log.h"
 #include "app_error.h"
 #include "rental_data.h"
 #include "storage_manager.h"
 
 // Define storage area in flash memory
 // In a real application, this would be carefully selected to avoid conflicts
 // with other flash usage (bootloader, application, etc.)
 #define STORAGE_START_ADDR        0x70000  // Example address, needs to be adjusted for actual device
 #define STORAGE_PAGE_SIZE         4096     // Flash page size
 #define STORAGE_NUM_PAGES         2        // Number of pages to use
 #define STORAGE_MAX_ITEMS         10       // Maximum number of items to store
 
 // Define storage layout
 typedef struct {
     uint32_t magic;              // Magic number to identify valid data
     uint32_t version;            // Storage format version
     uint8_t num_items;           // Number of items stored
     uint8_t reserved[3];         // Reserved for alignment
 } storage_header_t;
 
 #define STORAGE_MAGIC_NUMBER     0xABCD1234
 #define STORAGE_VERSION          1
 
 // Fstorage instance
 NRF_FSTORAGE_DEF(nrf_fstorage_t fstorage) =
 {
     .evt_handler = fstorage_evt_handler,
     .start_addr = STORAGE_START_ADDR,
     .end_addr   = STORAGE_START_ADDR + (STORAGE_PAGE_SIZE * STORAGE_NUM_PAGES),
 };
 
 // Flag indicating if storage operation is in progress
 static bool m_storage_busy = false;
 
 // Forward declarations
 static void fstorage_evt_handler(nrf_fstorage_evt_t * p_evt);
 
 /**
  * @brief Initialize the storage manager
  * 
  * @return NRF_SUCCESS if initialization was successful, error code otherwise
  */
 ret_code_t storage_manager_init(void)
 {
     ret_code_t err_code;
     
     // Initialize fstorage
     err_code = nrf_fstorage_init(&fstorage, &nrf_fstorage_sd, NULL);
     if (err_code != NRF_SUCCESS)
     {
         NRF_LOG_ERROR("Failed to initialize fstorage: %d", err_code);
         return err_code;
     }
     
     NRF_LOG_INFO("Storage manager initialized");
     return NRF_SUCCESS;
 }
 
 /**
  * @brief Event handler for fstorage events
  * 
  * @param p_evt Fstorage event
  */
 static void fstorage_evt_handler(nrf_fstorage_evt_t * p_evt)
 {
     if (p_evt->result != NRF_SUCCESS)
     {
         NRF_LOG_ERROR("fstorage operation failed: %d", p_evt->result);
     }
     else
     {
         NRF_LOG_INFO("fstorage operation completed");
     }
     
     // Operation completed, clear busy flag
     m_storage_busy = false;
 }
 
 /**
  * @brief Wait for any ongoing fstorage operations to complete
  */
 static void wait_for_fstorage(void)
 {
     // Simple busy-wait for any ongoing operations
     // In a real application, this should be more sophisticated
     // using events and possibly power management
     while (m_storage_busy)
     {
         // Wait for fstorage operation to complete
     }
 }
 
 /**
  * @brief Erase storage area
  * 
  * @return NRF_SUCCESS if erased successfully, error code otherwise
  */
 static ret_code_t erase_storage(void)
 {
     ret_code_t err_code;
     
     // Check if storage is busy
     if (m_storage_busy)
     {
         return NRF_ERROR_BUSY;
     }
     
     // Set busy flag
     m_storage_busy = true;
     
     // Erase storage pages
     for (uint32_t addr = STORAGE_START_ADDR; addr < STORAGE_START_ADDR + (STORAGE_PAGE_SIZE * STORAGE_NUM_PAGES); addr += STORAGE_PAGE_SIZE)
     {
         err_code = nrf_fstorage_erase(&fstorage, addr, 1, NULL);
         if (err_code != NRF_SUCCESS)
         {
             NRF_LOG_ERROR("Failed to erase storage at address 0x%x: %d", addr, err_code);
             m_storage_busy = false;
             return err_code;
         }
         
         // Wait for erase to complete
         wait_for_fstorage();
         
         // Set busy flag for next operation
         m_storage_busy = true;
     }
     
     m_storage_busy = false;
     NRF_LOG_INFO("Storage erased");
     return NRF_SUCCESS;
 }
 
 /**
  * @brief Save rental items to flash storage
  * 
  * @param[in] p_items Array of rental items to save
  * @param[in] num_items Number of items in the array
  * @return NRF_SUCCESS if saved successfully, error code otherwise
  */
 ret_code_t storage_manager_save_items(const rental_item_t * p_items, uint8_t num_items)
 {
     ret_code_t err_code;
     
     if (p_items == NULL || num_items == 0 || num_items > STORAGE_MAX_ITEMS)
     {
         return NRF_ERROR_INVALID_PARAM;
     }
     
     // Check if storage is busy
     if (m_storage_busy)
     {
         return NRF_ERROR_BUSY;
     }
     
     // Erase storage first
     err_code = erase_storage();
     if (err_code != NRF_SUCCESS)
     {
         return err_code;
     }
     
     // Prepare header
     storage_header_t header;
     header.magic = STORAGE_MAGIC_NUMBER;
     header.version = STORAGE_VERSION;
     header.num_items = num_items;
     memset(header.reserved, 0, sizeof(header.reserved));
     
     // Set busy flag
     m_storage_busy = true;
     
     // Write header
     err_code = nrf_fstorage_write(&fstorage, STORAGE_START_ADDR, &header, sizeof(header), NULL);
     if (err_code != NRF_SUCCESS)
     {
         NRF_LOG_ERROR("Failed to write storage header: %d", err_code);
         m_storage_busy = false;
         return err_code;
     }
     
     // Wait for write to complete
     wait_for_fstorage();
     
     // Write items
     uint32_t addr = STORAGE_START_ADDR + sizeof(header);
     for (uint8_t i = 0; i < num_items; i++)
     {
         // Set busy flag
         m_storage_busy = true;
         
         // Write the item
         err_code = nrf_fstorage_write(&fstorage, addr, &p_items[i], sizeof(rental_item_t), NULL);
         if (err_code != NRF_SUCCESS)
         {
             NRF_LOG_ERROR("Failed to write item %d: %d", i, err_code);
             m_storage_busy = false;
             return err_code;
         }
         
         // Wait for write to complete
         wait_for_fstorage();
         
         // Update address for next item
         addr += sizeof(rental_item_t);
     }
     
     NRF_LOG_INFO("Saved %d items to storage", num_items);
     return NRF_SUCCESS;
 }
 
 /**
  * @brief Save a single rental item to flash storage
  * 
  * This updates the item in flash if it exists, or adds it if it doesn't.
  * 
  * @param[in] p_item Rental item to save
  * @return NRF_SUCCESS if saved successfully, error code otherwise
  */
 ret_code_t storage_manager_save_rental_item(const rental_item_t * p_item)
 {
     ret_code_t err_code;
     rental_item_t items[STORAGE_MAX_ITEMS];
     uint8_t num_items = 0;
     bool item_exists = false;
     
     if (p_item == NULL)
     {
         return NRF_ERROR_INVALID_PARAM;
     }
     
     // Load existing items
     err_code = storage_manager_load_rental_items(items, STORAGE_MAX_ITEMS, &num_items);
     if (err_code != NRF_SUCCESS)
     {
         // If there's an error loading items (e.g., no valid data yet),
         // just start with an empty list
         num_items = 0;
     }
     
     // Check if the item already exists
     for (uint8_t i = 0; i < num_items; i++)
     {
         if (items[i].item_id == p_item->item_id)
         {
             // Update the existing item
             items[i] = *p_item;
             item_exists = true;
             break;
         }
     }
     
     // If the item doesn't exist, add it
     if (!item_exists && num_items < STORAGE_MAX_ITEMS)
     {
         items[num_items] = *p_item;
         num_items++;
     }
     else if (!item_exists)
     {
         // No space for new item
         return NRF_ERROR_NO_MEM;
     }
     
     // Save all items
     return storage_manager_save_items(items, num_items);
 }
 
 /**
  * @brief Remove a rental item from flash storage
  * 
  * @param[in] item_id ID of the item to remove
  * @return NRF_SUCCESS if removed successfully, error code otherwise
  */
 ret_code_t storage_manager_remove_rental_item(uint32_t item_id)
 {
     ret_code_t err_code;
     rental_item_t items[STORAGE_MAX_ITEMS];
     uint8_t num_items = 0;
     bool item_found = false;
     
     // Load existing items
     err_code = storage_manager_load_rental_items(items, STORAGE_MAX_ITEMS, &num_items);
     if (err_code != NRF_SUCCESS)
     {
         return err_code;
     }
     
     // Find and remove the item
     for (uint8_t i = 0; i < num_items; i++)
     {
         if (items[i].item_id == item_id)
         {
             // Remove the item by shifting remaining items
             for (uint8_t j = i; j < num_items - 1; j++)
             {
                 items[j] = items[j + 1];
             }
             num_items--;
             item_found = true;
             break;
         }
     }
     
     if (!item_found)
     {
         return NRF_ERROR_NOT_FOUND;
     }
     
     // Save the updated item list
     return storage_manager_save_items(items, num_items);
 }
 
 /**
  * @brief Load rental items from flash storage
  * 
  * @param[out] p_items Array to receive the rental items
  * @param[in] max_items Maximum number of items the array can hold
  * @param[out] p_num_items Number of items loaded
  * @return NRF_SUCCESS if loaded successfully, error code otherwise
  */
 ret_code_t storage_manager_load_rental_items(rental_item_t * p_items, uint8_t max_items, uint8_t * p_num_items)
 {
     storage_header_t header;
     
     if (p_items == NULL || p_num_items == NULL || max_items == 0)
     {
         return NRF_ERROR_INVALID_PARAM;
     }
     
     // Read header
     nrf_fstorage_read(&fstorage, STORAGE_START_ADDR, &header, sizeof(header));
     
     // Check if the data is valid
     if (header.magic != STORAGE_MAGIC_NUMBER || header.version != STORAGE_VERSION)
     {
         NRF_LOG_WARNING("No valid data found in storage");
         *p_num_items = 0;
         return NRF_ERROR_NOT_FOUND;
     }
     
     // Check if we have enough space for all items
     if (header.num_items > max_items)
     {
         NRF_LOG_WARNING("Not enough space to load all items (%d > %d)", header.num_items, max_items);
         *p_num_items = max_items;
     }
     else
     {
         *p_num_items = header.num_items;
     }
     
     // Read items
     uint32_t addr = STORAGE_START_ADDR + sizeof(header);
     for (uint8_t i = 0; i < *p_num_items; i++)
     {
         nrf_fstorage_read(&fstorage, addr, &p_items[i], sizeof(rental_item_t));
         addr += sizeof(rental_item_t);
     }
     
     NRF_LOG_INFO("Loaded %d items from storage", *p_num_items);
     return NRF_SUCCESS;
 }