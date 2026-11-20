#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

/**
 * Called for everything the reporting display tells about the tournament:
 * "run", "pause", "stop", "reset" and "state".
 *
 * @param resynced true when this report ended a stretch in which the
 *        controller did not know what the displays were doing - after its
 *        start and after the wifi was gone.
 */
typedef void (*NetEventHandler)(const char* command, JsonDocument& message, bool resynced);

void netBegin(NetEventHandler handler);
void netLoop();

bool netWifiConnected();
uint8_t netDisplaysOnline();
uint8_t netDisplaysKnown();

void netSendRun();
void netSendPause();
void netSendStop();
void netSendReset();
/** sets or releases the safety stop */
void netSendEmergency(bool on);
void netSendConfig();
void netSendVolume();
void netSendTestSignal();

/**
 * Puts a view on the displays: "shoot_in", "tournament", "re_shoot", "setup"
 * or "lunch". The lunch view gets the end of the break from the settings.
 */
void netChangeView(const char* viewName);

/** the view the displays are on, learned from a display instead of sent */
void netAdoptView(const char* viewName);

/** the clock was set by hand - the displays are to follow */
void netClockChanged();
