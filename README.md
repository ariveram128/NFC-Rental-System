# RentScan - Wireless NFC Tag Rental System (nRF Edition)

**Project for CS/ECE4501 - WiOT Final Project**

## Overview

RentScan is an nRF52840-based system for managing item rentals using NFC tags. Users scan an NFC tag attached to an item using the nRF52840DK's NFC reader to initiate or manage a rental. The system uses **Bluetooth Low Energy (BLE)** to communicate rental status updates wirelessly to a nearby gateway device. This gateway (which could be another nRF52840DK/Dongle connected to a computer or a smartphone) is responsible for forwarding the data to a backend service for logging and potential notifications (e.g., rental expiration).

## Key Features

*   **NFC Tag Interaction:** Utilizes the nRF52840DK's built-in NFC capabilities (Tag Type 4 emulation) to read item information from tags and potentially write rental status back to tags. Uses standard NDEF records.
*   **BLE Communication:** Sends rental status updates (e.g., Tag ID scanned, start/end time, user info if applicable) wirelessly via BLE advertisements or a GATT connection to a gateway.
*   **Gateway Bridge (Conceptual/Simulated):** A separate component (e.g., Python script on PC connected to a BLE dongle/DK, or potentially nRF Connect mobile app) receives BLE data and forwards it to a conceptual backend.
*   **Backend Integration (Conceptual):** Demonstrates sending formatted rental data to a simulated backend endpoint (e.g., printing JSON to gateway's console, sending HTTP POST to `httpbin.org`).
*   **Expiration Notifications (Simulated):** Logic resides primarily on the nRF52840DK or the conceptual backend to determine when a rental expires based on timestamps. Actual push notifications are out of scope, but status updates sent via BLE can indicate expiration.

## Hardware Requirements

*   **2 x nRF52840DK boards:** One acting as the primary NFC reader/BLE peripheral, the other potentially acting as the BLE central/gateway connected to a PC.
*   **(Optional) 1 x nRF52840 Dongle:** Can substitute for the second DK as the BLE gateway connected to a PC.
*   **NFC Antenna:** Attached to the primary nRF52840DK board.
*   **NFC Tags:** Compatible with NDEF format (e.g., NTAG21x series used in labs).
*   **USB Cables:** For programming, power, and connecting gateway device to PC.
*   **(Gateway PC):** A computer to run the BLE listener/forwarder script (if implementing the PC gateway).

## Software Requirements

*   **IDE:** Visual Studio Code with nRF Connect Extension.
*   **SDK:** **nRF Connect SDK v2.4.3 or v2.5.1** (Confirm based on lab experience - v2.5.1 needed for NFC samples in Prelab 5).
*   **OS:** Zephyr RTOS.
*   **Libraries:**
    *   Zephyr NFC libraries (for NDEF reading/writing - see Lab 5).
    *   Zephyr BLE libraries (for peripheral advertising/GATT server on reader node, and central scanning/GATT client on gateway node - see Lab 2).
    *   (Optional) Python with `bleak` or similar library for PC-based gateway script.
*   **Version Control:** Git

## Getting Started

1.  **Clone the repository:**
    ```bash
    git clone https://github.com/arriveram128/NFC-Rental-System.git
    cd NFC-Rental-System
    ```
2.  **Install IDE and SDK:** Set up VS Code with the correct nRF Connect SDK version.
3.  **Hardware Setup:** Attach NFC antenna to the primary DK board. Connect DK boards/dongle via USB.
4.  **Develop Firmware:** Use Zephyr examples (NFC record text/writable NDEF, BLE peripheral, BLE central) as starting points.
5.  **Build and Upload:** Compile and flash firmware to the nRF52840DK boards using VS Code.
6.  **(If using PC Gateway):** Develop Python script to scan for BLE data and forward/print it.

   ## Project Structure (Planned)

   ```
    NFC-Rental-System/
   ├── main_application/
   │   ├── src/
   │   │   ├── main.c              # Main application logic
   │   │   ├── nfc_handler.c       # NFC-specific code
   │   │   ├── ble_handler.c       # BLE-specific code
   │   │   └── rental_logic.c      # Rental system business logic
   │   ├── include/
   │   │   ├── nfc_handler.h
   │   │   ├── ble_handler.h
   │   │   └── rental_logic.h
   │   ├── CMakeLists.txt
   │   └── prj.conf
   ├── ble_gateway/
   │   ├── src/
   │   ├── CMakeLists.txt
   │   └── prj.conf
   ├── samples_reference/           # Keep original samples for reference
   │   ├── tag_reader/
   │   ├── peripheral_uart/
   │   └── central_uart/
   ├── docs/
   ├── tests/                      # Add unit tests here
   ├── .gitignore
   └── README.md
   ```

   ## Branching Strategy

   *   `main`: Stable releases.
   *   `develop`: Main development branch. Features are merged here.
   *   `feature/weekX-task-name`: Branches for specific features/tasks, based on `develop`.

   ## Contributing

   Please follow standard Git workflow:
   1.  Create a feature branch off `develop`.
   2.  Make your changes.
   3.  Commit and push your feature branch.
   4.  Create a Pull Request (PR) on GitHub to merge into `develop`.
   5.  Ensure code builds and is reasonably tested before creating a PR.

   ## Team

   *   Marvin Rivera | ariveram128
   *   Raul Cancho    | RaulCancho
   *   Salina Tran    | dinosaur-sal
   *   Sami Kang      | skang0812
