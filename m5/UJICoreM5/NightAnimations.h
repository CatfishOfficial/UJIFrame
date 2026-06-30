/*
 * NightAnimations.h — night-idle's dashboard (date/time/weather/battery/
 * IMU temp) plus three rotating ambient animations to look at while
 * falling asleep: dimmed matrix rain, a twinkling starfield, and
 * drifting fireflies. (A breathing-circle animation was tried and
 * dropped -- didn't look good enough to keep.)
 *
 * starfieldStep()/firefliesStep() follow matrixStep()'s shape from
 * MatrixEffect.h: push matrixBgCanvas (the dashboard, drawn by
 * drawNightIdleBackground() below) onto matrixCanvas, draw one frame of
 * the animation on top, push matrixCanvas to the display -- same sprite
 * pair MatrixEffect.h exposes, no extra full-screen sprites allocated.
 *
 * nightMatrixStep() is deliberately the *opposite* layering from the
 * standalone "matrix" command (MatrixEffect.h): there, rain sits in
 * front of a dimmed terminal snapshot. Here the dashboard text needs to
 * stay legible in front, so the rain is dimmed (same
 * MATRIX_BG_OPACITY_PERCENT treatment the terminal gets in the
 * standalone command) and drawn as the *back* layer, with the dashboard
 * text drawn on top via drawNightIdleTextOverlay() using a transparent
 * background so only its own glyph strokes cover the rain -- layers,
 * bottom to top: rain | dimmed-toward-black | text.
 *
 * Verified onboard hardware (read from the installed M5Unified/M5CoreS3
 * source, not assumed) before adding sensor readouts:
 *  - Battery: M5.Power.getBatteryLevel() (0-100%) / M5.Power.isCharging()
 *    (m5::Power_Class::is_charging_t -- AXP2101 PMU on CoreS3, the same
 *    chip M5.Power.powerOff() already talks to in this project).
 *  - IMU temperature: M5.Imu.getTemp(float*) -- BMI270, returns Celsius
 *    (confirmed against BMI270_Class.cpp's raw/512+23 conversion, which
 *    matches Bosch's official formula). This is the IMU chip's own die
 *    temperature, not room temperature -- it reads a few degrees warm
 *    since it's right next to the SoC. M5.Imu auto-initializes inside
 *    M5Unified's begin() already; no setup() changes needed.
 */
#ifndef UJI_M5_NIGHTANIMATIONS_H
#define UJI_M5_NIGHTANIMATIONS_H
#include "Globals.h"
#include <math.h>

#define NIGHT_PI 3.14159265f

// ── dashboard content ───────────────────────────────────────────────────
// Weather is cached and only re-fetched on IDLE_WEATHER_REFRESH_MS
// (Globals.h, shared with day-idle) -- it's a network call. Date/time/
// battery/IMU-temp are cheap (local or a quick I2C read) and just
// recomputed whenever the dashboard redraws.
static String nightWeatherCache = "";
static unsigned long nightWeatherCacheMs = 0;

static void nightRefreshWeatherIfDue() {
  unsigned long now = millis();
  if (nightWeatherCache.length() && now - nightWeatherCacheMs < IDLE_WEATHER_REFRESH_MS) return;
  String werr;
  String w = fetchWeatherShort(werr);
  nightWeatherCache = w.length() ? w : "(weather unavailable)";
  nightWeatherCacheMs = now;
}

// Not night-specific any more -- day-idle (Idle.h) shows this too.
String formatBatteryStatus() {
  int32_t level = M5.Power.getBatteryLevel(); // 0-100, or -1 if unknown
  if (level < 0) return "Battery: --";
  bool charging = (M5.Power.isCharging() == m5::Power_Class::is_charging);
  String s = "Battery: " + String(level) + "%";
  if (charging) s += " (charging)";
  return s;
}

static String formatNightImuTemp() {
  float c;
  if (!M5.Imu.getTemp(&c)) return "Device: --";
  float f = c * 9.0f / 5.0f + 32.0f;
  return "Device: " + String((int)lroundf(f)) + "F";
}

// Draws the whole dashboard fresh into dst (matrixBgCanvas in practice) --
// called on its own ~1s throttle from Idle.h, independent of the much
// faster animation-frame throttle, since none of this content changes
// fast enough to need redrawing every frame.
void drawNightIdleBackground(M5Canvas& dst) {
  nightRefreshWeatherIfDue();
  bool timeKnown = time(nullptr) > 1700000000;

  dst.fillRect(0, 0, SCR_W, SCR_H - BAR_H, COLOR_BG);
  dst.setTextDatum(top_left);

  int16_t y = 10;
  dst.setTextSize(IDLE_DATE_SCALE);
  dst.setTextColor(COLOR_DIM, COLOR_BG);
  dst.setCursor(8, y);
  dst.print(timeKnown ? formatIdleDate() : "(clock not synced)");
  y += (int16_t)lroundf(LINE_H * IDLE_DATE_SCALE) + 4;

  if (timeKnown) {
    dst.setTextSize(IDLE_TIME_SCALE);
    dst.setTextColor(colorAccent(), COLOR_BG);
    dst.setCursor(8, y);
    dst.print(formatIdleTime());
    y += (int16_t)lroundf(LINE_H * IDLE_TIME_SCALE) + 6;
  }

  dst.setTextSize(IDLE_WEATHER_SCALE);
  dst.setTextColor(COLOR_TEXT, COLOR_BG);
  dst.setCursor(8, y);
  dst.print(nightWeatherCache);
  y += (int16_t)lroundf(LINE_H * IDLE_WEATHER_SCALE) + 8;

  dst.setTextSize(1);
  dst.setTextColor(COLOR_DIM, COLOR_BG);
  dst.setCursor(8, y);
  dst.print(formatBatteryStatus());
  y += LINE_H + 2;

  dst.setCursor(8, y);
  dst.print(formatNightImuTemp());
}

// Same content as drawNightIdleBackground(), but transparent-background
// (single-arg setTextColor) instead of opaque, and no fillRect first --
// for nightMatrixStep()'s reversed layering, drawn directly onto
// matrixCanvas on top of the already-dimmed rain so only the glyph
// strokes themselves cover it.
void drawNightIdleTextOverlay(M5Canvas& dst) {
  nightRefreshWeatherIfDue();
  bool timeKnown = time(nullptr) > 1700000000;
  dst.setTextDatum(top_left);

  int16_t y = 10;
  dst.setTextSize(IDLE_DATE_SCALE);
  dst.setTextColor(COLOR_DIM);
  dst.setCursor(8, y);
  dst.print(timeKnown ? formatIdleDate() : "(clock not synced)");
  y += (int16_t)lroundf(LINE_H * IDLE_DATE_SCALE) + 4;

  if (timeKnown) {
    dst.setTextSize(IDLE_TIME_SCALE);
    dst.setTextColor(colorAccent());
    dst.setCursor(8, y);
    dst.print(formatIdleTime());
    y += (int16_t)lroundf(LINE_H * IDLE_TIME_SCALE) + 6;
  }

  dst.setTextSize(IDLE_WEATHER_SCALE);
  dst.setTextColor(COLOR_TEXT);
  dst.setCursor(8, y);
  dst.print(nightWeatherCache);
  y += (int16_t)lroundf(LINE_H * IDLE_WEATHER_SCALE) + 8;

  dst.setTextSize(1);
  dst.setTextColor(COLOR_DIM);
  dst.setCursor(8, y);
  dst.print(formatBatteryStatus());
  y += LINE_H + 2;

  dst.setCursor(8, y);
  dst.print(formatNightImuTemp());
}

// ── matrix rain, reversed layering (see file header) ─────────────────────
// Reuses MatrixEffect.h's matrixCols[]/matrixColCount state (Idle.h calls
// the existing matrixReset() when it's rain's turn) and matrixGlyphColor()
// -- only the compositing order and dimming are different from matrixStep().
void nightMatrixStep() {
  matrixEnsureCanvas();
  int16_t areaH = SCR_H - BAR_H;
  matrixCanvas.fillRect(0, 0, SCR_W, areaH, COLOR_BG);
  matrixCanvas.setTextDatum(top_left);
  matrixCanvas.setTextSize(2);

  for (uint8_t i = 0; i < matrixColCount; i++) {
    MatrixColumn& col = matrixCols[i];
    int16_t x = i * MATRIX_COL_W;
    for (uint8_t t = 0; t < col.length; t++) {
      int16_t y = col.headY - t * MATRIX_CHAR_H;
      if (y < 0 || y >= areaH) continue;
      uint16_t c = dimColorTowardBlack(matrixGlyphColor(t, col.length), MATRIX_BG_OPACITY_PERCENT);
      matrixCanvas.setTextColor(c, COLOR_BG); // opaque is fine here -- this is the back layer, drawn on a freshly-cleared base
      matrixCanvas.setCursor(x, y);
      matrixCanvas.print(MATRIX_CHARS[random(0, MATRIX_CHAR_COUNT)]);
    }
    col.headY += col.speed;
    if (col.headY - col.length * MATRIX_CHAR_H > SCR_H) matrixResetColumn(i);
  }

  drawNightIdleTextOverlay(matrixCanvas); // transparent bg -- only its own strokes cover the rain underneath
  matrixCanvas.pushSprite(0, 0);
}

// ── twinkling starfield ──────────────────────────────────────────────────
#define NIGHT_STAR_COUNT 28
struct NightStar { int16_t x, y; float phase; };
static NightStar nightStars[NIGHT_STAR_COUNT];
static bool nightStarsReady = false;

static void starfieldEnsureInit() {
  if (nightStarsReady) return;
  for (uint8_t i = 0; i < NIGHT_STAR_COUNT; i++) {
    nightStars[i].x = (int16_t)random(0, SCR_W);
    nightStars[i].y = (int16_t)random(0, SCR_H - BAR_H);
    nightStars[i].phase = (random(0, 1000) / 1000.0f) * 2.0f * NIGHT_PI;
  }
  nightStarsReady = true;
}

void starfieldStep() {
  matrixEnsureCanvas();
  starfieldEnsureInit();
  matrixBgCanvas.pushSprite(&matrixCanvas, 0, 0);

  float t = millis() / 1000.0f;
  for (uint8_t i = 0; i < NIGHT_STAR_COUNT; i++) {
    float b = sinf(t * 0.5f + nightStars[i].phase) * 0.5f + 0.5f; // 0..1
    uint8_t v = (uint8_t)(30 + b * 160); // dim baseline, brighter at twinkle peak
    matrixCanvas.fillRect(nightStars[i].x, nightStars[i].y, 2, 2, M5.Display.color565(v, v, v));
  }

  matrixCanvas.pushSprite(0, 0);
}

// ── drifting fireflies ───────────────────────────────────────────────────
#define NIGHT_FIREFLY_COUNT 12
struct NightFirefly { float x, y, vx, vy; };
static NightFirefly nightFireflies[NIGHT_FIREFLY_COUNT];
static bool nightFirefliesReady = false;

static void firefliesEnsureInit() {
  if (nightFirefliesReady) return;
  for (uint8_t i = 0; i < NIGHT_FIREFLY_COUNT; i++) {
    nightFireflies[i].x = (float)random(0, SCR_W);
    nightFireflies[i].y = (float)random(0, SCR_H - BAR_H);
    nightFireflies[i].vx = (float)random(-10, 11) / 40.0f; // slow drift, not a bounce-around bug
    nightFireflies[i].vy = (float)random(-10, 11) / 40.0f;
  }
  nightFirefliesReady = true;
}

void firefliesStep() {
  matrixEnsureCanvas();
  firefliesEnsureInit();
  matrixBgCanvas.pushSprite(&matrixCanvas, 0, 0);

  int16_t areaH = SCR_H - BAR_H;
  uint8_t lr, lg, lb;
  themeLeadRGB(lr, lg, lb);

  for (uint8_t i = 0; i < NIGHT_FIREFLY_COUNT; i++) {
    NightFirefly& f = nightFireflies[i];
    f.x += f.vx;
    f.y += f.vy;
    if (f.x < 0 || f.x > SCR_W)  { f.vx = -f.vx; f.x = (f.x < 0) ? 0 : (float)SCR_W; }
    if (f.y < 0 || f.y > areaH) { f.vy = -f.vy; f.y = (f.y < 0) ? 0 : (float)areaH; }

    float pulse = sinf(millis() / 600.0f + i) * 0.5f + 0.5f; // 0..1, gentle glow pulse per firefly
    uint8_t r = (uint8_t)(lr * pulse), g = (uint8_t)(lg * pulse), b = (uint8_t)(lb * pulse);
    matrixCanvas.fillCircle((int16_t)f.x, (int16_t)f.y, 2, M5.Display.color565(r, g, b));
  }

  matrixCanvas.pushSprite(0, 0);
}

#endif // UJI_M5_NIGHTANIMATIONS_H
