#pragma once

#include "settings.h"

enum TextId {
  TextTabSettings,
  TextReShoot,
  TextLunchBreak,
  TextGroup,
  TextPasses,
  TextConfirmReShoot,
  TextEmergency,
  TextYes,
  TextNo,
  TextOk,
  TextCancel,
  TextGroups,
  TextArrowCount,
  TextArrowTime,
  TextActionTime,
  TextPrepareTime,
  TextWarnTime,
  TextLunchEnd,
  TextFlashingPrepareLight,
  TextVolume,
  TextLanguage,
  TextClock,
  TextCount
};

static const char* const TEXTS[2][TextCount] = {
  {
    "Einstellungen",
    "Nachschiessen",
    "Mittagspause",
    "Gruppe",
    "Passen",
    "Wirklich nachschiessen?",
    "Sicherheit",
    "Ja",
    "Nein",
    "OK",
    "Abbrechen",
    "Gruppen",
    "Pfeile pro Passe",
    "Zeit pro Pfeil/s",
    "Zeit pro Passe/s",
    "Vorbereitungszeit/s",
    "Warnzeit/s",
    "Ende Mittagspause",
    "Vorbereitung blinkt",
    "Lautstaerke",
    "Sprache",
    "Uhrzeit",
  },
  {
    "Setup",
    "Re-shoot",
    "Lunch break",
    "Group",
    "Passes",
    "Sure to re-shoot?",
    "Emergency",
    "Yes",
    "No",
    "OK",
    "Cancel",
    "Groups",
    "Arrows per pass",
    "Time per arrow/s",
    "Time per pass/s",
    "Preparation time/s",
    "Warning time/s",
    "End of lunch break",
    "Flashing prepare phase",
    "Volume",
    "Language",
    "Time",
  }
};

inline const char* text(TextId id) {
  return TEXTS[settings.language == LanguageEnglish ? 1 : 0][id];
}
