/*
 * ArcheryTimer touch controller
 *
 * Drives the displays the way the android app does, from a Waveshare
 * ESP32-S3-Touch-LCD-7: the tournament page and the settings page.
 *
 * Arduino IDE
 *   Board             ESP32S3 Dev Module (esp32 core 3.x)
 *   PSRAM             OPI PSRAM
 *   Flash Size        8MB (64Mb)
 *   Partition Scheme  8M with spiffs (3MB APP/1.5MB SPIFFS)
 *   USB CDC On Boot   Disabled (the log goes out on the socket marked UART)
 *
 * Libraries
 *   lvgl 9.x          configured by build_opt.h in this folder, no lv_conf.h
 *   ArduinoJson 7.x
 *
 * Auth.h in this folder holds the wifi the displays are on:
 *   #define WIFI_SSID "..."
 *   #define WIFI_PASS "..."
 */

#include "board.h"
#include "net.h"
#include "settings.h"
#include "ui.h"
#include "wallclock.h"

// LVGL draws from loop(), which needs more room than the default stack gives
SET_LOOP_TASK_STACK_SIZE(16 * 1024);

void setup() {
  Serial.begin(115200);

  settingsLoad();
  clockBegin();

  if (!boardBegin()) {
    Serial.println("display did not come up");
    for (;;) {
      delay(1000);
    }
  }
  uiBegin();
  // the first picture is drawn before the light goes on
  boardLoop();
  boardBacklight(true);

  netBegin(uiHandleDisplayEvent);
}

void loop() {
  netLoop();
  uiLoop();
  boardLoop();
  delay(2);
}
