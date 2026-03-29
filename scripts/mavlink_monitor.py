#!/usr/bin/env python3
"""
MAVLink Monitor Script

Monitors incoming MAVLink messages from ESP32-S3 and displays them.
Useful for debugging MAVLink communication.

Usage:
    python3 mavlink_monitor.py --port /dev/cu.usbmodem1101 --baud 57600

Note: This script is NOT compiled with the project. It's a helper utility.
"""

import serial
import struct
import time
import argparse

# MAVLink constants
MAVLINK_START_BYTE_1 = 0xFE  # MAVLink 1
MAVLINK_START_BYTE_2 = 0xFD  # MAVLink 2


def parse_mavlink_message(data: bytes) -> dict:
    """Parse a MAVLink message"""
    if len(data) < 8:
        return None
    
    msg_len = data[1]
    seq = data[2]
    sys_id = data[3]
    comp_id = data[4]
    msg_id = data[5]
    payload = data[6:6+msg_len]
    checksum = data[6+msg_len:8+msg_len]
    
    return {
        'length': msg_len,
        'sequence': seq,
        'system_id': sys_id,
        'component_id': comp_id,
        'message_id': msg_id,
        'payload': payload,
        'checksum': checksum
    }


def decode_battery_status(payload: bytes) -> dict:
    """Decode battery status from payload"""
    if len(payload) < 30:
        return None
    
    try:
        voltages = struct.unpack('<10H', payload[0:20])
        current = struct.unpack('<h', payload[20:22])[0] / 100.0  # in 10mA units
        consumed = struct.unpack('<H', payload[22:24])[0]
        remaining = struct.unpack('<h', payload[24:26])[0]
        
        # Get first non-65535 voltage
        voltage = None
        for v in voltages:
            if v != 65535:
                voltage = v / 1000.0  # convert from mV to V
                break
        
        return {
            'voltage': voltage,
            'current': current,
            'consumed': consumed,
            'remaining_percent': remaining
        }
    except Exception as e:
        return {'error': str(e)}


def decode_heartbeat(payload: bytes) -> dict:
    """Decode heartbeat message"""
    if len(payload) < 9:
        return None
    
    try:
        custom_mode = struct.unpack('<I', payload[0:4])[0]
        type_ = payload[4]
        autopilot = payload[5]
        base_mode = payload[6]
        system_status = payload[7]
        mavlink_version = payload[8]
        
        return {
            'custom_mode': custom_mode,
            'type': type_,
            'autopilot': autopilot,
            'base_mode': base_mode,
            'system_status': system_status,
            'mavlink_version': mavlink_version
        }
    except Exception as e:
        return {'error': str(e)}


def mavlink_monitor(port: str, baud: int = 57600):
    """Monitor MAVLink messages"""
    print(f"MAVLink Monitor starting on {port} at {baud} baud")
    print("Press Ctrl+C to stop\n")
    
    message_names = {
        0: 'HEARTBEAT',
        147: 'BATTERY_STATUS',
        1: 'SYS_STATUS',
        33: 'GLOBAL_POSITION_INT',
    }
    
    try:
        with serial.Serial(port, baud, timeout=0.1) as ser:
            buffer = b''
            
            while True:
                data = ser.read(256)
                if data:
                    buffer += data
                    
                    # Look for MAVLink start byte
                    while True:
                        start_idx = buffer.find(bytes([MAVLINK_START_BYTE_1]))
                        if start_idx == -1:
                            buffer = b''
                            break
                        
                        if len(buffer) < start_idx + 8:
                            buffer = buffer[start_idx:]
                            break
                        
                        # Get message length
                        msg_len = buffer[start_idx + 1]
                        total_len = 6 + msg_len + 2  # header + payload + checksum
                        
                        if len(buffer) < start_idx + total_len:
                            buffer = buffer[start_idx:]
                            break
                        
                        # Extract message
                        msg_data = buffer[start_idx:start_idx + total_len]
                        buffer = buffer[start_idx + total_len:]
                        
                        # Parse message
                        msg = parse_mavlink_message(msg_data)
                        if msg:
                            msg_name = message_names.get(msg['message_id'], f'UNKNOWN({msg['message_id']})')
                            timestamp = time.strftime('%H:%M:%S')
                            
                            print(f"[{timestamp}] {msg_name}")
                            print(f"  System: {msg['system_id']}, Component: {msg['component_id']}, Seq: {msg['sequence']}")
                            
                            # Decode specific messages
                            if msg['message_id'] == 147:  # BATTERY_STATUS
                                battery = decode_battery_status(msg['payload'])
                                if battery and 'error' not in battery:
                                    print(f"  Battery: {battery['voltage']:.2f}V, {battery['current']:.2f}A, {battery['remaining_percent']}%")
                            
                            elif msg['message_id'] == 0:  # HEARTBEAT
                                hb = decode_heartbeat(msg['payload'])
                                if hb and 'error' not in hb:
                                    print(f"  Heartbeat: v{hb['mavlink_version']}, mode={hb['base_mode']}, status={hb['system_status']}")
                            
                            print()
                
                time.sleep(0.01)
                
    except serial.SerialException as e:
        print(f"Serial error: {e}")
    except KeyboardInterrupt:
        print("\nMonitor stopped")


def list_serial_ports():
    """List available serial ports"""
    import serial.tools.list_ports
    ports = serial.tools.list_ports.comports()
    print("Available serial ports:")
    for port in ports:
        print(f"  {port.device} - {port.description}")


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='MAVLink Monitor for debugging')
    parser.add_argument('--port', default='/dev/cu.usbmodem1101', help='Serial port')
    parser.add_argument('--baud', type=int, default=57600, help='Baud rate')
    parser.add_argument('--list-ports', action='store_true', help='List available ports')
    
    args = parser.parse_args()
    
    if args.list_ports:
        list_serial_ports()
    else:
        mavlink_monitor(args.port, args.baud)
