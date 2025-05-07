# RentScan - Wireless NFC Tag Rental System

A robust implementation of a rental system using NFC tags and Bluetooth Low Energy (BLE) communication.

## Architecture Overview

The system consists of two main components:

1. **Main Device** (NFC reader + BLE peripheral):
   - NFC tag reader for scanning items
   - BLE peripheral for wireless communication
   - Rental state management

2. **Gateway Device** (BLE central + backend connector):
   - BLE central for connecting to main devices
   - Backend communication for data forwarding
   - Shell interface for management

## Directory Structure

```
RentScan/
│
├── common/                 # Common code shared between devices
│   └── include/            # Common header files
│       └── rentscan_protocol.h  # Communication protocol definition
│
├── main_device/            # Main device (NFC reader + BLE peripheral)
│   ├── include/            # Header files for main device
│   │   └── main_device_config.h  # Configuration parameters
│   └── src/                # Source files for main device
│       ├── main.c          # Main application
│       ├── nfc_handler.h/c # NFC handling functionality
│       ├── ble_service.h/c # BLE peripheral service
│       └── rental_manager.h/c # Rental state management
│
└── gateway_device/         # Gateway device (BLE central + backend connector)
    ├── include/            # Header files for gateway device
    │   └── gateway_config.h # Configuration parameters
    └── src/                # Source files for gateway device
        ├── main.c          # Main application
        ├── ble_central.h/c # BLE central functionality
        └── gateway_service.h/c # Backend communication service
```

## Features

- **NFC Tag Reading/Writing**: Reads and writes NDEF data from/to NFC tags
- **BLE Communication**: Robust BLE connectivity with error recovery
- **Rental Management**: Tracking rental state, expiration, etc.
- **Gateway Functions**: Forwarding rental data to backend systems
- **Shell Interface**: Command-line management of gateway device
- **Modular Design**: Clean separation of concerns with well-defined interfaces

## Building and Flashing

### Prerequisites

- nRF Connect SDK v2.5.1
- nRF52840 DK (for both main and gateway devices)
- NFC tags (NTAG21x series recommended)

### Building the Main Device

```bash
cd RentScan
west build -p -b nrf52840dk_nrf52840 main_device
west flash
```

### Building the Gateway Device

```bash
cd RentScan
west build -p -b nrf52840dk_nrf52840 gateway_device
west flash
```

## Usage

### Main Device

1. Power on the main device
2. Bring an NFC tag close to the antenna
3. The device will read the tag and process the rental operation
4. Status will be sent to the gateway over BLE

### Gateway Device

1. Power on the gateway device
2. It will automatically scan for and connect to main devices
3. Use the shell commands to manage the system:

```
rentscan status             # Show current status
rentscan scan start         # Start scanning for devices
rentscan scan stop          # Stop scanning
rentscan whitelist add <addr> # Add device to whitelist
rentscan reset              # Reset the BLE stack
```

## Troubleshooting

- **Connection Issues**: Use `rentscan reset` to reset the BLE stack if connections fail
- **Scan Not Working**: Check that no active connections exist before scanning
- **Tag Reading Problems**: Ensure proper antenna positioning and tag compatibility

## Design Considerations

- **Error Recovery**: Robust error handling with automatic recovery mechanisms
- **Power Management**: Efficient BLE parameters to reduce power consumption
- **Memory Safety**: No dynamic memory allocation for increased stability
- **Modularity**: Clean component interfaces for easier maintenance

## License

[Insert license information here]

## Contributors

[Insert contributor information here] 