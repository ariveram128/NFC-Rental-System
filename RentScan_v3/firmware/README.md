# RentScan v3 Firmware

This firmware implements the RentScan NFC-based rental system on nRF52840 hardware using the nRF Connect SDK (Zephyr RTOS).

## Overview

The RentScan system uses NFC technology to manage rental items. Key features include:

- NFC tag reading/writing
- BLE connectivity for remote management
- Persistent storage of rental data
- Battery-efficient operation

## Components

- **NFC Handler**: Manages NFC tag operations
- **Rental Service**: Implements BLE service for rental management
- **Storage Manager**: Handles persistent data storage
- **Main Application**: System initialization and coordination

## Requirements

- nRF Connect SDK v2.5.1 or later
- nRF52840 Development Kit (or compatible hardware)
- Segger J-Link tools
- An NFC reader/writer (integrated or external)

## Build and Flash

1. Set up your environment for nRF Connect SDK
   ```
   west init -m https://github.com/nrfconnect/sdk-nrf --mr v2.5.1
   west update
   ```

2. Build the firmware
   ```
   west build -b nrf52840dk_nrf52840 .
   ```

3. Flash to the device
   ```
   west flash
   ```

## Compatibility Layer

This project includes a compatibility layer in `include/compat/` to help transition code from the older Nordic SDK style to the newer Zephyr-based nRF Connect SDK. This allows the reuse of existing Nordic SDK code patterns while running on the newer Zephyr-based system.

## Known Issues

- NFC functionality currently uses simulation since nRF52840 doesn't have an integrated NFC reader
- Some compatibility layer implementations are stubs and need real implementations
- Error handling needs improvement in several areas

## Next Steps

1. **Integrate Real NFC Hardware**: Connect an external NFC reader via SPI/I2C
2. **Complete Compatibility Layer**: Implement remaining functionality in compatibility files
3. **Add Security Features**: Implement authentication and encryption
4. **Improve Power Management**: Optimize for battery operation 