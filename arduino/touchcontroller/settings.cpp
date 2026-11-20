#include "settings.h"

#include <Preferences.h>

Settings settings;

namespace {
const char* NAMESPACE = "touchctl";
}

void settingsLoad() {
  Preferences prefs;
  prefs.begin(NAMESPACE, true);
  settings.prepareTime = prefs.getUShort("pt", settings.prepareTime);
  settings.arrowTime = prefs.getUShort("at", settings.arrowTime);
  settings.arrowCount = prefs.getUShort("ac", settings.arrowCount);
  settings.shootInTime = prefs.getUShort("sit", settings.shootInTime);
  settings.warnTime = prefs.getUShort("wt", settings.warnTime);
  settings.groups = prefs.getUShort("g", settings.groups);
  settings.passes = prefs.getUShort("p", settings.passes);
  settings.flashingPrepareLight = prefs.getBool("fpl", settings.flashingPrepareLight);
  settings.volume = prefs.getUShort("vol", settings.volume);
  settings.muted = prefs.getBool("mute", settings.muted);
  settings.lunchHour = prefs.getUShort("lh", settings.lunchHour);
  settings.lunchMinute = prefs.getUShort("lm", settings.lunchMinute);
  settings.language = prefs.getUShort("lang", settings.language);
  prefs.end();
}

void settingsSave() {
  Preferences prefs;
  prefs.begin(NAMESPACE, false);
  prefs.putUShort("pt", settings.prepareTime);
  prefs.putUShort("at", settings.arrowTime);
  prefs.putUShort("ac", settings.arrowCount);
  prefs.putUShort("sit", settings.shootInTime);
  prefs.putUShort("wt", settings.warnTime);
  prefs.putUShort("g", settings.groups);
  prefs.putUShort("p", settings.passes);
  prefs.putBool("fpl", settings.flashingPrepareLight);
  prefs.putUShort("vol", settings.volume);
  prefs.putBool("mute", settings.muted);
  prefs.putUShort("lh", settings.lunchHour);
  prefs.putUShort("lm", settings.lunchMinute);
  prefs.putUShort("lang", settings.language);
  prefs.end();
}
