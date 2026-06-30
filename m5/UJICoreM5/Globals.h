/*
 * Globals.h — shared types, constants and cross-file declarations.
 *
 * v2: this is a real terminal — a scrolling output buffer plus a single
 * persistent command bar at the bottom. There's no keyboard yet, so the
 * touch UI (TouchMenu.h) is just a soft-keyboard: it types command names
 * and arguments into the bar the same way a keyboard would, then the
 * bar's full text is parsed and dispatched (Parser.h) exactly like
 * UJICore.js's registerCommand(name, { run: (args) => ... }) — args is
 * the raw remainder of the line after the command name, not a split argv.
 *
 * The .ino concatenates with these headers into one translation unit
 * (each header is #include'd exactly once from UJICoreM5.ino), so plain
 * function definitions are safe — no multiple-definition concerns.
 */
#ifndef UJI_M5_GLOBALS_H
#define UJI_M5_GLOBALS_H
#include <M5CoreS3.h>

// ── screen size (queried once in setup(), used everywhere for layout) ────
extern int16_t SCR_W;
extern int16_t SCR_H;

// ── app config (declared early -- the theme helpers just below read
// CONFIG.color, the embedded analogue of UJICore.js's localStorage CONFIG) ──
struct AppConfig {
  bool beep;
  uint8_t brightness;    // 0 = low, 1 = med, 2 = high
  bool timestamps;
  uint8_t color;         // index into THEMES[] -- 0=green,1=amber,2=blue,3=purple
  uint8_t glitterSpeed;  // 0 = slow, 1 = normal, 2 = fast (matrix rain fall speed)
  bool wifiAutoConnect;  // connect using WifiSecrets.h on boot
  uint8_t timezone;      // index into TIMEZONE_NAMES[]/TIMEZONE_TZ_STRINGS[] -- 0=UTC,1=Eastern,2=Central,3=Mountain,4=Pacific
  String lastWifiSsid;   // SSID of the most recent successful connect -- tried first next time (WifiCommand.h)
};
extern AppConfig CONFIG;

// One entry in WifiSecrets.h's WIFI_NETWORKS[] -- the device tries each in
// turn (last-connected first) until one associates. See WifiCommand.h.
struct WifiNetwork { const char* ssid; const char* password; };

// POSIX TZ strings (with US DST rules baked in) parallel to TIMEZONE_NAMES.
#define TIMEZONE_COUNT 5
extern const char* const TIMEZONE_NAMES[TIMEZONE_COUNT];
extern const char* const TIMEZONE_TZ_STRINGS[TIMEZONE_COUNT];

// ── theme palette, ported from UJICore.js's THEME_PALETTE (green theme
// was already what UJI M5 used; amber/blue/purple are the others) ─────
struct ThemeColors {
  uint8_t accent[3];
  uint8_t lead[3];
  uint8_t trail[3];
};
#define THEME_COUNT 4
extern const ThemeColors THEMES[THEME_COUNT];
extern const char* const THEME_NAMES[THEME_COUNT];

// ── colors ────────────────────────────────────────────────────────────
// NB: the builtin GLCD font only covers ASCII 0x20-0x7E, so labels and
// dividers stick to plain ASCII (hyphens, "X", "OK") rather than the
// unicode box-drawing/arrow glyphs the browser version used.
#define COLOR_BG     TFT_BLACK
#define COLOR_TEXT   TFT_WHITE
#define COLOR_DIM    0x4208 /* dark gray, RGB565 */
#define COLOR_DANGER TFT_RED
inline uint16_t colorAccent() {
  const ThemeColors& t = THEMES[CONFIG.color];
  return M5.Display.color565(t.accent[0], t.accent[1], t.accent[2]);
}
inline uint16_t colorAccentDim() {
  const ThemeColors& t = THEMES[CONFIG.color];
  return M5.Display.color565(t.accent[0] / 4, t.accent[1] / 4, t.accent[2] / 4);
}
inline void themeLeadRGB(uint8_t& r, uint8_t& g, uint8_t& b) {
  const ThemeColors& t = THEMES[CONFIG.color];
  r = t.lead[0]; g = t.lead[1]; b = t.lead[2];
}
inline void themeTrailRGB(uint8_t& r, uint8_t& g, uint8_t& b) {
  const ThemeColors& t = THEMES[CONFIG.color];
  r = t.trail[0]; g = t.trail[1]; b = t.trail[2];
}

// UJICore.js's matrix rain sits on a canvas layered over the terminal at
// CSS opacity:0.78 (createCanvas(10, 0.78)) -- i.e. a black layer over
// the terminal, not a gray one. This is the same math (multiplicative
// dim toward black, not a blend toward neutral gray) applied to a
// *logical* RGB565 color value (one already produced by color565()/a
// TFT_* macro -- safe, documented format). Deliberately does NOT touch
// a sprite's raw getBuffer() bytes: those can be stored in a different
// internal byte order than the RGB565 values passed to color565(),
// which is exactly what turned this purple the first time around.
inline uint16_t dimColorTowardBlack(uint16_t color565value, uint8_t opacityPercent) {
  uint8_t r5 = (color565value >> 11) & 0x1F;
  uint8_t g6 = (color565value >> 5) & 0x3F;
  uint8_t b5 = color565value & 0x1F;
  uint16_t r8 = (r5 * 255) / 31;
  uint16_t g8 = (g6 * 255) / 63;
  uint16_t b8 = (b5 * 255) / 31;
  uint8_t nr = (uint8_t)(r8 * (100 - opacityPercent) / 100);
  uint8_t ng = (uint8_t)(g8 * (100 - opacityPercent) / 100);
  uint8_t nb = (uint8_t)(b8 * (100 - opacityPercent) / 100);
  return M5.Display.color565(nr, ng, nb);
}

// ── generic touch button (used by the bar's tiles and every overlay) ────
struct Button {
  int16_t x, y, w, h;
  const char* label;
  uint8_t id;
};

void drawButton(const Button& b, bool danger = false);
int hitTestButtons(const Button* buttons, int count, int16_t x, int16_t y);
void playClick();
void renderAll(); // full redraw: scrollback + bar + (if open) the overlay

// ── command registry ──────────────────────────────────────────────────
enum ExtType : uint8_t { EXT_NONE, EXT_NUMERIC, EXT_CHOICES, EXT_CONFIG, EXT_TIME_ENTRY };

struct CommandDef {
  const char* name;
  const char* category;
  const char* description;
  ExtType ext;
  const char* const* choices; // for EXT_CHOICES only; nullptr otherwise
  uint8_t choiceCount;        // for EXT_CHOICES only; 0 otherwise
  void (*run)(const String& args);
};

#define CATEGORY_COUNT 4
extern const char* const CATEGORIES[CATEGORY_COUNT];
extern const CommandDef COMMANDS[];
extern const uint8_t COMMAND_COUNT;
uint8_t commandsInCategory(const char* category, const CommandDef** outPtrs, uint8_t maxOut);
const CommandDef* findCommand(const String& name);

// ── config (NVS-backed, the embedded analogue of localStorage) ───────
// AppConfig/CONFIG are declared up top, above the theme color helpers.
struct ConfigKeyDef {
  const char* key;
  const char* const* values;
  uint8_t valueCount;
};
#define CONFIG_KEY_COUNT 7
extern const ConfigKeyDef CONFIG_KEYS[CONFIG_KEY_COUNT];

void loadConfig();
void saveConfig();
void resetConfigToDefaults();
void applyBrightness();
const char* currentConfigValue(uint8_t keyIndex); // for rendering the picker / "config" with no args
void configRun(const String& args);

// ── terminal: scrollback + persistent bottom bar ─────────────────────
#define BAR_H 28 // height of the persistent bottom bar; everything above it is scrollback
void termInit();
// scale > 1 prints this line in a bigger builtin-font size (used by the
// "face" command so kaomoji can stand out from normal output) -- the
// scrollback's row height/scroll math accounts for mixed line heights.
void termPrintln(const String& text, uint16_t color = COLOR_TEXT, float scale = 1);
int16_t maxCharsForScale(float scale = 1); // how many builtin-font chars fit one row at this text size
// For content that refreshes in place (the idle-mode widget's clock/
// weather/face lines) instead of always appending a new line.
uint8_t printLineGetIndex(const String& text, uint16_t color, float scale = 1);
void updateScrollLineAt(uint8_t idx, const String& text, uint16_t color, float scale = 1);
void redrawVisibleLine(uint8_t idx); // repaints just that one row, not the whole scrollback
extern const char* const TERM_DIVIDER; // a "----" line sized to match termPrintln's wrap width
// For ephemeral content (the idle-mode widget) that should vanish on
// wake instead of becoming permanent scrollback history.
void termGetMark(uint8_t& count, uint8_t& next);
void termRewindToMark(uint8_t count, uint8_t next);
void termEcho(const String& line);
void termClear();
void captureTerminalSnapshot(M5Canvas& dst, uint8_t opacityPercent = 0); // redraw current screen into an offscreen sprite (not a pixel readback); opacityPercent dims every color used toward black, like a black overlay at that CSS opacity
void beginScrollBatch(); // suppress termPrintln's per-call redraw...
void endScrollBatch();   // ...until this, which redraws once for the batch
void drawScrollback();
void handleTerminalDrag(int16_t deltaY); // drag-to-scroll the scrollback; uiMode == UI_CLOSED only
void resetTerminalDrag();
void drawBar();
const Button* barEnterButton();
const Button* barBackspaceButton();
void barAppendText(const String& s);
void barBackspace();
void barClearText();
extern String composedLine;

// ── parser / dispatch ─────────────────────────────────────────────────
void dispatchLine(const String& rawLine);

// ── touch-menu overlay (category -> commands -> extension picker) ────
enum UiMode : uint8_t { UI_CLOSED, UI_CATEGORY, UI_COMMANDS, UI_EXTENSION, UI_CONFIRM, UI_IDLE_POWER_PROMPT };
extern UiMode uiMode;

void openCategoryMenu();
void closeMenu();
void drawOverlay();             // dispatches on uiMode to the right drawer below
void touchMenuHandleTouch(int16_t x, int16_t y); // for CATEGORY/COMMANDS/EXTENSION

// Generic Confirm/Cancel prompt for destructive commands (purge, poweroff).
// Confirm calls onConfirm(); Cancel just prints "Cancelled." and closes.
void armConfirm(const char* line1, const char* line2, void (*onConfirm)());
void drawConfirmOverlay();
void handleConfirmTouch(int16_t x, int16_t y);
void purgeRun(const String& args);
void poweroffRun(const String& args);

// ── history ring buffer (full executed lines, not just labels) ───────
#define HISTORY_CAPACITY 12
void pushHistory(const String& line);
void clearHistory();
String renderHistory();

// ── calc ───────────────────────────────────────────────────────────────
String evalMathExprToString(const String& expr);
void calcRun(const String& args); // also handles "calc solve <eqn>[, <eqn2>]"
void drawCalcExtension(int16_t x, int16_t y, int16_t w, int16_t h);
void handleCalcExtensionTouch(int16_t x, int16_t y);
void calcResetKeypadPage(); // back to the arithmetic keypad page; call when (re)arming the calc extension

// ── cowsay ─────────────────────────────────────────────────────────────
void cowsayRun(const String& args);
#define COWSAY_PRESET_COUNT 5
extern const char* const COWSAY_PRESETS[COWSAY_PRESET_COUNT];

// ── boot splash / "uji" command ───────────────────────────────────────
void showBootLogo();
void drainTapWithCooldown(); // wait for finger-up + a short cooldown after a wake-up tap
void waitTapOrTimeout(uint32_t ms);
void ujiRun(const String& args);

// ── matrix effect ──────────────────────────────────────────────────────
void matrixRun(const String& args);       // standalone Fun command
void runMatrixBusy(uint32_t ms);          // brief flourish, e.g. during purge
// Shared full-screen sprite pair, reused by NightAnimations.h's steppers
// too -- whatever's drawn into matrixBgCanvas before a *Step() call is
// "what the foreground effect renders over."
extern M5Canvas matrixCanvas;
extern M5Canvas matrixBgCanvas;
void matrixEnsureCanvas();   // sizes matrixCanvas (the foreground/output sprite) once
void matrixEnsureBgCanvas(); // sizes matrixBgCanvas (the background sprite) once

// ── wifi (credentials compiled in via WifiSecrets.h, never typed on-device) ──
#define WIFI_SUBACTION_COUNT 4
extern const char* const WIFI_SUBACTIONS[WIFI_SUBACTION_COUNT]; // status/connect/disconnect/scan
void wifiRun(const String& args);
void wifiAutoConnectIfEnabled(); // called once from setup(); non-blocking (kicks off background failover)
void wifiTick();                 // called every loop(); advances the non-blocking boot failover

// ── http (shared by any command that calls an API) ───────────────────
// Returns the response body, or "" on failure with errorOut set to a
// short human-readable reason (timeout, no connection, HTTP <code>, ...).
String httpGet(const String& url, String& errorOut);

// ── clock (NTP time over WiFi; Http.h also uses ensureTimeSynced() for
// TLS certificate date checks) ────────────────────────────────────────
bool ensureTimeSynced();
void applyTimezone(); // sets the TZ env var from CONFIG.timezone so localtime_r() converts correctly (incl. DST)
String formatIdleDate(); // short formats for the idle-mode widget, e.g. "Wed Jun 28" / "2:43 PM"
String formatIdleTime();
int currentLocalHour(); // 0-23; caller must already know time is synced

// ── time features (alarms, timer, stopwatch — TimeFeatures.h) ────────────
void timeInit();          // load persisted alarms; call once from setup()
void timeRun(const String& args);
void alarmRun(const String& args);
void timerRun(const String& args);
void stopwatchRun(const String& args);
void timeTick();          // call every loop() iteration
bool timeConsumeStopwatchTap(); // true = stopwatch consumed a scrollback tap (caller skips openCategoryMenu)
void drawTimeEntryExtension();
void handleTimeEntryExtensionTouch(int16_t x, int16_t y);

// ── weather (Tools; wttr.in, free, no API key, geolocates by request IP) ──
void weatherRun(const String& args);
String fetchWeatherShort(String& errorOut); // terser one-line format for the idle-mode widget

// ── faces (Stack-chan ASCII kaomoji; see Faces.h for why these are
// plain-ASCII style rather than unicode kaomoji) ──────────────────────
enum FaceMood : uint8_t {
  FACE_NEUTRAL, FACE_HAPPY, FACE_EXCITED, FACE_SAD, FACE_FRUSTRATED,
  FACE_SURPRISED, FACE_SLEEPY, FACE_THINKING, FACE_EMBARRASSED,
  FACE_APOLOGETIC, FACE_PLAYFUL,
};
#define FACE_MOOD_COUNT 11
extern const char* const FACE_MOOD_NAMES[FACE_MOOD_COUNT]; // for the touch picker
String randomFaceString(FaceMood mood); // the building block other call sites should use later
void faceRun(const String& args);

// ── idle mode (pastes a status block into the terminal during
// inactivity, refreshing it in place; see Idle.h) ─────────────────────
// Text sizes shared by day-idle's scrollback lines (Idle.h) and night-
// idle's sprite dashboard (NightAnimations.h), so both stay visually
// consistent without duplicating the constants.
#define IDLE_DATE_SCALE    2
#define IDLE_TIME_SCALE    3
#define IDLE_WEATHER_SCALE 2
#define IDLE_WEATHER_REFRESH_MS 180000UL // 3 min (day-idle and NightAnimations.h both use this)

void idleTick();             // call once per loop() iteration
void idleNotifyInteraction(bool isTap); // call whenever a real tap/drag is processed; isTap = a discrete click/release (vs a drag frame)
bool idleConsumeWakeTap();    // true exactly once per wake -- caller should skip normal tap dispatch that time
// Day-idle-only "still there?" prompt after a longer stretch of inactivity --
// its own UiMode (UI_IDLE_POWER_PROMPT) since it needs a timeout/escalation
// idleTick() drives itself, not just a Confirm/Cancel tap.
void drawIdlePowerPromptOverlay();
void handleIdlePowerPromptTouch(int16_t x, int16_t y);

// ── night-idle dashboard + rotating ambient animations (NightAnimations.h) ──
void drawNightIdleBackground(M5Canvas& dst); // date/time/weather/battery/IMU-temp, opaque, drawn into a sprite (not the scrollback)
void drawNightIdleTextOverlay(M5Canvas& dst); // same content, transparent background -- for nightMatrixStep()'s reversed layering
void nightMatrixStep(); // rain behind, dimmed; dashboard text in front -- the opposite layering from MatrixEffect.h's matrixStep()
void starfieldStep();
void firefliesStep();
String formatBatteryStatus(); // "Battery: 82%" / "Battery: 82% (charging)" -- shared by day-idle (Idle.h) and night-idle's dashboard

#endif // UJI_M5_GLOBALS_H
