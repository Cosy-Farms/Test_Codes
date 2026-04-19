# ESP32-S3 Relay + RGB + EZO-pH/EC Test Firmware

## Features
- Toggle 4 relays (GPIO 4-7) every 30s
- Cycle onboard RGB LED (GPIO 48)
- Read Atlas EZO-pH (16/17) & EZO-EC (18/8) every 2s via SoftwareSerial UART
- Print all to USB CDC Serial (115200 baud)

## Wiring
| Component | Pins (RX/TX or GPIO) |
|-----------|----------------------|
| Relays 1A-2B | 4,5,6,7 |
| RGB LED | 48 |
| EZO-pH | TX→16, RX→17, VCC/GND |
| EZO-EC | TX→18, RX→8, VCC/GND |

## Build/Upload (PlatformIO)
```
pio run --target upload
pio device monitor
```

## Serial Output Example
```
[Sensors] pH=7.02  Raw:'7.02,24.5'
[Sensors] EC=1.23  Raw:'1.23,24.5'
[35s] Relays -> ON
```

## Notes
- Sensors need calibration ('cal,7.00' via serial).
- GPIO8/18 compatible for SoftwareSerial.
- VSCode lint ignores compile fine.
