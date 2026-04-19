/*************************************************************************************************/
// Code for ESP32-S3 N8R2 — Wi-Fi Provisioning + Standalone FOTA (Version 2)
// Author: Biswajit
// Platform: PlatformIO + Arduino Framework
// Board: ESP32-S3-DevKitC-1 (N8R2 — 8MB Flash, 2MB OPI PSRAM)
// Last modified: 27-03-2026
/*************************************************************************************************/

/*BEGINS - Include all the Libraries*/
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <esp32FOTA.hpp>
/*ENDS - Include all the Libraries*/

/*BEGINS - Serial abstraction for ESP32-S3 USB CDC compatibility*/
#if ARDUINO_USB_CDC_ON_BOOT
  #define HWSERIAL USBSerial
#else
  #define HWSERIAL Serial
#endif
/*ENDS - Serial abstraction*/

/*BEGINS - Mapping the input output devices to pin numbers*/
#define BUTTON_PIN 0    // GPIO0 = BOOT button on ESP32-S3 DevKitC-1
#define LED_PIN    2    // Onboard LED
/*ENDS - Mapping the input output devices to pin numbers*/

/*BEGINS - Create the required variables and RTOS Handles*/
const int FIRMWARE_VERSION = 2;

const char* manifest_url = "http://52.66.175.35/fota/fota_merged.json";
esp32FOTA esp32FOTA("esp32-fota-merged", FIRMWARE_VERSION, false);

TaskHandle_t TaskComms_Handle;
TaskHandle_t TaskStatus_Handle;
volatile bool isWiFiConnected = false;
/*ENDS - Create the required variables and RTOS Handles*/


//  ===============================================================================================
//  ||                                  TASK - COMMUNICATIONS                                    ||
//  ===============================================================================================
void TaskComms(void *pvParameters) {

  /*BEGINS - Initialize Non-Blocking WiFiManager*/
  WiFiManager wm;
  wm.setConfigPortalBlocking(false);
  wm.setConfigPortalTimeout(180);

  if (wm.autoConnect("CosyFarms_IoT_Config")) {
    isWiFiConnected = true;
  }
  /*ENDS - Initialize Non-Blocking WiFiManager*/

  /*BEGINS - Local Task Variables*/
  unsigned long previousFotaMillis = 0;
  const long fotaInterval = 15000;
  /*ENDS - Local Task Variables*/

  for (;;) {

    /*BEGINS - Handle Hardware Reset Button*/
    if (digitalRead(BUTTON_PIN) == LOW) {
      vTaskDelay(50 / portTICK_PERIOD_MS);
      if (digitalRead(BUTTON_PIN) == LOW) {
        HWSERIAL.println(F("Reset button pressed. Clearing Wi-Fi credentials..."));
        wm.resetSettings();
        HWSERIAL.println(F("Restarting in 3 seconds..."));
        vTaskDelay(3000 / portTICK_PERIOD_MS);
        ESP.restart();
      }
    }
    /*ENDS - Handle Hardware Reset Button*/

    /*BEGINS - Maintain WiFi Portal and Status*/
    wm.process();
    isWiFiConnected = (WiFi.status() == WL_CONNECTED);
    /*ENDS - Maintain WiFi Portal and Status*/

    /*BEGINS - Handle FOTA Checking*/
    unsigned long currentMillis = millis();
    if (isWiFiConnected && (currentMillis - previousFotaMillis >= fotaInterval)) {
      previousFotaMillis = currentMillis;
      HWSERIAL.println(F("Checking for firmware updates..."));
      bool updateNeeded = esp32FOTA.execHTTPcheck();
      if (updateNeeded) {
        HWSERIAL.println(F("Update found! Starting OTA download..."));
        esp32FOTA.execOTA();
      }
    }
    /*ENDS - Handle FOTA Checking*/

    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}


//  ===============================================================================================
//  ||                                  TASK - SYSTEM STATUS (LED)                               ||
//  ===============================================================================================
void TaskStatus(void *pvParameters) {

  /*BEGINS - Status Task Setup*/
  pinMode(LED_PIN, OUTPUT);
  bool ledState = LOW;
  /*ENDS - Status Task Setup*/

  for (;;) {
    /*BEGINS - Update LED Based on WiFi and Firmware Version*/
    if (FIRMWARE_VERSION == 1) {
      if (isWiFiConnected) {
        digitalWrite(LED_PIN, HIGH);
        vTaskDelay(500 / portTICK_PERIOD_MS);
      } else {
        ledState = !ledState;
        digitalWrite(LED_PIN, ledState);
        vTaskDelay(500 / portTICK_PERIOD_MS);
      }
    }
    else if (FIRMWARE_VERSION == 2) {
      if (isWiFiConnected) {
        ledState = !ledState;
        digitalWrite(LED_PIN, ledState);
        vTaskDelay(250 / portTICK_PERIOD_MS);
      } else {
        ledState = !ledState;
        digitalWrite(LED_PIN, ledState);
        vTaskDelay(100 / portTICK_PERIOD_MS);
      }
    }
    /*ENDS - Update LED Based on WiFi and Firmware Version*/
  }
}


//  ===============================================================================================
//  ||                                        SETUP                                              ||
//  ===============================================================================================
void setup() {

  /*BEGINS - Serial Init*/
  HWSERIAL.begin(115200);
  unsigned long t = millis();
  while (!HWSERIAL && (millis() - t < 3000)); // Wait up to 3s for USB CDC to connect
  /*ENDS - Serial Init*/

  /*BEGINS - PSRAM Check*/
  if (psramFound()) {
    HWSERIAL.printf("PSRAM OK: %lu bytes\n", ESP.getPsramSize());
  } else {
    HWSERIAL.println("WARNING: PSRAM not found!");
  }
  /*ENDS - PSRAM Check*/

  /*BEGINS - Serial Banner*/
  HWSERIAL.print(F("\n Starting RTOS System - VERSION: "));
  HWSERIAL.println(FIRMWARE_VERSION);
  /*ENDS - Serial Banner*/

  /*BEGINS - Declaring all Input Output Ports*/
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  /*ENDS - Declaring all Input Output Ports*/

  /*BEGINS - Initialize FOTA*/
  esp32FOTA.setManifestURL(manifest_url);
  /*ENDS - Initialize FOTA*/

  /*BEGINS - Create RTOS Tasks*/
  xTaskCreatePinnedToCore(TaskComms,  "CommsTask",  16384, NULL, 1, &TaskComms_Handle,  0);
  xTaskCreatePinnedToCore(TaskStatus, "StatusTask", 2048,  NULL, 1, &TaskStatus_Handle, 1);
  /*ENDS - Create RTOS Tasks*/
}


//  ===============================================================================================
//  ||                                        MAIN LOOP                                          ||
//  ===============================================================================================
void loop() {
  vTaskDelete(NULL);
}