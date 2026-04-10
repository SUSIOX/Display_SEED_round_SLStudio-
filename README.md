# SYS EYE

Projekt i pro SLStudio na platformě Seeed Studio Round Display (ESP32-S3).

## Struktura projektu

- `src/main.cpp` - Hlavní program s inicializací displeje a LVGL
- `src/lv_conf.h` - Konfigurace LVGL pro round display
- `platformio.ini` - PlatformIO konfigurace pro ESP32-S3

## Hardwarové nastavení

- Board: Seeed Studio XIAO ESP32-S3
- Display: Round Display 240x240px (GC9A01)
- Touch: Kapacitní touch controller

## Kompilace a nahrání

```bash
pio run
pio run --target upload
```

## Použití

Projekt je připravený na integraci UI z SLStudio. Stačí nahrát UI soubory do `src/` adresáře a zkompilovat.

## MAVLink Zapojení

Pro připojení k flight controlleru:

- Flight Controller TX → Xiao D3 (GPIO3) RX
- Flight Controller RX → Xiao D5 (GPIO5) TX  
- GND → GND
- Baudrate: 57600

## Pinout displeje

- TFT_CS: GPIO2
- TFT_DC: GPIO4  
- TFT_SCLK: GPIO7
- TFT_MOSI: GPIO9
- TFT_BL: GPIO6
- TOUCH_INT: GPIO44

## MeshCore Telemetry (KISS)
Součástí projektu je také integrace dedikovaného KISS framingu, který odesílá GPS pozici a události z MAVLinku do externího MeshCore Node modulu (Heltec CT62 s KISS Modem firmware).

### Výchozí režim: Geowork API JSON
Výchozí formát pro pozemní stanici s přímou integrací na Geowork API:

- Komunikace: Sériová na **115200 baud** (RX/TX přes spodní JTAG pady)
- RX: GPIO 41 (MTDI) purple
- TX: GPIO 39 (MTCK) white
- **KISS Frame Type: 0x01** (Geowork JSON payload)
- Formát: `{"lat":48.2082,"lng":16.3738,"projectId":"PROJECT_ID_PLACEHOLDER","logLocation":true}`
- Pozemní stanice nahradí `PROJECT_ID_PLACEHOLDER` skutečným projectId a pošle na Geowork API

### Legacy režim: NMEA 0183
Pro starší integrace odkomentujte v `config.h`:
```cpp
#define TELEMETRY_MODE_NMEA
```

- **KISS Frame Type: 0x00** (NMEA věty)
- Podporované věty: `$GPGGA`, `$GPRMC`, `$GPHOME`, `$GPTRN`

### Dokumentace
Podrobnosti o formátu: Viz `MESHCORE_PROTOCOL.md`
