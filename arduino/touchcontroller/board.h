#pragma once

#include <Arduino.h>

/** brings up the io expander, the panel, the touch controller and LVGL */
bool boardBegin();

/** runs LVGL - to be called from loop() */
void boardLoop();

void boardBacklight(bool on);
