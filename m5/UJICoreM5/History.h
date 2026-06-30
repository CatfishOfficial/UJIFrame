/*
 * History.h — ring buffer of recently executed full command lines
 * (e.g. "calc 2^10", "config beep off"), not just command names -- this
 * is now a real shell-style history since lines carry real arguments.
 * RAM-only, resets on reboot.
 */
#ifndef UJI_M5_HISTORY_H
#define UJI_M5_HISTORY_H
#include "Globals.h"

struct HistoryEntry {
  String line;
  unsigned long atMillis;
};

static HistoryEntry historyBuf[HISTORY_CAPACITY];
static uint8_t historyCount = 0;
static uint8_t historyNext = 0;

void pushHistory(const String& line) {
  HistoryEntry& e = historyBuf[historyNext];
  e.line = line;
  e.atMillis = millis();
  historyNext = (historyNext + 1) % HISTORY_CAPACITY;
  if (historyCount < HISTORY_CAPACITY) historyCount++;
}

void clearHistory() {
  historyCount = 0;
  historyNext = 0;
}

static String formatElapsed(unsigned long ms) {
  unsigned long s = ms / 1000;
  unsigned long mm = (s / 60) % 60;
  unsigned long hh = s / 3600;
  unsigned long ss = s % 60;
  char buf[12];
  snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu", hh, mm, ss);
  return String(buf);
}

String renderHistory() {
  if (historyCount == 0) return String("No commands run yet.");

  String out;
  uint8_t oldestIdx = (historyCount < HISTORY_CAPACITY) ? 0 : historyNext;
  for (uint8_t i = 0; i < historyCount; i++) {
    uint8_t idx = (oldestIdx + i) % HISTORY_CAPACITY;
    HistoryEntry& e = historyBuf[idx];
    out += String(i + 1) + ". " + e.line;
    if (CONFIG.timestamps) out += "  +" + formatElapsed(e.atMillis);
    out += "\n";
  }
  return out;
}

#endif // UJI_M5_HISTORY_H
