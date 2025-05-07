/**
 * @file main.c
 * @brief RentScan - Wireless NFC Tag Rental System (nRF52840 Edition)
 *
 * This file contains the main application for the RentScan system.
 * It initializes the NFC reader, BLE communication, and handles the
 * rental management logic.
 *
 * Author: Generated Code Example
 */

 #include <stdint.h>
 #include <stdbool.h>
 #include <string.h>
 #include "nordic_common.h"
 #include "nrf.h"
 #include "app_error.h"
 #include "app_timer.h"
 #include "app_button.h"
 #include "nrf_pwr_mgmt.h"
 #include "nrf_log.h"
 #include "nrf_log_ctrl.h"
 #include "nrf_log_default_backends.h"
 #include <zephyr/kernel.h>
 
 // BLE related includes
 #include "ble_advdata.h"
 #include "ble_advertising.h"
 #include "ble_conn_params.h"
 #include "ble_hci.h"
 #include "ble_srv_common.h"
 #include "ble_dis.h"
 #include "nrf_sdh.h"
 #include "nrf_sdh_soc.h"
 #include "nrf_sdh_ble.h"
 #include "nrf_ble_gatt.h"
 #include "nrf_ble_qwr.h"
 
 // NFC related includes
 #include "nfc_t4t_lib.h"
 #include "nfc_ndef_msg.h"
 #include "nfc_ndef_record.h"
 #include "nfc_ndef_text_rec.h"
 #include "nfc_t4t_ndef_file.h"
 
 // Custom includes
 #include "rental_service.h"
 #include "rental_data.h"
 #include "nfc_handler.h"
 #include "storage_manager.h"
 
 // Macros and definitions
 #define APP_BLE_CONN_CFG_TAG    1                                  /**< Tag identifying the SoftDevice BLE configuration. */
 #define APP_BLE_OBSERVER_PRIO   3                                  /**< BLE observer priority of the application. */
 #define APP_ADV_INTERVAL        300                                /**< The advertising interval (in units of 0.625 ms). */
 #define APP_ADV_DURATION        18000                              /**< The advertising duration (180 seconds) in units of 10 milliseconds. */
 
 #define MIN_CONN_INTERVAL       MSEC_TO_UNITS(100, UNIT_1_25_MS)   /**< Minimum acceptable connection interval (0.1 seconds). */
 #define MAX_CONN_INTERVAL       MSEC_TO_UNITS(200, UNIT_1_25_MS)   /**< Maximum acceptable connection interval (0.2 seconds). */
 #define SLAVE_LATENCY           0                                  /**< Slave latency. */
 #define CONN_SUP_TIMEOUT        MSEC_TO_UNITS(4000, UNIT_10_MS)    /**< Connection supervisory time-out (4 seconds). */
 
 #define FIRST_CONN_PARAMS_UPDATE_DELAY  APP_TIMER_TICKS(5000)      /**< Time from initiating event (connect or start of notification) to first time sd_ble_gap_conn_param_update is called (5 seconds). */
 #define NEXT_CONN_PARAMS_UPDATE_DELAY   APP_TIMER_TICKS(30000)     /**< Time between each call to sd_ble_gap_conn_param_update after the first call (30 seconds). */
 #define MAX_CONN_PARAMS_UPDATE_COUNT    3                          /**< Number of attempts before giving up the connection parameter negotiation. */
 
 #define BUTTON_DETECTION_DELAY          APP_TIMER_TICKS(50)        /**< Delay from a GPIOTE event until a button is reported as pushed (in number of timer ticks). */
 
 // Timers
 APP_TIMER_DEF(m_scan_timer_id);                                    /**< Timer for periodic NFC scanning. */
 #define NFC_SCAN_INTERVAL               APP_TIMER_TICKS(1000)      /**< NFC scan interval (value of 1000 gives approximately 1 second). */
 
 // BLE related variables
 NRF_BLE_GATT_DEF(m_gatt);                                          /**< GATT module instance. */
 NRF_BLE_QWR_DEF(m_qwr);                                            /**< Context for the Queued Write module. */
 BLE_ADVERTISING_DEF(m_advertising);                                /**< Advertising module instance. */
 BLE_RENTAL_SERVICE_DEF(m_rental_service);                          /**< Rental Service instance. */
 
 static uint16_t m_conn_handle = BLE_CONN_HANDLE_INVALID;           /**< Handle of the current connection. */
 static uint8_t m_adv_handle = BLE_GAP_ADV_SET_HANDLE_NOT_SET;      /**< Advertising handle used to identify an advertising set. */
 static uint8_t m_enc_advdata[BLE_GAP_ADV_SET_DATA_SIZE_MAX];       /**< Buffer for storing an encoded advertising set. */
 
 // NFC related variables
 static uint8_t m_ndef_msg_buf[NFC_NDEF_MSG_BUF_SIZE];              /**< Buffer for NDEF message. */
 static nfc_ndef_record_desc_t m_ndef_record;                       /**< NDEF record descriptor. */
 static uint32_t m_ndef_msg_len;                                    /**< Length of the NDEF message. */
 
 // Application variables
 static rental_item_t m_current_item;                               /**< Current rental item being processed. */
 static bool m_nfc_field_active = false;                            /**< Flag indicating if NFC field is active. */
 
 // Function prototypes
 static void log_init(void);
 static void timer_init(void);
 static void buttons_init(void);
 static void power_management_init(void);
 static void ble_stack_init(void);
 static void gap_params_init(void);
 static void gatt_init(void);
 static void advertising_init(void);
 static void services_init(void);
 static void conn_params_init(void);
 static void advertising_start(void);
 static void nfc_init(void);
 static void nfc_callback(void * p_context, nfc_t4t_event_t event, const uint8_t * p_data, size_t data_length);
 static void scan_timer_handler(void * p_context);
 static void on_rental_write(uint16_t conn_handle, ble_rental_service_t * p_rental_service, const rental_item_t * p_item);
 static void on_rental_evt(ble_rental_evt_t * p_evt);
 
 /**@brief Function for initializing the logging module. */
 static void log_init(void)
 {
     ret_code_t err_code = NRF_LOG_INIT(NULL);
     APP_ERROR_CHECK(err_code);
 
     NRF_LOG_DEFAULT_BACKENDS_INIT();
     NRF_LOG_INFO("RentScan NFC Tag Rental System started.");
 }
 
 /**@brief Function for initializing the timer module. */
 static void timer_init(void)
 {
     ret_code_t err_code = app_timer_init();
     APP_ERROR_CHECK(err_code);
 
     // Create timers
     err_code = app_timer_create(&m_scan_timer_id, APP_TIMER_MODE_REPEATED, scan_timer_handler);
     APP_ERROR_CHECK(err_code);
 }
 
 /**@brief Function for initializing the button module. */
 static void buttons_init(void)
 {
     ret_code_t err_code;
 
     // Configure application button
     static app_button_cfg_t app_buttons[] =
     {
         {BUTTON_1, false, BUTTON_PULL, NULL},
         {BUTTON_2, false, BUTTON_PULL, NULL}
     };
 
     err_code = app_button_init(app_buttons, ARRAY_SIZE(app_buttons), BUTTON_DETECTION_DELAY);
     APP_ERROR_CHECK(err_code);
 
     err_code = app_button_enable();
     APP_ERROR_CHECK(err_code);
 }
 
 /**@brief Function for initializing the power management module. */
 static void power_management_init(void)
 {
     ret_code_t err_code = nrf_pwr_mgmt_init();
     APP_ERROR_CHECK(err_code);
 }
 
 /**@brief Function for initializing the BLE stack. */
 static void ble_stack_init(void)
 {
     ret_code_t err_code;
 
     err_code = nrf_sdh_enable_request();
     APP_ERROR_CHECK(err_code);
 
     // Configure the BLE stack using the default settings.
     uint32_t ram_start = 0;
     err_code = nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start);
     APP_ERROR_CHECK(err_code);
 
     // Enable BLE stack.
     err_code = nrf_sdh_ble_enable(&ram_start);
     APP_ERROR_CHECK(err_code);
 
     // Register a handler for BLE events.
     NRF_SDH_BLE_OBSERVER(m_ble_observer, APP_BLE_OBSERVER_PRIO, NULL, NULL);
 }
 
 /**@brief Function for initializing the GAP parameters. */
 static void gap_params_init(void)
 {
     ret_code_t              err_code;
     ble_gap_conn_params_t   gap_conn_params;
     ble_gap_conn_sec_mode_t sec_mode;
 
     BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);
 
     // Set GAP device name
     err_code = sd_ble_gap_device_name_set(&sec_mode, (const uint8_t *)"RentScanNRF", strlen("RentScanNRF"));
     APP_ERROR_CHECK(err_code);
 
     memset(&gap_conn_params, 0, sizeof(gap_conn_params));
 
     gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
     gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
     gap_conn_params.slave_latency     = SLAVE_LATENCY;
     gap_conn_params.conn_sup_timeout  = CONN_SUP_TIMEOUT;
 
     err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
     APP_ERROR_CHECK(err_code);
 }
 
 /**@brief Function for initializing the GATT module. */
 static void gatt_init(void)
 {
     ret_code_t err_code = nrf_ble_gatt_init(&m_gatt, NULL);
     APP_ERROR_CHECK(err_code);
 }
 
 /**@brief Function for initializing the Advertising functionality. */
 static void advertising_init(void)
 {
     ret_code_t             err_code;
     ble_advertising_init_t init;
 
     memset(&init, 0, sizeof(init));
 
     init.advdata.name_type               = BLE_ADVDATA_FULL_NAME;
     init.advdata.include_appearance      = true;
     init.advdata.flags                   = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;
     init.advdata.uuids_complete.uuid_cnt = 0;
     init.advdata.uuids_complete.p_uuids  = NULL;
 
     init.config.ble_adv_fast_enabled  = true;
     init.config.ble_adv_fast_interval = APP_ADV_INTERVAL;
     init.config.ble_adv_fast_timeout  = APP_ADV_DURATION;
 
     init.evt_handler = NULL;
 
     err_code = ble_advertising_init(&m_advertising, &init);
     APP_ERROR_CHECK(err_code);
 
     ble_advertising_conn_cfg_tag_set(&m_advertising, APP_BLE_CONN_CFG_TAG);
 }
 
 /**@brief Function for initializing services that will be used by the application. */
 static void services_init(void)
 {
     ret_code_t err_code;
     ble_rental_service_init_t rental_init;
     ble_dis_init_t dis_init;
     nrf_ble_qwr_init_t qwr_init = {0};
 
     // Initialize Queued Write Module
     qwr_init.error_handler = NULL;
     err_code = nrf_ble_qwr_init(&m_qwr, &qwr_init);
     APP_ERROR_CHECK(err_code);
 
     // Initialize Device Information Service
     memset(&dis_init, 0, sizeof(dis_init));
     ble_srv_ascii_to_utf8(&dis_init.manufact_name_str, (char *)"RentScan Inc.");
     ble_srv_ascii_to_utf8(&dis_init.model_num_str, (char *)"RentScan-NRF-1.0");
 
     dis_init.dis_char_rd_sec = SEC_OPEN;
     err_code = ble_dis_init(&dis_init);
     APP_ERROR_CHECK(err_code);
 
     // Initialize Rental Service
     memset(&rental_init, 0, sizeof(rental_init));
     rental_init.evt_handler = on_rental_evt;
     rental_init.write_handler = on_rental_write;
 
     err_code = ble_rental_service_init(&m_rental_service, &rental_init);
     APP_ERROR_CHECK(err_code);
 }
 
 /**@brief Function for initializing the Connection Parameters module. */
 static void conn_params_init(void)
 {
     ret_code_t             err_code;
     ble_conn_params_init_t cp_init;
 
     memset(&cp_init, 0, sizeof(cp_init));
 
     cp_init.p_conn_params                  = NULL;
     cp_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
     cp_init.next_conn_params_update_delay  = NEXT_CONN_PARAMS_UPDATE_DELAY;
     cp_init.max_conn_params_update_count   = MAX_CONN_PARAMS_UPDATE_COUNT;
     cp_init.start_on_notify_cccd_handle    = BLE_GATT_HANDLE_INVALID;
     cp_init.disconnect_on_fail             = false;
     cp_init.evt_handler                    = NULL;
     cp_init.error_handler                  = NULL;
 
     err_code = ble_conn_params_init(&cp_init);
     APP_ERROR_CHECK(err_code);
 }
 
 /**@brief Function for starting advertising. */
 static void advertising_start(void)
 {
     ret_code_t err_code = ble_advertising_start(&m_advertising, BLE_ADV_MODE_FAST);
     APP_ERROR_CHECK(err_code);
 }
 
 /**@brief Function for initializing the NFC module. */
 static void nfc_init(void)
 {
     ret_code_t err_code;
 
     /* Initialize the NFC Tag interface module */
     err_code = nfc_t4t_setup(nfc_callback, NULL);
     APP_ERROR_CHECK(err_code);
 
     /* Create an empty NDEF message to be written to the tag */
     uint8_t language[] = {'e', 'n'};
     uint8_t text[] = "RentScan NFC Tag - Tap to rent";
     uint32_t payload_size = sizeof(text) + 1 + sizeof(language);
     uint8_t payload[payload_size];
 
     // Construct NDEF text record
     err_code = nfc_ndef_record_init(&m_ndef_record, TNF_WELL_KNOWN, NFC_RECORD_TYPE_TEXT, strlen("T"), (uint8_t*)"T", payload_size, payload);
     APP_ERROR_CHECK(err_code);
 
     // Create NDEF message
     uint32_t ndef_msg_len = sizeof(m_ndef_msg_buf);
     err_code = nfc_ndef_msg_encode(&m_ndef_record, m_ndef_msg_buf, &ndef_msg_len);
     APP_ERROR_CHECK(err_code);
 
     // Set created message as the NFC payload
     err_code = nfc_t4t_ndef_file_set(m_ndef_msg_buf, ndef_msg_len);
     APP_ERROR_CHECK(err_code);
 
     // Start sensing NFC field
     err_code = nfc_t4t_emulation_start();
     APP_ERROR_CHECK(err_code);
 
     NRF_LOG_INFO("NFC Tag emulation started.");
 }
 
 /**@brief Function for handling NFC events. */
 static void nfc_callback(void * p_context, nfc_t4t_event_t event, const uint8_t * p_data, size_t data_length)
 {
     ret_code_t err_code;
 
     switch (event)
     {
         case NFC_T4T_EVENT_FIELD_ON:
             NRF_LOG_INFO("NFC field detected.");
             m_nfc_field_active = true;
             
             // Turn on LED to indicate NFC field
             bsp_board_led_on(BSP_BOARD_LED_0);
             
             // Attempt to read tag ID
             nfc_handler_read_tag();
             break;
 
         case NFC_T4T_EVENT_FIELD_OFF:
             NRF_LOG_INFO("NFC field lost.");
             m_nfc_field_active = false;
             
             // Turn off LED
             bsp_board_led_off(BSP_BOARD_LED_0);
             break;
 
         case NFC_T4T_EVENT_NDEF_READ:
             NRF_LOG_INFO("NDEF message read.");
             break;
 
         case NFC_T4T_EVENT_NDEF_UPDATED:
             NRF_LOG_INFO("NDEF message updated.");
             
             // Process the updated NDEF message (could contain rental commands)
             if (data_length > 0 && p_data != NULL)
             {
                 // Parse NDEF message and check if it contains rental information
                 rental_item_t parsed_item;
                 if (nfc_handler_parse_rental_data(p_data, data_length, &parsed_item))
                 {
                     // Process rental operation
                     rental_operation_type_t op_type = rental_data_determine_operation(&parsed_item);
                     switch (op_type)
                     {
                         case RENTAL_OP_CHECKOUT:
                             NRF_LOG_INFO("Rental checkout operation detected.");
                             rental_data_process_checkout(&parsed_item);
                             m_current_item = parsed_item;
                             
                             // Notify BLE if connected
                             if (m_conn_handle != BLE_CONN_HANDLE_INVALID)
                             {
                                 ble_rental_service_item_update(&m_rental_service, &parsed_item);
                             }
                             break;
                             
                         case RENTAL_OP_CHECKIN:
                             NRF_LOG_INFO("Rental checkin operation detected.");
                             rental_data_process_checkin(&parsed_item);
                             m_current_item = parsed_item;
                             
                             // Notify BLE if connected
                             if (m_conn_handle != BLE_CONN_HANDLE_INVALID)
                             {
                                 ble_rental_service_item_update(&m_rental_service, &parsed_item);
                             }
                             break;
                             
                         case RENTAL_OP_STATUS:
                             NRF_LOG_INFO("Rental status check operation detected.");
                             rental_data_get_status(&parsed_item);
                             m_current_item = parsed_item;
                             break;
                             
                         default:
                             NRF_LOG_INFO("Unknown rental operation.");
                             break;
                     }
                     
                     // Update tag content to reflect current status
                     nfc_handler_update_tag_content(&m_current_item);
                 }
             }
             break;
 
         default:
             break;
     }
 }
 
 /**@brief Function for handling NFC scan timer timeout. */
 static void scan_timer_handler(void * p_context)
 {
     // If no NFC field is active, we can actively scan for tags
     if (!m_nfc_field_active)
     {
         // Scan for NFC tags (this would be implemented in nfc_handler.c)
         bool tag_found = nfc_handler_scan_for_tag(&m_current_item);
         
         if (tag_found)
         {
             NRF_LOG_INFO("NFC tag found during scan.");
             
             // Process the tag data and send over BLE if connected
             if (m_conn_handle != BLE_CONN_HANDLE_INVALID)
             {
                 ble_rental_service_item_update(&m_rental_service, &m_current_item);
             }
             
             // Turn on LED to indicate tag found
             bsp_board_led_on(BSP_BOARD_LED_1);
         }
         else
         {
             // Turn off LED if no tag found
             bsp_board_led_off(BSP_BOARD_LED_1);
         }
     }
 }
 
 /**@brief Function for handling rental service write events. */
 static void on_rental_write(uint16_t conn_handle, ble_rental_service_t * p_rental_service, const rental_item_t * p_item)
 {
     NRF_LOG_INFO("Rental data received over BLE.");
     
     // Process the rental data
     m_current_item = *p_item;
     
     // Determine operation and process
     rental_operation_type_t op_type = rental_data_determine_operation(&m_current_item);
     switch (op_type)
     {
         case RENTAL_OP_CHECKOUT:
             NRF_LOG_INFO("Rental checkout operation from BLE.");
             rental_data_process_checkout(&m_current_item);
             break;
             
         case RENTAL_OP_CHECKIN:
             NRF_LOG_INFO("Rental checkin operation from BLE.");
             rental_data_process_checkin(&m_current_item);
             break;
             
         case RENTAL_OP_STATUS:
             NRF_LOG_INFO("Rental status check from BLE.");
             rental_data_get_status(&m_current_item);
             break;
             
         default:
             NRF_LOG_INFO("Unknown rental operation from BLE.");
             break;
     }
     
     // Update tag content if NFC field is active
     if (m_nfc_field_active)
     {
         nfc_handler_update_tag_content(&m_current_item);
     }
     
     // Save to storage
     storage_manager_save_rental_item(&m_current_item);
 }
 
 /**@brief Function for handling rental service events. */
 static void on_rental_evt(ble_rental_evt_t * p_evt)
 {
     switch (p_evt->evt_type)
     {
         case BLE_RENTAL_EVT_NOTIFICATION_ENABLED:
             NRF_LOG_INFO("Rental notifications enabled.");
             break;
             
         case BLE_RENTAL_EVT_NOTIFICATION_DISABLED:
             NRF_LOG_INFO("Rental notifications disabled.");
             break;
             
         default:
             // No implementation needed
             break;
     }
 }
 
 /**@brief Function for application main entry.
  */
 int main(void)
 {
     // Initialize modules
     log_init();
     timer_init();
     buttons_init();
     bsp_board_init(BSP_INIT_LEDS);
     power_management_init();
     ble_stack_init();
     gap_params_init();
     gatt_init();
     services_init();
     advertising_init();
     conn_params_init();
     
     // Initialize NFC
     nfc_init();
     
     // Initialize storage for rental items
     storage_manager_init();
     
     // Start execution
     advertising_start();
     
     // Start timer for NFC scanning
     app_timer_start(m_scan_timer_id, NFC_SCAN_INTERVAL, NULL);
     
     NRF_LOG_INFO("RentScan system initialization complete.");
     
     // Enter main loop
     for (;;)
     {
         // Process pending logs
         if (NRF_LOG_PROCESS() == false)
         {
             // No logs to process, so wait for an event
             nrf_pwr_mgmt_run();
         }
     }
 }