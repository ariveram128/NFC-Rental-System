# RentScan - Wireless NFC Tag Rental System

**Project for CS/ECE4501 - WiOT Final Project**

## Overview

RentScan is a wireless rental management system that uses NFC technology to track items and manage rentals. The system consists of two main components: a main device (NFC reader with BLE peripheral) and a gateway device (BLE central) that processes rental information.

Users scan an NFC tag attached to an item using the nRF52840DK's NFC reader to initiate or manage a rental. The system uses **Bluetooth Low Energy (BLE)** to communicate rental status updates wirelessly to the gateway device, which maintains a rental database and simulates backend connectivity.

## System Operation

The RentScan system enables a simple, contactless rental process:

1. The main device reads NFC tags attached to rental items
2. The gateway device manages the rental database and communicates with a backend
3. When a tag is scanned, the rental status is toggled (available → rented, or rented → returned)

## Components

- **Main Device**: nRF52840 with NFC reading capabilities
  - Reads NTAG216 NFC tags
  - Processes tag data
  - Communicates with gateway via BLE

- **Gateway Device**: nRF52840 with BLE central capabilities
  - Maintains rental database
  - Processes tag data from main device
  - Simulates backend connectivity
  - Provides command-line interface for management

## Key Features

* **NFC Tag Interaction:** Utilizes the nRF52840DK's built-in NFC capabilities to read item information from tags.
* **BLE Communication:** Sends rental status updates (e.g., Tag ID scanned, start/end time, user info) wirelessly via BLE GATT connection to a gateway.
* **Gateway Service:** Manages rental database with start/end functionality and status tracking.
* **Backend Simulation:** Simulates connecting to a backend service with connection state changes and message queuing.
* **Rental Expiration:** Logic to determine when a rental expires based on timestamps, with automatic expiration handling.

## Current Implementation

The system currently provides the following functionality:

- NFC tag reading from the main device (NTAG216 tags)
- BLE connectivity between devices
- Rental management (start/end rentals, check rental status)
- Simulated backend connection with offline queuing
- Command-line interface for manual control

## Hardware Requirements

* **2 x nRF52840DK boards:** One acting as the main NFC reader/BLE peripheral, the other acting as the BLE central/gateway.
* **NFC Antenna:** Attached to the primary nRF52840DK board.
* **NFC Tags:** NTAG216 tags (even read-only tags work with the system).
* **USB Cables:** For programming, power, and connecting devices to PC.

## Software Requirements

* **IDE:** Visual Studio Code with nRF Connect Extension.
* **SDK:** nRF Connect SDK v2.5.1
* **OS:** Zephyr RTOS.

## Known Issues

1. **BLE GATT Subscription**: The gateway device sometimes fails to properly subscribe to BLE notifications from the main device. This causes a "Device is not subscribed to characteristic" error when trying to send tag data.

2. **BLE Connection Stability**: Occasionally, the system encounters "Found valid connection in disconnected state" errors that require device reset.

3. **Manual Command Issue**: Some shell commands like `manual_sub` and `show_handles` are not recognized properly due to build configuration issues.

## Demo Workaround

For demonstration purposes, the system can be operated with this workflow:

1. The main device successfully reads NFC tags and displays the tag ID
2. The gateway device can manually start/end rentals using the CLI:
   ```
   rentscan rental start <tag_id> <user_id> <duration>
   rentscan rental end <tag_id>
   ```

See [DEMO_GUIDE.md](RentScan/DEMO_GUIDE.md) for detailed demonstration instructions.

## Project Structure

```
NFC-Rental-System/
├── RentScan/
│   ├── common/                 # Common code shared between devices
│   │   └── include/            # Common header files
│   │       └── rentscan_protocol.h  # Communication protocol definition
│   │
│   ├── main_device/            # Main device (NFC reader + BLE peripheral)
│   │   ├── include/            # Header files for main device
│   │   │   └── main_device_config.h  # Configuration parameters
│   │   └── src/                # Source files for main device
│   │       ├── main.c          # Main application
│   │       ├── nfc_handler.h/c # NFC handling functionality
│   │       ├── ble_service.h/c # BLE peripheral service
│   │       └── rental_manager.h/c # Rental state management
│   │
│   └── gateway_device/         # Gateway device (BLE central + backend connector)
│       ├── include/            # Header files for gateway device
│       │   └── gateway_config.h # Configuration parameters
│       └── src/                # Source files for gateway device
│           ├── main.c          # Main application
│           ├── ble_central.h/c # BLE central functionality
│           └── gateway_service.h/c # Backend communication service
│
├── samples_reference/           # Keep original samples for reference
│   ├── tag_reader/
│   ├── peripheral_uart/
│   └── central_uart/
├── docs/
├── .gitignore
└── README.md
```

## Building and Flashing

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

## Gateway Commands

```
rentscan status             # Show current status
rentscan scan start         # Start scanning for devices
rentscan scan stop          # Stop scanning
rentscan reset              # Reset the BLE stack
rentscan rental start <id> <user> <duration>  # Start a rental
rentscan rental end <id>    # End a rental
rentscan rental list        # List active rentals
```

## Future Improvements

1. **Robust BLE Implementation**: Implement proper retry mechanisms for GATT discovery and handle connection edge cases more gracefully.

2. **Enhanced Security**: Add encryption for NFC and BLE communication to protect rental data.

3. **User Authentication**: Implement user identification through separate NFC cards or mobile app integration.

4. **Actual Backend Integration**: Replace the simulated backend with a real cloud database service.

5. **Battery Optimization**: Implement power management features to extend battery life for portable deployment.

6. **Mobile Application**: Develop a companion mobile app to manage rentals and view rental history.

## Complete System Vision

In a full implementation, the RentScan system would work as follows:

When a customer wants to rent an item:
1. They bring the item to a rental station
2. The item's NFC tag is scanned by the main device
3. The main device reads the unique tag ID and sends it to the gateway
4. The gateway checks if the item is available and starts a rental
5. A receipt could be printed or sent electronically
6. The item status is updated in the backend database

When the item is returned:
1. The item's tag is scanned again
2. The main device reads the tag ID and sends it to the gateway
3. The gateway recognizes the item is already rented and ends the rental
4. Rental duration and any fees are calculated
5. The item becomes available for the next customer

The system could be expanded to multiple stations, all connected to a central backend, enabling items to be rented from one location and returned to another.

## Team

* Marvin Rivera | ariveram128
* Raul Cancho  | RaulCancho
* Salina Tran  | dinosaur-sal
* Sami Kang    | skang0812
