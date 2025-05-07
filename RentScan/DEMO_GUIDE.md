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
   [00:00:00.115,478] <inf> ble_central: Scanning started successfully
   [00:00:00.160,095] <inf> ble_central: Found RentScan device FC:4E:16:98:0D:96, RSSI -65
   [00:00:00.161,071] <inf> ble_central: Connection pending
   [00:00:00.789,428] <inf> ble_central: Connected to device FC:4E:16:98:0D:96
   ```

2. **CRITICAL**: Wait for GATT service discovery and subscription to complete. You MUST see these additional logs on the gateway:
   ```
   [xx:xx:xx.xxx,xxx] <inf> ble_central: RentScan service found, discovering RX characteristic
   [xx:xx:xx.xxx,xxx] <inf> ble_central: RX characteristic found
   [xx:xx:xx.xxx,xxx] <inf> ble_central: TX characteristic found
   [xx:xx:xx.xxx,xxx] <inf> ble_central: CCC descriptor found, subscribing to notifications
   [xx:xx:xx.xxx,xxx] <inf> ble_central: Successfully subscribed to notifications
   ```

3. On the main device, you should see:
   ```
   [00:00:29.935,760] <inf> ble_service: Connected
   [xx:xx:xx.xxx,xxx] <inf> ble_service: Client subscribed to notifications
   ```

**IMPORTANT**: DO NOT proceed to scanning NFC tags until you see the "Successfully subscribed to notifications" message on the gateway logs. The BLE connection is not fully ready until this point.

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

### Incomplete BLE Connection

The most common issue is scanning NFC tags before the BLE connection is fully established. The complete connection sequence requires:

1. Initial connection (logs will show "Connected to device")
2. Service discovery (logs will show several "[ATTRIBUTE] handle X" messages)
3. Characteristic discovery (logs will show "RX/TX characteristic found")
4. Subscription (logs will show "Successfully subscribed to notifications")

**If any step is missing, the connection is incomplete and tag data will not be transmitted.**

To resolve this:
1. Reset both devices using the physical reset buttons
2. Wait at least 30 seconds after seeing "Connected to device" before scanning any tags
3. Verify that you see the "Successfully subscribed to notifications" message
4. Only then proceed with scanning NFC tags

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

### GATT Subscription Error

If you see this error:
```
[00:01:04.224,029] <wrn> bt_gatt: Device is not subscribed to characteristic
[00:01:04.224,060] <err> main: Failed to send tag data via BLE: -22
```

This is a critical error indicating that although the BLE connection is established, the gateway has not completed the GATT service discovery and subscription process. To fix:

1. Reset both devices using the reset button on the boards
2. After connecting, wait at least 10-15 seconds before scanning any NFC tags
3. In the gateway log, you MUST see "Service discovery completed" before proceeding
4. If the issue persists, try increasing the BLE connection interval in `gateway_config.h`

### NFC Tag Detection Issues

If tags are not detected:
1. Try repositioning the tag near the NFC antenna
2. Check Main device logs for NFC initialization:
   ```
   [xx:xx:xx.xxx,xxx] <inf> nfc_handler: NFC initialized
   ```
3. Try with different tag types if available

### Simulated Backend Connection

The gateway device simulates backend connection cycles. You'll see these messages periodically:
```
[00:00:10.009,490] <wrn> gateway_service: Backend connection lost
[00:00:20.009,582] <inf> gateway_service: Backend connection established
```

This is normal and part of the simulation. The system will queue messages when the backend is disconnected.

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