#pragma once

#include <Arduino.h>

enum Language { LanguageGerman = 0,
                LanguageEnglish = 1 };

/**
 * What the settings page holds. The defaults are the ones the android app
 * starts with; unlike the app the controller keeps changes in flash.
 */
struct Settings {
  uint16_t prepareTime = 10;
  uint16_t arrowTime = 30;
  uint16_t arrowCount = 3;
  uint16_t shootInTime = 45;
  uint16_t warnTime = 30;
  uint16_t groups = 1;
  uint16_t passes = 10;
  bool flashingPrepareLight = true;
  uint16_t volume = 10;
  // the speaker of the displays is off, the volume is kept for later
  bool muted = false;
  uint16_t lunchHour = 13;
  uint16_t lunchMinute = 0;
  uint16_t language = LanguageGerman;

  // chosen on the tournament page for the pass at hand, not kept in flash
  uint16_t reshootArrowCount = 0;

  uint16_t actionTime() const {
    return arrowTime * arrowCount;
  }
};

extern Settings settings;

void settingsLoad();
void settingsSave();
