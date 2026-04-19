#include "sensor_utils.h"

bool readPH(HardwareSerial& ser, float& value, float compTemp, String& raw) {
  // Set temp compensation (EZO command is "T,n")
  while (ser.available()) ser.read();
  ser.print("T,");
  ser.print(compTemp, 2);
  ser.print("\r");
  
  unsigned long start = millis();
  String resp = "";
  resp.reserve(32); // Pre-allocate memory
  bool t_ok = false;
  
  while (millis() - start < 500UL && !t_ok) {
    while (ser.available()) {
      char c = ser.read();
      if (c == '\r' || c == '\n') {
        if (resp.indexOf("*OK") >= 0) {
          t_ok = true;
          break;
        }
        resp = "";
      } else {
        if (c >= ' ' && c <= '~' && resp.length() < 30) {
          resp += c;
        }
      }
    }
    delay(5);
  }

  // Clear buffer and send read command (Standard Atlas EZO is "R")
  while (ser.available()) ser.read();
  ser.print("R\r");

  start = millis();
  raw = "";
  raw.reserve(64); // Pre-allocate memory to prevent heap fragmentation
  
  while (millis() - start < 2000UL) {
    while (ser.available()) {
      char c = ser.read();

      if (c == '\r' || c == '\n') {
        raw.trim();
        
        // Skip empty or command-ack lines
        if (raw.length() == 0 || raw == "*OK") {
          raw = "";
          continue; // Keep checking for actual data
        }

        if (raw.indexOf("*ER") >= 0) {
          return false; // Error response from sensor
        }

        // Clean up extended prefixes if they exist (e.g., "?pH,7.02" or "*OK,7.02")
        String parsedStr = raw;
        if (parsedStr.startsWith("?")) {
          int idx = parsedStr.indexOf(',');
          if (idx > 0) parsedStr = parsedStr.substring(idx + 1);
        } else if (parsedStr.startsWith("*OK,")) {
          parsedStr = parsedStr.substring(4);
        }

        // Extract value and temp (safely stops at comma if present)
        int firstComma = parsedStr.indexOf(',');
        if (firstComma > 0) {
          value = parsedStr.substring(0, firstComma).toFloat();
        } else {
          value = parsedStr.toFloat();
        }

        return true;
      } else {
        // Append valid characters with an absolute max length to prevent OOM crashes on UART noise
        if (c >= ' ' && c <= '~' && raw.length() < 60) {
          raw += c;
        }
      }
    }
    delay(5); // Yield to watchdog/FreeRTOS
  }
  return false;
}

bool readEC(HardwareSerial& ser, float& value, float compTemp, String& raw) {
  // Reuse logic since both pH and EC use exact same T,n and R\r commands
  return readPH(ser, value, compTemp, raw); 
}
