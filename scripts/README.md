# Helper Scripts for MAVLink Display Project

This directory contains helper scripts that are NOT compiled with the main project.
They are used for testing, simulation, and debugging.

## Scripts

### mavlink_simulator.py
Simulates MAVLink messages for testing the display without actual hardware.

```bash
# Run simulator
python3 scripts/mavlink_simulator.py --port /dev/cu.usbmodem1101 --baud 57600

# List available ports
python3 scripts/mavlink_simulator.py --list-ports
```

Sends:
- HEARTBEAT messages every 1 second
- BATTERY_STATUS messages with simulated voltage/current

### mavlink_monitor.py
Monitors and displays incoming MAVLink messages for debugging.

```bash
# Run monitor
python3 scripts/mavlink_monitor.py --port /dev/cu.usbmodem1101 --baud 57600

# List available ports
python3 scripts/mavlink_monitor.py --list-ports
```

Decodes and displays:
- Message types and IDs
- System/component IDs
- Battery voltage, current, percentage
- Heartbeat status

## Requirements

Install Python dependencies:
```bash
pip3 install pyserial
```

## Notes

- These scripts are utility tools only
- Not included in PlatformIO build
- Used for development and testing
