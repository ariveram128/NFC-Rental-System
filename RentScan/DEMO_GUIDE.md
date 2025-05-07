# RentScan System Demo Guide

This guide provides step-by-step instructions to demo the RentScan NFC-based rental system using the nRF52840 devices.

## Setup Requirements

- 2x nRF52840 development kits (one for main device, one for gateway)
- NFC tags (compatible with Type 2/4)
- Computer with nRF Connect for Desktop or similar terminal program
- USB cables for both devices

## Preparation

1. Flash the correct firmware to each device:
   - Main device: `RentScan/main_device` firmware
   - Gateway device: `RentScan/gateway_device` firmware

2. Connect both devices to your computer via USB

3. Open two terminal windows, one for each device:
   ```
   # Terminal options in nRF Connect
   Baud rate: 115200
   Parity: None
   Data bits: 8
   Stop bits: 1
   Flow control: None
   ```

## Demo Sequence

### Step 1: Power up both devices

1. Make sure both devices are powered on
2. Check that both terminal windows show boot messages

### Step 2: Verify BLE connection

1. Observe the Gateway device logs:
   ```
   [xx:xx:xx.xxx,xxx] <inf> ble_central: Scanning started successfully
   [xx:xx:xx.xxx,xxx] <inf> ble_central: Found RentScan device xx:xx:xx:xx:xx:xx, RSSI -xx
   [xx:xx:xx.xxx,xxx] <inf> ble_central: Connection pending
   [xx:xx:xx.xxx,xxx] <inf> ble_central: Connected to device xx:xx:xx:xx:xx:xx
   [xx:xx:xx.xxx,xxx] <inf> ble_central: Service discovery completed
   ```

2. Observe the Main device logs:
   ```
   [xx:xx:xx.xxx,xxx] <inf> ble_service: Device connected
   [xx:xx:xx.xxx,xxx] <inf> ble_service: Client subscribed to notifications
   ```

**IMPORTANT**: Ensure BLE connection is established before proceeding. If not connected, reset both devices and wait for connection to establish.

### Step 3: Scan an NFC tag for the first time (start rental)

1. Bring an NFC tag close to the Main device antenna
2. Observe the Main device logs:
   ```
   [xx:xx:xx.xxx,xxx] <inf> nfc_handler: NFC field detected
   [xx:xx:xx.xxx,xxx] <inf> nfc_handler: NFC data read: ItemID: xxx
   [xx:xx:xx.xxx,xxx] <inf> main: Processing NFC tag with ID: xxx
   [xx:xx:xx.xxx,xxx] <inf> main: Tag data sent to gateway
   ```

3. Observe the Gateway device logs:
   ```
   [xx:xx:xx.xxx,xxx] <inf> main: Received tag data from main device: ItemID: xxx
   [xx:xx:xx.xxx,xxx] <inf> main: Item xxx is available. Starting rental.
   [xx:xx:xx.xxx,xxx] <inf> gateway_service: Rental started for item xxx by user auto_user for 300 seconds
   ```

### Step 4: Scan the same NFC tag again (end rental)

1. Bring the same NFC tag close to the Main device antenna again
2. Observe the Main device logs showing the tag processing
3. Observe the Gateway device logs:
   ```
   [xx:xx:xx.xxx,xxx] <inf> main: Received tag data from main device: ItemID: xxx
   [xx:xx:xx.xxx,xxx] <inf> main: Item xxx is currently rented. Ending rental.
   [xx:xx:xx.xxx,xxx] <inf> gateway_service: Rental ended for item xxx (duration: xx seconds)
   ```

### Step 5: Demo automatic rental expiration (optional)

1. Start a rental by scanning a tag as in Step 3
2. Wait for 5 minutes (300 seconds, default rental duration)
3. Observe the logs showing rental expiration:
   ```
   [xx:xx:xx.xxx,xxx] <inf> rental_manager: 1 rentals expired
   ```

## Troubleshooting

### BLE Connection Issues

If devices are not connecting:
1. Reset both devices and wait for reconnection
2. Check if Main device is advertising:
   ```
   [xx:xx:xx.xxx,xxx] <inf> ble_service: Starting BLE advertising
   ```
3. Check if Gateway device is scanning:
   ```
   [xx:xx:xx.xxx,xxx] <inf> ble_central: Scanning started successfully
   ```

### NFC Tag Detection Issues

If tags are not detected:
1. Try repositioning the tag near the NFC antenna
2. Check Main device logs for NFC initialization:
   ```
   [xx:xx:xx.xxx,xxx] <inf> nfc_handler: NFC initialized
   ```

### Failed to Send Tag Data

If you see this error:
```
[xx:xx:xx.xxx,xxx] <wrn> bt_gatt: Device is not subscribed to characteristic
[xx:xx:xx.xxx,xxx] <err> main: Failed to send tag data via BLE: -22
```

The BLE connection may be partially established. Try:
1. Reset both devices
2. Wait for full connection sequence to complete before scanning tags
3. Verify subscription status in logs

## Command Reference

### Useful Commands to Run on Gateway Device (Debug mode only)

1. Check connection status:
   ```
   bt_status
   ```

2. Check active rentals:
   ```
   rentals
   ```

3. Manually start a rental:
   ```
   start_rental <item_id> <user_id> <duration>
   ```

4. Manually end a rental:
   ```
   end_rental <item_id>
   ```

5. Reset BLE connection:
   ```
   ble_reset
   ```

### Useful Commands to Run on Main Device (Debug mode only)

1. Show stored tags:
   ```
   tags
   ```

2. Simulate tag read:
   ```
   read_tag <tag_id>
   ```

---

**Note**: This demo guide assumes the system is configured with default settings. Your specific deployment may have different timing, rental duration, or backend simulation settings. 