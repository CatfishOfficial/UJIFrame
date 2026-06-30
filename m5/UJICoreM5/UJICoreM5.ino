/*
 * UJI M5 — a real terminal (scrollback + a persistent command bar) for
 * the M5Stack CoreS3, with touch standing in for a keyboard until one
 * gets wired up. Touch only ever types into the bar (TouchMenu.h);
 * Enter is the one thing that runs a line (Parser.h -> Registry.h).
 *
 * Board setup matches the working test sketch this was built from:
 * M5.config() / M5.begin(cfg) / M5.Display / M5.Touch.getCount().
 */
#include "Globals.h"
#include "Terminal.h"
#include "History.h"
#include "Config.h"
#include "Calc.h"
#include "Cowsay.h"
#include "Faces.h"
#include "Logo.h"
#include "MatrixEffect.h"
#include "WifiCommand.h"
#include "Clock.h"
#include "Http.h"
#include "Weather.h"
#include "NightAnimations.h"
#include "Idle.h"
#include "Registry.h"
#include "Parser.h"
#include "TouchMenu.h"

// ── globals declared extern in Globals.h ──────────────────────────────
int16_t SCR_W = 320;
int16_t SCR_H = 240;
AppConfig CONFIG;
UiMode uiMode = UI_CLOSED;

// ── shared drawing/touch helpers ──────────────────────────────────────
void playClick() {
  if (CONFIG.beep) M5.Speaker.tone(1800, 30);
}

int hitTestButtons(const Button* buttons, int count, int16_t x, int16_t y) {
  for (int i = 0; i < count; i++) {
    const Button& b = buttons[i];
    if (x >= b.x && x < b.x + b.w && y >= b.y && y < b.y + b.h) return i;
  }
  return -1;
}

void drawButton(const Button& b, bool danger) {
  uint16_t border = danger ? COLOR_DANGER : colorAccent();
  M5.Display.fillRoundRect(b.x + 3, b.y + 3, b.w - 6, b.h - 6, 6, COLOR_BG);
  M5.Display.drawRoundRect(b.x + 3, b.y + 3, b.w - 6, b.h - 6, 6, border);
  M5.Display.setTextDatum(middle_center);
  M5.Display.setTextSize(b.h > 36 ? 2 : 1);
  M5.Display.setTextColor(COLOR_TEXT, COLOR_BG);
  M5.Display.drawString(b.label, b.x + b.w / 2, b.y + b.h / 2);
  M5.Display.setTextDatum(top_left);
}

void renderAll() {
  drawScrollback();
  drawBar();
  if (uiMode == UI_CATEGORY || uiMode == UI_COMMANDS || uiMode == UI_EXTENSION) drawOverlay();
  else if (uiMode == UI_CONFIRM) drawConfirmOverlay();
  else if (uiMode == UI_IDLE_POWER_PROMPT) drawIdlePowerPromptOverlay();
}

static void onEnterPressed() {
  if (composedLine.length() == 0) return;
  String line = composedLine;
  barClearText();   // bar shows an empty prompt immediately
  uiMode = UI_CLOSED; // closes any open overlay; commands that need a
                      // follow-up (purge) re-arm their own UiMode
  dispatchLine(line);
}

// ── arduino entry points ───────────────────────────────────────────────
void setup() {
  auto cfg = M5.config();
  M5.begin(cfg);

  SCR_W = M5.Display.width();
  SCR_H = M5.Display.height();
  // Terminal.h does its own word-wrapping/line-chunking already (every
  // printed line is pre-cut to fit), so the library's own auto-wrap is
  // turned off here -- leaving it on let the two disagree about exactly
  // where a line broke, and the bit that auto-wrapped onto a second
  // visual row could bleed into the row below and survive that row's
  // next redraw (its clear-rect didn't know to cover it).
  M5.Display.setTextWrap(false, false);

  loadConfig();
  applyBrightness();
  wifiAutoConnectIfEnabled();

  showBootLogo();
  waitTapOrTimeout(10000);

  termInit();
  drawBar();
}

void loop() {
  M5.update();

  if (M5.Touch.getCount() > 0) {
    auto detail = M5.Touch.getDetail(0);
    bool dragging = uiMode == UI_CLOSED && detail.isDragging() && detail.y < SCR_H - BAR_H;
    bool clicked = detail.wasClicked();

    if (dragging || clicked) {
      idleNotifyInteraction(clicked); // clicked == a discrete tap/release; drag frames pass false
      // The tap/drag that wakes the device from idle/sleep is consumed
      // here -- it shouldn't also fall through and fire whatever's
      // underneath it (a menu button, a drag-scroll, etc.).
      if (idleConsumeWakeTap()) return;
    }

    // Drag-to-scroll the scrollback -- only when there's no overlay over
    // it and nothing else claiming touch (the menu/bar still take taps
    // normally; this only fires while actively dragging over the open
    // terminal view).
    if (dragging) {
      handleTerminalDrag(detail.deltaY());
    } else if (clicked) {
      int16_t x = detail.x, y = detail.y;

      // Enter/Backspace are pinned in the bar and stay live in every
      // mode except a destructive confirm or the idle power-off prompt
      // (too easy to fat-finger a re-run right next to Confirm/Cancel,
      // or to fire a stray command instead of answering the prompt).
      if (uiMode != UI_CONFIRM && uiMode != UI_IDLE_POWER_PROMPT) {
        if (hitTestButtons(barEnterButton(), 1, x, y) == 0) {
          playClick();
          onEnterPressed();
          return;
        }
        if (hitTestButtons(barBackspaceButton(), 1, x, y) == 0) {
          playClick();
          barBackspace();
          return;
        }
      }

      switch (uiMode) {
        case UI_CLOSED:
          if (y < SCR_H - BAR_H) openCategoryMenu();
          break;
        case UI_CONFIRM:
          handleConfirmTouch(x, y);
          break;
        case UI_IDLE_POWER_PROMPT:
          handleIdlePowerPromptTouch(x, y);
          break;
        default: // UI_CATEGORY / UI_COMMANDS / UI_EXTENSION
          touchMenuHandleTouch(x, y);
          break;
      }
    }
  } else {
    resetTerminalDrag();
  }

  idleTick();
  wifiTick();
}
