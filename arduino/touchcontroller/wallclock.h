#pragma once

#include <Arduino.h>

/**
 * The time of day. The board has no clock chip, so the time comes from
 * wherever it can be had - in rising order of trust: a display that reports
 * its DS3231, the settings page, an ntp server.
 *
 * The countdown does not depend on it. It is needed for the end of the lunch
 * break, for the clock on the status view of the displays and for sequence
 * numbers that stay ahead of the ones the android app hands out.
 */
enum ClockSource { ClockNone = 0,
                   ClockDisplay,
                   ClockManual,
                   ClockNtp };

void clockBegin();

/** asks the ntp server - to be called whenever the wifi comes up */
void clockStartNtp();

ClockSource clockSource();
bool clockValid();

/** ms since 1970 (UTC) */
uint64_t clockNowMs();

/** takes over the clock of a display, as long as there is nothing better */
void clockAdopt(uint64_t epochMs);

/** sets the local time of day; the date is kept, or made up when unknown */
void clockSetManual(uint8_t hour, uint8_t minute);

/** local time minus UTC in seconds */
int32_t clockTzOffset();

/** that local time of today in seconds since 1970, 0 when the clock is not set */
uint32_t clockEpochToday(uint8_t hour, uint8_t minute);

/** the local time of day, false when the clock is not set */
bool clockLocalTime(uint8_t* hour, uint8_t* minute);
