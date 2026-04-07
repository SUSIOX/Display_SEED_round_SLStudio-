#!/usr/bin/env python3
"""
Pixhawk TELEM2 Diagnostic Tool
Zkontroluje, zda Pixhawk posílá data na TELEM2 portu
a pokusí se detekovat správný baudrate.

Použití:
1. Odpoj Xiao ESP32 od PC (pouze na chvíli)
2. Připoj Pixhawk TELEM2 → USB-serial adaptér (např. FTDI, CP2102)
3. Uprav SERIAL_PORT níže podle tvého systému
4. Spusť: python3 pixhawk_telem2_check.py

Autodetect baudrate bude postupně zkoušet nejběžnější rychlosti.
"""

import serial
import serial.tools.list_ports
import time
import sys

# Nastav správný port podle svého systému:
# Windows: "COM3" nebo "COM4"
# macOS: "/dev/tty.usbserial-*" nebo "/dev/cu.usbserial-*"
# Linux: "/dev/ttyUSB0" nebo "/dev/ttyACM0"
SERIAL_PORT = "/dev/tty.usbserial-110"  # <-- UPRAV TOTO!

BAUDRATES = [57600, 921600, 115200, 38400, 19200, 9600]

def list_available_ports():
    """Vypíše všechny dostupné sériové porty."""
    print("Dostupné sériové porty:")
    print("-" * 40)
    ports = serial.tools.list_ports.comports()
    for port in sorted(ports):
        print(f"  {port.device} - {port.description}")
    print()

def try_baudrate(port, baudrate, timeout=3):
    """Zkusí daný baudrate po dobu timeout sekund."""
    print(f"\nTestuji baudrate: {baudrate}")
    print("-" * 40)
    
    try:
        with serial.Serial(port, baudrate, timeout=1) as ser:
            bytes_received = 0
            start_time = time.time()
            
            # Čekej a sbírej data
            while time.time() - start_time < timeout:
                if ser.in_waiting:
                    data = ser.read(ser.in_waiting)
                    bytes_received += len(data)
                    
                    # Vypiš HEX formát prvních 50 bytů pro kontrolu
                    hex_str = ' '.join(f'{b:02X}' for b in data[:50])
                    print(f"  Přijato {len(data):3d} B | HEX: {hex_str[:100]}...")
                    
                    # Detekce MAVLink start byte (0xFD pro v2, 0xFE pro v1)
                    for b in data:
                        if b in [0xFD, 0xFE]:
                            print(f"\n*** DETEKOVÁN MAVLink start byte: {b:02X} ***")
                            print(f"*** Správný baudrate je pravděpodobně: {baudrate} ***")
                            return True, baudrate
            
            if bytes_received > 0:
                print(f"  Celkem přijato: {bytes_received} bytů (ale bez MAVLink hlaviček)")
                return True, baudrate
            else:
                print(f"  Žádná data nepřijata")
                return False, baudrate
                
    except serial.SerialException as e:
        print(f"  Chyba: {e}")
        return False, baudrate

def main():
    print("=" * 60)
    print("  Pixhawk TELEM2 Diagnostic Tool")
    print("=" * 60)
    print()
    
    # Vypiš dostupné porty
    list_available_ports()
    
    # Zkontroluj nastavení portu
    if SERIAL_PORT == "/dev/tty.usbserial-110":
        print("⚠️  UPOZORNĚNÍ: Nastav SERIAL_PORT podle svého systému!")
        print("   Změň řádek: SERIAL_PORT = \"tvůj_port\"")
        print()
        print("Často používané porty:")
        print("   macOS: /dev/tty.usbserial-XXXX nebo /dev/cu.usbserial-XXXX")
        print("   Linux: /dev/ttyUSB0 nebo /dev/ttyACM0")
        print("   Windows: COM3, COM4, atd.")
        print()
        return
    
    print(f"Testuji port: {SERIAL_PORT}")
    print(f"Baudraty k testování: {BAUDRATES}")
    print()
    
    # Zkus každý baudrate
    for baud in BAUDRATES:
        success, baudrate = try_baudrate(SERIAL_PORT, baud)
        if success:
            print()
            print("=" * 60)
            print(f"  NALEZENO! Pixhawk posílá data na {baudrate} baud")
            print("=" * 60)
            print()
            print("Změň v platformio.ini nebo main.cpp:")
            print(f"  #define MAVLINK_BAUD {baudrate}")
            return
    
    print()
    print("=" * 60)
    print("  Pixhawk neposílá data na žádném testovaném baudratu")
    print("=" * 60)
    print()
    print("Možné příčiny:")
    print("  1. Špatně připojený kabel (zkus otočit TX/RX)")
    print("  2. Pixhawk má vypnutý TELEM2 port")
    print("  3. Pixhawk není napájený (potřebuje baterii nebo USB)")
    print("  4. Špatný sériový port v nastavení")
    print("  5. Pixhawk má jiný baudrate (zkus 4800 nebo 230400)")
    print()
    print("Zkontroluj v QGroundControl:")
    print("  - SER_TEL2_BAUD parameter")
    print("  - MAV_1_CONFIG = TELEM 2")

if __name__ == "__main__":
    main()
