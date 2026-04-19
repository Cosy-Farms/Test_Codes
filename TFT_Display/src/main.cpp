#include <Arduino.h>
#include <SPI.h>

// --- Adjust these if you swapped pins ---
#define TFT_CS   5
#define TFT_DC   2
#define TFT_RST  4
#define TFT_MOSI 23
#define TFT_MISO 19
#define TFT_CLK  18

void sendCommand(uint8_t cmd) {
  digitalWrite(TFT_DC, LOW);
  digitalWrite(TFT_CS, LOW);
  SPI.transfer(cmd);
  digitalWrite(TFT_CS, HIGH);
}

uint8_t readData() {
  digitalWrite(TFT_DC, HIGH);
  digitalWrite(TFT_CS, LOW);
  uint8_t val = SPI.transfer(0x00);
  digitalWrite(TFT_CS, HIGH);
  return val;
}

uint32_t readDisplayID() {
  digitalWrite(TFT_DC, LOW);
  digitalWrite(TFT_CS, LOW);
  SPI.transfer(0x04);          // Read Display ID command
  digitalWrite(TFT_DC, HIGH);
  uint8_t b0 = SPI.transfer(0x00); // dummy
  uint8_t b1 = SPI.transfer(0x00);
  uint8_t b2 = SPI.transfer(0x00);
  uint8_t b3 = SPI.transfer(0x00);
  digitalWrite(TFT_CS, HIGH);
  return ((uint32_t)b1 << 16) | ((uint32_t)b2 << 8) | b3;
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=== TFT Chip ID Diagnostic ===");

  // Init pins
  pinMode(TFT_CS,  OUTPUT); digitalWrite(TFT_CS,  HIGH);
  pinMode(TFT_DC,  OUTPUT); digitalWrite(TFT_DC,  HIGH);
  pinMode(TFT_RST, OUTPUT);

  // Hard reset
  digitalWrite(TFT_RST, HIGH); delay(50);
  digitalWrite(TFT_RST, LOW);  delay(200);
  digitalWrite(TFT_RST, HIGH); delay(200);

  SPI.begin(TFT_CLK, TFT_MISO, TFT_MOSI, TFT_CS);
  SPI.setFrequency(2000000);   // Slow speed for reliable read
  SPI.setDataMode(SPI_MODE0);

  uint32_t id = readDisplayID();

  Serial.printf("Raw Chip ID: 0x%06X\n", id);

  if      (id == 0x9341) Serial.println("Driver: ILI9341 ✓");
  else if (id == 0x9488) Serial.println("Driver: ILI9488");
  else if (id == 0x7789) Serial.println("Driver: ST7789 ✓");
  else if (id == 0x9225) Serial.println("Driver: ILI9225");
  else if (id == 0x0000) Serial.println("ID = 0x000000 → wiring issue or wrong pins!");
  else if (id == 0xFFFFFF) Serial.println("ID = 0xFFFFFF → MISO not connected or floating");
  else Serial.println("Unknown driver — share this ID and we'll look it up");

  // Also try command 0xD3 (alternate ID read used by some drivers)
  delay(100);
  digitalWrite(TFT_DC, LOW);
  digitalWrite(TFT_CS, LOW);
  SPI.transfer(0xD3);
  digitalWrite(TFT_DC, HIGH);
  uint8_t d0 = SPI.transfer(0x00);
  uint8_t d1 = SPI.transfer(0x00);
  uint8_t d2 = SPI.transfer(0x00);
  uint8_t d3 = SPI.transfer(0x00);
  digitalWrite(TFT_CS, HIGH);
  Serial.printf("Alt ID (0xD3):  0x%02X 0x%02X 0x%02X 0x%02X\n", d0, d1, d2, d3);
}

void loop() {}