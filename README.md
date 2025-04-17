    # RentScan - Wireless NFC Tag Rental System

    **Project for CS/ECE4501 - WiOT Final Project**

    ## Overview

    RentScan is an ESP32-based system for managing item rentals using NFC tags. Users scan an NFC tag to initiate a rental subscription. The system uses WiFi for network connectivity to send notifications, primarily when a rental period expires or potentially when a rented item moves out of a predefined range.

    ## Key Features

    *   **NFC Tag Interaction:** Subscribe to rentals by scanning NFC tags containing item information.
    *   **WiFi Connectivity:** Connects to a local WiFi network for communication.
    *   **Backend Integration (Conceptual):** Sends rental data (tag ID, user, timestamps) to a backend service.
    *   **Expiration Notifications:** The backend triggers notifications when rental periods expire.
    *   **Proximity Notifications (Exploratory):** Investigate and potentially implement notifications based on WiFi signal strength changes to indicate if an item is out of range.

    ## Hardware Requirements (Initial)

    *   ESP32 Development Board with WiFi
    *   NFC Reader/Writer Module compatible with ESP32 (e.g., PN532, MFRC522 - *Note: MFRC522 is 13.56MHz RFID, might not be fully NFC NDEF compliant, PN532 is generally better for NFC*)
    *   NFC Tags (NTAG21x series recommended for NDEF compatibility)
    *   Jumper Wires
    *   USB Cable for programming and power

    ## Software Requirements

    *   **IDE:** PlatformIO 
    *   **ESP32 Board Support:** ESP32 board package installed in the IDE.
    *   **Libraries:**
        *   WiFi libraries (usually built-in with ESP32 Arduino core)
        *   NFC Library (e.g., Adafruit_PN532, MFRC522-spi-i2c-uart-async) - *To be determined based on specific NFC module*
        *   ArduinoJson (likely needed for handling data)
        *   Libraries for communicating with the backend (e.g., HTTPClient)
    *   **Version Control:** Git

    ## Getting Started

    1.  **Clone the repository:**
        ```bash
        git clone https://github.com/arriveram128/NFC-Rental-System.git
        cd NFC-Rental-System
        ```
    2.  **Install IDE and Board Support:** Set up PlatformIO with ESP32 support.
    3.  **Install Libraries:** Install the required libraries mentioned above via the IDE's library manager.
    4.  **Hardware Setup:** Connect the NFC module to the ESP32 (wiring details TBD).
    5.  **Configure WiFi:** Update WiFi credentials (SSID/Password) in the relevant code file (e.g., `config.h` - *we will create this later*).
    6.  **Build and Upload:** Compile and upload the firmware to the ESP32.

    ## Project Structure (Planned)

    ```
    ├── src/                    # Main source code (.ino or .cpp files)
    ├── include/                # Header files (.h)
    ├── lib/                    # Project-specific libraries (if any)
    ├── data/                   # Files for SPIFFS/LittleFS (e.g., web interface assets)
    ├── docs/                   # Documentation files
    ├── .gitignore              # Specifies intentionally untracked files that Git should ignore
    ├── platformio.ini          # PlatformIO project configuration (if using PlatformIO)
    └── README.md               # This file
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
    *   Raul Cancho   | RaulCancho
    *   Salina Tran   | dinosaur-sal
    *   Sami Kang     | skang0812
