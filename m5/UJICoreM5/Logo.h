/*
 * Logo.h — the UJI ascii-art logo (verbatim from UJICore.js's UJI_LOGO,
 * shown there by the hidden "uji" command). Used for the boot splash
 * and, now, as the actual "uji" command too -- same screen, two
 * triggers. waitTapOrTimeout() is the shared "hold this screen until
 * tapped or N ms pass" logic both use.
 */
#ifndef UJI_M5_LOGO_H
#define UJI_M5_LOGO_H
#include "Globals.h"

static const char* const UJI_LOGO_LINES[] = {
  "     ___            ___               ",
  "    /\\__\\          /\\  \\        ___   ",
  "   /:/  /          \\:\\  \\      /\\  \\  ",
  "  /:/  /       ___ /::\\__\\     \\:\\  \\ ",
  " /:/  /  ___  /\\  /:/\\/__/     /::\\__\\",
  "/:/__/  /\\__\\ \\:\\/:/  /     __/:/\\/__/",
  "\\:\\  \\ /:/  /  \\::/  /     /\\/:/  /   ",
  " \\:\\  /:/  /    \\/__/      \\::/__/    ",
  "  \\:\\/:/  /                 \\:\\__\\    ",
  "   \\::/  /                   \\/__/    ",
  "    \\/__/                             ",
};
static const uint8_t UJI_LOGO_LINE_COUNT = sizeof(UJI_LOGO_LINES) / sizeof(UJI_LOGO_LINES[0]);

void showBootLogo() {
  M5.Display.fillScreen(COLOR_BG);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(colorAccent(), COLOR_BG);
  M5.Display.setTextDatum(top_left);

  // Center the whole block as one unit: find the widest line, then
  // position so the block (not any single line) sits in the middle.
  uint16_t maxLen = 0;
  for (uint8_t i = 0; i < UJI_LOGO_LINE_COUNT; i++) {
    uint16_t len = strlen(UJI_LOGO_LINES[i]);
    if (len > maxLen) maxLen = len;
  }

  const int16_t charW = 6; // builtin 6x8 font at textSize 1
  const int16_t charH = 8;
  int16_t blockW = maxLen * charW;
  int16_t blockH = UJI_LOGO_LINE_COUNT * charH;
  int16_t x = (SCR_W - blockW) / 2;
  int16_t y = (SCR_H - blockH) / 2;
  if (x < 0) x = 0;
  if (y < 0) y = 0;

  for (uint8_t i = 0; i < UJI_LOGO_LINE_COUNT; i++) {
    M5.Display.setCursor(x, y + i * charH);
    M5.Display.print(UJI_LOGO_LINES[i]);
  }
}

// Waits for finger-up plus a short cooldown so a wake-up tap can't also
// register as a click on whatever gets drawn next.
void drainTapWithCooldown() {
  while (M5.Touch.getCount() > 0) {
    M5.update();
    delay(10);
  }
  delay(250);
}

// Holds whatever's currently on screen until either a tap or `ms`
// milliseconds pass.
void waitTapOrTimeout(uint32_t ms) {
  unsigned long start = millis();
  bool tapped = false;
  while (millis() - start < ms) {
    M5.update();
    if (M5.Touch.getCount() > 0) { tapped = true; break; }
    delay(10);
  }
  if (tapped) drainTapWithCooldown();
}

// The "uji" command prints the logo into the scrollback like any other
// command's output (matching how UJICore.js's cmdUji() just does a
// println) -- it's the boot splash (showBootLogo(), full-screen) that's
// the special case, not this.
void ujiRun(const String& args) {
  String out;
  for (uint8_t i = 0; i < UJI_LOGO_LINE_COUNT; i++) {
    out += UJI_LOGO_LINES[i];
    out += "\n";
  }
  termPrintln(out, colorAccent());
}

#endif // UJI_M5_LOGO_H
