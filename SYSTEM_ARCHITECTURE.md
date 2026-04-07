# Architektura Systému & Rozložení RTOS (ESP32-S3)

Tento dokument popisuje architekturu a rozdělení subsystémů pro MAVLink displej (Seeed Studio Round). Systém se opírá o asymetrickou definici vláken (FreeRTOS) pro maximální efektivitu a paralelizaci činností bez blokování UI obrazovky rozsáhlým síťovým In/Out tokem.

## 1. Topologie FreeRTOS (Dual-Core)

Základní koncepce spočívá v absolutním oddělení zpracování datových toků (Sériové porty) od vykreslování obrazu (SPI Displej, LVGL, a detekce vstupu uživatele). 

### Jádro 0: COMM Core (Protokolární Jádro)
Jádro s vysokou spolehlivostí zcela zproštěno jakékoliv prodlevy vlivem displeje. Běží na něm komunikační úkoly:
- **`mavlink_rx_task` (Priorita 2):** Vyčítá přes `Serial1` (115200 bps) telemetrii jdoucí od autopilota (dronu). Příchozí byty jsou bleskově parsovány a zapisovány do globální paměťové struktury `mavlink_data`.
- **`meshcore_telemetry_task` (Priorita 1):** Komunikuje se vzdáleným Node prostředím skrze spodní JTAG piny (`Serial2` UART). Přebírá zachycená MAVLink data z paměti a emuluje je do formátu tradiční NMEA GPS (`$GPGGA`, `$GPTRN`). Může také přijímat nastavení (povel `!SET_INT`). 

### Jádro 1: APP Core (Uživatelské rozhraní)
Funguje jako podřízené jádro pro rutinní aktualizaci rozhraní (volá smyčku `loop()`):
- **GUI Engine (LVGL):** Spravuje překreslování obrazu a plynulé 2D animace.
- **I2C Touch:** Čte status polohy prstu napřímo přes polling na I2C sběrnici dotykového panelu.

---

## 2. Diagram Architektury

```mermaid
flowchart TD
    subgraph Core0["Jádro 0 (Komunikace)"]
    direction TB
        MavRX["Task: mavlink_rx_task\n(Priorita: 2)"]
        MeshTX["Task: meshcore_telemetry_task\n(Priorita: 1)"]
        UART1[/"Serial1 (UART)\nMAVLink 115200bps"/]
        UART2[/"Serial2 (UART)\nMeshCore 115200bps"/]

        UART1 -->|Čtení po bytech| MavRX
        MavRX -->|Spouští event flags| MeshTX
        MeshTX -->|NMEA Data ($GPGGA, $GPTRN)| UART2
        UART2 -->|Commandy (!SET_INT)| MeshTX
    end

    subgraph Memory["RAM (Sdílená paměť)"]
        RTOSMutex{"mavlink_mutex\n(Thread-Safe Lock)"}
        MavStruct[("mavlink_data (Struct)")]
    end

    subgraph Core1["Jádro 1 (UI Rozhraní)"]
    direction TB
        MainLoop["Task: loop()\n(Priorita: 1)"]
        LVGL["LVGL Renderer\nlv_timer_handler()"]
        Touch["I2C Polling\n(Dotyk)"]
        SPI[/"SPI Displej\nGC9A01"/]
        
        MainLoop --> LVGL
        MainLoop --> Touch
        LVGL -->|Zápis pole obrazu| SPI
    end

    MavRX -- Zamyká/Zapisuje --> RTOSMutex
    MainLoop -- Zamyká/Čte --> RTOSMutex
    RTOSMutex --- MavStruct
```

---

## 3. Předávání prostředků a Thread-Safety

K zabránění *Race Condition* a k poškození paměti při současném čtení a zápisu mezi Jádrem 0 a Jádrem 1 je uplatněn **FreeRTOS Mutex** (`mavlink_mutex`).
- Jakákoliv manipulace se sdílenou entitou `mavlink_data` obnáší zamknutí struktury s Timeoutem 10 ms.
- Zápis paketu na Jádře 0 je atomický a trvá řádově nižší mikrosekundy. Nebrzdí tak renderovací pipeline Jádra 1.

## 4. Letové události (Event Triggers)

Pro redukci toku NMEA dat na MESHCORE síti se odesílají cyklické updaty polohy v nastavitelném (1 - 300 sekundovém) intervalu. Některé kritické chvíle ovšem vyžadují okamžité odeslání do sítě ignorujíc tyto intervaly. Řízení těchto zpráv probíhá v architektuře jako záchyt vlajky z MAVLinku:

1. **Vlajka (Home Position):** Při příjmu zprávy #242 z MAVLinku se do paměti nastavní `pending_home_send = true`. MESHCORE task na Jádře 0 si to při prvním volném ticku získá a odesílá na Serial2 GPS NMEA formát s prefixem `$GPHOME`.
2. **Vlajka (Transition Event):** Autopilot odesílá `EXTENDED_SYS_STATE` (zpráva #245). Při rozpoznání přechodové fáze (VTOL to Fixed Wing Mode) se propíše `pending_transition_send = true` a pro síť se generuje věta `$GPTRN`.

---

## 5. Náročnost a Analýza Výkonu

Systém je kompilován nad `PlatformIO` s masivní rezervou výpočetního i paměťového výkonu:

### Odtisk v paměti (Memory Footprint)
Díky statické alokaci převážné většiny struktur a efektivnímu balení LVGL buffrů je spotřeba pevně vymezená:
- **SRAM (Operační paměť):** ~ 60 % obsazeno (alokováno cca 197 KB z 327 KB).
- **Flash (Úložiště programu):** ~ 24 % obsazeno (alokováno cca 810 KB z dostupných 3,3 MB).
- Projektu tak zůstává zcela **nepokryto více než 2,5 MB Flash a 130 KB volné SRAM**, ideální pro případné přidání databázových map nebo ukládání lokálních letových logů na Flash.

### Zátěž a rezerva procesorů (ESP32-S3 @ 240 MHz)
Extrémně vysoký takt procesoru na úrovni 240 MHz garantuje nulové Drop Raty při dekódování:
- **Jádro 0 (I/O Komunikace):** Snímá a generuje desítky stringů za vteřinu, což vytěžuje procesor v průměru jen z **cca 1 až 4 %**. Většina času cyklu je v nativním `IDLE` (přes asynchronní `vTaskDelay()`).
- **Jádro 1 (Application):** Vyrenderování komplexního stíněného prvku na 240x240 LCD po SPI trvá řádově několik málo desítek milisekund, vyvolávajíc tak zátěž max **15 až 30 %** v lokálních špičkách.

Díky této obrovské prostorové a výpočetní bariéře je systém zcela rigidní vůči budoucím integracím, jak co se týká přidávání nových Serial zařízení (senzorů štěkajících na jiných baudech), tak vizuálním inovacím v LVGL.

---

## 6. Ověřené Hardwarové Pinout Schéma

Klíčovým architektonickým pravidlem z testování bylo nalezení nekolidujících GPIO padů, ze kterých aktuálně displej saje obě komunikační linky a I2C:
Zde je absolutní pravda o zapojených I/O k aktuálnímu dni (Odpovídá souboru `xiao_pinout.h`):

| Pól desky | XIAO GPIO | Softwarová Role | Popis Omezení / Subsystém |
|---|---|---|---|
| **D0** | `GPIO 1` | **MAVLink TX** | UART Vysílač na Drone / FC. (Serial1) |
| **D1** | `GPIO 2` | `TFT_CS` | Vyhrazeno vnitřně pro Displej (Kreslení) |
| **D2** | `GPIO 3` | **MAVLink RX** | UART Přijímač z Drone / FC. (Serial1) |
| **D3** | `GPIO 4` | `TFT_DC` | Data/Command pin obrazovky |
| **D4** | `GPIO 5` | `I2C_SDA` | Dotykový I2C. *Zákaz modifikovat Pull-Up!* |
| **D5** | `GPIO 6` | `I2C_SCL` / `TFT_BL` | Dotyk clock a Podsvícení. *Zákaz GPIO manipů.* |
| **D6** | `GPIO 43`| `USB_TX` | Debug zprávy do PC terminálu. |
| **D7** | `GPIO 44`| (Nepoužito) | Původní konfliktní `TOUCH_INT`. Po přechodu na I2C Polling nyní volný pro případný fallback. |
| **D8** | `GPIO 7` | `TFT_SCLK`| SPI Clock displej. |
| **D9** | `GPIO 8` | **Volné (Relay)**| Momentálně použitelné jako spínač výkonového prvku. Může blokovat MISO port na zadní SD kartu. |
| **D10**| `GPIO 9` | `TFT_MOSI` | SPI displeje (Odesílá video byty) |

### Linka MeshCore (JTAG Pady Spodní Desky)
Důrazně upozorňujeme, že druhá síťová sběrnice (`Serial2`) není vytažena jako D-Pin, ale je nasměrována na testovací plošky (Pady) ze spodu desky. Není tak viditelná klasickému shield headeru.
- **MTDI Pad (GPIO 41)** = MeshCore RX
- **MTCK Pad (GPIO 39)** = MeshCore TX
