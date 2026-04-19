#ifndef SENSOR_UTILS_H
#define SENSOR_UTILS_H

#include <Arduino.h>
#include <HardwareSerial.h>

bool readPH(HardwareSerial& ser, float& value, float compTemp, String& raw);
bool readEC(HardwareSerial& ser, float& value, float compTemp, String& raw);

#endif
