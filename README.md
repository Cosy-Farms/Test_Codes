# ESP32-S3 Hydroponics Controller

A comprehensive hydroponics monitoring and control system built on the ESP32-S3-DevKitC-1-N8R2 board. Handles EC/pH sensing, air and solution temperature, humidity, water level detection, peristaltic pump dosing, CO2 monitoring, and relay control for pumps and lighting.

## Hardware

- **MCU**: ESP32-S3-DevKitC-1-N8R2 (8MB Flash, 2MB PSRAM QSPI)
- **Framework**: Arduino via PlatformIO
- **Upload / Monitor**: UART port (COM13) at 115200 baud

## Pin Assignments

| Component | GPIO(s) | Notes |
|---|---|---|
| Relay 1A / 1B | 4, 5 | 5V coil, active HIGH |
| Relay 2A / 2B | 6, 7 | 5V coil, active HIGH |
| RGB NeoPixel | 48 | Onboard, NEO_GRB |
| Push Button 1 | 1 | INPUT_PULLUP, active LOW |
| Push Button 2 | 2 | INPUT_PULLUP, active LOW |
| EZO-pH | RX=16, TX=17 | UART1, 9600 baud, 3.3V |
| EZO-EC | RX=18, TX=8 | UART2, 9600 baud, 3.3V |
| DS18B20 (solution temp) | 38 | 4.7kΩ pull-up to 3.3V |
| DHT22 #1 (air T/RH) | 10 | 4.7–10kΩ pull-up to 3.3V |
| DHT22 #2 (air T/RH) | 11 | 4.7–10kΩ pull-up to 3.3V |
| Pump 1 PWM / DIR | 12, 13 | LEDC ch0, 10kΩ pull-down on PWM |
| Pump 2 PWM / DIR | 14, 15 | LEDC ch1, 10kΩ pull-down on PWM |
| Pump 3 PWM / DIR | 21, 33 | LEDC ch2, 10kΩ pull-down on PWM |
| Pump 4 PWM / DIR | 34, 35 | LEDC ch3, 10kΩ pull-down on PWM |
| MH-Z19E CO2 | RX=36, TX=40 | SoftwareSerial, 9600 baud, UART mode |
| XKC-Y26 water level #1 | 37 | 10k/20k divider (5V→3.3V) |
| XKC-Y26 water level #2 | 39 | 10k/20k divider (5V→3.3V) |

**Total GPIOs in use**: 26

## Detailed Wiring Notes

### Peristaltic Pumps (Kamoer KFS-HE1S10P)

12V DC brushless, 0.4A, one slowdown gear, silicone 3×5mm tubing. Each pump has 5 wires:

| Wire | Function | Connection |
|---|---|---|
| Red | Power + | +12V external supply |
| Black | Power – | GND (common with ESP32) |
| Blue | PWM speed (5–20kHz) | GPIO pin + **10kΩ pull-down to GND** |
| Yellow | Direction (LOW=CW, HIGH=CCW) | GPIO pin |
| Green | FG speed feedback (1 pulse/rev) | Leave floating unless RPM monitoring is needed |

The 10kΩ pull-down on the PWM line is essential — without it the pin floats during boot/reset and the pump can ghost-run. PWM runs at 10kHz with 8-bit resolution (0–255 duty).

### Water Level Sensors (XKC-Y26-V)

Capacitive non-contact sensor, 5–24V input. Output is **5V high/low** — an external 10k/20k divider is required to step down to 3.3V for ESP32 inputs:

```
Yellow wire ──┬── 10kΩ ──┬── ESP32 GPIO
              │          │
              │         20kΩ
              │          │
              └──────────┴── GND
```

5V × (20k / (10k + 20k)) = **3.33V** → safe for ESP32.

Black wire left floating selects NO mode (HIGH output when liquid is detected, matching the `WET` reading in firmware). Connecting Black to Blue selects NC mode (inverted).

### EZO pH/EC Sensors (Atlas Scientific)

UART mode, 9600 baud, 3.3V or 5V supply. Each sensor uses a dedicated hardware UART on the ESP32-S3. Calibration (single-point 1413 μS/cm for EC K=1.0 probe, multi-point for pH) is stored in the board itself and persists across sketch changes and power cycles.

EC output configured for EC-only (TDS, salinity, SG disabled) at firmware boot.

### DHT22 (DFRobot DFR0067 / SEN0137)

Single-wire digital sensor. Most DFRobot modules include an onboard pull-up; bare sensors need a 4.7–10kΩ pull-up from DATA to 3.3V. Minimum 2 seconds between reads — firmware reads every 5 seconds.

### DS18B20 (solution temperature)

Submersible probe, OneWire protocol. Single 4.7kΩ pull-up from DATA to 3.3V. Multiple probes can share the same bus but only index 0 is read.

### MH-Z19E CO2 Sensor (UART mode)

NDIR CO2 sensor, 0–5000 ppm range, ±50 ppm accuracy. Runs in UART mode (9600 baud) via SoftwareSerial since both hardware UARTs are occupied by the EZO boards.

| MH-Z19E Pin | ESP32 | Notes |
|---|---|---|
| VIN | +5V | Sensor needs 4.5–5.5V |
| GND | GND | Common ground |
| TX | GPIO 36 (ESP32 RX) | Sensor TX is 3.3V TTL — direct connection OK |
| RX | GPIO 40 (ESP32 TX) | ESP32 TX is 3.3V — compatible with sensor |
| HD | Leave floating | Manual zero-point calibration trigger |
| PWM | Leave floating | Unused in UART mode |

**ABC (Auto Baseline Calibration) disabled in firmware** for indoor hydroponics use. ABC assumes the sensor sees fresh-air (~400 ppm) baseline every 24 hours; in sealed grow rooms where plants consume CO2, this assumption breaks and the sensor drifts. Manual calibration via the HD pin or the `0x87` UART command in fresh outdoor air every few months keeps readings accurate.

First 3 minutes after power-on are warmup — firmware skips readings until the sensor reports ready.

### Push Buttons

Active LOW with internal pull-up — no external resistors needed. 50ms software debounce.

## LEDC Channel Map

| Channel | Pump | GPIO |
|---|---|---|
| 0 | Pump 1 | 12 |
| 1 | Pump 2 | 14 |
| 2 | Pump 3 | 21 |
| 3 | Pump 4 | 34 |

Channels 4–7 remain free for additional PWM loads.

## Power Requirements

- **ESP32-S3**: 3.3V (supplied via USB or regulator)
- **Pumps**: 12V DC, ~0.4A each (up to 1.6A for four pumps)
- **Water level sensors**: 5V, ~5mA each
- **DHT22 / DS18B20**: 3.3V
- **EZO boards**: 3.3V or 5V (check your specific model)
- **Relays**: 5V coil

Keep 12V (pump) and 3.3V/5V (logic) supplies with a common ground.

## Firmware Behavior

- **Relays**: Toggle HIGH/LOW every 30 seconds (test mode — replace with real logic for production)
- **Pump 1**: Starts immediately on boot (direct ON at ~78% speed)
- **Sensors**: Read every 5 seconds
- **RGB LED**: Continuous hue cycle on Core 0 (isolated from sensor UART on Core 1)
- **Boot delay**: 10 seconds to allow sensor power stabilization

## Serial Output Format

```
SolT: 24.50C  AirT: 26.3C RH: 65.2%  CO2: 612ppm  TankHi:DRY TankLo:WET  pH: 6.85 (avg/6.83 n3) '6.85'  EC: 1.415 (avg/1.410 n3) '1415'
```

Fields:
- `SolT` — solution temperature (DS18B20)
- `AirT`, `RH` — air temperature and humidity (DHT22)
- `CO2` — ambient CO2 in ppm (MH-Z19E)
- `TankHi`, `TankLo` — water level states (WET/DRY)
- `pH` / `EC` — smoothed (5-point moving average) + instantaneous value + sample count + raw response

## Build & Upload

```bash
pio run -t clean
pio run -t upload
pio device monitor
```

COM port is pinned to COM13 in `platformio.ini`. Update `upload_port` and `monitor_port` if the enumeration changes.

## Available GPIOs (Free)

On the N8R2 variant, these safe pins remain free for future additions:

**Clean (preferred first)**: 3, 9, 41, 42, 47

**Use with caution**:
- GPIO 0 — BOOT button (has pull-up)
- GPIO 45, 46 — strapping pins (require specific boot state)
- GPIO 19, 20 — USB D-/D+ (avoid if using native USB port)
- GPIO 43, 44 — UART0 (reserved for upload/monitor)

**Reserved (do not use)**:
- GPIO 26–32 — Flash SPI

## Notes

- The N8R2 variant uses **QSPI PSRAM**, which leaves GPIOs 33–37 free. This configuration will NOT work on the N16R8 variant (OPI PSRAM) without reassigning those pins.
- EZO sensor calibration is device-side, not code-side — reflashing does not affect calibration.
- Moving-average smoothing (n=5) hides short transients. Raw values are also logged for debugging.
