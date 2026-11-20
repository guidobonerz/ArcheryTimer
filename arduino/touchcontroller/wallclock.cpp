#include "wallclock.h"

#include <sys/time.h>
#include <time.h>

#include "esp_sntp.h"

#include "config.h"

namespace {

// 2026-01-01 00:00 UTC - the day the clock is set on when only the time of
// day is known. Displays and controller then agree on a made up date, which
// is all the end of the lunch break needs.
const time_t FALLBACK_DAY = 1767225600;

ClockSource source = ClockNone;

// set from the sntp task
volatile bool ntpSynced = false;

void onNtpSync(struct timeval* tv) {
  ntpSynced = true;
}

void setClock(uint64_t epochMs) {
  struct timeval tv = { (time_t)(epochMs / 1000), (suseconds_t)((epochMs % 1000) * 1000) };
  settimeofday(&tv, NULL);
}

}  // namespace

void clockBegin() {
  setenv("TZ", TIMEZONE, 1);
  tzset();
}

void clockStartNtp() {
#ifdef NTP_SERVER
  sntp_set_time_sync_notification_cb(onNtpSync);
  configTzTime(TIMEZONE, NTP_SERVER);
#endif
}

ClockSource clockSource() {
  if (ntpSynced) {
    source = ClockNtp;
  }
  return source;
}

bool clockValid() {
  return clockSource() != ClockNone;
}

uint64_t clockNowMs() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (uint64_t)tv.tv_sec * 1000ULL + (uint64_t)(tv.tv_usec / 1000);
}

void clockAdopt(uint64_t epochMs) {
  if (clockValid()) {
    return;
  }
  setClock(epochMs);
  source = ClockDisplay;
}

void clockSetManual(uint8_t hour, uint8_t minute) {
  time_t now = clockValid() ? time(NULL) : FALLBACK_DAY;
  struct tm local;
  localtime_r(&now, &local);
  local.tm_hour = hour;
  local.tm_min = minute;
  local.tm_sec = 0;
  local.tm_isdst = -1;
  setClock((uint64_t)mktime(&local) * 1000ULL);
  // stands until the ntp server answers the next time
  ntpSynced = false;
  source = ClockManual;
}

int32_t clockTzOffset() {
  time_t now = time(NULL);
  struct tm local;
  struct tm utc;
  localtime_r(&now, &local);
  gmtime_r(&now, &utc);
  // read as local time the UTC fields give an instant that is off by exactly
  // the offset
  utc.tm_isdst = local.tm_isdst;
  return (int32_t)(now - mktime(&utc));
}

uint32_t clockEpochToday(uint8_t hour, uint8_t minute) {
  if (!clockValid()) {
    return 0;
  }
  time_t now = time(NULL);
  struct tm local;
  localtime_r(&now, &local);
  local.tm_hour = hour;
  local.tm_min = minute;
  local.tm_sec = 0;
  local.tm_isdst = -1;
  return (uint32_t)mktime(&local);
}

bool clockLocalTime(uint8_t* hour, uint8_t* minute) {
  if (!clockValid()) {
    return false;
  }
  time_t now = time(NULL);
  struct tm local;
  localtime_r(&now, &local);
  *hour = local.tm_hour;
  *minute = local.tm_min;
  return true;
}
