# RentScan Backend Simulator

This directory contains a Python script to simulate the backend server for the RentScan system. The simulator handles rental operations and communicates with the gateway device over a serial connection.

## Prerequisites

- Python 3.6 or higher
- PySerial library (`pip install pyserial`)

## Getting Started

1. Ensure your nRF52840 gateway device is connected to your computer via USB
2. Note the COM port (Windows) or device path (Linux/macOS) assigned to your device
3. Run the simulator with the port specified:

```bash
# Windows example
python simulate_backend.py --port COM3

# Linux/macOS example
python simulate_backend.py --port /dev/ttyACM0
```

## Command Line Options

- `--port`: (Required) The serial port or TCP address to connect to
- `--mode`: (Optional) Communication mode, either 'serial' (default) or 'tcp'

## Simulator Commands

While the simulator is running, you can use the following commands:

- `list`: Display all rental items in the system
- `status <tag_id>`: Show details for a specific tag
- `clear`: Remove all rentals from the system
- `exit`: Exit the simulator

## JSON Message Format

The simulator uses a simple JSON message protocol:

```json
{
  "cmd": 1,              // Command type (1=start, 2=end, 3=status request, 4=status response)
  "status": 0,           // Status code (0=available, 1=rented, 2=expired, 255=error)
  "tag_id": "0123456789abcdef", // Tag ID in hex
  "tag_id_len": 8,      // Length of tag ID
  "timestamp": 1634567890, // Unix timestamp (optional)
  "duration": 3600       // Rental duration in seconds (optional)
}
```

Messages are separated by newline characters (`\n`).

## Integration with Gateway Device

The simulator communicates with the gateway device via UART. The gateway device should:

1. Send JSON messages to request or update rental status
2. Parse JSON responses from the simulator
3. Update its internal state based on the responses

## Troubleshooting

If you encounter connection issues:

1. Verify the COM port or device path is correct
2. Ensure the baud rate matches (115200 by default)
3. Check that the gateway device is properly flashed with the correct firmware
4. Verify that the serial connection isn't already in use by another application

## Development

To extend the simulator, you can modify the `simulate_backend.py` script to:

- Add database persistence (currently uses in-memory storage)
- Implement more sophisticated rental rules
- Add API endpoints for web-based management
- Include notification mechanisms for expired rentals 