#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

/** builds the screen - LVGL has to be up */
void uiBegin();

/** status line, clock and delayed saving of the settings - call from loop() */
void uiLoop();

/** what the reporting display tells about the tournament, see NetEventHandler */
void uiHandleDisplayEvent(const char* command, JsonDocument& message, bool resynced);
