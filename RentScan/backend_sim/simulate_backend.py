#!/usr/bin/env python3
"""
RentScan Backend Simulator

This script simulates a backend server for the RentScan system.
It receives messages from the gateway device over a serial connection
and processes rental operations.
"""

import argparse
import json
import logging
import serial
import socket
import sys
import threading
import time
from enum import Enum
from datetime import datetime, timedelta

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger('rentscan-backend')

# Constants matching the firmware protocol
class CommandType(Enum):
    RENTAL_START = 1
    RENTAL_END = 2
    STATUS_REQ = 3
    STATUS_RESP = 4
    ERROR = 255

class RentalStatus(Enum):
    AVAILABLE = 0
    RENTED = 1
    EXPIRED = 2
    ERROR = 255

# In-memory database for rentals
rental_db = {}

class RentalItem:
    def __init__(self, tag_id):
        self.tag_id = tag_id
        self.status = RentalStatus.AVAILABLE.value
        self.rent_timestamp = 0
        self.duration = 0
        self.return_timestamp = 0
        
    def to_dict(self):
        return {
            'tag_id': self.tag_id.hex(),
            'status': self.status,
            'rent_timestamp': self.rent_timestamp,
            'duration': self.duration,
            'return_timestamp': self.return_timestamp,
            'status_text': RentalStatus(self.status).name if self.status in [e.value for e in RentalStatus] else 'UNKNOWN'
        }

class BackendServer:
    def __init__(self, port, mode='serial'):
        self.port = port
        self.mode = mode
        self.connection = None
        self.running = False
        
    def start(self):
        """Start the backend server"""
        if self.mode == 'serial':
            try:
                self.connection = serial.Serial(self.port, 115200, timeout=1)
                logger.info(f"Connected to serial port {self.port}")
            except serial.SerialException as e:
                logger.error(f"Failed to open serial port {self.port}: {e}")
                return False
        elif self.mode == 'tcp':
            try:
                self.server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
                host, port = self.port.split(':')
                self.server_socket.bind((host, int(port)))
                self.server_socket.listen(1)
                logger.info(f"Listening on {host}:{port}")
                self.connection, addr = self.server_socket.accept()
                logger.info(f"Connection from {addr}")
            except Exception as e:
                logger.error(f"Failed to setup TCP connection: {e}")
                return False
        else:
            logger.error(f"Unsupported mode: {self.mode}")
            return False
            
        self.running = True
        self.receive_thread = threading.Thread(target=self.receive_loop)
        self.receive_thread.daemon = True
        self.receive_thread.start()
        
        # Start the expiration checker thread
        self.expiration_thread = threading.Thread(target=self.check_expirations)
        self.expiration_thread.daemon = True
        self.expiration_thread.start()
        
        return True
    
    def stop(self):
        """Stop the backend server"""
        self.running = False
        if self.connection:
            if self.mode == 'serial':
                self.connection.close()
            else:
                self.connection.close()
                self.server_socket.close()
        logger.info("Backend server stopped")
    
    def receive_loop(self):
        """Main receive loop to process incoming messages"""
        logger.info("Receive loop started")
        
        buffer = bytearray()
        message_start = False
        
        while self.running:
            try:
                if self.mode == 'serial':
                    if self.connection.in_waiting:
                        data = self.connection.read(self.connection.in_waiting)
                        buffer.extend(data)
                else:
                    data = self.connection.recv(1024)
                    if not data:
                        logger.warning("Connection closed by client")
                        break
                    buffer.extend(data)
                
                # Simple protocol: JSON messages separated by newlines
                if b'\n' in buffer:
                    parts = buffer.split(b'\n')
                    for i in range(len(parts) - 1):
                        try:
                            message = json.loads(parts[i].decode('utf-8'))
                            self.process_message(message)
                        except json.JSONDecodeError:
                            logger.error(f"Invalid JSON: {parts[i]}")
                        except Exception as e:
                            logger.error(f"Error processing message: {e}")
                    
                    buffer = parts[-1]
            
            except Exception as e:
                logger.error(f"Error in receive loop: {e}")
                # Give some time before retrying
                time.sleep(0.1)
        
        logger.info("Receive loop ended")
    
    def process_message(self, message):
        """Process a received message"""
        logger.info(f"Received message: {message}")
        
        if 'cmd' not in message:
            logger.error("Invalid message format: missing 'cmd' field")
            return
            
        cmd = message['cmd']
        
        if cmd == CommandType.RENTAL_START.value:
            self.handle_rental_start(message)
        elif cmd == CommandType.RENTAL_END.value:
            self.handle_rental_end(message)
        elif cmd == CommandType.STATUS_REQ.value:
            self.handle_status_request(message)
        else:
            logger.warning(f"Unknown command: {cmd}")
    
    def handle_rental_start(self, message):
        """Handle rental start command"""
        if 'tag_id' not in message or 'tag_id_len' not in message:
            logger.error("Invalid rental start message: missing tag info")
            return
            
        tag_id_hex = message.get('tag_id', '')
        tag_id = bytes.fromhex(tag_id_hex)
        tag_id_str = tag_id_hex
        
        # Get or create rental item
        if tag_id_str not in rental_db:
            rental_db[tag_id_str] = RentalItem(tag_id)
        
        rental_item = rental_db[tag_id_str]
        
        # Check if the item is already rented
        if rental_item.status == RentalStatus.RENTED.value:
            logger.warning(f"Item {tag_id_str} is already rented")
            self.send_status_response(tag_id, RentalStatus.RENTED.value)
            return
            
        # Set rental information
        rental_item.status = RentalStatus.RENTED.value
        rental_item.rent_timestamp = int(time.time())
        rental_item.duration = message.get('duration', 3600)  # Default 1 hour
        rental_item.return_timestamp = 0
        
        logger.info(f"Started rental for item {tag_id_str}")
        logger.info(f"Rental details: {rental_item.to_dict()}")
        
        # Send confirmation
        self.send_status_response(tag_id, RentalStatus.RENTED.value)
    
    def handle_rental_end(self, message):
        """Handle rental end command"""
        if 'tag_id' not in message or 'tag_id_len' not in message:
            logger.error("Invalid rental end message: missing tag info")
            return
            
        tag_id_hex = message.get('tag_id', '')
        tag_id = bytes.fromhex(tag_id_hex)
        tag_id_str = tag_id_hex
        
        if tag_id_str not in rental_db:
            logger.warning(f"No rental found for item {tag_id_str}")
            self.send_status_response(tag_id, RentalStatus.AVAILABLE.value)
            return
            
        rental_item = rental_db[tag_id_str]
        
        # Check if the item is actually rented
        if rental_item.status != RentalStatus.RENTED.value and rental_item.status != RentalStatus.EXPIRED.value:
            logger.warning(f"Item {tag_id_str} is not currently rented")
            self.send_status_response(tag_id, rental_item.status)
            return
            
        # End the rental
        rental_item.status = RentalStatus.AVAILABLE.value
        rental_item.return_timestamp = int(time.time())
        
        logger.info(f"Ended rental for item {tag_id_str}")
        logger.info(f"Rental details: {rental_item.to_dict()}")
        
        # Send confirmation
        self.send_status_response(tag_id, RentalStatus.AVAILABLE.value)
    
    def handle_status_request(self, message):
        """Handle status request command"""
        if 'tag_id' not in message or 'tag_id_len' not in message:
            logger.error("Invalid status request message: missing tag info")
            return
            
        tag_id_hex = message.get('tag_id', '')
        tag_id = bytes.fromhex(tag_id_hex)
        tag_id_str = tag_id_hex
        
        if tag_id_str not in rental_db:
            logger.info(f"No rental record found for item {tag_id_str}")
            self.send_status_response(tag_id, RentalStatus.AVAILABLE.value)
            return
            
        rental_item = rental_db[tag_id_str]
        logger.info(f"Status request for item {tag_id_str}: {rental_item.to_dict()}")
        
        # Send status
        self.send_status_response(tag_id, rental_item.status, rental_item.rent_timestamp, rental_item.duration)
    
    def send_status_response(self, tag_id, status, timestamp=0, duration=0):
        """Send a status response to the gateway"""
        message = {
            'cmd': CommandType.STATUS_RESP.value,
            'status': status,
            'tag_id': tag_id.hex(),
            'tag_id_len': len(tag_id),
            'timestamp': timestamp,
            'duration': duration
        }
        
        try:
            json_data = json.dumps(message) + '\n'
            if self.mode == 'serial':
                self.connection.write(json_data.encode('utf-8'))
            else:
                self.connection.sendall(json_data.encode('utf-8'))
            logger.debug(f"Sent response: {message}")
        except Exception as e:
            logger.error(f"Error sending response: {e}")
    
    def check_expirations(self):
        """Check for expired rentals periodically"""
        logger.info("Expiration checker started")
        
        while self.running:
            now = int(time.time())
            
            for tag_id_str, rental_item in rental_db.items():
                if rental_item.status == RentalStatus.RENTED.value:
                    # Check if rental has expired
                    expiration_time = rental_item.rent_timestamp + rental_item.duration
                    if now > expiration_time:
                        logger.info(f"Rental expired for item {tag_id_str}")
                        rental_item.status = RentalStatus.EXPIRED.value
                        
                        # Send status update
                        tag_id = bytes.fromhex(tag_id_str)
                        self.send_status_response(tag_id, RentalStatus.EXPIRED.value, 
                                                 rental_item.rent_timestamp, rental_item.duration)
            
            # Check every 10 seconds
            time.sleep(10)
            
        logger.info("Expiration checker ended")

def main():
    parser = argparse.ArgumentParser(description="RentScan Backend Simulator")
    parser.add_argument('--port', required=True, help='Serial port or TCP address (host:port)')
    parser.add_argument('--mode', choices=['serial', 'tcp'], default='serial', help='Communication mode')
    args = parser.parse_args()
    
    server = BackendServer(args.port, args.mode)
    
    logger.info("Starting RentScan Backend Simulator")
    if not server.start():
        logger.error("Failed to start backend server")
        sys.exit(1)
    
    try:
        # Print help information
        print("\nRentScan Backend Simulator")
        print("=========================")
        print("Press Ctrl+C to exit")
        print("\nAvailable commands:")
        print("  list - List all rental items")
        print("  status <tag_id> - Show details for a specific tag")
        print("  clear - Clear all rentals")
        print("  exit - Exit the simulator")
        
        while True:
            cmd = input("> ").strip().lower()
            
            if cmd == "exit":
                break
            elif cmd == "list":
                if not rental_db:
                    print("No rental items found")
                else:
                    print("\nRental Items:")
                    print("=============")
                    for tag_id, item in rental_db.items():
                        print(f"Tag: {tag_id}")
                        details = item.to_dict()
                        for k, v in details.items():
                            if k in ['rent_timestamp', 'return_timestamp'] and v > 0:
                                dt = datetime.fromtimestamp(v)
                                print(f"  {k}: {v} ({dt})")
                            else:
                                print(f"  {k}: {v}")
                        print()
            elif cmd.startswith("status "):
                tag_id = cmd[7:].strip()
                if tag_id in rental_db:
                    details = rental_db[tag_id].to_dict()
                    print(f"\nRental details for {tag_id}:")
                    for k, v in details.items():
                        if k in ['rent_timestamp', 'return_timestamp'] and v > 0:
                            dt = datetime.fromtimestamp(v)
                            print(f"  {k}: {v} ({dt})")
                        else:
                            print(f"  {k}: {v}")
                else:
                    print(f"No rental found for tag {tag_id}")
            elif cmd == "clear":
                rental_db.clear()
                print("All rentals cleared")
            else:
                print("Unknown command")
                
    except KeyboardInterrupt:
        print("\nExiting...")
    finally:
        server.stop()

if __name__ == "__main__":
    main()
