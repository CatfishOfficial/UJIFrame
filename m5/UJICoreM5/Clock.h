/*
 * Clock.h — pulls real time over WiFi via NTP and exposes it as the
 * "time" command. ensureTimeSynced() lives here (not Http.h) since
 * knowing the current time is its own capability now, not just a
 * prerequisite for checking a TLS certificate's valid-date range --
 * Http.h calls the version declared here.
 *
 * NTP always syncs the underlying clock in UTC (configTime(0, 0, ...)
 * below) -- local-time display is a separate concern, handled by
 * applyTimezone() setting the C library's TZ environment variable from
 * a POSIX TZ string (config timezone <name>, see Config.h's
 * TIMEZONE_TZ_STRINGS). Once that's set, localtime_r()/%Z below convert
 * and label the zone correctly, DST included.
 *
 * Gotcha that actually bit this once already: configTime() doesn't just
 * sync the clock -- on this core it *also* calls setenv("TZ", ...) +
 * tzset() itself (deriving a fixed-offset TZ string from the 0,0 passed
 * in), which silently stomps whatever applyTimezone() had set. Every
 * configTime() call site (here, and wifiConnect()/
 * wifiAutoConnectIfEnabled() in WifiCommand.h) re-applies the timezone
 * right after, and formatCurrentTime() re-applies it again immediately
 * before reading local time, so display is correct even if something
 * else resynced in between.
 *
 * No RTC-chip/persistence work here on purpose -- just the sync + a way
 * to read it. Once configTime() has synced, the ESP32's own system
 * clock keeps ticking in the background on its own; nothing extra is
 * needed to keep it "running."
 */
#ifndef UJI_M5_CLOCK_H
#define UJI_M5_CLOCK_H
#include "Globals.h"
#include <WiFi.h>

void applyTimezone() {
  setenv("TZ", TIMEZONE_TZ_STRINGS[CONFIG.timezone], 1);
  tzset();
}

bool ensureTimeSynced() {
  if (time(nullptr) > 1700000000) { applyTimezone(); return true; } // already synced (some time in 2023+)
  if (WiFi.status() != WL_CONNECTED) return false;

  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  applyTimezone(); // configTime() above just reset TZ to UTC -- put it back
  unsigned long start = millis();
  while (time(nullptr) < 1700000000 && millis() - start < 8000) {
    delay(200);
  }
  return time(nullptr) > 1700000000;
}

static String formatCurrentTime() {
  applyTimezone(); // belt-and-suspenders: correct even if something resynced since the last apply
  time_t now = time(nullptr);
  struct tm tmInfo;
  localtime_r(&now, &tmInfo);
  char buf[40];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S %Z", &tmInfo);
  return String(buf);
}

// Shorter, glanceable formats for the idle-mode widget -- formatCurrentTime()
// above stays as the technical one used by the "time" command.
String formatIdleDate() {
  applyTimezone();
  time_t now = time(nullptr);
  struct tm tmInfo;
  localtime_r(&now, &tmInfo);
  char buf[16];
  strftime(buf, sizeof(buf), "%a %b %d", &tmInfo);
  return String(buf);
}

String formatIdleTime() {
  applyTimezone();
  time_t now = time(nullptr);
  struct tm tmInfo;
  localtime_r(&now, &tmInfo);
  char buf[16];
  strftime(buf, sizeof(buf), "%I:%M %p", &tmInfo);
  return String(buf);
}

// Local hour (0-23), used by Idle.h to decide day vs. night -- callers
// must already know time is synced (ensureTimeSynced()/time(nullptr) check).
int currentLocalHour() {
  applyTimezone();
  time_t now = time(nullptr);
  struct tm tmInfo;
  localtime_r(&now, &tmInfo);
  return tmInfo.tm_hour;
}

void timeRun(const String& args) {
  if (!ensureTimeSynced()) {
    termPrintln("Couldn't sync time -- check WiFi (\"wifi connect\").", COLOR_DANGER);
    return;
  }
  termPrintln(formatCurrentTime(), COLOR_TEXT);
}

#endif // UJI_M5_CLOCK_H
