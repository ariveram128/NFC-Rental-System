/**
 * @file rental_service.h
 * @brief BLE Rental Service declarations
 */

 #ifndef RENTAL_SERVICE_H__
 #define RENTAL_SERVICE_H__
 
 #include <stdint.h>
 #include <stdbool.h>
 #include "ble.h"
 #include "ble_srv_common.h"
 #include "rental_data.h"
 
 /**
  * @brief Rental service event types
  */
 typedef enum
 {
     BLE_RENTAL_EVT_NOTIFICATION_ENABLED,  /**< Rental value notification enabled */
     BLE_RENTAL_EVT_NOTIFICATION_DISABLED, /**< Rental value notification disabled */
 } ble_rental_evt_type_t;
 
 /**
  * @brief Rental service event
  */
 typedef struct
 {
     ble_rental_evt_type_t evt_type;       /**< Type of event */
 } ble_rental_evt_t;
 
 // Forward declaration of the ble_rental_service_t type
 typedef struct ble_rental_service_s ble_rental_service_t;
 
 /**
  * @brief Rental Service event handler type
  */
 typedef void (*ble_rental_evt_handler_t) (ble_rental_evt_t * p_evt);
 
 /**
  * @brief Rental Service write handler type
  */
 typedef void (*ble_rental_write_handler_t) (uint16_t conn_handle, ble_rental_service_t * p_rental_service, const rental_item_t * p_item);
 
 /**
  * @brief Rental Service initialization structure
  */
 typedef struct
 {
     ble_rental_evt_handler_t evt_handler;    /**< Event handler to be called for handling events in the Rental Service */
     ble_rental_write_handler_t write_handler; /**< Write handler to be called when rental data is received */
 } ble_rental_service_init_t;
 
 /**
  * @brief Rental Service structure
  */
 struct ble_rental_service_s
 {
     uint16_t service_handle;                   /**< Handle of Rental Service (as provided by the BLE stack). */
     ble_gatts_char_handles_t rental_item_char_handles; /**< Handles related to the Rental Item characteristic. */
     uint8_t uuid_type;                         /**< UUID type for the Rental Service. */
     uint16_t conn_handle;                      /**< Handle of the current connection (BLE_CONN_HANDLE_INVALID if not in a connection). */
     ble_rental_evt_handler_t evt_handler;      /**< Event handler to be called for handling events in the Rental Service. */
     ble_rental_write_handler_t write_handler;  /**< Write handler to be called when rental data is received. */
 };
 
 /**
  * @brief Macro for defining a ble_rental_service instance
  */
 #define BLE_RENTAL_SERVICE_DEF(_name)                                                \
 static ble_rental_service_t _name;                                                   \
 NRF_SDH_BLE_OBSERVER(_name ## _obs,                                                  \
                      BLE_HRS_BLE_OBSERVER_PRIO,                                      \
                      NULL, NULL)
 
 /**
  * @brief Function for initializing the Rental Service
  *
  * @param[out] p_rental_service  Rental Service structure
  * @param[in]  p_rental_service_init  Information needed to initialize the service
  *
  * @return     NRF_SUCCESS on success, otherwise an error code
  */
 uint32_t ble_rental_service_init(ble_rental_service_t * p_rental_service, const ble_rental_service_init_t * p_rental_service_init);
 
 /**
  * @brief Function for sending rental item data
  *
  * @param[in] p_rental_service  Rental Service structure
  * @param[in] p_item            Rental item structure
  *
  * @return    NRF_SUCCESS on success, otherwise an error code
  */
 uint32_t ble_rental_service_item_update(ble_rental_service_t * p_rental_service, const rental_item_t * p_item);
 
 #endif // RENTAL_SERVICE_H__