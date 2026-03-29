# Display SEED Round SLStudio

Čistý projekt pre SLStudio na platforme Seeed Studio Round Display (ESP32-S3).

## Štruktúra projektu

- `src/main.cpp` - Hlavný program s inicializáciou displeja a LVGL
- `src/lv_conf.h` - Konfigurácia LVGL pre round display
- `platformio.ini` - PlatformIO konfigurácia pre ESP32-S3

## Hardvérové nastavenie

- Board: Seeed Studio XIAO ESP32-S3
- Display: Round Display 240x240px (GC9A01)
- Touch: Kapacitný touch controller

## Kompilácia a nahratie

```bash
pio run
pio run --target upload
```

## Použitie

Projekt je pripravený na integráciu UI z SLStudio. Stačí nahrať UI súbory do `src/` adresára a skompilovať.

## Pinout

- TFT_CS: GPIO2
- TFT_DC: GPIO4  
- TFT_SCLK: GPIO7
- TFT_MOSI: GPIO9
- TFT_BL: GPIO6
- TOUCH_INT: GPIO44
