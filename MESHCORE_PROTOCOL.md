# MeshCore <-> ESP32 Display Protocol v1.2 (KISS)

Tato dokumentace definuje sériový komunikační protokol mezi MeshCore LoRa uzlem a SLStudio Round Display (XIAO ESP32-S3) pomocí KISS framingu.

## 1. Fyzické rozhraní
- **Konektor**: JTAG plošky na spodní straně XIAO desky.
- **Parametry**: 115200 baud, 8N1.
- **RX (vstup do displeje)**: MTDI / GPIO 41
- **TX (výstup z displeje)**: MTCK / GPIO 39

## 2. KISS Framing Protokol

Všechna data jsou posílana v KISS framech dle standardu KA9Q/K3MC.

### Frame struktura:
```
┌──────┬───────────┬──────────────────┬──────┐
│ FEND │ Type Byte │ Data (escaped)   │ FEND │
│ 0xC0 │   0x00    │ NMEA sentence... │ 0xC0 │
└──────┴───────────┴──────────────────┴──────┘
```

### KISS Control Characters:
| Byte | Name  | Popis |
|------|-------|-------|
| 0xC0 | FEND  | Frame End (frame delimiter) |
| 0xDB | FESC  | Frame Escape |
| 0xDC | TFEND | Transposed FEND (escaped 0xC0) |
| 0xDD | TFESC | Transposed FESC (escaped 0xDB) |

### Escaping pravidla:
- Byte 0xC0 (FEND) v datech → poslat jako 0xDB 0xDC
- Byte 0xDB (FESC) v datech → poslat jako 0xDB 0xDD
- Ostatní bajty → poslat přímo

### Type Byte:
- **0x00** - Data frame (NMEA telemetrie)

### Příklad KISS frame s $GPGGA:
```
C0 00 24 47 50 47 47 41 ... 0D 0A C0
^  ^  ^ NMEA text ($GPGGA...)     ^
|  |  └ CR/LF                     |
|  └ Type=0x00                    └ FEND (konec)
└ FEND (začátek)
```

## 3. Telemetrie (Displej -> MeshCore)
Displej posílá GPS data ve formátu NMEA 0183 zabaleném v KISS framech. MeshCore KISS Modem automaticky forwarduje payload do LoRa sítě jako textovou zprávu.

**Firmware MeshCore:** `KISS Modem` (meshcore.co.uk/flasher.html - Heltec CT62)
**Zapojení:** Display TX (GPIO39) → Heltec RX (GPIO20)
**Baudrate:** 115200 8N1

### Podporované věty:
1. **$GPGGA**: Standardní pozice, nadmořská výška, počet satelitů a kvalita fixu.
2. **$GPRMC**: Standardní pozice, čas, rychlost a kurz (Heading).
3. **$GPHOME**: Home pozice (Take-off). Odesílá se automaticky v momentě, kdy autopilot nastaví Home.
   - Formát: `$GPHOME,lat,lon,alt*hh`
4. **$GPTRN**: Event transice VTOL (Quad -> Plane). Odesílá se v momentě zahájení a ukončení přechodu do dopředného letu.
   - Formát: `$GPTRN,lat,lon,alt*hh`

---

## 4. Ovládání (MeshCore -> Displej)
MeshCore může měnit chování displeje zasláním ASCII příkazů zakončených `\n` uvnitř KISS frame (typ 0x00).

### Nastavení intervalu odesílání:
Změní frekvenci, se kterou displej posílá periodické NMEA věty ($GPGGA, $GPRMC).

- **Příkaz**: `!SET_INT:[ms]`
- **Příklad**: `!SET_INT:300000` (změní na 5 minut)
- **Rozsah**: 1000 - 300000 ms (1 sekunda až 5 minut).
- **Default**: 3000 ms.

---

## 5. Poznámky pro vývojáře MeshCore
- **KISS Modem**: Na MeshCore straně použij firmware `KISS Modem` (ne Companion/Bridge)
- **Eventy**: Věty `$GPHOME` a `$GPTRN` jsou prioritní a odesílají se okamžitě mimo nastavený periodický interval.
- **Transice**: `$GPTRN` se posílá při změně MAVLink stavu `MAV_VTOL_STATE_TRANSITION_TO_FW`.
