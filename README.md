# LED Tube

## Overview
This project controls **2-meter-long LED tubes** built from a **WS2812B LED strip with 120 LEDs**.
The firmware runs on an **ESP32 (Arduino Core)** and supports standalone operation, a custom
**wireless DMX protocol over ESP-NOW**, wired **DMX input**, and an (experimental) **Art-Net** path.

The codebase builds two different devices from the same sources, selected via the `DEVICE_MODE`
build flag in [`platformio.ini`](platformio.ini):

| Device | PlatformIO env | Board | Role |
| ------ | -------------- | ----- | ---- |
| **Tube**   | `led_tube` | Seeed XIAO ESP32-S3 | Drives one LED tube and receives commands |
| **Bridge** | `bridge`   | ESP32 dev board     | Generates/forwards commands to the tubes  |

## Hardware Components
Schematics and PCB designs live in `pcb/`, enclosure models in `body/`.

Datasheets for the components used are not redistributed in this repository. The part numbers are
given in the schematics and can be looked up on the manufacturers' sites.

### User Interface (both devices)
- **Three buttons**: Up, Down, Enter
- **A monochrome 0.91" OLED display** (SSD1306) connected via **I2C**, showing the current menu item

## Operating Modes

### Tube
1. **Standalone** – the tube shows a locally selected color and effect.
2. **Wireless DMX** – the tube listens on a configurable DMX address and renders the data it
   receives from a bridge over ESP-NOW.

### Bridge
1. **Master** – generates beat-synchronized choreography (driven by a configurable BPM) for a
   configurable number of tubes and broadcasts it over ESP-NOW.
2. **DMX** – reads a wired DMX universe (via `esp_dmx`) and forwards the relevant slice to the
   tubes over ESP-NOW.
3. **Art-Net** – receives Art-Net over Ethernet (W5500). *Experimental / partially implemented.*

### Channels
The **Channel** menu item (1–3) maps to distinct Wi-Fi channels for the ESP-NOW link, so multiple
independent light systems can run side by side without interference. The bridge and its tubes must
use the same channel.

## Wireless Protocol
Communication uses **ESP-NOW broadcast** (no pairing). The payload is a lightweight DMX frame:

```
[ size (1 byte) ][ start_address (1 byte) ][ data ... up to 240 bytes ]
```

Each tube occupies `TUBE_DATASET_SIZE` (4) channels: `effect, red, green, blue`.

## Pin Configuration
Pins are defined in [`src/Config.hpp`](src/Config.hpp).

### Tube (Seeed XIAO ESP32-S3)
- **LED data**: GPIO 4
- **OLED (I2C)**: SDA GPIO 3, SCL GPIO 2
- **Buttons**: Up GPIO 9, Down GPIO 7, Enter GPIO 8

### Bridge (ESP32 dev board)
- **LED data**: GPIO 14
- **OLED (I2C)**: SDA GPIO 26, SCL GPIO 25
- **Buttons**: Up GPIO 27, Down GPIO 32, Enter GPIO 33
- **DMX input**: RX GPIO 16, EN/TX GPIO 17
- **Ethernet (W5500, SPI)**: CS GPIO 5, SCK GPIO 18, MISO GPIO 19, MOSI GPIO 23, RST GPIO 4

## Building & Flashing
This is a [PlatformIO](https://platformio.org/) project.

```bash
# Tube firmware
pio run -e led_tube -t upload

# Bridge firmware
pio run -e bridge -t upload
```

Configuration values selected in the on-device menu are persisted to NVS flash and restored on boot.

### Dependencies
Managed automatically by PlatformIO (see [`platformio.ini`](platformio.ini)):
- FastLED
- Adafruit GFX Library
- Adafruit SSD1306
- esp_dmx
- Ethernet
- ArtNet

ESP-NOW / Wi-Fi and `Preferences` (NVS) are provided by the ESP32 Arduino Core.
