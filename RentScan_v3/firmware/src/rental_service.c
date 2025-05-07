/**
 * @file rental_service.c
 * @brief Implementation of the BLE Rental Service
 *
 * This service handles rental item data communication over BLE
 */

 #include <string.h>
 #include "sdk_common.h"
 #include "ble_srv_common.h"
 #include "app_error.h"
 #include "nrf_log.h"
 #include "rental_service.h"
 
 // Define UUIDs for the service and characteristics
 // Custom UUIDs - create your own with https://www.uuidgenerator.net/
 #define BLE_UUID_RENTAL_SERVICE_BASE_UUID  {{0x23, 0xD1, 0x13, 0xEF, 0x5F, 0x78, 0x23, 0x15, 0xDE, 0xEF, 0x12, 0x12, 0x00, 0x00, 0x00, 0x00}} // Base UUID
 #define BLE_UUID_RENTAL_SERVICE_UUID       0xFE01                                                                         // Service UUID
 #define BLE_UUID_RENTAL_ITEM_CHAR_UUID     0xFE02                                                                         // Characteristic UUID
 
 /**@brief Function for handling the Connect event.
  *
  * @param[in]   p_rental_service  Rental Service structure.
  * @param[in]   p_ble_evt         Event received from the BLE stack.
  */
 static void on_connect(ble_rental_service_t * p_rental_service, ble_evt_t const * p_ble_evt)
 {
     p_rental_service->conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
 }
 
 /**@brief Function for handling the Disconnect event.
  *
  * @param[in]   p_rental_service  Rental Service structure.
  * @param[in]   p_ble_evt         Event received from the BLE stack.
  */
 static void on_disconnect(ble_rental_service_t * p_rental_service, ble_evt_t const * p_ble_evt)
 {
     UNUSED_PARAMETER(p_ble_evt);
     p_rental_service->conn_handle = BLE_CONN_HANDLE_INVALID;
 }
 
 /**@brief Function for handling the Write event.
  *
  * @param[in]   p_rental_service  Rental Service structure.
  * @param[in]   p_ble_evt         Event received from the BLE stack.
  */
 static void on_write(ble_rental_service_t * p_rental_service, ble_evt_t const * p_ble_evt)
 {
     ble_gatts_evt_write_t const * p_evt_write = &p_ble_evt->evt.gatts_evt.params.write;
     
     // Check if this is a write to the rental item characteristic
     if (p_evt_write->handle == p_rental_service->rental_item_char_handles.value_handle)
     {
         // Ensure write data is of the correct size
         if (p_evt_write->len == sizeof(rental_item_t))
         {
             rental_item_t received_item;
             memcpy(&received_item, p_evt_write->data, sizeof(rental_item_t));
             
             // Call the write handler if it's registered
             if (p_rental_service->write_handler != NULL)
             {
                 p_rental_service->write_handler(p_ble_evt->evt.gap_evt.conn_handle, p_rental_service, &received_item);
             }
         }
     }
     // Check if this is a CCCD write
     else if (p_evt_write->handle == p_rental_service->rental_item_char_handles.cccd_handle)
     {
         // Check if notifications are enabled
         if (p_evt_write->len == 2)
         {
             ble_rental_evt_t evt;
             
             if (ble_srv_is_notification_enabled(p_evt_write->data))
             {
                 evt.evt_type = BLE_RENTAL_EVT_NOTIFICATION_ENABLED;
             }
             else
             {
                 evt.evt_type = BLE_RENTAL_EVT_NOTIFICATION_DISABLED;
             }
             
             // Call the event handler if it's registered
             if (p_rental_service->evt_handler != NULL)
             {
                 p_rental_service->evt_handler(&evt);
             }
         }
     }
 }
 
 /**@brief Function for handling BLE events.
  *
  * @param[in]   p_ble_evt   Event received from the BLE stack.
  * @param[in]   p_context   Rental Service structure.
  */
 static void on_ble_evt(ble_evt_t const * p_ble_evt, void * p_context)
 {
     ble_rental_service_t * p_rental_service = (ble_rental_service_t *)p_context;
     
     switch (p_ble_evt->header.evt_id)
     {
         case BLE_GAP_EVT_CONNECTED:
             on_connect(p_rental_service, p_ble_evt);
             break;
             
         case BLE_GAP_EVT_DISCONNECTED:
             on_disconnect(p_rental_service, p_ble_evt);
             break;
             
         case BLE_GATTS_EVT_WRITE:
             on_write(p_rental_service, p_ble_evt);
             break;
             
         default:
             // No implementation needed
             break;
     }
 }
 
 /**@brief Function for initializing the Rental Service.
  *
  * @param[out]  p_rental_service  Rental Service structure.
  * @param[in]   p_rental_service_init  Information needed to initialize the service.
  *
  * @return      NRF_SUCCESS on success, otherwise an error code.
  */
 uint32_t ble_rental_service_init(ble_rental_service_t * p_rental_service, const ble_rental_service_init_t * p_rental_service_init)
 {
     ret_code_t err_code;
     ble_uuid_t ble_uuid;
     ble_uuid128_t base_uuid = BLE_UUID_RENTAL_SERVICE_BASE_UUID;
     ble_add_char_params_t add_char_params;
     
     // Initialize service structure
     p_rental_service->evt_handler = p_rental_service_init->evt_handler;
     p_rental_service->write_handler = p_rental_service_init->write_handler;
     p_rental_service->conn_handle = BLE_CONN_HANDLE_INVALID;
     
     // Add the custom base UUID
     err_code = sd_ble_uuid_vs_add(&base_uuid, &p_rental_service->uuid_type);
     VERIFY_SUCCESS(err_code);
     
     // Add the service
     ble_uuid.type = p_rental_service->uuid_type;
     ble_uuid.uuid = BLE_UUID_RENTAL_SERVICE_UUID;
     
     err_code = sd_ble_gatts_service_add(BLE_GATTS_SRVC_TYPE_PRIMARY, &ble_uuid, &p_rental_service->service_handle);
     VERIFY_SUCCESS(err_code);
     
     // Register for BLE events
     err_code = sd_ble_gatts_sys_attr_set(p_rental_service->conn_handle, NULL, 0, 0);
     VERIFY_SUCCESS(err_code);
     
     // Add the rental item characteristic
     memset(&add_char_params, 0, sizeof(add_char_params));
     add_char_params.uuid                 = BLE_UUID_RENTAL_ITEM_CHAR_UUID;
     add_char_params.uuid_type            = p_rental_service->uuid_type;
     add_char_params.max_len              = sizeof(rental_item_t);
     add_char_params.init_len             = sizeof(rental_item_t);
     add_char_params.p_init_value         = NULL; // No initial value
     add_char_params.char_props.read      = 1;
     add_char_params.char_props.write     = 1;
     add_char_params.char_props.notify    = 1;
     add_char_params.read_access          = SEC_OPEN;
     add_char_params.write_access         = SEC_OPEN;
     add_char_params.cccd_write_access    = SEC_OPEN;
     
     err_code = characteristic_add(p_rental_service->service_handle,
                                   &add_char_params,
                                   &p_rental_service->rental_item_char_handles);
     VERIFY_SUCCESS(err_code);
     
     // Register for BLE events
     NRF_SDH_BLE_OBSERVER(m_rental_service_obs, BLE_HRS_BLE_OBSERVER_PRIO, on_ble_evt, p_rental_service);
     
     return NRF_SUCCESS;
 }
 
 /**@brief Function for sending rental item data.
  *
  * @param[in]   p_rental_service  Rental Service structure.
  * @param[in]   p_item            Rental item structure.
  *
  * @return      NRF_SUCCESS on success, otherwise an error code.
  */
 uint32_t ble_rental_service_item_update(ble_rental_service_t * p_rental_service, const rental_item_t * p_item)
 {
     // Check if connected and if notifications are enabled
     if (p_rental_service->conn_handle == BLE_CONN_HANDLE_INVALID)
     {
         return NRF_ERROR_INVALID_STATE;
     }
     
     ble_gatts_hvx_params_t hvx_params;
     uint16_t len = sizeof(rental_item_t);
     
     memset(&hvx_params, 0, sizeof(hvx_params));
     hvx_params.handle = p_rental_service->rental_item_char_handles.value_handle;
     hvx_params.type   = BLE_GATT_HVX_NOTIFICATION;
     hvx_params.offset = 0;
     hvx_params.p_len  = &len;
     hvx_params.p_data = (uint8_t *)p_item;
     
     return sd_ble_gatts_hvx(p_rental_service->conn_handle, &hvx_params);
 }