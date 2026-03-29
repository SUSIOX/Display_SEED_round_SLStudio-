#!/usr/bin/env python3
"""
MAVLink Simulator Script

This script simulates MAVLink messages for testing the Display without actual hardware.
Connects to ESP32-S3 serial port and sends simulated MAVLink messages.

Usage:
    python3 mavlink_simulator.py --port /dev/cu.usbmodem1101 --baud 57600

Note: This script is NOT compiled with the project. It's a helper utility.
"""

import serial
import struct
import time
import argparse
from typing import Optional

# MAVLink constants
MAVLINK_START_BYTE = 0xFE
MAVLINK_SYSTEM_ID = 1
MAVLINK_COMPONENT_ID = 1

# Message IDs
MAVLINK_MSG_ID_HEARTBEAT = 0
MAVLINK_MSG_ID_BATTERY_STATUS = 147


def calculate_checksum(data: bytes, extra: int = 0) -> int:
    """Calculate MAVLink checksum (CRC16-MCRF4XX)"""
    checksum = 0xFFFF
    for byte in data:
        tmp = byte ^ (checksum & 0xFF)
        tmp ^= (tmp << 4) & 0xFF
        checksum = ((checksum >> 8) ^ (tmp << 8) ^ (tmp << 3) ^ (tmp >> 4)) & 0xFFFF
    
    tmp = extra ^ (checksum & 0xFF)
    tmp ^= (tmp << 4) & 0xFF
    checksum = ((checksum >> 8) ^ (tmp << 8) ^ (tmp << 3) ^ (tmp >> 4)) & 0xFFFF
    
    return checksum & 0xFF, (checksum >> 8) & 0xFF


def create_heartbeat_message() -> bytes:
    """Create MAVLink heartbeat message"""
    # Simplified heartbeat payload
    payload = struct.pack('<I', 0)  # custom_mode
    payload += struct.pack('<B', 0)  # type
    payload += struct.pack('<B', 0)  # autopilot
    payload += struct.pack('<B', 4)  # base_mode (MAV_MODE_FLAG_MANUAL_INPUT_ENABLED)
    payload += struct.pack('<B', 0)  # system_status
    payload += struct.pack('<B', 3)  # mavlink_version
    
    msg_len = len(payload)
    
    # Build message
    msg = bytes([MAVLINK_START_BYTE, msg_len, 0])  # start, length, sequence
    msg += struct.pack('<B', MAVLINK_SYSTEM_ID)  # system id
    msg += struct.pack('<B', MAVLINK_COMPONENT_ID)  # component id
    msg += struct.pack('<B', MAVLINK_MSG_ID_HEARTBEAT)  # message id
    msg += payload
    
    # Calculate and add checksum
    crc_low, crc_high = calculate_checksum(msg[1:], MAVLINK_MSG_ID_HEARTBEAT)
    msg += bytes([crc_low, crc_high])
    
    return msg


def create_battery_status_message(
    voltage: float = 12.6,
    current: float = 2.1,
    remaining: int = 85
) -> bytes:
    """Create MAVLink battery status message"""
    # Simplified BATTERY_STATUS message
    voltages = [int(voltage * 1000)] + [65535] * 9  # cell voltages in mV
    
    payload = struct.pack('<10H', *voltages)  # voltages
    payload += struct.pack('<h', int(current * 100))  # current in 10mA units
    payload += struct.pack('<H', 0)  # consumed energy
    payload += struct.pack('<h', int(remaining))  # battery remaining %
    payload += struct.pack('<h', 2500)  # time remaining in seconds
    payload += struct.pack('<b', 0)  # charge state
    payload += struct.pack('<B', 0)  # id
    payload += struct.pack('<B', 3)  # battery function
    payload += struct.pack('<B', 0)  # type
    
    msg_len = len(payload)
    
    # Build message
    msg = bytes([MAVLINK_START_BYTE, msg_len, 0])
    msg += struct.pack('<B', MAVLINK_SYSTEM_ID)
    msg += struct.pack('<B', MAVLINK_COMPONENT_ID)
    msg += struct.pack('<B', MAVLINK_MSG_ID_BATTERY_STATUS)
    msg += payload
    
    # Calculate checksum (using extra CRC for this message ID)
    extra = MAVLINK_MSG_ID_BATTERY_STATUS
    crc_low, crc_high = calculate_checksum(msg[1:], extra)
    msg += bytes([crc_low, crc_high])
    
    return msg


def mavlink_simulator(port: str, baud: int = 57600):
    """Main MAVLink simulator loop"""
    print(f"MAVLink Simulator starting on {port} at {baud} baud")
    print("Press Ctrl+C to stop")
    
    try:
        with serial.Serial(port, baud, timeout=1) as ser:
            sequence = 0
            voltage = 12.6
            current = 2.1
            remaining = 85
            
            while True:
                # Send heartbeat every 1 second
                heartbeat = create_heartbeat_message()
                ser.write(heartbeat)
                print(f"Sent HEARTBEAT ({len(heartbeat)} bytes)")
                
                time.sleep(0.5)
                
                # Send battery status
                battery_msg = create_battery_status_message(voltage, current, remaining)
                # Update sequence number
                battery_msg = bytearray(battery_msg)
                battery_msg[2] = sequence & 0xFF
                battery_msg = bytes(battery_msg)
                
                ser.write(battery_msg)
                print(f"Sent BATTERY_STATUS: {voltage}V, {current}A, {remaining}%")
                
                # Simulate battery drain
                voltage -= 0.01
                current += 0.05
                remaining -= 1
                if remaining < 20:
                    remaining = 85
                    voltage = 12.6
                
                sequence = (sequence + 1) & 0xFF
                time.sleep(0.5)
                
    except serial.SerialException as e:
        print(f"Serial error: {e}")
    except KeyboardInterrupt:
        print("\nSimulator stopped")


def list_serial_ports():
    """List available serial ports"""
    import serial.tools.list_ports
    ports = serial.tools.list_ports.comports()
    print("Available serial ports:")
    for port in ports:
        print(f"  {port.device} - {port.description}")


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='MAVLink Simulator for Display testing')
    parser.add_argument('--port', default='/dev/cu.usbmodem1101', help='Serial port')
    parser.add_argument('--baud', type=int, default=57600, help='Baud rate')
    parser.add_argument('--list-ports', action='store_true', help='List available ports')
    
    args = parser.parse_args()
    
    if args.list_ports:
        list_serial_ports()
    else:
        mavlink_simulator(args.port, args.baud)
