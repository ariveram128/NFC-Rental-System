# RentScan Project Status

## Current Status (April 21st, 2025)

### Project Structure
We've created a clean, modular structure that separates different parts of the system:
- `main_application/` - The core rental system
  - `src/` - Source code files
  - `include/` - Header files
  - `prj.conf` - Configuration settings
- `ble_gateway/` - The receiving device (central)
- `samples_reference/` - Original samples kept for reference
- `tests/` - Directory for test code
- `docs/` - Documentation

### Created Files
- Basic header files for NFC, BLE, and rental logic
- Main application file with initialization structure
- Combined configuration file merging NFC and BLE settings
- BLE gateway based on `central_uart` sample

### Key Components
1. **NFC Handler** - Will manage reading NFC tags
2. **BLE Handler** - Will send data wirelessly 
3. **Rental Logic** - Will handle the business rules for rentals
4. **Main Application** - Coordinates all components

## Next Steps

### 1. Implement NFC Reading
- Copy relevant functions from `tag_reader` sample
- Create callback for when tag is detected
- Parse NDEF messages from tags
- Extract rental item information

### 2. Implement BLE Communication
- Copy UART service from `peripheral_uart` sample
- Create function to send rental data over BLE
- Set up advertising with "RentScan" name
- Implement connection handling

### 3. Create Rental Business Logic
- Define rental data structure (item ID, start time, duration)
- Implement rental start/stop functionality
- Add expiration checking
- Handle edge cases (duplicate scans, etc.)

### 4. Complete BLE Gateway
- Modify scanning to look for "RentScan" devices
- Implement data receiving and parsing
- Add logging/forwarding to conceptual backend
- Test with PC or other DK board

### 5. Testing and Integration
- Test NFC reading independently
- Test BLE communication separately
- Integrate both components
- System-level testing with real NFC tags

### 6. Documentation
- Write clear API documentation
- Create user guide
- Document hardware setup
- Add troubleshooting section

## Getting Started

1. Make sure you have nRF Connect SDK v2.5.1 installed
2. Connect your nRF52840 DK boards
3. Start implementing components in their respective files
4. Test each component independently before integration

## Team Tasks

Each team member can focus on:
- Marvin: NFC implementation
- Raul: BLE communication
- Salina: Rental logic
- Sami: Gateway and integration

Remember to create feature branches for each task and merge via pull requests!
