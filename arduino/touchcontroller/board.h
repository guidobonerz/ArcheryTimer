#pragma once

#include <Arduino.h>

/** brings up the io expander, the panel, the touch controller and LVGL */
bool boardBegin();

/** runs LVGL - to be called from loop() */
void boardLoop();

void boardBacklight(bool on);

/** charge of the battery in percent, -1 as long as none is measured */
int8_t boardBatteryPercent();
