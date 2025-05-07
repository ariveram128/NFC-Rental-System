# Compatibility Layer for Nordic SDK to nRF Connect SDK Migration

This directory contains compatibility headers and implementations to help migrate code from the older Nordic SDK style to the newer Zephyr-based nRF Connect SDK.

## Purpose

The compatibility layer helps bridge the gap between:
- Nordic SDK (nRF5 SDK) - The older, proprietary SDK from Nordic Semiconductor
- nRF Connect SDK - The newer, Zephyr RTOS-based SDK

## Files

The compatibility layer includes:

- **BLE Related**
  - `ble.h` - BLE type and structure definitions
  - `ble_srv_common.h` - Common BLE service definitions

- **NFC Related**
  - `nfc_ndef_msg.h` - NDEF message handling
  - `nfc_ndef_record.h` - NDEF record handling
  - `nfc_ndef_text_rec.h` - NDEF text record handling

- **Storage Related**
  - `nrf_fstorage.h` - Flash storage API
  - `nrf_fstorage_sd.h` - SoftDevice flash storage
  - `nrf_fstorage_impl.c` - Implementation of flash storage

- **Logging Related**
  - `nrf_log.h` - Logging API
  - `nrf_log_ctrl.h` - Logging control
  - `nrf_log_default_backends.h` - Logging backends

- **Error Handling**
  - `app_error.h` - Error handling and reporting

- **Others**
  - `nordic_common.h` - Common utilities
  - `app_timer.h` - Timer functionality
  - `app_button.h` - Button handling
  - `nrf_pwr_mgmt.h` - Power management

## Usage

1. Include these headers in your code instead of the original Nordic SDK headers
2. Build your application with nRF Connect SDK toolchain
3. Make adjustments as needed for Zephyr-specific functionality

## Limitations

This compatibility layer provides the minimal functionality needed to compile existing Nordic SDK code with nRF Connect SDK. It may not perfectly replicate all behaviors of the original SDK. 