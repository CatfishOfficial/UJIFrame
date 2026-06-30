/*
 * MatrixEffect.h — column-rain effect, styled after UJICore.js's
 * runMatrixRain(): a bright "lead" character at the head of each column
 * fading down through the active theme's "trail" color toward black
 * (config color green/amber/blue/purple -- see Config.h/THEMES), a
 * centered bordered label badge, and continuous (sub-character-height)
 * fall speed (tunable via config glitterSpeed slow/normal/fast) rather
 * than jumping a full glyph per frame. Katakana from the original isn't
 * renderable on the builtin ASCII-only font, so the charset substitutes
 * hex digits + symbols for a similar "glyph soup" feel instead.
 *
 * The original ran the rain on a semi-transparent canvas layered over
 * the terminal (real alpha blending, easy in a browser). True alpha
 * blending against the live display isn't something to rely on here --
 * see captureTerminalSnapshot() in Terminal.h for why -- so instead the
 * terminal is captured into an offscreen "background" sprite once when
 * the effect starts (nothing else runs while it's playing, so it can't
 * go stale) and recomposited under the rain every frame, with glyphs
 * drawn in transparent-background mode so only their actual strokes
 * cover the terminal underneath. Opaque-over-snapshot, not blended --
 * but it reads as "rain falling over my terminal" either way.
 *
 * matrixRun() is the standalone Fun-category command (runs until
 * tapped, no time limit -- it's meant to just be left on);
 * runMatrixBusy() is a brief non-skippable flourish other commands can
 * call (currently: purge, on confirm).
 */
#ifndef UJI_M5_MATRIXEFFECT_H
#define UJI_M5_MATRIXEFFECT_H
#include "Globals.h"

#define MATRIX_MAX_COLS 24
struct MatrixColumn {
  int16_t headY;
  uint8_t length;
  uint8_t speed; // pixels per frame (continuous fall, not a full glyph jump)
};
static MatrixColumn matrixCols[MATRIX_MAX_COLS];
static uint8_t matrixColCount = 0;
static const int16_t MATRIX_COL_W = 16;
static const int16_t MATRIX_CHAR_H = 16;
static const char MATRIX_CHARS[] = "0123456789ABCDEF<>|/\\+-*";
static const uint8_t MATRIX_CHAR_COUNT = sizeof(MATRIX_CHARS) - 1;

// Not static any more -- NightAnimations.h reuses this exact sprite pair
// for its own animations (starfield/fireflies, plus its own reversed-
// layering matrix renderer) instead of allocating more full-screen
// 320x240x16bpp sprites. Whatever fills matrixBgCanvas before
// matrixStep() runs becomes "what the foreground effect is drawn over"
// -- matrixCaptureBackground() below fills it with the real terminal for
// matrix-rain's own uses (the "matrix" command, the purge busy-flourish).
M5Canvas matrixCanvas(&M5.Display);
bool matrixCanvasReady = false;
M5Canvas matrixBgCanvas(&M5.Display);
bool matrixBgCanvasReady = false;

void matrixEnsureCanvas() {
  if (matrixCanvasReady) return;
  matrixCanvas.setColorDepth(16);
  matrixCanvas.createSprite(SCR_W, SCR_H);
  matrixCanvasReady = true;
}

// Same sprite, no terminal snapshot -- for callers (NightAnimations.h)
// that draw their own background content into matrixBgCanvas directly
// instead of capturing the terminal.
void matrixEnsureBgCanvas() {
  if (matrixBgCanvasReady) return;
  matrixBgCanvas.setColorDepth(16);
  matrixBgCanvas.createSprite(SCR_W, SCR_H);
  matrixBgCanvasReady = true;
}

// UJICore.js's original createCanvas(10, 0.78) value (78) made the dimmed
// text underneath nearly impossible to read on real hardware -- lowered
// for actual visibility; a faithful port isn't worth it if you can't see
// it. Also reused by NightAnimations.h's nightMatrixStep() to dim the
// rain layer there.
#define MATRIX_BG_OPACITY_PERCENT 55

// Snapshots the terminal once per effect-start, not per frame -- see
// Terminal.h's captureTerminalSnapshot() for why this is a redraw, not a
// pixel readback. Dimmed toward black at capture time (via
// dimColorTowardBlack() on each logical color used, not by poking the
// sprite's raw buffer -- that raw-buffer approach is what turned this
// purple earlier on) -- same direction as the original's black canvas
// layered over the terminal (CSS opacity), at MATRIX_BG_OPACITY_PERCENT
// rather than the original's literal 0.78 (see that constant's comment).
static void matrixCaptureBackground() {
  matrixEnsureBgCanvas();
  captureTerminalSnapshot(matrixBgCanvas, MATRIX_BG_OPACITY_PERCENT);
}

// Ported from UJICore.js's GLITTER_SPEED_MULT.
static const float GLITTER_SPEED_MULT[3] = { 0.6f, 1.0f, 1.8f };

static void matrixResetColumn(uint8_t i) {
  matrixCols[i].headY = -(int16_t)random(0, 200);
  matrixCols[i].length = random(5, 14);
  uint8_t base = random(3, 8);
  float scaled = base * GLITTER_SPEED_MULT[CONFIG.glitterSpeed];
  matrixCols[i].speed = (uint8_t)(scaled < 1 ? 1 : scaled);
}

static void matrixReset() {
  matrixColCount = SCR_W / MATRIX_COL_W;
  if (matrixColCount > MATRIX_MAX_COLS) matrixColCount = MATRIX_MAX_COLS;
  for (uint8_t i = 0; i < matrixColCount; i++) matrixResetColumn(i);
}

// t=0 is the bright lead glyph (theme's lead color); deeper into the
// trail it fades from the active theme's trail color down toward black
// -- the discrete-glyph analogue of the original's translucent-overlay
// fade. Reads CONFIG.color via themeLeadRGB()/themeTrailRGB(), so this
// re-themes automatically with "config color <name>".
static uint16_t matrixGlyphColor(uint8_t t, uint8_t length) {
  uint8_t lr, lg, lb;
  themeLeadRGB(lr, lg, lb);
  if (t == 0) return M5.Display.color565(lr, lg, lb);

  uint8_t tr, tg, tb;
  themeTrailRGB(tr, tg, tb);
  float frac = (float)t / (float)length;
  uint8_t r = (uint8_t)(tr * (1.0f - frac) + 8 * frac);
  uint8_t g = (uint8_t)(tg * (1.0f - frac) + 8 * frac);
  uint8_t b = (uint8_t)(tb * (1.0f - frac) + 8 * frac);
  return M5.Display.color565(r, g, b);
}

static void matrixDrawLabel(const char* label) {
  matrixCanvas.setTextSize(1);
  matrixCanvas.setTextDatum(middle_center);
  int16_t tw = matrixCanvas.textWidth(label) + 16;
  int16_t th = 20;
  int16_t tx = (SCR_W - tw) / 2;
  int16_t ty = (SCR_H - th) / 2;
  uint16_t glow = colorAccent();
  uint16_t bg = colorAccentDim();
  matrixCanvas.fillRect(tx, ty, tw, th, bg);
  matrixCanvas.drawRect(tx, ty, tw, th, glow);
  matrixCanvas.setTextColor(glow, bg);
  matrixCanvas.drawString(label, SCR_W / 2, SCR_H / 2);
  matrixCanvas.setTextDatum(top_left);
}

static void matrixStep(const char* label) {
  matrixEnsureCanvas();
  matrixBgCanvas.pushSprite(&matrixCanvas, 0, 0); // terminal snapshot as this frame's base layer
  matrixCanvas.setTextSize(2);
  matrixCanvas.setTextDatum(top_left);

  for (uint8_t i = 0; i < matrixColCount; i++) {
    MatrixColumn& col = matrixCols[i];
    int16_t x = i * MATRIX_COL_W;
    for (uint8_t t = 0; t < col.length; t++) {
      int16_t y = col.headY - t * MATRIX_CHAR_H;
      if (y < 0 || y >= SCR_H) continue;
      matrixCanvas.setTextColor(matrixGlyphColor(t, col.length)); // transparent bg: only the glyph's own strokes cover the terminal below
      matrixCanvas.setCursor(x, y);
      matrixCanvas.print(MATRIX_CHARS[random(0, MATRIX_CHAR_COUNT)]);
    }
    col.headY += col.speed;
    if (col.headY - col.length * MATRIX_CHAR_H > SCR_H) matrixResetColumn(i);
  }

  if (label) matrixDrawLabel(label);
  matrixCanvas.pushSprite(0, 0);
}

void matrixRun(const String& args) {
  // The tap that pressed OK to run this command can still be "down" for
  // a few ms after dispatch -- without draining it first, the loop below
  // sees that same touch, thinks it's a dismiss tap, and exits after one
  // frame. Drain it before watching for a real dismiss tap.
  drainTapWithCooldown();

  matrixCaptureBackground();
  matrixReset();
  while (true) {
    M5.update();
    if (M5.Touch.getCount() > 0) break;
    matrixStep("tap to stop");
    delay(45);
  }
  drainTapWithCooldown();
  renderAll();
}

void runMatrixBusy(uint32_t ms) {
  matrixCaptureBackground();
  matrixReset();
  unsigned long start = millis();
  while (millis() - start < ms) {
    matrixStep("please wait");
    delay(45);
  }
}

#endif // UJI_M5_MATRIXEFFECT_H
