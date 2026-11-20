#include "ui.h"

#include <lvgl.h>

#include "config.h"
#include "net.h"
#include "settings.h"
#include "texts.h"
#include "wallclock.h"

/*
 * The tournament page and the settings page of the android app on 800x480.
 * What the pages do follows TournamentFragment and SettingsFragment.
 */

// defined in font_condensed_*.c - has to stay outside of the namespace
LV_FONT_DECLARE(font_condensed_20);
LV_FONT_DECLARE(font_condensed_28);

namespace {

// Roboto Condensed Medium (SIL Open Font License 1.1), made with lv_font_conv -
// the options are in the head of the files. It holds latin-1; the symbols come
// from the Montserrat of the same size, which is set as its fallback.
const lv_font_t* const FontSmall = &font_condensed_20;
const lv_font_t* const FontNormal = &font_condensed_28;
// the symbols on the big buttons and the warning of the safety stop
const lv_font_t* const FontBig = &lv_font_montserrat_48;

const uint32_t ColorBackground = 0x000000;
const uint32_t ColorBar = 0x1A1A1A;
const uint32_t ColorButton = 0x333333;
const uint32_t ColorAccent = 0xF39C12;
const uint32_t ColorText = 0xFFFFFF;
const uint32_t ColorDim = 0x9E9E9E;
const uint32_t ColorDisabled = 0x555555;
const uint32_t ColorStart = 0x2E7D32;
const uint32_t ColorStop = 0xC62828;
const uint32_t ColorEmergency = 0xE67E22;
const uint32_t ColorReset = 0x455A64;
const uint32_t ColorPrepare = 0xFF0000;
const uint32_t ColorAction = 0x00FF00;
const uint32_t ColorWarn = 0xF1C40F;
const uint32_t ColorOnline = 0x00C853;
const uint32_t ColorOffline = 0xE53935;

const int16_t BarHeight = 56;
const int16_t PageHeight = LCD_V_RES - BarHeight;
const int16_t ProgressWidth = LCD_H_RES - 24;
const int16_t ProgressHeight = 50;
const int16_t RowWidth = 388;
const uint8_t MaxArrows = 6;

const uint16_t SettingsSaveDelay = 400;
const uint16_t StatusInterval = 500;

enum Tab { TabTournament,
           TabSettings };

// ---------------------------------------------------------------------------
// state - the screen is rebuilt from it when the language changes
// ---------------------------------------------------------------------------

uint8_t activeTab = TabTournament;
bool running = false;
bool hold = false;
bool reshootAction = false;
// without a display on the network there is nobody to command
bool displaysOnline = false;
bool lunchBreak = false;
// the safety stop: the displays show the warning and hold the countdown until
// the bell is pressed again
bool emergency = false;
// the display reports minutes of shooting in, not seconds of a pass
bool shootInReported = false;

uint8_t currentGroup = 1;
uint16_t currentPass = 0;

// The segment lengths of the progress bar as reported by the display - it is
// the only place that knows whether a re-shoot or a full pass is running.
uint16_t segmentPrepare = 0;
uint16_t segmentAction = 0;
uint16_t segmentWarn = 0;
bool warnOnly = false;
uint16_t remainingPrepare = 0;
uint16_t remainingAction = 0;

bool settingsDirty = false;
unsigned long settingsDirtyAt = 0;
unsigned long statusNextTime = 0;

// ---------------------------------------------------------------------------
// widgets
// ---------------------------------------------------------------------------

lv_obj_t* settingsButton;
lv_obj_t* backButton;
lv_obj_t* pages[2];
lv_obj_t* statusLabel;
lv_obj_t* clockLabel;

lv_obj_t* reshootSwitch;
lv_obj_t* arrowRow;
lv_obj_t* arrowButtons[MaxArrows];
lv_obj_t* lunchSwitch;
lv_obj_t* startButton;
lv_obj_t* stopButton;
lv_obj_t* resetButton;
lv_obj_t* emergencyButton;
lv_obj_t* progressPrepare;
lv_obj_t* progressAction;
lv_obj_t* progressWarn;
lv_obj_t* progressSeparator1;
lv_obj_t* progressSeparator2;
lv_obj_t* progressOverlay;
lv_obj_t* passLabel;
lv_obj_t* progressFrame;
// takes the place of the progress bar during the safety stop and blinks
lv_obj_t* emergencyLabel;

struct Stepper {
  uint16_t* value;
  uint16_t minimum;
  uint16_t maximum;
  lv_obj_t* label;
};

Stepper steppers[] = {
  { &settings.passes, 1, 99, NULL },
  { &settings.arrowCount, 1, MaxArrows, NULL },
  { &settings.arrowTime, 1, 240, NULL },
  { &settings.prepareTime, 0, 120, NULL },
  { &settings.warnTime, 0, 240, NULL },
};

enum StepperId { StepperPasses,
                 StepperArrowCount,
                 StepperArrowTime,
                 StepperPrepareTime,
                 StepperWarnTime };

lv_obj_t* groupButtons[2];
lv_obj_t* actionTimeLabel;
lv_obj_t* lunchEndButton;
lv_obj_t* muteButton;
lv_obj_t* clockButton;
lv_obj_t* languageButtons[2];

lv_obj_t* dialog = NULL;
lv_obj_t* hourRoller;
lv_obj_t* minuteRoller;
typedef void (*TimeHandler)(uint8_t hour, uint8_t minute);
TimeHandler timeHandler = NULL;
typedef void (*ConfirmHandler)();
ConfirmHandler confirmHandler = NULL;

char hourOptions[24 * 3];
char minuteOptions[60 * 3];

// ---------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------

intptr_t eventData(lv_event_t* e) {
  return (intptr_t)lv_event_get_user_data(e);
}

void setBackground(lv_obj_t* object, uint32_t color) {
  lv_obj_set_style_bg_color(object, lv_color_hex(color), 0);
  lv_obj_set_style_bg_opa(object, LV_OPA_COVER, 0);
}

/** a plain rectangle without any of the theme on it */
lv_obj_t* makeBox(lv_obj_t* parent, int16_t x, int16_t y, int16_t width, int16_t height) {
  lv_obj_t* object = lv_obj_create(parent);
  lv_obj_remove_style_all(object);
  lv_obj_set_pos(object, x, y);
  lv_obj_set_size(object, width, height);
  lv_obj_set_scrollable(object, false);
  return object;
}

lv_obj_t* makeLabel(lv_obj_t* parent, const char* content, const lv_font_t* font, uint32_t color) {
  lv_obj_t* label = lv_label_create(parent);
  lv_label_set_text(label, content);
  lv_obj_set_style_text_font(label, font, 0);
  lv_obj_set_style_text_color(label, lv_color_hex(color), 0);
  return label;
}

lv_obj_t* makeButton(lv_obj_t* parent, int16_t x, int16_t y, int16_t width, int16_t height,
                     const char* content, const lv_font_t* font, uint32_t color) {
  lv_obj_t* button = lv_button_create(parent);
  lv_obj_set_pos(button, x, y);
  lv_obj_set_size(button, width, height);
  lv_obj_set_style_bg_color(button, lv_color_hex(color), 0);
  lv_obj_set_style_radius(button, 10, 0);
  lv_obj_set_style_shadow_width(button, 0, 0);
  lv_obj_set_style_pad_all(button, 0, 0);
  lv_obj_center(makeLabel(button, content, font, ColorText));
  return button;
}

lv_obj_t* buttonLabel(lv_obj_t* button) {
  return lv_obj_get_child(button, 0);
}

void onClicked(lv_obj_t* button, lv_event_cb_t handler, intptr_t data) {
  lv_obj_add_event_cb(button, handler, LV_EVENT_CLICKED, (void*)data);
}

/**
 * Setting a style redraws the widget even when nothing changes. The pages are
 * refreshed with every report of a display - once a second during a pass - so
 * a colour is only set when it is a different one.
 */
void setBgColor(lv_obj_t* object, uint32_t color) {
  lv_color_t wanted = lv_color_hex(color);
  if (!lv_color_eq(lv_obj_get_style_bg_color(object, LV_PART_MAIN), wanted)) {
    lv_obj_set_style_bg_color(object, wanted, 0);
  }
}

void setTextColor(lv_obj_t* object, uint32_t color) {
  lv_color_t wanted = lv_color_hex(color);
  if (!lv_color_eq(lv_obj_get_style_text_color(object, LV_PART_MAIN), wanted)) {
    lv_obj_set_style_text_color(object, wanted, 0);
  }
}

/** the look of a radio button or check box that is set */
void setChecked(lv_obj_t* button, bool checked) {
  setBgColor(button, checked ? ColorAccent : ColorButton);
  setTextColor(buttonLabel(button), checked ? 0x000000 : ColorText);
}

/** sets a switch from the code - that does not send its value changed event */
void setSwitch(lv_obj_t* object, bool on) {
  if (on) {
    lv_obj_add_state(object, LV_STATE_CHECKED);
  } else {
    lv_obj_remove_state(object, LV_STATE_CHECKED);
  }
}

/** a disabled widget is greyed out by the theme and takes no touches */
void setEnabled(lv_obj_t* object, bool enabled) {
  if (enabled) {
    lv_obj_remove_state(object, LV_STATE_DISABLED);
  } else {
    lv_obj_add_state(object, LV_STATE_DISABLED);
  }
}

/** a switch of the tournament page together with the name in front of it */
void setSwitchEnabled(lv_obj_t* object, bool enabled) {
  setEnabled(object, enabled);
  // makeSwitch() puts the label first into the box the two share
  lv_obj_t* label = lv_obj_get_child(lv_obj_get_parent(object), 0);
  setTextColor(label, enabled ? ColorText : ColorDisabled);
}

void setVisible(lv_obj_t* object, bool visible) {
  lv_obj_set_hidden(object, !visible);
}

/** only touches the label when the text changed, so nothing is redrawn in vain */
void setText(lv_obj_t* label, const char* content) {
  if (strcmp(lv_label_get_text(label), content) != 0) {
    lv_label_set_text(label, content);
  }
}

/** the tournament, or the re-shoot as long as its switch is on */
const char* subViewName() {
  return reshootAction ? "re_shoot" : "tournament";
}

/** the lunch break as long as its switch is on, else the timer */
const char* currentViewName() {
  return lunchBreak ? "lunch" : subViewName();
}

// ---------------------------------------------------------------------------
// tournament page
// ---------------------------------------------------------------------------

void setSegment(lv_obj_t* object, int16_t x, int16_t width) {
  setVisible(object, width > 0);
  if (width > 0) {
    lv_obj_set_pos(object, x, 0);
    lv_obj_set_size(object, width, ProgressHeight);
  }
}

void refreshProgress() {
  int32_t total = segmentPrepare + segmentAction;
  if (total <= 0) {
    setSegment(progressPrepare, 0, 0);
    setSegment(progressAction, 0, 0);
    setSegment(progressWarn, 0, 0);
    setSegment(progressSeparator1, 0, 0);
    setSegment(progressSeparator2, 0, 0);
    setSegment(progressOverlay, 0, 0);
    return;
  }

  int16_t prepareWidth = (segmentPrepare * (int32_t)ProgressWidth + total - 1) / total;
  int16_t warnWidth = warnOnly ? ProgressWidth - prepareWidth : (segmentWarn * (int32_t)ProgressWidth) / total;
  int16_t actionWidth = ProgressWidth - prepareWidth - warnWidth;

  setSegment(progressPrepare, 0, prepareWidth);
  setSegment(progressAction, prepareWidth, actionWidth);
  setSegment(progressWarn, prepareWidth + actionWidth, warnWidth);
  setSegment(progressSeparator1, prepareWidth - 6, prepareWidth > 6 ? 6 : 0);
  setSegment(progressSeparator2, prepareWidth + actionWidth - 6, actionWidth > 6 && warnWidth > 0 ? 6 : 0);

  int32_t elapsed = total - (remainingPrepare + remainingAction);
  elapsed = constrain(elapsed, 0, total);
  setSegment(progressOverlay, 0, elapsed * ProgressWidth / total);
}

void setSegments(uint16_t prepare, uint16_t action, uint16_t warn) {
  if (action == 0) {
    return;
  }
  segmentPrepare = prepare;
  segmentAction = action;
  warnOnly = warn >= action;
  segmentWarn = warnOnly ? action : warn;
}

/**
 * The bar of a pass that has not started - a guess until a display reports.
 * A re-shoot only runs for the arrows chosen on the tournament page.
 */
void resetProgress() {
  segmentPrepare = settings.prepareTime;
  segmentAction = reshootAction ? settings.reshootArrowCount * settings.arrowTime : settings.actionTime();
  warnOnly = settings.warnTime >= segmentAction;
  segmentWarn = warnOnly ? segmentAction : settings.warnTime;
  remainingPrepare = segmentPrepare;
  remainingAction = segmentAction;
  shootInReported = false;
}

void refreshTournament() {
  setSwitch(reshootSwitch, reshootAction);
  setSwitch(lunchSwitch, lunchBreak);

  setVisible(arrowRow, reshootAction);
  for (uint8_t i = 0; i < MaxArrows; i++) {
    setVisible(arrowButtons[i], i < settings.arrowCount);
    setChecked(arrowButtons[i], i + 1 == settings.reshootArrowCount);
  }

  char buffer[48];
  setText(buttonLabel(startButton), running && !hold ? LV_SYMBOL_PAUSE : LV_SYMBOL_PLAY);

  setBgColor(emergencyButton, emergency ? ColorPrepare : ColorEmergency);
  // during the safety stop only the bell is left to press
  // and without a display nobody would hear start or stop
  setEnabled(startButton, !emergency && displaysOnline);
  setEnabled(stopButton, !emergency && displaysOnline);
  // a reset would throw away the pass that is running - it has to be stopped first
  setEnabled(resetButton, !emergency && !running && displaysOnline);
  // the displays keep their view and their arrows for the length of a pass
  setSwitchEnabled(reshootSwitch, !emergency && !running && displaysOnline);
  setSwitchEnabled(lunchSwitch, !emergency && !running);
  for (uint8_t i = 0; i < MaxArrows; i++) {
    setEnabled(arrowButtons[i], !emergency && !running && displaysOnline);
  }
  snprintf(buffer, sizeof(buffer), "%s: %s - %s: %02u/%02u",
           text(TextGroup), currentGroup == 1 ? "AB" : "CD", text(TextPasses), currentPass, settings.passes);
  setText(passLabel, buffer);

  // the warning stands where the progress bar is; uiLoop() makes it blink
  setVisible(progressFrame, !emergency);
  // the bar shows what a display reports - without one it shows nothing true
  lv_obj_set_style_opa(progressFrame, displaysOnline ? LV_OPA_COVER : LV_OPA_30, 0);
  if (!emergency) {
    setVisible(emergencyLabel, false);
  }

  if (shootInReported) {
    // the bar shows a pass, there is nothing to show of it while shooting in
    uint16_t savedPrepare = remainingPrepare;
    remainingPrepare = segmentPrepare;
    remainingAction = segmentAction;
    refreshProgress();
    remainingPrepare = savedPrepare;
    remainingAction = 0;
  } else {
    refreshProgress();
  }
}

void onReshootChanged(lv_event_t* e) {
  // the displays do not change the view during a pass, so the switch must
  // not claim they did - and neither during the safety stop
  if (running || emergency) {
    refreshTournament();
    return;
  }
  reshootAction = lv_obj_has_state(reshootSwitch, LV_STATE_CHECKED);
  // the switch puts the timer on the displays - the break is over
  lunchBreak = false;
  netChangeView(subViewName());
  if (reshootAction) {
    settings.reshootArrowCount = 1;
  }
  netSendConfig();
  resetProgress();
  refreshTournament();
}

void onArrowClicked(lv_event_t* e) {
  // chosen before the start - during the pass the displays are counting with it
  if (running || emergency) {
    return;
  }
  settings.reshootArrowCount = eventData(e);
  netSendConfig();
  resetProgress();
  refreshTournament();
}

void onLunchChanged(lv_event_t* e) {
  if (running || emergency) {
    refreshTournament();
    return;
  }
  lunchBreak = lv_obj_has_state(lunchSwitch, LV_STATE_CHECKED);
  netChangeView(currentViewName());
  refreshTournament();
}

void startPass() {
  netSendRun();
  running = true;
  hold = false;
  refreshTournament();
}

void openConfirmDialog(const char* question, ConfirmHandler handler);

void onStartClicked(lv_event_t* e) {
  // only the bell lets the tournament go on
  if (emergency) {
    return;
  }
  if (!running) {
    // back to the timer first - the commands reach the displays in the order
    // sent
    if (lunchBreak) {
      lunchBreak = false;
      netChangeView(subViewName());
    }
    if (reshootAction) {
      openConfirmDialog(text(TextConfirmReShoot), startPass);
    } else {
      startPass();
    }
  } else if (!hold) {
    hold = true;
    netSendPause();
  } else {
    hold = false;
    netSendRun();
  }
  refreshTournament();
}

void onStopClicked(lv_event_t* e) {
  if (emergency) {
    return;
  }
  running = false;
  hold = false;
  netSendStop();
  refreshTournament();
}

/** the bell sets the safety stop, the next press releases it */
void onEmergencyClicked(lv_event_t* e) {
  emergency = !emergency;
  netSendEmergency(emergency);
  refreshTournament();
}

void onResetClicked(lv_event_t* e) {
  if (emergency || running) {
    return;
  }
  netSendConfig();
  netSendReset();
}

/** a switch with its name in front of it, as the settings page has them */
lv_obj_t* makeSwitch(lv_obj_t* parent, int16_t x, int16_t y, int16_t width, TextId label, lv_event_cb_t handler) {
  lv_obj_t* box = makeBox(parent, x, y, width, 56);
  lv_obj_align(makeLabel(box, text(label), FontNormal, ColorText), LV_ALIGN_LEFT_MID, 0, 0);
  lv_obj_t* object = lv_switch_create(box);
  lv_obj_set_size(object, 92, 48);
  lv_obj_align(object, LV_ALIGN_RIGHT_MID, 0, 0);
  lv_obj_add_event_cb(object, handler, LV_EVENT_VALUE_CHANGED, NULL);
  return object;
}

void buildTournamentPage(lv_obj_t* page) {
  reshootSwitch = makeSwitch(page, 12, 12, 300, TextReShoot, onReshootChanged);

  // the arrows of the re-shoot, only there while its switch is on
  arrowRow = makeBox(page, 368, 12, MaxArrows * 70, 56);
  for (uint8_t i = 0; i < MaxArrows; i++) {
    char number[2] = { (char)('1' + i), 0 };
    arrowButtons[i] = makeButton(arrowRow, i * 70, 0, 62, 56, number, FontNormal, ColorButton);
    onClicked(arrowButtons[i], onArrowClicked, i + 1);
  }

  startButton = makeButton(page, 12, 84, 185, 160, LV_SYMBOL_PLAY, FontBig, ColorStart);
  onClicked(startButton, onStartClicked, 0);
  stopButton = makeButton(page, 209, 84, 185, 160, LV_SYMBOL_STOP, FontBig, ColorStop);
  onClicked(stopButton, onStopClicked, 0);
  emergencyButton = makeButton(page, 406, 84, 185, 160, LV_SYMBOL_BELL, FontBig, ColorEmergency);
  onClicked(emergencyButton, onEmergencyClicked, 0);
  resetButton = makeButton(page, 603, 84, 185, 160, LV_SYMBOL_REFRESH, FontBig, ColorReset);
  onClicked(resetButton, onResetClicked, 0);

  // a grey line above and below the bar
  char warning[48];
  snprintf(warning, sizeof(warning), LV_SYMBOL_WARNING "  %s  " LV_SYMBOL_WARNING, text(TextEmergency));
  emergencyLabel = makeLabel(page, warning, FontBig, ColorPrepare);
  lv_obj_set_width(emergencyLabel, ProgressWidth);
  lv_obj_set_style_text_align(emergencyLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_pos(emergencyLabel, 12, 264);
  setVisible(emergencyLabel, false);

  progressFrame = makeBox(page, 12, 262, ProgressWidth, ProgressHeight + 8);
  lv_obj_t* frame = progressFrame;
  setBackground(frame, 0x787878);
  lv_obj_t* bar = makeBox(frame, 0, 4, ProgressWidth, ProgressHeight);
  setBackground(bar, 0x000000);
  progressPrepare = makeBox(bar, 0, 0, 0, ProgressHeight);
  setBackground(progressPrepare, ColorPrepare);
  progressAction = makeBox(bar, 0, 0, 0, ProgressHeight);
  setBackground(progressAction, ColorAction);
  progressWarn = makeBox(bar, 0, 0, 0, ProgressHeight);
  setBackground(progressWarn, ColorWarn);
  progressSeparator1 = makeBox(bar, 0, 0, 0, ProgressHeight);
  setBackground(progressSeparator1, 0x000000);
  progressSeparator2 = makeBox(bar, 0, 0, 0, ProgressHeight);
  setBackground(progressSeparator2, 0x000000);
  // what is over of the pass is dimmed
  progressOverlay = makeBox(bar, 0, 0, 0, ProgressHeight);
  setBackground(progressOverlay, 0x000000);
  lv_obj_set_style_bg_opa(progressOverlay, 150, 0);

  passLabel = makeLabel(page, "", FontNormal, ColorText);
  lv_obj_set_pos(passLabel, 12, 355);

  lunchSwitch = makeSwitch(page, 488, 344, 300, TextLunchBreak, onLunchChanged);
}

// ---------------------------------------------------------------------------
// dialogs
// ---------------------------------------------------------------------------

void closeDialog() {
  if (dialog != NULL) {
    lv_obj_delete_async(dialog);
    dialog = NULL;
  }
}

/** dims the screen, keeps the touches from it and returns the panel to fill */
lv_obj_t* openDialog(int16_t width, int16_t height) {
  closeDialog();
  dialog = makeBox(lv_layer_top(), 0, 0, LCD_H_RES, LCD_V_RES);
  setBackground(dialog, 0x000000);
  lv_obj_set_style_bg_opa(dialog, 180, 0);
  lv_obj_set_clickable(dialog, true);

  lv_obj_t* panel = makeBox(dialog, (LCD_H_RES - width) / 2, (LCD_V_RES - height) / 2, width, height);
  setBackground(panel, 0x222222);
  lv_obj_set_style_radius(panel, 12, 0);
  lv_obj_set_style_border_width(panel, 2, 0);
  lv_obj_set_style_border_color(panel, lv_color_hex(ColorAccent), 0);
  return panel;
}

void onDialogCancel(lv_event_t* e) {
  closeDialog();
}

void onConfirmYes(lv_event_t* e) {
  closeDialog();
  if (confirmHandler != NULL) {
    confirmHandler();
  }
}

void openConfirmDialog(const char* question, ConfirmHandler handler) {
  confirmHandler = handler;
  lv_obj_t* panel = openDialog(460, 220);
  lv_obj_align(makeLabel(panel, question, FontNormal, ColorText), LV_ALIGN_TOP_MID, 0, 36);
  onClicked(makeButton(panel, 30, 120, 180, 70, text(TextYes), FontNormal, ColorStart), onConfirmYes, 0);
  onClicked(makeButton(panel, 246, 120, 180, 70, text(TextNo), FontNormal, ColorButton), onDialogCancel, 0);
}

void onTimeOk(lv_event_t* e) {
  uint8_t hour = lv_roller_get_selected(hourRoller);
  uint8_t minute = lv_roller_get_selected(minuteRoller);
  closeDialog();
  if (timeHandler != NULL) {
    timeHandler(hour, minute);
  }
}

lv_obj_t* makeRoller(lv_obj_t* parent, const char* options, uint8_t selected, int16_t x) {
  lv_obj_t* roller = lv_roller_create(parent);
  lv_roller_set_options(roller, options, LV_ROLLER_MODE_INFINITE);
  lv_roller_set_visible_row_count(roller, 3);
  lv_obj_set_width(roller, 130);
  lv_obj_set_style_text_font(roller, FontNormal, 0);
  lv_obj_set_style_text_font(roller, FontNormal, LV_PART_SELECTED);
  lv_obj_set_style_bg_color(roller, lv_color_hex(ColorAccent), LV_PART_SELECTED);
  lv_obj_set_style_text_color(roller, lv_color_hex(0x000000), LV_PART_SELECTED);
  lv_roller_set_selected(roller, selected, LV_ANIM_OFF);
  lv_obj_set_pos(roller, x, 62);
  return roller;
}

/** the clock to set a time of day with, as the app has it */
void openTimeDialog(const char* title, uint8_t hour, uint8_t minute, TimeHandler handler) {
  if (hourOptions[0] == 0) {
    char* out = hourOptions;
    for (uint8_t i = 0; i < 24; i++) {
      out += sprintf(out, i < 23 ? "%02u\n" : "%02u", i);
    }
    out = minuteOptions;
    for (uint8_t i = 0; i < 60; i++) {
      out += sprintf(out, i < 59 ? "%02u\n" : "%02u", i);
    }
  }
  timeHandler = handler;
  lv_obj_t* panel = openDialog(420, 340);
  lv_obj_align(makeLabel(panel, title, FontNormal, ColorText), LV_ALIGN_TOP_MID, 0, 16);
  hourRoller = makeRoller(panel, hourOptions, hour % 24, 64);
  lv_obj_set_pos(makeLabel(panel, ":", FontNormal, ColorText), 205, 112);
  minuteRoller = makeRoller(panel, minuteOptions, minute % 60, 226);
  onClicked(makeButton(panel, 24, 254, 180, 64, text(TextCancel), FontNormal, ColorButton), onDialogCancel, 0);
  onClicked(makeButton(panel, 216, 254, 180, 64, text(TextOk), FontNormal, ColorStart), onTimeOk, 0);
}

// ---------------------------------------------------------------------------
// settings page
// ---------------------------------------------------------------------------

void refreshSettings() {
  char buffer[16];
  for (uint8_t i = 0; i < sizeof(steppers) / sizeof(steppers[0]); i++) {
    snprintf(buffer, sizeof(buffer), "%u", *steppers[i].value);
    setText(steppers[i].label, buffer);
  }
  snprintf(buffer, sizeof(buffer), "%u", settings.actionTime());
  setText(actionTimeLabel, buffer);

  setChecked(groupButtons[0], settings.groups == 1);
  setChecked(groupButtons[1], settings.groups == 2);
  setChecked(languageButtons[0], settings.language == LanguageGerman);
  setChecked(languageButtons[1], settings.language == LanguageEnglish);

  snprintf(buffer, sizeof(buffer), "%02u:%02u", settings.lunchHour, settings.lunchMinute);
  setText(buttonLabel(lunchEndButton), buffer);

  setText(buttonLabel(muteButton), settings.muted ? LV_SYMBOL_MUTE : LV_SYMBOL_VOLUME_MAX);
  setBgColor(muteButton, settings.muted ? ColorStop : ColorButton);
}

/**
 * Every change goes to the displays, as in the app - but a held down key
 * changes a value many times a second, so flash and network wait until the
 * finger has come to rest.
 */
void markSettingsChanged() {
  settingsDirty = true;
  settingsDirtyAt = millis();
  refreshSettings();
}

void flushSettings() {
  settingsDirty = false;
  if (settings.reshootArrowCount > settings.arrowCount) {
    settings.reshootArrowCount = settings.arrowCount;
  }
  settingsSave();
  netSendConfig();
  if (!running) {
    resetProgress();
  }
  refreshTournament();
}

void onStepper(lv_event_t* e) {
  intptr_t data = eventData(e);
  Stepper& stepper = steppers[data >> 1];
  if (data & 1) {
    if (*stepper.value >= stepper.maximum) {
      return;
    }
    (*stepper.value)++;
  } else {
    if (*stepper.value <= stepper.minimum) {
      return;
    }
    (*stepper.value)--;
  }
  markSettingsChanged();
}

void onGroupClicked(lv_event_t* e) {
  settings.groups = eventData(e);
  markSettingsChanged();
}

void onFlashingChanged(lv_event_t* e) {
  settings.flashingPrepareLight = lv_obj_has_state(lv_event_get_target_obj(e), LV_STATE_CHECKED);
  markSettingsChanged();
}

void onVolumeReleased(lv_event_t* e) {
  settings.volume = lv_slider_get_value(lv_event_get_target_obj(e));
  // who sets a volume wants to hear it
  settings.muted = false;
  settingsSave();
  netSendVolume();
  refreshSettings();
}

/** the speaker switches the sound of the displays off and on again */
void onMuteClicked(lv_event_t* e) {
  settings.muted = !settings.muted;
  settingsSave();
  netSendVolume();
  refreshSettings();
}

void onTestSignalClicked(lv_event_t* e) {
  netSendTestSignal();
}

void onLunchEndSet(uint8_t hour, uint8_t minute) {
  settings.lunchHour = hour;
  settings.lunchMinute = minute;
  settingsSave();
  refreshSettings();
}

void onLunchEndClicked(lv_event_t* e) {
  openTimeDialog(text(TextLunchEnd), settings.lunchHour, settings.lunchMinute, onLunchEndSet);
}

void onClockSet(uint8_t hour, uint8_t minute) {
  clockSetManual(hour, minute);
  netClockChanged();
  statusNextTime = millis();
}

void onClockClicked(lv_event_t* e) {
  uint8_t hour = 12;
  uint8_t minute = 0;
  clockLocalTime(&hour, &minute);
  openTimeDialog(text(TextClock), hour, minute, onClockSet);
}

void buildScreen();

void rebuildScreen(void* unused) {
  buildScreen();
}

void onLanguageClicked(lv_event_t* e) {
  uint16_t language = eventData(e);
  if (language == settings.language) {
    return;
  }
  settings.language = language;
  settingsSave();
  // the button that was just pressed is part of what gets rebuilt, so that
  // has to wait until its event is through
  lv_async_call(rebuildScreen, NULL);
}

lv_obj_t* makeRow(lv_obj_t* page, uint8_t column, uint8_t row, TextId label) {
  lv_obj_t* box = makeBox(page, 8 + column * 396, 6 + row * 59, RowWidth, 55);
  lv_obj_align(makeLabel(box, text(label), FontSmall, ColorText), LV_ALIGN_LEFT_MID, 4, 0);
  return box;
}

void makeStepper(lv_obj_t* row, uint8_t id) {
  lv_obj_t* minus = makeButton(row, RowWidth - 160, 3, 50, 48, LV_SYMBOL_MINUS, FontSmall, ColorButton);
  lv_obj_t* plus = makeButton(row, RowWidth - 50, 3, 50, 48, LV_SYMBOL_PLUS, FontSmall, ColorButton);
  // a short tap is one step, a held down key keeps stepping
  lv_obj_add_event_cb(minus, onStepper, LV_EVENT_SHORT_CLICKED, (void*)(intptr_t)(id << 1));
  lv_obj_add_event_cb(minus, onStepper, LV_EVENT_LONG_PRESSED_REPEAT, (void*)(intptr_t)(id << 1));
  lv_obj_add_event_cb(plus, onStepper, LV_EVENT_SHORT_CLICKED, (void*)(intptr_t)((id << 1) | 1));
  lv_obj_add_event_cb(plus, onStepper, LV_EVENT_LONG_PRESSED_REPEAT, (void*)(intptr_t)((id << 1) | 1));

  lv_obj_t* label = makeLabel(row, "", FontNormal, ColorAccent);
  lv_obj_set_width(label, 56);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_pos(label, RowWidth - 108, 10);
  steppers[id].label = label;
}

void buildSettingsPage(lv_obj_t* page) {
  lv_obj_t* row = makeRow(page, 0, 0, TextGroups);
  groupButtons[0] = makeButton(row, RowWidth - 190, 3, 80, 48, "AB", FontSmall, ColorButton);
  onClicked(groupButtons[0], onGroupClicked, 1);
  groupButtons[1] = makeButton(row, RowWidth - 102, 3, 102, 48, "AB-CD", FontSmall, ColorButton);
  onClicked(groupButtons[1], onGroupClicked, 2);

  makeStepper(makeRow(page, 0, 1, TextPasses), StepperPasses);
  makeStepper(makeRow(page, 0, 2, TextArrowCount), StepperArrowCount);
  makeStepper(makeRow(page, 0, 3, TextArrowTime), StepperArrowTime);

  row = makeRow(page, 0, 4, TextActionTime);
  actionTimeLabel = makeLabel(row, "", FontNormal, ColorDim);
  lv_obj_set_width(actionTimeLabel, 56);
  lv_obj_set_style_text_align(actionTimeLabel, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_set_pos(actionTimeLabel, RowWidth - 108, 10);

  makeStepper(makeRow(page, 0, 5, TextPrepareTime), StepperPrepareTime);
  makeStepper(makeRow(page, 0, 6, TextWarnTime), StepperWarnTime);

  row = makeRow(page, 1, 1, TextLunchEnd);
  lunchEndButton = makeButton(row, RowWidth - 120, 3, 120, 48, "", FontNormal, ColorButton);
  lv_obj_set_style_text_color(buttonLabel(lunchEndButton), lv_color_hex(ColorAccent), 0);
  onClicked(lunchEndButton, onLunchEndClicked, 0);

  row = makeRow(page, 1, 2, TextFlashingPrepareLight);
  lv_obj_t* flashing = lv_switch_create(row);
  lv_obj_set_size(flashing, 76, 40);
  lv_obj_align(flashing, LV_ALIGN_RIGHT_MID, 0, 0);
  if (settings.flashingPrepareLight) {
    lv_obj_add_state(flashing, LV_STATE_CHECKED);
  }
  lv_obj_add_event_cb(flashing, onFlashingChanged, LV_EVENT_VALUE_CHANGED, NULL);

  row = makeRow(page, 1, 3, TextVolume);
  muteButton = makeButton(row, RowWidth - 286, 3, 50, 48, LV_SYMBOL_VOLUME_MAX, FontSmall, ColorButton);
  onClicked(muteButton, onMuteClicked, 0);
  lv_obj_t* volume = lv_slider_create(row);
  lv_obj_set_size(volume, 130, 16);
  lv_obj_set_pos(volume, RowWidth - 214, 20);
  lv_slider_set_range(volume, 0, 30);
  lv_slider_set_value(volume, settings.volume, LV_ANIM_OFF);
  // a bigger area to catch the finger than the thin bar
  lv_obj_set_ext_click_area(volume, 18);
  lv_obj_add_event_cb(volume, onVolumeReleased, LV_EVENT_RELEASED, NULL);
  onClicked(makeButton(row, RowWidth - 60, 3, 60, 48, LV_SYMBOL_BELL, FontSmall, ColorButton), onTestSignalClicked, 0);

  row = makeRow(page, 1, 4, TextLanguage);
  languageButtons[0] = makeButton(row, RowWidth - 168, 3, 80, 48, "DE", FontSmall, ColorButton);
  onClicked(languageButtons[0], onLanguageClicked, LanguageGerman);
  languageButtons[1] = makeButton(row, RowWidth - 80, 3, 80, 48, "EN", FontSmall, ColorButton);
  onClicked(languageButtons[1], onLanguageClicked, LanguageEnglish);

  row = makeRow(page, 1, 0, TextClock);
  clockButton = makeButton(row, RowWidth - 120, 3, 120, 48, "--:--", FontNormal, ColorButton);
  lv_obj_set_style_text_color(buttonLabel(clockButton), lv_color_hex(ColorAccent), 0);
  onClicked(clockButton, onClockClicked, 0);
}

// ---------------------------------------------------------------------------
// frame: tabs and status
// ---------------------------------------------------------------------------

void refreshTabs() {
  for (uint8_t i = 0; i < 2; i++) {
    setVisible(pages[i], i == activeTab);
  }
  // the bar only offers the way to the other page
  setVisible(settingsButton, activeTab == TabTournament);
  setVisible(backButton, activeTab == TabSettings);
}

/**
 * The displays follow the page that is open, as they follow the tabs of the
 * app: the settings page puts them on their setup view.
 */
void onTabClicked(lv_event_t* e) {
  uint8_t tab = eventData(e);
  if (tab == activeTab) {
    return;
  }
  if (settingsDirty) {
    flushSettings();
  }
  activeTab = tab;
  refreshTabs();
  netChangeView(tab == TabSettings ? "setup" : currentViewName());
}

void refreshStatus() {
  char buffer[32];
  uint8_t online = netDisplaysOnline();
  uint8_t known = netDisplaysKnown();
  if ((online > 0) != displaysOnline) {
    displaysOnline = online > 0;
    refreshTournament();
  }
  snprintf(buffer, sizeof(buffer), LV_SYMBOL_WIFI " %u/%u", online, known);
  setText(statusLabel, buffer);
  bool good = netRadioReady() && online > 0 && online == known;
  bool bad = !netRadioReady() || online == 0;
  setTextColor(statusLabel, good ? ColorOnline : bad ? ColorOffline : ColorAccent);

  uint8_t hour;
  uint8_t minute;
  if (clockLocalTime(&hour, &minute)) {
    snprintf(buffer, sizeof(buffer), "%02u:%02u", hour, minute);
  } else {
    strcpy(buffer, "--:--");
  }
  setText(clockLabel, buffer);
  setText(buttonLabel(clockButton), buffer);
}

void buildScreen() {
  lv_obj_t* screen = lv_screen_active();
  lv_obj_clean(screen);
  setBackground(screen, ColorBackground);
  lv_obj_set_scrollable(screen, false);

  lv_obj_t* bar = makeBox(screen, 0, 0, LCD_H_RES, BarHeight);
  setBackground(bar, ColorBar);
  settingsButton = makeButton(bar, 8, 4, 230, 48, text(TextTabSettings), FontNormal, ColorButton);
  onClicked(settingsButton, onTabClicked, TabSettings);
  backButton = makeButton(bar, 8, 4, 90, 48, LV_SYMBOL_LEFT, FontNormal, ColorButton);
  onClicked(backButton, onTabClicked, TabTournament);

  statusLabel = makeLabel(bar, "", FontNormal, ColorOffline);
  lv_obj_align(statusLabel, LV_ALIGN_RIGHT_MID, -120, 0);
  clockLabel = makeLabel(bar, "--:--", FontNormal, ColorText);
  lv_obj_align(clockLabel, LV_ALIGN_RIGHT_MID, -12, 0);

  pages[TabTournament] = makeBox(screen, 0, BarHeight, LCD_H_RES, PageHeight);
  buildTournamentPage(pages[TabTournament]);
  pages[TabSettings] = makeBox(screen, 0, BarHeight, LCD_H_RES, PageHeight);
  buildSettingsPage(pages[TabSettings]);

  refreshTabs();
  refreshSettings();
  refreshTournament();
  refreshStatus();
}

/**
 * After a start the displays may be on a view somebody
 * else put them on - the app, or this controller before it was restarted.
 * The page takes it over instead of pushing its own choice back.
 */
void adoptView(const char* viewName) {
  if (strcmp(viewName, "lunch") == 0) {
    lunchBreak = true;
  } else if (strcmp(viewName, "tournament") == 0) {
    reshootAction = false;
    lunchBreak = false;
  } else if (strcmp(viewName, "re_shoot") == 0) {
    reshootAction = true;
    lunchBreak = false;
  } else {
    return;  // home, setup, state, shoot_in: views this controller has no page for
  }
  if (reshootAction && settings.reshootArrowCount == 0) {
    settings.reshootArrowCount = 1;
  }
  if (activeTab == TabTournament) {
    netAdoptView(currentViewName());
  }
}

}  // namespace

// ---------------------------------------------------------------------------
// public
// ---------------------------------------------------------------------------

void uiBegin() {
  lv_display_t* display = lv_display_get_default();
  lv_display_set_theme(display, lv_theme_default_init(display, lv_color_hex(ColorAccent), lv_color_hex(ColorReset), true, FontSmall));

  resetProgress();
  buildScreen();
  netAdoptView(currentViewName());
}

void uiLoop() {
  unsigned long now = millis();
  if (settingsDirty && now - settingsDirtyAt > SettingsSaveDelay) {
    flushSettings();
  }
  if ((long)(now - statusNextTime) >= 0) {
    statusNextTime = now + StatusInterval;
    refreshStatus();
  }
  if (emergency) {
    // half a second on, half a second off
    setVisible(emergencyLabel, (now / 500) % 2 == 0);
  }
}

void uiHandleDisplayEvent(const char* command, JsonDocument& message, bool resynced) {
  if (strcmp(command, "run") == 0) {
    running = true;
    hold = false;
  } else if (strcmp(command, "pause") == 0) {
    hold = running;
  } else if (strcmp(command, "stop") == 0) {
    running = false;
    hold = false;
  } else if (strcmp(command, "reset") == 0) {
    running = false;
    hold = false;
    currentGroup = 1;
    currentPass = 0;
    resetProgress();
  } else if (strcmp(command, "emergency") == 0) {
    // set or released from the app
    emergency = message["e"] | false;
  } else if (strcmp(command, "state") == 0) {
    if (!message["em"].isNull()) {
      emergency = message["em"] | false;
    }
    currentGroup = message["cg"] | 1;
    currentPass = message["cp"] | 0;
    if (!message["sat"].isNull()) {
      setSegments(message["spt"] | 0, message["sat"] | 0, message["swt"] | 0);
    }
    remainingPrepare = message["cpt"] | 0;
    remainingAction = message["cat"] | 0;
    const char* viewName = message["v"] | "";
    shootInReported = strcmp(viewName, "shoot_in") == 0;

    // The display is the one that knows whether a pass is still running: it
    // ends on its own when the time is up. Following its report keeps the
    // start button from showing "pause" for a pass that is long over.
    if (!message["ph"].isNull()) {
      running = (message["ph"] | 0) == 1;
      hold = running && (message["h"] | hold);
    }
    if (resynced) {
      adoptView(viewName);
    }
  } else {
    return;
  }
  refreshTournament();
}
