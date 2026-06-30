/*
 * Config.h — persisted settings, stored in NVS via Preferences (the
 * embedded analogue of UJI Core's localStorage-backed CONFIG). The
 * command-facing surface is "config <key> <value>", matching UJICore.js's
 * real `config` command syntax.
 *
 * Keys ported from UJICore.js's CONFIG_SCHEMA where they actually apply
 * here: timestamps (already had it), color (the theme picker -- green/
 * amber/blue/purple, skipping "lightmode" since that's a full light-theme
 * recolor, not a quick port), glitterSpeed (matrix rain fall speed).
 * Not ported: glitter/chainOverlay/maxLoop -- those gate behavior
 * (loading-overlay intensity levels, "|" chaining, the loop command)
 * that doesn't exist in UJI M5 yet. glitterRainbow is a fun one but
 * needs HSV->RGB conversion; flagged, not done here.
 *
 * `autoconnect` (on/off) and `timezone` (UTC/Eastern/Central/Mountain/Pacific)
 * have no UJICore.js equivalent -- both are UJI M5-specific. `autoconnect`
 * toggles whether wifiAutoConnectIfEnabled() (WifiCommand.h) tries
 * WifiSecrets.h's credentials on boot; `timezone` picks which POSIX TZ
 * string applyTimezone() (Clock.h) applies for local-time display.
 * (Named "autoconnect" rather than "wifi" so it doesn't read as "is the
 * WiFi radio on" -- this only ever controls auto-connect-at-boot.)
 */
#ifndef UJI_M5_CONFIG_H
#define UJI_M5_CONFIG_H
#include "Globals.h"
#include <Preferences.h>

static const char* CONFIG_NAMESPACE = "uji";

// THEME_PALETTE from UJICore.js, minus "lightmode".
const ThemeColors THEMES[THEME_COUNT] = {
  { { 76, 175, 80 },   { 200, 255, 200 }, { 0, 187, 0 } },     // green  (#4CAF50 / #c8ffc8 / #00bb00)
  { { 255, 179, 0 },   { 255, 233, 179 }, { 204, 139, 0 } },   // amber  (#FFB300 / #ffe9b3 / #cc8b00)
  { { 100, 181, 246 }, { 207, 232, 255 }, { 47, 127, 209 } },  // blue   (#64B5F6 / #cfe8ff / #2f7fd1)
  { { 186, 104, 200 }, { 236, 205, 242 }, { 142, 63, 158 } },  // purple (#BA68C8 / #eccdf2 / #8e3f9e)
};
const char* const THEME_NAMES[THEME_COUNT] = { "green", "amber", "blue", "purple" };

// POSIX TZ strings with the US DST rule (2nd Sun in March -> 1st Sun in
// Nov) baked in, so the displayed time is correct year-round without
// manually flipping anything twice a year.
const char* const TIMEZONE_NAMES[TIMEZONE_COUNT]     = { "UTC", "Eastern", "Central", "Mountain", "Pacific" };
const char* const TIMEZONE_TZ_STRINGS[TIMEZONE_COUNT] = {
  "UTC0",
  "EST5EDT,M3.2.0,M11.1.0",
  "CST6CDT,M3.2.0,M11.1.0",
  "MST7MDT,M3.2.0,M11.1.0",
  "PST8PDT,M3.2.0,M11.1.0",
};

void loadConfig() {
  Preferences prefs;
  prefs.begin(CONFIG_NAMESPACE, true); // read-only
  CONFIG.beep = prefs.getBool("beep", true);
  CONFIG.brightness = prefs.getUChar("bright", 1);
  CONFIG.timestamps = prefs.getBool("stamp", true);
  CONFIG.color = prefs.getUChar("color", 0);
  CONFIG.glitterSpeed = prefs.getUChar("gspeed", 1);
  CONFIG.wifiAutoConnect = prefs.getBool("wifiauto", false);
  CONFIG.timezone = prefs.getUChar("tz", 0);
  CONFIG.lastWifiSsid = prefs.getString("lastssid", "");
  prefs.end();
  if (CONFIG.brightness > 2) CONFIG.brightness = 1;
  if (CONFIG.color >= THEME_COUNT) CONFIG.color = 0;
  if (CONFIG.glitterSpeed > 2) CONFIG.glitterSpeed = 1;
  if (CONFIG.timezone >= TIMEZONE_COUNT) CONFIG.timezone = 0;
  applyTimezone();
}

void saveConfig() {
  Preferences prefs;
  prefs.begin(CONFIG_NAMESPACE, false);
  prefs.putBool("beep", CONFIG.beep);
  prefs.putUChar("bright", CONFIG.brightness);
  prefs.putBool("stamp", CONFIG.timestamps);
  prefs.putUChar("color", CONFIG.color);
  prefs.putUChar("gspeed", CONFIG.glitterSpeed);
  prefs.putBool("wifiauto", CONFIG.wifiAutoConnect);
  prefs.putUChar("tz", CONFIG.timezone);
  prefs.putString("lastssid", CONFIG.lastWifiSsid);
  prefs.end();
}

void resetConfigToDefaults() {
  Preferences prefs;
  prefs.begin(CONFIG_NAMESPACE, false);
  prefs.clear();
  prefs.end();
  CONFIG.beep = true;
  CONFIG.brightness = 1;
  CONFIG.timestamps = true;
  CONFIG.color = 0;
  CONFIG.glitterSpeed = 1;
  CONFIG.wifiAutoConnect = false;
  CONFIG.timezone = 0;
  CONFIG.lastWifiSsid = "";
  applyBrightness();
  applyTimezone();
}

void applyBrightness() {
  static const uint8_t LEVELS[3] = { 60, 160, 255 };
  M5.Display.setBrightness(LEVELS[CONFIG.brightness]);
}

// ── key/value schema, shared by configRun() and TouchMenu's EXT_CONFIG picker ──
static const char* BEEP_VALUES[]   = { "on", "off" };
static const char* BRIGHT_VALUES[] = { "low", "med", "high" };   // index doubles as CONFIG.brightness
static const char* STAMP_VALUES[]  = { "on", "off" };
static const char* SPEED_VALUES[]  = { "slow", "normal", "fast" }; // index doubles as CONFIG.glitterSpeed
static const char* WIFI_AUTO_VALUES[] = { "on", "off" };

const ConfigKeyDef CONFIG_KEYS[CONFIG_KEY_COUNT] = {
  { "beep",         BEEP_VALUES,      2 },
  { "brightness",   BRIGHT_VALUES,    3 },
  { "timestamps",   STAMP_VALUES,     2 },
  { "color",        THEME_NAMES,      THEME_COUNT },
  { "glitterSpeed", SPEED_VALUES,     3 },
  { "autoconnect",  WIFI_AUTO_VALUES, 2 },
  { "timezone",     TIMEZONE_NAMES,   TIMEZONE_COUNT },
};

const char* currentConfigValue(uint8_t keyIndex) {
  switch (keyIndex) {
    case 0: return CONFIG.beep ? "on" : "off";
    case 1: return BRIGHT_VALUES[CONFIG.brightness];
    case 2: return CONFIG.timestamps ? "on" : "off";
    case 3: return THEME_NAMES[CONFIG.color];
    case 4: return SPEED_VALUES[CONFIG.glitterSpeed];
    case 5: return CONFIG.wifiAutoConnect ? "on" : "off";
    case 6: return TIMEZONE_NAMES[CONFIG.timezone];
    default: return "";
  }
}

void configRun(const String& args) {
  if (args.length() == 0) {
    String out;
    for (uint8_t i = 0; i < CONFIG_KEY_COUNT; i++) {
      out += String(CONFIG_KEYS[i].key) + " = " + currentConfigValue(i) + "\n";
    }
    termPrintln(out, COLOR_TEXT);
    return;
  }

  int sp = args.indexOf(' ');
  if (sp == -1) {
    termPrintln("Usage: config <key> <value>", COLOR_DIM);
    return;
  }
  String key = args.substring(0, sp);
  String value = args.substring(sp + 1);
  key.trim();
  value.trim();
  String keyLower = key;
  keyLower.toLowerCase();

  for (uint8_t i = 0; i < CONFIG_KEY_COUNT; i++) {
    if (!key.equalsIgnoreCase(CONFIG_KEYS[i].key)) continue;
    for (uint8_t v = 0; v < CONFIG_KEYS[i].valueCount; v++) {
      if (!value.equalsIgnoreCase(CONFIG_KEYS[i].values[v])) continue;
      if (keyLower == "beep") CONFIG.beep = (v == 0);
      else if (keyLower == "brightness") { CONFIG.brightness = v; applyBrightness(); }
      else if (keyLower == "timestamps") CONFIG.timestamps = (v == 0);
      else if (keyLower == "color") CONFIG.color = v;
      else if (keyLower == "glitterspeed") CONFIG.glitterSpeed = v;
      else if (keyLower == "autoconnect") CONFIG.wifiAutoConnect = (v == 0);
      else if (keyLower == "timezone") { CONFIG.timezone = v; applyTimezone(); }
      saveConfig();
      termPrintln(key + " set to " + value, COLOR_TEXT);
      return;
    }
    termPrintln("Invalid value for " + key, COLOR_DANGER);
    return;
  }
  termPrintln("Unknown config key: " + key, COLOR_DANGER);
}

#endif // UJI_M5_CONFIG_H
