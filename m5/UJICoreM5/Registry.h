/*
 * Registry.h — the command table. This is the touch-CLI equivalent of
 * UJICore.js's registerCommand(name, { run: (args) => ... }) calls: each
 * entry here is one line in that file's command list, just declared as
 * data instead of a function call.
 */
#ifndef UJI_M5_REGISTRY_H
#define UJI_M5_REGISTRY_H
#include "Globals.h"

void helpRun(const String& args) {
  String out;
  for (uint8_t i = 0; i < COMMAND_COUNT; i++) {
    out += String(COMMANDS[i].name) + " - " + COMMANDS[i].description + "\n";
  }
  termPrintln(out, COLOR_TEXT);
}

void aboutRun(const String& args) {
  String out;
  out += "UJI M5\n\n";
  out += "Touch port of UJI Core, a no-build-step\n";
  out += "admin console.\n\n";
  out += "Original: github.com/CatfishOfficial/UJIFrame\n";
  out += "Runs on M5Stack CoreS3 (M5Unified).\n";
  termPrintln(out, COLOR_TEXT);
}

void clearRun(const String& args) {
  termClear();
}

void historyRun(const String& args) {
  termPrintln(renderHistory(), COLOR_TEXT);
}

static void doPurgeAction() {
  runMatrixBusy(700);
  resetConfigToDefaults();
  clearHistory();
  termPrintln("Config and history reset to defaults.", COLOR_TEXT);
  drawBar();
}

void purgeRun(const String& args) {
  // Doesn't purge directly -- arms the confirm overlay. doPurgeAction()
  // only runs if the user taps Confirm.
  armConfirm("Erase saved config and history?", "This can't be undone.", doPurgeAction);
}

static void doPoweroffAction() {
  termPrintln("Powering off...", COLOR_TEXT);
  drawBar();
  delay(400); // let the message actually paint before power cuts
  M5.Power.powerOff();
}

void poweroffRun(const String& args) {
  // The side button on this Stack-chan build doesn't reliably power the
  // device off, so this is a software fallback through the same AXP2101
  // PMU path M5.Power talks to (independent of the physical button).
  armConfirm("Power off the device?", "Use the side button to power back on.", doPoweroffAction);
}

const CommandDef COMMANDS[] = {
  { "help",     "System", "List all commands",               EXT_NONE,    nullptr,         0,                    helpRun },
  { "about",    "System", "About this device",               EXT_NONE,    nullptr,         0,                    aboutRun },
  { "config",   "System", "View/set beep, brightness, etc.", EXT_CONFIG,  nullptr,         0,                    configRun },
  { "purge",    "System", "Erase saved config + history",    EXT_NONE,    nullptr,         0,                    purgeRun },
  { "poweroff", "System", "Power off the device",            EXT_NONE,    nullptr,         0,                    poweroffRun },
  { "clear",    "System", "Clear the screen",                EXT_NONE,    nullptr,         0,                    clearRun },
  { "wifi",     "System", "Connect/check WiFi",               EXT_CHOICES, WIFI_SUBACTIONS, WIFI_SUBACTION_COUNT, wifiRun },
  { "calc",     "Tools",  "Calculator",                       EXT_NUMERIC, nullptr,         0,                    calcRun },
  { "weather",  "Tools",  "Current weather (needs WiFi)",     EXT_NONE,    nullptr,         0,                    weatherRun },
  { "cowsay",   "Fun",    "A cow says something",             EXT_CHOICES, COWSAY_PRESETS,  COWSAY_PRESET_COUNT,  cowsayRun },
  { "uji",      "Fun",    "Show the UJI logo",                EXT_NONE,    nullptr,         0,                    ujiRun },
  { "matrix",   "Fun",    "Matrix rain",                       EXT_NONE,    nullptr,         0,                    matrixRun },
  { "face",     "Fun",    "Show a kaomoji face",              EXT_CHOICES, FACE_MOOD_NAMES, FACE_MOOD_COUNT,      faceRun },
  { "history",  "Info",   "Recently run commands",            EXT_NONE,    nullptr,         0,                    historyRun },
  { "time",     "Info",   "Current time (NTP, needs WiFi)",   EXT_NONE,    nullptr,         0,                    timeRun },
};
const uint8_t COMMAND_COUNT = sizeof(COMMANDS) / sizeof(COMMANDS[0]);

const char* const CATEGORIES[CATEGORY_COUNT] = { "System", "Tools", "Fun", "Info" };

uint8_t commandsInCategory(const char* category, const CommandDef** outPtrs, uint8_t maxOut) {
  uint8_t count = 0;
  for (uint8_t i = 0; i < COMMAND_COUNT && count < maxOut; i++) {
    if (strcmp(COMMANDS[i].category, category) == 0) outPtrs[count++] = &COMMANDS[i];
  }
  return count;
}

const CommandDef* findCommand(const String& name) {
  for (uint8_t i = 0; i < COMMAND_COUNT; i++) {
    if (name.equalsIgnoreCase(COMMANDS[i].name)) return &COMMANDS[i];
  }
  return nullptr;
}

#endif // UJI_M5_REGISTRY_H
