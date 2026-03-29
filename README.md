# Display SEED Round SLStudio

Čistý projekt pro SLStudio na platformě Seeed Studio Round Display (ESP32-S3).

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
