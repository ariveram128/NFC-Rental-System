/**
 * @file storage_manager.h
 * @brief Declarations for persistent storage operations
 */

 #ifndef STORAGE_MANAGER_H__
 #define STORAGE_MANAGER_H__
 
 #include <stdint.h>
 #include "app_error.h"
 #include "rental_data.h"
 
 /**
  * @brief Initialize the storage manager
  * 
  * @return NRF_SUCCESS if initialization was successful, error code otherwise
  */
 ret_code_t storage_manager_init(void);
 
 /**
  * @brief Save rental items to flash storage
  * 
  * @param[in] p_items Array of rental items to save
  * @param[in] num_items Number of items in the array
  * @return NRF_SUCCESS if saved successfully, error code otherwise
  */
 ret_code_t storage_manager_save_items(const rental_item_t * p_items, uint8_t num_items);
 
 /**
  * @brief Save a single rental item to flash storage
  * 
  * This updates the item in flash if it exists, or adds it if it doesn't.
  * 
  * @param[in] p_item Rental item to save
  * @return NRF_SUCCESS if saved successfully, error code otherwise
  */
 ret_code_t storage_manager_save_rental_item(const rental_item_t * p_item);
 
 /**
  * @brief Remove a rental item from flash storage
  * 
  * @param[in] item_id ID of the item to remove
  * @return NRF_SUCCESS if removed successfully, error code otherwise
  */
 ret_code_t storage_manager_remove_rental_item(uint32_t item_id);
 
 /**
  * @brief Load rental items from flash storage
  * 
  * @param[out] p_items Array to receive the rental items
  * @param[in] max_items Maximum number of items the array can hold
  * @param[out] p_num_items Number of items loaded
  * @return NRF_SUCCESS if loaded successfully, error code otherwise
  */
 ret_code_t storage_manager_load_rental_items(rental_item_t * p_items, uint8_t max_items, uint8_t * p_num_items);
 
 #endif // STORAGE_MANAGER_H__