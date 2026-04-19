/*
 * ESP32-S3 Hydroponics Controller v2
 * ==========================================================================
 *
 *  Peripherals:
 *   - 4x Relays                    (5V, digital out)
 *   - Onboard RGB NeoPixel         (Core 0 task, color cycle)
 *   - 2x DHT22                     (air temp + RH)
 *   - 1x DS18B20                   (solution temp, for pH/EC comp)
 *   - 1x Atlas EZO-pH              (UART1, temp-comp + 5pt smoothing)
 *   - 1x Atlas EZO-EC              (UART2, temp-comp + 5pt smoothing)
 *   - 4x KFS-HE1S10P peristaltic   (LEDC PWM, channels 0-3)
 *   - 1x MH-Z19E CO2               (PWM mode via pulseIn)
 *   - 1x XKC-Y26 water level       (digital, 5V→3.3V divider)
 *
 *  Timing:
 *   - Relay demo toggle : 15 s
 *   - Sensor read cycle : 5  s   (DS18B20 + DHT×2 + pH + EC)
 *   - CO2 read          : 15 s   (pulseIn blocks ~1-2s)
 *   - Water level       : every loop iteration (change-triggered log)
 *   - RGB cycle         : continuous on Core 0
 *
 *  CRITICAL HARDWARE REMINDERS:
 *   ✓ 10kΩ pull-down on EACH pump PWM pin (GPIO 12, 14, 21, 34)
 *   ✓ 4.7kΩ pull-up on DS18B20 data line to 3.3V
 *   ✓ 4.7-10kΩ pull-up on EACH DHT22 data line to 3.3V
 *   ✓ 10k/20k divider on XKC yellow output (5V → 3.3V)
 *   ✓ 100µF + 100nF decoupling near ESP32 5V input (pump noise!)
 *   ✓ Shared GND between 12V pump rail and 5V buck/ESP32 GND
 * ==========================================================================
 */

#include <Arduino.h>
#include "pins.h"
#include <Adafruit_NeoPixel.h>
#include "sensor_utils.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <DHT.h>

// =============================================================================
// Config
// =============================================================================

// XKC-Y26 logic mode:
//   1 = Sensor reads HIGH when water is DETECTED (PNP NO, or NPN NC)
//   0 = Sensor reads HIGH when water is ABSENT  (NPN NO - black wire floating)
// Flip this if your "low water" alarm fires when reservoir is actually full.
#define XKC_HIGH_MEANS_WATER_PRESENT   1

// Auto-start pump 1 on boot (matches original behavior for easy verification)
#define AUTO_START_PUMP_1              1

// =============================================================================
// Hardware Objects
// =============================================================================

Adafruit_NeoPixel rgb(NUM_LEDS, RGB_PIN, NEO_GRB + NEO_KHZ800);

const int relayPins[] = { RELAY_1A, RELAY_1B, RELAY_2A, RELAY_2B };
const int numRelays   = 4;

HardwareSerial phSerial(1);
HardwareSerial ecSerial(2);

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature ds18b20(&oneWire);

#define DHT_TYPE DHT22
DHT airSensor1(DHT_PIN,  DHT_TYPE);
DHT airSensor2(DHT2_PIN, DHT_TYPE);

// =============================================================================
// Pump Driver (KFS-HE1S10P, up to 4 units)
// =============================================================================

#define PUMP_PWM_FREQ      10000   // 10 kHz (spec: 5-20 kHz)
#define PUMP_PWM_RES       8       // 8-bit duty (0-255)
#define PUMP_SPEED_DEFAULT 200     // ~78%

struct Pump {
    const char* name;
    uint8_t pwmPin;
    uint8_t dirPin;
    uint8_t ledcChannel;
    uint8_t speed;
    bool    running;
    bool    reversed;
};

Pump pumps[4] = {
    { "P1", PUMP1_PIN, PUMP1_DIR_PIN, 0, PUMP_SPEED_DEFAULT, false, false },
    { "P2", PUMP2_PIN, PUMP2_DIR_PIN, 1, PUMP_SPEED_DEFAULT, false, false },
    { "P3", PUMP3_PIN, PUMP3_DIR_PIN, 2, PUMP_SPEED_DEFAULT, false, false },
    { "P4", PUMP4_PIN, PUMP4_DIR_PIN, 3, PUMP_SPEED_DEFAULT, false, false },
};

void pumpInit(int idx) {
    Pump &p = pumps[idx];
    pinMode(p.pwmPin, OUTPUT);   digitalWrite(p.pwmPin, LOW);  // safe state
    pinMode(p.dirPin, OUTPUT);   digitalWrite(p.dirPin, LOW);  // CW default
    Serial.printf("  %s: PWM=GPIO%-2d (ch%d)  DIR=GPIO%-2d\n",
                  p.name, p.pwmPin, p.ledcChannel, p.dirPin);
}

void pumpStart(int idx, uint8_t speed) {
    Pump &p = pumps[idx];
    ledcSetup(p.ledcChannel, PUMP_PWM_FREQ, PUMP_PWM_RES);
    ledcAttachPin(p.pwmPin, p.ledcChannel);
    ledcWrite(p.ledcChannel, speed);
    p.speed = speed;
    p.running = true;
    Serial.printf("[%s] START speed=%d (%.0f%%)\n",
                  p.name, speed, (speed / 255.0f) * 100);
}

void pumpStop(int idx) {
    Pump &p = pumps[idx];
    ledcWrite(p.ledcChannel, 0);   // duty 0 first
    delay(50);                     // let motor decel
    ledcDetachPin(p.pwmPin);       // release LEDC
    pinMode(p.pwmPin, OUTPUT);     // manual control
    digitalWrite(p.pwmPin, LOW);   // force LOW (pull-down backs this up)
    p.running = false;
    Serial.printf("[%s] STOP\n", p.name);
}

void pumpSetSpeed(int idx, uint8_t speed) {
    Pump &p = pumps[idx];
    if (!p.running) { Serial.printf("[%s] not running\n", p.name); return; }
    ledcWrite(p.ledcChannel, speed);
    p.speed = speed;
    Serial.printf("[%s] Speed -> %d\n", p.name, speed);
}

void pumpSetDirection(int idx, bool reverse) {
    Pump &p = pumps[idx];
    if (p.running) {
        Serial.printf("[%s] WARN: changing direction while running!\n", p.name);
    }
    digitalWrite(p.dirPin, reverse ? HIGH : LOW);
    p.reversed = reverse;
    Serial.printf("[%s] Direction: %s\n", p.name, reverse ? "CCW" : "CW");
}

// =============================================================================
// MH-Z19E CO2 Sensor (PWM mode)
// =============================================================================
// Default range: 0-5000 ppm, cycle ~1004 ms
// ppm = 5000 * (Th_ms - 2) / (Th_ms + Tl_ms - 4)
// NOTE: Sensor needs ~3 min warm-up after power-on for accurate readings.

bool readCO2_PWM(uint8_t pin, int &ppm_out) {
    unsigned long th_us = pulseIn(pin, HIGH, 1500000UL);
    if (th_us == 0) return false;
    unsigned long tl_us = pulseIn(pin, LOW,  1500000UL);
    if (tl_us == 0) return false;

    float th_ms    = th_us / 1000.0f;
    float tl_ms    = tl_us / 1000.0f;
    float cycle_ms = th_ms + tl_ms;

    // Cycle sanity check (~1004ms nominal)
    if (cycle_ms < 900.0f || cycle_ms > 1100.0f) return false;

    float ppm = 5000.0f * (th_ms - 2.0f) / (cycle_ms - 4.0f);
    if (ppm < 0 || ppm > 5100) return false;

    ppm_out = (int)ppm;
    return true;
}

// =============================================================================
// Water Level (XKC-Y26)
// =============================================================================

bool readWaterPresent() {
    int raw = digitalRead(WATER_LEVEL_PIN);
#if XKC_HIGH_MEANS_WATER_PRESENT
    return (raw == HIGH);
#else
    return (raw == LOW);
#endif
}

// =============================================================================
// State
// =============================================================================

bool          relayState         = false;
unsigned long lastRelayToggle    = 0;
const unsigned long RELAY_INTERVAL = 15000UL;

unsigned long lastSensorRead     = 0;
const unsigned long SENSOR_INTERVAL = 5000UL;

unsigned long lastCO2Read        = 0;
const unsigned long CO2_INTERVAL = 15000UL;

uint16_t hue = 0;

// Sensor values
float   solutionTemp = 25.0f;
float   airTemp1 = -999, airRH1 = -999;
float   airTemp2 = -999, airRH2 = -999;
float   phValue  = -999, ecValue = -999;
String  phRaw    = "",  ecRaw   = "";
int     co2_ppm  = -1;
bool    waterPresent = true;   // cached; updated on change
bool    lastWaterState = true;

// Smoothing buffers
float phReadings[5] = {0};
int   phIndex = 0, phCount = 0;
float ecReadings[5] = {0};
int   ecIndex = 0, ecCount = 0;

// =============================================================================
// RGB Task (Core 0)
// =============================================================================

void rgbTask(void *pvParameters) {
    for (;;) {
        rgb.setPixelColor(0, rgb.ColorHSV(hue, 255, 255));
        rgb.show();
        hue += 256;
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// =============================================================================
// Setup
// =============================================================================

void setup() {
    Serial.begin(115200);
    delay(3000);

    Serial.println("\n========================================");
    Serial.println("  ESP32-S3 Hydroponics Controller v2");
    Serial.println("  4 Pumps | 4 Relays | 8 Sensors");
    Serial.println("========================================\n");

    // ---- Relays ----
    Serial.println("[INIT] Relays:");
    for (int i = 0; i < numRelays; i++) {
        pinMode(relayPins[i], OUTPUT);
        digitalWrite(relayPins[i], LOW);
        Serial.printf("  R%d: GPIO%d OFF\n", i + 1, relayPins[i]);
    }

    // ---- Pumps (init all to safe state) ----
    Serial.println("[INIT] Pumps:");
    for (int i = 0; i < 4; i++) pumpInit(i);

    // ---- Water Level ----
    pinMode(WATER_LEVEL_PIN, INPUT);
    Serial.printf("[INIT] Water Level: GPIO%d (via 10k/20k divider)\n", WATER_LEVEL_PIN);

    // ---- CO2 ----
    pinMode(CO2_PWM_PIN, INPUT);
    Serial.printf("[INIT] CO2 PWM: GPIO%d (3.3V logic, direct)\n", CO2_PWM_PIN);

    // ---- DS18B20 ----
    pinMode(ONE_WIRE_BUS, INPUT_PULLUP);
    ds18b20.begin();
    Serial.printf("[INIT] DS18B20: GPIO%d  (%d device%s found)\n",
                  ONE_WIRE_BUS, ds18b20.getDeviceCount(),
                  ds18b20.getDeviceCount() == 1 ? "" : "s");

    // ---- DHT22 x2 ----
    airSensor1.begin();
    airSensor2.begin();
    Serial.printf("[INIT] DHT22 #1: GPIO%d\n", DHT_PIN);
    Serial.printf("[INIT] DHT22 #2: GPIO%d\n", DHT2_PIN);

    // ---- EZO pH + EC ----
    phSerial.begin(9600, SERIAL_8N1, PH_RX, PH_TX);
    ecSerial.begin(9600, SERIAL_8N1, EC_RX, EC_TX);
    delay(100);
    ecSerial.print("O,EC,1\r");  delay(300);
    ecSerial.print("O,TDS,0\r"); delay(300);
    ecSerial.print("O,S,0\r");   delay(300);
    ecSerial.print("O,SG,0\r");  delay(300);
    phSerial.print("C,0\r");
    ecSerial.print("C,0\r");
    delay(300);
    phSerial.print("i\r");
    ecSerial.print("i\r");
    delay(500);
    Serial.printf("[INIT] EZO-pH: UART1 RX%d TX%d @9600\n", PH_RX, PH_TX);
    Serial.printf("[INIT] EZO-EC: UART2 RX%d TX%d @9600\n", EC_RX, EC_TX);

    // ---- RGB ----
    rgb.begin();
    rgb.setBrightness(30);
    rgb.show();
    xTaskCreatePinnedToCore(rgbTask, "RGB", 2048, NULL, 1, NULL, 0);
    Serial.printf("[INIT] RGB: GPIO%d (Core 0 task)\n", RGB_PIN);

    Serial.println("\n========================================");
    Serial.println("  READY. Sensor loop starting.");
    Serial.println("  MH-Z19E needs 3 min warm-up for accuracy.");
    Serial.println("========================================\n");

#if AUTO_START_PUMP_1
    pumpStart(0, PUMP_SPEED_DEFAULT);
#endif

    // Seed water state
    waterPresent   = readWaterPresent();
    lastWaterState = waterPresent;
    Serial.printf("[WATER] Initial: %s\n", waterPresent ? "PRESENT" : "LOW");

    lastRelayToggle = millis();
    lastSensorRead  = millis();
    lastCO2Read     = millis() - (CO2_INTERVAL - 5000);  // first read in ~5s
}

// =============================================================================
// Loop
// =============================================================================

void loop() {
    unsigned long now = millis();

    // ---- Water Level (change-triggered; 500ms sensor response is enough) ----
    waterPresent = readWaterPresent();
    if (waterPresent != lastWaterState) {
        lastWaterState = waterPresent;
        Serial.printf("[WATER] -> %s\n",
                      waterPresent ? "PRESENT" : "LOW - refill needed!");
        // Hook: add pump-safety-stop here if you want to halt dosing on empty reservoir
        // e.g.:  if (!waterPresent) for (int i=0; i<4; i++) if (pumps[i].running) pumpStop(i);
    }

    // ---- Relay Demo Toggle (replace with your real logic) ----
    if (now - lastRelayToggle >= RELAY_INTERVAL) {
        lastRelayToggle = now;
        relayState = !relayState;
        Serial.printf("[%5lus] Relays -> %s\n",
                      now / 1000, relayState ? "ON" : "OFF");
        for (int i = 0; i < numRelays; i++) {
            digitalWrite(relayPins[i], relayState ? HIGH : LOW);
        }
    }

    // ---- CO2 (every 15s; blocks up to ~3s in worst case) ----
    if (now - lastCO2Read >= CO2_INTERVAL) {
        lastCO2Read = now;
        int ppm = -1;
        if (readCO2_PWM(CO2_PWM_PIN, ppm)) {
            co2_ppm = ppm;
            Serial.printf("[CO2] %d ppm\n", co2_ppm);
        } else {
            Serial.println("[CO2] Read failed (warming up or check wiring)");
        }
    }

    // ---- Temp / Humidity / pH / EC (every 5s) ----
    if (now - lastSensorRead >= SENSOR_INTERVAL) {
        lastSensorRead = now;

        // 1. DS18B20 Solution Temp
        ds18b20.requestTemperatures();
        float t = ds18b20.getTempCByIndex(0);
        if (t != DEVICE_DISCONNECTED_C && t > -20.0f && t < 80.0f) {
            solutionTemp = t;
        } else {
            ds18b20.begin();  // attempt re-init
        }
        Serial.printf("SolT:%.2fC ", solutionTemp);

        // 2. DHT22 #1
        float h1 = airSensor1.readHumidity();
        float t1 = airSensor1.readTemperature();
        if (!isnan(h1) && !isnan(t1)) {
            airRH1   = h1;
            airTemp1 = t1;
            Serial.printf("A1:%.1fC/%.0f%% ", airTemp1, airRH1);
        } else {
            Serial.print("A1:ERR ");
        }

        // 3. DHT22 #2
        float h2 = airSensor2.readHumidity();
        float t2 = airSensor2.readTemperature();
        if (!isnan(h2) && !isnan(t2)) {
            airRH2   = h2;
            airTemp2 = t2;
            Serial.printf("A2:%.1fC/%.0f%% ", airTemp2, airRH2);
        } else {
            Serial.print("A2:ERR ");
        }

        // 4. pH (temp-compensated + smoothed)
        if (readPH(phSerial, phValue, solutionTemp, phRaw)) {
            phReadings[phIndex] = phValue;
            phCount = (phCount < 5) ? phCount + 1 : 5;
            phIndex = (phIndex + 1) % 5;
            float avg = 0;
            for (int j = 0; j < phCount; j++) avg += phReadings[j];
            avg /= phCount;
            Serial.printf("pH:%.2f(avg) ", avg);
        } else {
            Serial.print("pH:ERR ");
        }

        // 5. EC (temp-compensated + smoothed)
        if (readEC(ecSerial, ecValue, solutionTemp, ecRaw)) {
            ecReadings[ecIndex] = ecValue;
            ecCount = (ecCount < 5) ? ecCount + 1 : 5;
            ecIndex = (ecIndex + 1) % 5;
            float avg = 0;
            for (int j = 0; j < ecCount; j++) avg += ecReadings[j];
            avg /= ecCount;
            Serial.printf("EC:%.3f(avg) ", avg);
        } else {
            Serial.print("EC:ERR ");
        }

        // 6. Summary line
        Serial.printf("H2O:%s CO2:%dppm\n",
                      waterPresent ? "OK" : "LOW",
                      co2_ppm);
    }

    delay(10);  // feed FreeRTOS idle task
}

/* ==========================================================================
 *  QUICK REFERENCE - How to control pumps from anywhere in your code
 * ==========================================================================
 *
 *  pumpStart(idx, speed);         // idx: 0-3, speed: 0-255
 *  pumpStop(idx);                 // graceful stop
 *  pumpSetSpeed(idx, newSpeed);   // while running
 *  pumpSetDirection(idx, true);   // true=CCW, false=CW; STOP FIRST
 *
 *  Example - dose 5ml acid on pump 2 for 3 seconds:
 *    pumpStart(1, 150);
 *    delay(3000);
 *    pumpStop(1);
 *
 *  Safety pattern - halt all pumps if reservoir is dry:
 *    if (!waterPresent) {
 *      for (int i = 0; i < 4; i++) if (pumps[i].running) pumpStop(i);
 *    }
 * ========================================================================== */