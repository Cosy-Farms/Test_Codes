# Test_Codes
Testing done by Ishan for sensors for Hydroponics 
Organisation: Cosy Farms

# ESP32-S3 Hydroponics Controller v2

A full-featured hydroponics monitoring and dosing controller built on the ESP32-S3 (N16R8), developed with PlatformIO/Arduino framework.

## Features

- 4x Relay outputs for lighting, pumps, or valves
- 4x KFS-HE1S10P peristaltic pumps with LEDC PWM speed control and direction control
- Atlas EZO-pH & EZO-EC sensors with temperature compensation and 5-point rolling average
- DS18B20 solution temperature sensor (OneWire)
- 2x DHT22 air temperature & humidity sensors
- MH-Z19E CO2 sensor (PWM mode)
- XKC-Y26 non-contact water level sensor
- Onboard RGB NeoPixel (continuous hue cycle on Core 0)
- FreeRTOS dual-core: RGB task pinned to Core 0, sensor/control loop on Core 1

## Hardware

| Component | GPIO(s) | Notes |
|---|---|---|
| Relay 1A / 1B | 4, 5 | 5V coil, active HIGH |
| Relay 2A / 2B | 6, 7 | 5V coil, active HIGH |
| RGB NeoPixel | 48 | Onboard, NEO_GRB |
| EZO-pH | RX=16, TX=17 | UART1, 9600 baud, 3.3V |
| EZO-EC | RX=18, TX=8 | UART2, 9600 baud, 3.3V |
| DS18B20 | 38 | 4.7kΩ pull-up to 3.3V |
| DHT22 #1 | 10 | 4.7–10kΩ pull-up to 3.3V |
| DHT22 #2 | 11 | 4.7–10kΩ pull-up to 3.3V |
| Pump 1 PWM / DIR | 12, 13 | LEDC ch0, 10kΩ pull-down on PWM |
| Pump 2 PWM / DIR | 14, 15 | LEDC ch1, 10kΩ pull-down on PWM |
| Pump 3 PWM / DIR | 21, 33 | LEDC ch2, 10kΩ pull-down on PWM |
| Pump 4 PWM / DIR | 34, 35 | LEDC ch3, 10kΩ pull-down on PWM |
| MH-Z19E CO2 | 36 | PWM mode, 3.3V direct |
| XKC-Y26 water level | 37 | 10k/20kΩ divider (5V→3.3V) |

## Critical Hardware Notes

- 10kΩ pull-down resistor on **each** pump PWM pin — prevents ghost-run on boot
- 4.7kΩ pull-up on DS18B20 data line to 3.3V
- 4.7–10kΩ pull-up on each DHT22 data line to 3.3V
- 10kΩ / 20kΩ voltage divider on XKC-Y26 output (5V → 3.3V)
- 100µF + 100nF decoupling caps near ESP32 5V input (pump switching noise)
- Shared GND between 12V pump rail, 5V buck converter, and ESP32

## Timing

| Task | Interval |
|---|---|
| Sensor read (DS18B20, DHT×2, pH, EC) | 5 s |
| CO2 read (blocks ~1–2 s) | 15 s |
| Relay demo toggle | 15 s |
| Water level check | Every loop iteration |
| RGB hue cycle | 20 ms (Core 0) |

## Pump API

```cpp
pumpStart(idx, speed);        // idx: 0–3, speed: 0–255
pumpStop(idx);                // graceful stop (ramps down, detaches LEDC)
pumpSetSpeed(idx, newSpeed);  // change speed while running
pumpSetDirection(idx, true);  // true = CCW, false = CW — stop pump first!
```

Example — dose 5 ml on pump 2 for 3 seconds:
```cpp
pumpStart(1, 150);
delay(3000);
pumpStop(1);
```

Safety pattern — halt all pumps if reservoir is empty:
```cpp
if (!waterPresent) {
    for (int i = 0; i < 4; i++)
        if (pumps[i].running) pumpStop(i);
}
```

## Configuration

Edit the defines at the top of `src/main.cpp`:

| Define | Default | Description |
|---|---|---|
| `XKC_HIGH_MEANS_WATER_PRESENT` | `1` | Set to `0` if sensor logic is inverted |
| `AUTO_START_PUMP_1` | `1` | Auto-start pump 1 on boot for testing |

## Build & Flash (PlatformIO)

```bash
pio run --target upload
pio device monitor --baud 115200
```

## Serial Output Example

```
========================================
  ESP32-S3 Hydroponics Controller v2
  4 Pumps | 4 Relays | 8 Sensors
========================================

[INIT] Relays: R1:GPIO4 OFF ...
[INIT] Pumps:  P1: PWM=GPIO12 (ch0)  DIR=GPIO13
[P1] START speed=200 (78%)
[WATER] Initial: PRESENT
SolT:23.45C  A1:24.1C/62%  A2:24.3C/61%  pH:6.82(avg)  EC:1.450(avg)  H2O:OK  CO2:824ppm
```

## Sensor Warm-up

- **MH-Z19E CO2**: requires ~3 minutes after power-on for accurate readings
- **EZO-pH / EZO-EC**: allow ~2 minutes for circuit stabilization; calibrate before use

## Calibration

Send calibration commands via serial (115200 baud) or over UART directly to the EZO circuits:

```
# pH — 3-point calibration
cal,mid,7.00
cal,low,4.00
cal,high,10.00

# EC — dry + single-point
cal,dry
cal,one,1413
```

## Dependencies

Managed by PlatformIO (`platformio.ini`):

- `Adafruit NeoPixel`
- `DallasTemperature` + `OneWire`
- `DHT sensor library` (Adafruit)
