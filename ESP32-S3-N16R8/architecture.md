# Project Architecture Documentation

## Overview
This is a **PlatformIO project** for the **ESP32-S3-DevKitC-1-N8R2** development board (8MB Flash, 2MB PSRAM). The project implements a **multi-component test firmware** for:
- **2x 2-Channel Relay Modules** (4 relays total)
- **Onboard RGB LED** (NeoPixel WS2812B)
- **Atlas Scientific EZO-pH & EZO-EC sensors** (UART/SoftwareSerial @9600 baud)

The firmware:
- Toggles relays every 30s
- Cycles RGB continuously
- Reads/parses pH/EC every 2s, prints to USB CDC Serial

**Current Status**: Extended test firmware with sensors. Custom `pins.h`. Raw SoftwareSerial protocol for EZO (no external libs needed).

## Directory Structure
```
ESP32-S3-N16R8/
├── .gitignore                  # Git ignore rules (standard PlatformIO template)
├── platformio.ini             # Project configuration
├── include/                   # Header files (empty except README)
│   └── README                 # Standard header usage docs
├── lib/                       # Custom libraries (empty except README)
│   └── README                 # Library structure docs
├── src/                       # Source code
│   └── main.cpp               # Main firmware (only source file)
└── test/                      # Unit tests (empty except README)
    └── README                 # Testing docs
```

- **No additional source files**, custom headers, or libraries detected.
- **Empty directories**: `include/`, `lib/`, `test/` contain only template READMEs.

## Core Files

### 1. `platformio.ini` (Project Configuration)
```ini
[env:esp32-s3-devkitc-1]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino
monitor_speed = 115200

build_flags =
    -Os                    # Optimize for size
    -DCORE_DEBUG_LEVEL=1   # Debug level 1
    -DARDUINO_USB_CDC_ON_BOOT=1  # USB CDC on boot
    -DCONFIG_ESP_TASK_WDT_INIT=1
    -DCONFIG_ESP_TASK_WDT_TIMEOUT_S=5  # 5s watchdog

lib_deps =
    adafruit/Adafruit NeoPixel@^1.12.0  # For RGB LED control
```
**Key Configs**:
- **Board**: ESP32-S3-DevKitC-1 (N8R2 variant)
- **Framework**: Arduino
- **Optimizations**: Size-optimized (`-Os`), debug logging, USB serial, watchdog timer.
- **Dependencies**: Only Adafruit NeoPixel library.

### 2. `src/main.cpp` (Main Firmware)
**Purpose**: Hardware test routine.

**Pin Assignments** (from `include/pins.h`):
| Component       | GPIO RX/TX | Notes                          |
|-----------------|------------|--------------------------------|
| Relay 1A        | 4          | Module 1, Channel 1            |
| Relay 1B        | 5          | Module 1, Channel 2            |
| Relay 2A        | 6          | Module 2, Channel 1            |
| Relay 2B        | 7          | Module 2, Channel 2            |
| EZO-pH UART     | 16/17      | SoftwareSerial RX/TX           |
| EZO-EC UART     | 18/8       | SoftwareSerial RX/TX           |
| RGB LED         | 48         | Onboard NeoPixel               |

**Behavior**:
- **setup()**:
  - Initializes Serial at 115200 baud.
  - Configures relays as OUTPUT, sets LOW (OFF).
  - Initializes NeoPixel (1 LED, brightness 30%).
  - Prints wiring info and test description.
- **loop()**:
  - **Relays**: Toggle all HIGH/LOW every 30s (`TOGGLE_INTERVAL=30000ms`).
  - **RGB**: Continuous HSV color wheel cycle (`hue += 256` every 30ms).

**Libraries Used**:
- `Arduino.h` (core)
- `Adafruit_NeoPixel.h` (RGB control)

**Serial Output Example**:
```
================================
  Relay + RGB LED Test
  Board: ESP32-S3-N8R2
================================

  Relay 1 on GPIO4 -> OFF
  Relay 2 on GPIO5 -> OFF
  ...
  RGB LED on GPIO48

  Relays toggle every 30 seconds.
  RGB cycling colors continuously.

[30s] Relays -> ON (HIGH)
  Relay 1 (GPIO4) = HIGH
  ...
```

## Software Architecture
```
+-------------------+     
| Hardware Drivers  |
| - SoftwareSerial  | --> EZO UART Protocol (r,0 read, parse)
| - Digital Pins    | --> Relays
| - NeoPixel        | --> RGB
+-------------------+
          |
          v
+-------------------+     +-------------------+
| Abstraction (pins.h)   |   Timers (millis()) |
+-------------------+     + Relay/Read/RGB   |
          |                           |
          v                           v
    +-------------+           +-----------------+
    |   Arduino   |           |   main.cpp loop()|
    |  Framework  |           +-----------------+
    +-------------+
          |
          v
+-------------------+
| ESP32-S3 Core (FreeRTOS/HAL) |
+-------------------+
```

- **Single-File Monolith**: All logic in `main.cpp`. No modularity.
- **Non-Blocking**: Uses `millis()` for timing (no `delay()` except 30ms for RGB smoothness).
- **State Management**: Global `relayState` bool and `lastToggle` timestamp.

## Hardware Architecture
```
ESP32-S3-N8R2
├── GPIOs 4-7 → 2x 2-Channel Relay Modules (NO/NC/COM)
└── GPIO48  → Onboard RGB LED (WS2812B)
```

**Assumptions**:
- Relays active-HIGH (HIGH = ON).
- Standard 5V relay modules (ESP32 GPIO safe with logic level).
- Single RGB LED.

## Dependencies
| Library              | Version | Purpose             |
|----------------------|---------|--------------------|
| Adafruit NeoPixel    | ^1.12.0 | RGB LED control   |
| Arduino SoftwareSerial | Built-in | EZO UART comms |

**Custom**: `include/pins.h`
**Raw Protocol**: EZO-pH/EC (no lib, direct UART commands/parse).

## Build & Deployment
```
# Build
pio run

# Upload (auto-detects port)
pio run --target upload

# Monitor
pio device monitor
```
- **Upload Port**: Commented as `COM12` (Windows).
- **Monitor**: 115200 baud.

## Testing
- **No unit tests** implemented (`test/` empty).
- **Manual Verification**:
  1. Relays click/toggle every 30s.
  2. RGB cycles red→green→blue→purple→...
  3. Serial logs match expected output.

## Limitations & Potential Improvements
1. **No WiFi/OTA/HTTP** (bare-metal test).
2. **Hardcoded Pins**: Move to config header.
3. **No Error Handling**: e.g., NeoPixel init failure.
4. **Synchronous RGB**: Could use interrupt/timer.
5. **Empty Modules**: `include/`, `lib/`, `test/` unused.
6. **No Configurability**: Fixed 30s interval, brightness.

**Scalability**: Easy to extend with FreeRTOS tasks, WiFi, web dashboard, MQTT, etc.

## Code Quality
- **Strengths**: Clean, well-commented, descriptive Serial output.
- **Issues**: Magic numbers, globals, no abstraction.
- **Style**: Modern C++ (Arduino), consistent indentation.

---

*Generated by analyzing full project structure and contents. Project is minimal/test-oriented.*
