/*
 * TimeFeatures.h — alarms, countdown timer, and stopwatch, all terminal-
 * based (no new display modes or UI pages). Fits the existing "a line in
 * the scrollback that updates in place" pattern Idle.h uses for its day-
 * idle widget, via printLineGetIndex()/updateScrollLineAt()/redrawVisibleLine().
 *
 * Alarms are persisted to NVS (Preferences, "ujialm" namespace). Timer
 * and stopwatch are ephemeral -- millis()-based, reset on reboot.
 *
 * Alarm list color coding: repeat alarms in accent color, one-shot in dim.
 * Alarm firing: brief double-beep (if beep on), ~4s color flash on the
 * notification line (accent <-> dim, 500ms alternating -- enough to notice,
 * not enough to be annoying).
 *
 * Timer hourglass and stopwatch state icons are marked for easy removal --
 * see HOURGLASS ANIMATION BEGIN/END and STOPWATCH STATE ICON BEGIN/END.
 *
 * Stopwatch tap-to-start/stop: timeConsumeStopwatchTap() intercepts
 * scrollback-area taps when the stopwatch is READY or RUNNING (caller in
 * UJICoreM5.ino's loop() skips openCategoryMenu() when this returns true).
 * Once STOPPED, taps fall through normally again.
 */
#ifndef UJI_M5_TIMEFEATURES_H
#define UJI_M5_TIMEFEATURES_H
#include "Globals.h"
#include <Preferences.h>

// ── alarms ────────────────────────────────────────────────────────────────
#define MAX_ALARMS 8
#define TIME_NO_LINE 255

struct Alarm { uint8_t hour; uint8_t minute; bool repeat; };
static Alarm alarms[MAX_ALARMS];
static uint8_t alarmCount = 0;

static void saveAlarms() {
  Preferences p;
  p.begin("ujialm", false);
  p.putUChar("cnt", alarmCount);
  for (uint8_t i = 0; i < alarmCount; i++) {
    char kh[5], km[5], kr[5];
    snprintf(kh, 5, "a%dh", i);
    snprintf(km, 5, "a%dm", i);
    snprintf(kr, 5, "a%dr", i);
    p.putUChar(kh, alarms[i].hour);
    p.putUChar(km, alarms[i].minute);
    p.putBool(kr,  alarms[i].repeat);
  }
  p.end();
}

void timeInit() {
  Preferences p;
  p.begin("ujialm", true);
  alarmCount = p.getUChar("cnt", 0);
  if (alarmCount > MAX_ALARMS) alarmCount = 0;
  for (uint8_t i = 0; i < alarmCount; i++) {
    char kh[5], km[5], kr[5];
    snprintf(kh, 5, "a%dh", i);
    snprintf(km, 5, "a%dm", i);
    snprintf(kr, 5, "a%dr", i);
    alarms[i].hour   = p.getUChar(kh, 0);
    alarms[i].minute = p.getUChar(km, 0);
    alarms[i].repeat = p.getBool(kr,  false);
  }
  p.end();
}

static void listAlarms() {
  if (alarmCount == 0) { termPrintln("No alarms set.", COLOR_DIM); return; }
  for (uint8_t i = 0; i < alarmCount; i++) {
    char buf[36];
    snprintf(buf, sizeof(buf), "%d.  %02d:%02d   %s",
             (int)(i + 1), (int)alarms[i].hour, (int)alarms[i].minute,
             alarms[i].repeat ? "(repeat)" : "(once)");
    termPrintln(String(buf), alarms[i].repeat ? colorAccent() : COLOR_DIM);
  }
  termPrintln("accent = repeat   dim = once   |   alarm delete N to remove", COLOR_DIM);
}

// ── alarm firing state ────────────────────────────────────────────────────
static uint8_t       alarmNotifyLineIdx  = TIME_NO_LINE;
static String        alarmNotifyText     = "";
static unsigned long alarmNotifyMs       = 0;
static unsigned long alarmLastFlipMs     = 0;
static bool          alarmNotifyFlip     = false;
static int           alarmLastCheckedMin = -1; // prevents double-fire within a minute

// ── time parsing helpers ──────────────────────────────────────────────────
static bool parseHMTime(const String& s, uint8_t& h, uint8_t& m) {
  int colon = s.indexOf(':');
  if (colon <= 0 || colon >= (int)s.length() - 1) return false;
  int hr = s.substring(0, colon).toInt();
  int mn = s.substring(colon + 1).toInt();
  if (hr < 0 || hr > 23 || mn < 0 || mn > 59) return false;
  h = (uint8_t)hr; m = (uint8_t)mn;
  return true;
}

static bool parseTimerDuration(const String& s, uint32_t& outMs) {
  int first = s.indexOf(':');
  if (first == -1) {
    int sec = s.toInt();
    if (sec <= 0) return false;
    outMs = (uint32_t)sec * 1000UL;
    return true;
  }
  int second = s.indexOf(':', first + 1);
  if (second == -1) {
    int mm = s.substring(0, first).toInt();
    int ss = s.substring(first + 1).toInt();
    if (mm < 0 || ss < 0 || ss > 59) return false;
    outMs = ((uint32_t)mm * 60UL + (uint32_t)ss) * 1000UL;
    return true;
  }
  int hh = s.substring(0, first).toInt();
  int mm = s.substring(first + 1, second).toInt();
  int ss = s.substring(second + 1).toInt();
  if (hh < 0 || mm < 0 || mm > 59 || ss < 0 || ss > 59) return false;
  outMs = ((uint32_t)hh * 3600UL + (uint32_t)mm * 60UL + (uint32_t)ss) * 1000UL;
  return true;
}

static String formatDuration(uint32_t ms) {
  unsigned long s   = ms / 1000;
  unsigned long h   = s / 3600;
  unsigned long m   = (s % 3600) / 60;
  unsigned long sec = s % 60;
  char buf[12];
  snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu", h, m, sec);
  return String(buf);
}

static String formatStopwatchTime(uint32_t ms) {
  unsigned long cs  = (ms / 10) % 100;
  unsigned long s   = ms / 1000;
  unsigned long h   = s / 3600;
  unsigned long m   = (s % 3600) / 60;
  unsigned long sec = s % 60;
  char buf[16];
  snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu.%02lu", h, m, sec, cs);
  return String(buf);
}

// ── timer ─────────────────────────────────────────────────────────────────
static bool          timerRunning      = false;
static bool          timerDone         = false;
static uint32_t      timerDurationMs   = 0;
static unsigned long timerStartMs      = 0;
static uint8_t       timerLineIdx      = TIME_NO_LINE;
static unsigned long timerLastUpdateMs = 0;

static String buildTimerLine(uint32_t remainingMs) {
  String line = "Timer  " + formatDuration(remainingMs);

  // ── HOURGLASS ANIMATION BEGIN (remove this block if not desired) ──
  if (timerDone) {
    line += "   [done]";
  } else if (timerDurationMs > 0) {
    uint8_t pct = (uint8_t)(100 - (remainingMs * 100UL / timerDurationMs));
    if      (pct < 25)  line += "   [===]";
    else if (pct < 50)  line += "   [== ]";
    else if (pct < 75)  line += "   [=  ]";
    else                line += "   [   ]";
  }
  // ── HOURGLASS ANIMATION END ──

  return line;
}

void timerRun(const String& args) {
  String a = args;
  a.trim();

  if (a == "stop" || a == "cancel") {
    if (!timerRunning && !timerDone) { termPrintln("No timer running.", COLOR_DIM); return; }
    timerRunning = false; timerDone = false; timerLineIdx = TIME_NO_LINE;
    termPrintln("Timer cancelled.", COLOR_DIM);
    return;
  }

  if (a.length() == 0) {
    if (timerRunning) {
      uint32_t elapsed = (uint32_t)(millis() - timerStartMs);
      uint32_t rem = elapsed >= timerDurationMs ? 0 : timerDurationMs - elapsed;
      termPrintln("Timer: " + formatDuration(rem) + " remaining.", COLOR_TEXT);
    } else if (timerDone) {
      termPrintln("Timer done. Run 'timer MM:SS' to start a new one.", COLOR_DIM);
    } else {
      termPrintln("Usage: timer MM:SS  |  timer HH:MM:SS  |  timer <seconds>  |  timer stop", COLOR_DIM);
    }
    return;
  }

  uint32_t dur;
  if (!parseTimerDuration(a, dur)) {
    termPrintln("Usage: timer MM:SS  |  timer HH:MM:SS  |  timer <seconds>", COLOR_DIM);
    return;
  }

  timerDurationMs   = dur;
  timerStartMs      = millis();
  timerRunning      = true;
  timerDone         = false;
  timerLastUpdateMs = millis();

  timerLineIdx = printLineGetIndex(buildTimerLine(dur), colorAccent());
  drawScrollback();
}

// ── stopwatch ─────────────────────────────────────────────────────────────
enum SwState : uint8_t { SW_INACTIVE, SW_READY, SW_RUNNING, SW_STOPPED };
static SwState       swState         = SW_INACTIVE;
static unsigned long swStartMs       = 0;
static uint32_t      swAccumMs       = 0;
static uint8_t       swLineIdx       = TIME_NO_LINE;
static unsigned long swLastUpdateMs  = 0;

static uint32_t swElapsed() {
  if (swState == SW_RUNNING) return swAccumMs + (uint32_t)(millis() - swStartMs);
  return swAccumMs;
}

static String buildSwLine() {
  const char* label = (swState == SW_READY)   ? "RDY" :
                      (swState == SW_RUNNING)  ? "RUN" : "STP";
  String line = "Stopwatch [" + String(label) + "]  " + formatStopwatchTime(swElapsed());

  // ── STOPWATCH STATE ICON BEGIN (remove this block if not desired) ──
  const char* icon = (swState == SW_READY)   ? "  [-]" :
                     (swState == SW_RUNNING)  ? "  [>]" : "  [|]";
  line += icon;
  // ── STOPWATCH STATE ICON END ──

  return line;
}

void stopwatchRun(const String& args) {
  swState    = SW_READY;
  swStartMs  = 0;
  swAccumMs  = 0;
  swLastUpdateMs = millis();

  beginScrollBatch();
  swLineIdx = printLineGetIndex(buildSwLine(), COLOR_DIM);
  termPrintln("Tap the screen to start / stop.", COLOR_DIM);
  endScrollBatch();
}

// Called from loop() before openCategoryMenu() in the UI_CLOSED tap handler.
// Returns true if the stopwatch consumed the tap.
bool timeConsumeStopwatchTap() {
  if (swState == SW_INACTIVE || swState == SW_STOPPED) return false;

  if (swState == SW_READY) {
    swState   = SW_RUNNING;
    swStartMs = millis();
    swLastUpdateMs = millis();
    if (swLineIdx != TIME_NO_LINE) {
      updateScrollLineAt(swLineIdx, buildSwLine(), colorAccent());
      redrawVisibleLine(swLineIdx);
    }
    playClick();
    return true;
  }

  if (swState == SW_RUNNING) {
    swAccumMs = swElapsed();
    swState   = SW_STOPPED;
    if (swLineIdx != TIME_NO_LINE) {
      updateScrollLineAt(swLineIdx, buildSwLine(), COLOR_DIM);
      redrawVisibleLine(swLineIdx);
    }
    playClick();
    return true;
  }
  return false;
}

// ── alarm command ─────────────────────────────────────────────────────────
void alarmRun(const String& args) {
  String a = args;
  a.trim();

  if (a.length() == 0) { listAlarms(); return; }

  if (a == "delete" || a.startsWith("delete ")) {
    String rest = a.length() > 7 ? a.substring(7) : "";
    rest.trim();
    if (rest.length() == 0) { termPrintln("Usage: alarm delete <number>", COLOR_DIM); return; }
    int n = rest.toInt();
    if (n < 1 || n > (int)alarmCount) {
      termPrintln("No alarm #" + String(n) + ". Run 'alarm' to see the list.", COLOR_DANGER);
      return;
    }
    for (uint8_t i = (uint8_t)(n - 1); i < alarmCount - 1; i++) alarms[i] = alarms[i + 1];
    alarmCount--;
    saveAlarms();
    termPrintln("Alarm #" + String(n) + " deleted.", COLOR_TEXT);
    return;
  }

  int sp = a.indexOf(' ');
  String timeStr = (sp == -1) ? a : a.substring(0, sp);
  String modeStr = (sp == -1) ? "" : a.substring(sp + 1);
  modeStr.trim();
  modeStr.toLowerCase();

  uint8_t h, m;
  if (!parseHMTime(timeStr, h, m)) {
    termPrintln("Usage: alarm HH:MM [repeat|once]  |  alarm  |  alarm delete N", COLOR_DIM);
    return;
  }

  if (alarmCount >= MAX_ALARMS) {
    termPrintln("Alarm limit (" + String(MAX_ALARMS) + ") reached. Delete one first.", COLOR_DANGER);
    return;
  }

  bool repeat = (modeStr == "repeat");
  alarms[alarmCount++] = { h, m, repeat };
  saveAlarms();

  char buf[44];
  snprintf(buf, sizeof(buf), "Alarm set: %02d:%02d  (%s)", (int)h, (int)m, repeat ? "repeat" : "once");
  termPrintln(String(buf), COLOR_TEXT);
}

// ── main time command ─────────────────────────────────────────────────────
void timeRun(const String& args) {
  String a = args;
  a.trim();

  if (a.length() == 0) {
    if (!ensureTimeSynced()) {
      termPrintln("Couldn't sync time -- check WiFi (\"wifi connect\").", COLOR_DANGER);
      return;
    }
    termPrintln(formatCurrentTime(), COLOR_TEXT);
    return;
  }

  // Convenience routing so "time alarm ..." / "time timer ..." / "time stopwatch" also work.
  if (a.startsWith("alarm")) {
    String sub = a.substring(5); sub.trim();
    alarmRun(sub);
    return;
  }
  if (a.startsWith("timer")) {
    String sub = a.substring(5); sub.trim();
    timerRun(sub);
    return;
  }
  if (a.startsWith("stopwatch")) { stopwatchRun(""); return; }

  termPrintln("Usage: time  |  alarm HH:MM [repeat|once]  |  timer MM:SS  |  stopwatch", COLOR_DIM);
}

// ── timeTick() — call from loop() ─────────────────────────────────────────
void timeTick() {
  unsigned long now = millis();

  // ── alarm check: once per minute, only when time is known ────────────
  if (time(nullptr) > 1700000000) {
    time_t t = time(nullptr);
    struct tm tm;
    localtime_r(&t, &tm);
    int curMin = tm.tm_hour * 60 + tm.tm_min;

    if (curMin != alarmLastCheckedMin) {
      alarmLastCheckedMin = curMin;
      for (uint8_t i = 0; i < alarmCount; ) {
        if (alarms[i].hour == (uint8_t)tm.tm_hour && alarms[i].minute == (uint8_t)tm.tm_min) {
          char buf[32];
          snprintf(buf, sizeof(buf), "** Alarm %02d:%02d **", (int)alarms[i].hour, (int)alarms[i].minute);
          alarmNotifyText    = String(buf);
          alarmNotifyLineIdx = printLineGetIndex(alarmNotifyText, colorAccent());
          alarmNotifyMs      = now;
          alarmLastFlipMs    = now;
          alarmNotifyFlip    = false;
          drawScrollback();
          if (CONFIG.beep) {
            M5.Speaker.tone(1200, 120); delay(180);
            M5.Speaker.tone(1200, 120);
          }
          if (!alarms[i].repeat) {
            for (uint8_t j = i; j < alarmCount - 1; j++) alarms[j] = alarms[j + 1];
            alarmCount--;
            saveAlarms();
          } else {
            i++;
          }
        } else {
          i++;
        }
      }
    }
  }

  // ── alarm notification flash: ~4s, accent/dim alternating every 500ms ─
  if (alarmNotifyLineIdx != TIME_NO_LINE) {
    if (now - alarmNotifyMs >= 4000) {
      alarmNotifyLineIdx = TIME_NO_LINE; // done -- line stays, just stops flashing
    } else if (now - alarmLastFlipMs >= 500) {
      alarmLastFlipMs = now;
      alarmNotifyFlip = !alarmNotifyFlip;
      uint16_t fc = alarmNotifyFlip ? colorAccent() : colorAccentDim();
      updateScrollLineAt(alarmNotifyLineIdx, alarmNotifyText, fc);
      redrawVisibleLine(alarmNotifyLineIdx);
    }
  }

  // ── timer countdown (1s cadence) ─────────────────────────────────────
  if (timerRunning && timerLineIdx != TIME_NO_LINE && now - timerLastUpdateMs >= 1000) {
    timerLastUpdateMs = now;
    uint32_t elapsed = (uint32_t)(now - timerStartMs);
    if (elapsed >= timerDurationMs) {
      timerRunning = false;
      timerDone    = true;
      updateScrollLineAt(timerLineIdx, buildTimerLine(0), colorAccent());
      redrawVisibleLine(timerLineIdx);
      termPrintln("Timer done.", colorAccent());
      if (CONFIG.beep) {
        M5.Speaker.tone(1400, 180); delay(240);
        M5.Speaker.tone(1600, 180);
      }
    } else {
      uint32_t remaining = timerDurationMs - elapsed;
      updateScrollLineAt(timerLineIdx, buildTimerLine(remaining), colorAccent());
      redrawVisibleLine(timerLineIdx);
    }
  }

  // ── stopwatch (100ms cadence while running) ───────────────────────────
  if (swState == SW_RUNNING && swLineIdx != TIME_NO_LINE && now - swLastUpdateMs >= 100) {
    swLastUpdateMs = now;
    updateScrollLineAt(swLineIdx, buildSwLine(), colorAccent());
    redrawVisibleLine(swLineIdx);
  }
}

// drawTimeEntryExtension() and handleTimeEntryExtensionTouch() are implemented
// in TouchMenu.h where armedCommand is in scope (it's a static local there).
// Declarations for both are in Globals.h so the rest of the codebase can call
// them; the EXT_TIME_ENTRY cases in TouchMenu.h dispatch to them directly.

#endif // UJI_M5_TIMEFEATURES_H
