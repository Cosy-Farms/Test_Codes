#ifndef PINS_H
#define PINS_H

/* ==========================================================================
 *  ESP32-S3-DevKitC-1 (N8R2) - Hydroponics Controller Pin Map
 * ==========================================================================
 *  Power: 12V PSU → pumps direct | 12V→5V buck → ESP32 VIN, relays, XKC, MH-Z19E
 *  Logic: ESP32 onboard 3.3V LDO → DHT22 x2, DS18B20, EZO circuits
 * ========================================================================== */

// ---- Relays (2x 2-Channel Modules, 5V coil) ----
#define RELAY_1A        4
#define RELAY_1B        5
#define RELAY_2A        6
#define RELAY_2B        7

// ---- Onboard RGB NeoPixel ----
#define RGB_PIN         48
#define NUM_LEDS        1

// ---- EZO-pH (Hardware UART1, 3.3V logic) ----
#define PH_RX           16
#define PH_TX           17

// ---- EZO-EC (Hardware UART2, 3.3V logic) ----
#define EC_RX           18
#define EC_TX           8

// ---- DS18B20 Solution Temperature (OneWire, 3.3V, 4.7kΩ pull-up to 3.3V) ----
#define ONE_WIRE_BUS    38

// ---- DHT22 Air Temp/RH (both 3.3V, 4.7-10kΩ pull-up to 3.3V each) ----
#define DHT_PIN         10    // Sensor #1
#define DHT2_PIN        11    // Sensor #2

// ---- KFS-HE1S10P Brushless Peristaltic Pumps ----
// Wiring per pump:
//   Red    → +12V
//   Black  → GND (shared with ESP32 GND)
//   Blue   → PWM pin + 10kΩ pull-down to GND  ← CRITICAL, prevents boot ghost-run
//   Yellow → DIR pin (LOW=CW, HIGH=CCW)
//   Green  → FG feedback (leave disconnected for now)
#define PUMP1_PIN       12    // LEDC channel 0
#define PUMP1_DIR_PIN   13
#define PUMP2_PIN       14    // LEDC channel 1
#define PUMP2_DIR_PIN   15
#define PUMP3_PIN       21    // LEDC channel 2
#define PUMP3_DIR_PIN   33
#define PUMP4_PIN       34    // LEDC channel 3
#define PUMP4_DIR_PIN   35

// ---- MH-Z19E CO2 (5V power, 3.3V PWM output - direct connect, NO divider) ----
#define CO2_PWM_PIN     36

// ---- XKC-Y26 Non-Contact Water Level (5V power, divider on OUT) ----
// Yellow (OUT) ──[10kΩ]──┬── GPIO 37
//                        [20kΩ]
//                        GND
#define WATER_LEVEL_PIN 37

/* ==========================================================================
 *  Free GPIOs:
 *    ADC1:  GPIO 1, 2, 9
 *    GPIO:  GPIO 39, 40, 41, 42
 *  Blocked: 0, 3, 19, 20, 22-32, 43-47 (strapping/USB/flash/console)
 * ========================================================================== */

#endif // PINS_H