# MeshCore <-> ESP32 Display Protocol v1.1

Tato dokumentace definuje sériový komunikační protokol mezi MeshCore LoRa uzlem a SLStudio Round Display (XIAO ESP32-S3).

## 1. Fyzické rozhraní
- **Konektor**: JTAG plošky na spodní straně XIAO desky.
- **Parametry**: 115200 baud, 8N1.
- **RX (vstup do displeje)**: MTDI / GPIO 41
- **TX (výstup z displeje)**: MTCK / GPIO 39

## 2. Telemetrie (Displej -> MeshCore)
Displej emuluje standardní GPS modul a posílá data ve formátu NMEA 0183 + vlastní věty pro specifické události.

### Podporované věty:
1. **$GPGGA**: Standardní pozice, nadmořská výška, počet satelitů a kvalita fixu.
2. **$GPRMC**: Standardní pozice, čas, rychlost a kurz (Heading).
3. **$GPHOME**: Home pozice (Take-off). Odesílá se automaticky v momentě, kdy autopilot nastaví Home.
   - Formát: `$GPHOME,lat,lon,alt*hh`
4. **$GPTRN**: Event transice VTOL (Quad -> Plane). Odesílá se v momentě zahájení a ukončení přechodu do dopředného letu.
   - Formát: `$GPTRN,lat,lon,alt*hh`

---

## 3. Ovládání (MeshCore -> Displej)
MeshCore může měnit chování displeje zasláním ASCII příkazů zakončených `\n`.

### Nastavení intervalu odesílání:
Změní frekvenci, se kterou displej posílá periodické NMEA věty ($GPGGA, $GPRMC).

- **Příkaz**: `!SET_INT:[ms]`
- **Příklad**: `!SET_INT:300000` (změní na 5 minut)
- **Rozsah**: 1000 - 300000 ms (1 sekunda až 5 minut).
- **Default**: 3000 ms.

---

## 4. Poznámky pro vývojáře MeshCore
- **Eventy**: Věty `$GPHOME` a `$GPTRN` jsou prioritní a odesílají se okamžitě mimo nastavený periodický interval.
- **Transice**: `$GPTRN` se posílá při změně MAVLink stavu `MAV_VTOL_STATE_TRANSITION_TO_FW`.
