/**
 * @file rental_data.h
 * @brief Definitions and declarations for rental data operations
 */

 #ifndef RENTAL_DATA_H__
 #define RENTAL_DATA_H__
 
 #include <stdint.h>
 #include <stdbool.h>
 #include "app_error.h"
 
 // Maximum length of tag IDs
 #define NFC_TAG_ID_MAX_LENGTH   16
 #define ITEM_NAME_MAX_LENGTH    32
 
 /**
  * @brief Rental item status values
  */
 typedef enum {
     RENTAL_STATUS_AVAILABLE = 0,   /**< Item is available for rental */
     RENTAL_STATUS_RENTED,          /**< Item is currently rented out */
     RENTAL_STATUS_MAINTENANCE,     /**< Item is under maintenance */
     RENTAL_STATUS_LOST             /**< Item is lost or missing */
 } rental_status_t;
 
 /**
  * @brief Rental operation types
  */
 typedef enum {
     RENTAL_OP_UNKNOWN = 0,         /**< Unknown operation */
     RENTAL_OP_CHECKOUT,            /**< Checkout operation */
     RENTAL_OP_CHECKIN,             /**< Checkin operation */
     RENTAL_OP_STATUS               /**< Status query operation */
 } rental_operation_type_t;
 
 /**
  * @brief Rental item structure
  */
 typedef struct {
     uint32_t item_id;                              /**< Unique item identifier */
     char item_name[ITEM_NAME_MAX_LENGTH];          /**< Name/description of the item */
     rental_status_t status;                        /**< Current rental status */
     uint16_t rental_duration;                      /**< Rental duration in hours */
     uint32_t timestamp;                            /**< Timestamp of checkout (0 if not rented) */
     uint8_t tag_id[NFC_TAG_ID_MAX_LENGTH];         /**< NFC tag ID */
     uint8_t tag_id_length;                         /**< Length of the tag ID */
     uint8_t reserved[3];                           /**< Reserved for alignment */
 } rental_item_t;
 
 /**
  * @brief Initialize the rental data module
  *
  * @return NRF_SUCCESS if successful, error code otherwise
  */
 ret_code_t rental_data_init(void);
 
 /**
  * @brief Update the current system time
  * 
  * @param timestamp New timestamp (seconds since epoch)
  */
 void rental_data_update_time(uint32_t timestamp);
 
 /**
  * @brief Get the current system time
  * 
  * @return Current timestamp (seconds since epoch)
  */
 uint32_t rental_data_get_time(void);
 
 /**
  * @brief Determine the type of rental operation
  * 
  * @param[in] p_item Rental item to analyze
  * @return Type of rental operation
  */
 rental_operation_type_t rental_data_determine_operation(const rental_item_t * p_item);
 
 /**
  * @brief Process a rental checkout operation
  * 
  * @param[in,out] p_item Rental item to process (will be updated with current data)
  * @return true if checkout was successful, false otherwise
  */
 bool rental_data_process_checkout(rental_item_t * p_item);
 
 /**
  * @brief Process a rental checkin operation
  * 
  * @param[in,out] p_item Rental item to process (will be updated with current data)
  * @return true if checkin was successful, false otherwise
  */
 bool rental_data_process_checkin(rental_item_t * p_item);
 
 /**
  * @brief Get the status of a rental item
  * 
  * @param[in,out] p_item Rental item to query (will be updated with current data)
  * @return true if status retrieved successfully, false otherwise
  */
 bool rental_data_get_status(rental_item_t * p_item);
 
 /**
  * @brief Add a new rental item to the database
  * 
  * @param[in] p_item New rental item to add
  * @return true if item was added successfully, false otherwise
  */
 bool rental_data_add_item(const rental_item_t * p_item);
 
 /**
  * @brief Remove a rental item from the database
  * 
  * @param[in] item_id ID of item to remove
  * @return true if item was removed successfully, false otherwise
  */
 bool rental_data_remove_item(uint32_t item_id);
 
 /**
  * @brief Check for overdue rental items
  * 
  * @param[out] p_items Array to receive overdue items
  * @param[in] max_items Maximum number of items the array can hold
  * @param[out] p_num_items Number of overdue items found
  * @return true if operation was successful, false otherwise
  */
 bool rental_data_check_overdue(rental_item_t * p_items, uint8_t max_items, uint8_t * p_num_items);
 
 #endif // RENTAL_DATA_H__