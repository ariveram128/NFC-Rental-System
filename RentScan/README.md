# RentScan: NFC-Based Rental System

RentScan is a wireless rental management system that uses NFC technology to track items and manage rentals. The system consists of two main components: a main device (NFC reader) and a gateway device that processes rental information.

## System Overview

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

## Current Implementation

The system currently provides the following functionality:

- NFC tag reading from the main device
- BLE connectivity between devices
- Rental management (start/end rentals, check rental status)
- Simulated backend connection with offline queuing

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

## See Also

- [DEMO_GUIDE.md](DEMO_GUIDE.md) - Detailed instructions for demonstrating the system
- [gateway_device/README.md](gateway_device/README.md) - Gateway device documentation
- [main_device/README.md](main_device/README.md) - Main device documentation 