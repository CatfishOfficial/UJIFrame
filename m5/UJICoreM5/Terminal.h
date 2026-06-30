/*
 * Terminal.h — the scrollback feed + the persistent bottom command bar.
 * This is the "real CLI" half of the UI: everything above the bar is
 * output history (command echo + results), the bar is a single text
 * buffer that touch (TouchMenu.h) or, eventually, a real keyboard types
 * into. Nothing here ever covers the bar.
 */
#ifndef UJI_M5_TERMINAL_H
#define UJI_M5_TERMINAL_H
#include "Globals.h"
#include <math.h> // lroundf(), for fractional text-scale pixel math

#define SCROLLBACK_CAPACITY 60
static const int16_t LINE_H = 10;
static const int16_t CHAR_W = 6; // builtin GLCD font at textSize 1

// Wrap prose at the same width as the "----" divider lines instead of
// running it out to the literal screen edge -- TERM_DIVIDER below is
// exactly this many dashes, so the two always match.
#define TERM_LINE_WIDTH_CHARS 48
const char* const TERM_DIVIDER = "------------------------------------------------"; // must be exactly TERM_LINE_WIDTH_CHARS dashes

struct ScrollLine {
  String text;
  uint16_t color;
  float scale; // builtin-font text size for this line (M5GFX setTextSize takes a float, e.g. 1.5); 1 = normal
};
static ScrollLine scrollBuf[SCROLLBACK_CAPACITY];
static uint8_t scrollCount = 0;
static uint8_t scrollNext = 0;
static bool scrollBatchActive = false;

// 0 = pinned to the live edge (newest line at the bottom); higher values
// look further back into history. Drag-to-scroll (see handleTerminalDrag()
// below) is the only thing that changes this -- new output always snaps
// back to 0, same as a chat app or a real terminal.
static uint8_t termScrollOffset = 0;
static int16_t scrollDragAccum = 0; // raw drag pixels not yet converted into a whole line

String composedLine = "";
static Button enterBtn;
static Button backspaceBtn;

// The builtin GLCD font scales by simple multiplication (M5GFX's
// setTextSize() takes a float, not just whole numbers), so a row at text
// size N is N times as wide/tall as size 1 (plus the same small fixed
// gap between rows) -- used for normal lines, the bigger "face" prints,
// and the idle-mode widget's in-between sizes (e.g. 1.5x).
static int16_t lineHeightForScale(float scale) { return (int16_t)lroundf(LINE_H * scale); }

int16_t maxCharsForScale(float scale) {
  return (int16_t)(TERM_LINE_WIDTH_CHARS / scale);
}

static uint8_t pushScrollLine(const String& text, uint16_t color, float scale) {
  uint8_t idx = scrollNext;
  ScrollLine& e = scrollBuf[idx];
  e.text = text;
  e.color = color;
  e.scale = scale;
  scrollNext = (scrollNext + 1) % SCROLLBACK_CAPACITY;
  if (scrollCount < SCROLLBACK_CAPACITY) scrollCount++;
  termScrollOffset = 0; // new output snaps the view back to live
  return idx;
}

// For callers that want to mutate a line later instead of always appending
// (the idle-mode widget's clock/weather/face lines, which refresh in place
// rather than spamming a new line every tick) -- prints exactly one row, no
// '\n' splitting/wrapping, and hands back the scrollBuf slot it landed in.
uint8_t printLineGetIndex(const String& text, uint16_t color, float scale) {
  uint8_t idx = pushScrollLine(text, color, scale);
  if (!scrollBatchActive) drawScrollback();
  return idx;
}

// Overwrites a previously-printed line's content in place (no shifting, no
// new line) -- idx must still be a line that hasn't scrolled out of
// scrollBuf's ring since it was handed out. Doesn't redraw itself; the
// caller redraws once after updating whatever lines changed this tick,
// same batching spirit as beginScrollBatch/endScrollBatch.
void updateScrollLineAt(uint8_t idx, const String& text, uint16_t color, float scale) {
  ScrollLine& e = scrollBuf[idx];
  e.text = text;
  e.color = color;
  e.scale = scale;
}

// Not every line is the same height any more (see termPrintln's scale
// param), so this walks the stored lines from the oldest forward,
// summing real per-line heights, until a screenful is used -- the same
// shape as the old "areaH / LINE_H" division, just no longer assuming a
// fixed row height. That count is exactly what's shown when scrolled all
// the way to the top, so termScrollOffset is capped at scrollCount minus
// it (mirrors the old fixed-height formula).
static uint8_t maxTermScrollOffset() {
  int16_t areaH = SCR_H - BAR_H;
  uint8_t oldestStoredIdx = (scrollCount < SCROLLBACK_CAPACITY) ? 0 : scrollNext;
  int16_t used = 0;
  uint8_t count = 0;
  while (count < scrollCount) {
    uint8_t idx = (uint8_t)((oldestStoredIdx + count) % SCROLLBACK_CAPACITY);
    int16_t h = lineHeightForScale(scrollBuf[idx].scale);
    if (used + h > areaH && count > 0) break;
    used += h;
    count++;
  }
  return (scrollCount > count) ? (uint8_t)(scrollCount - count) : 0;
}

// Figures out which stored lines are visible right now (accounting for
// termScrollOffset and each line's own height) by walking backward from
// whatever's pinned at the bottom of the view, accumulating real heights
// until the visible area is full. Fills outIndices oldest-first, ready
// to render top-down. Shared by drawScrollback() and
// captureTerminalSnapshot() so this math only lives in one place.
static void computeVisibleLines(int16_t areaH, uint8_t outIndices[SCROLLBACK_CAPACITY], uint8_t& outCount) {
  uint8_t oldestStoredIdx = (scrollCount < SCROLLBACK_CAPACITY) ? 0 : scrollNext;
  int16_t lastVisible = (int16_t)scrollCount - 1 - termScrollOffset;
  int16_t used = 0;
  uint8_t count = 0;
  int16_t i = lastVisible;
  while (i >= 0 && count < SCROLLBACK_CAPACITY) {
    uint8_t idx = (uint8_t)(((int16_t)oldestStoredIdx + i) % SCROLLBACK_CAPACITY);
    int16_t h = lineHeightForScale(scrollBuf[idx].scale);
    if (used + h > areaH && count > 0) break;
    used += h;
    outIndices[count++] = idx;
    i--;
  }
  for (uint8_t a = 0, b = count; a < b / 2; a++) {
    uint8_t tmp = outIndices[a];
    outIndices[a] = outIndices[count - 1 - a];
    outIndices[count - 1 - a] = tmp;
  }
  outCount = count;
}

static void termScrollBy(int delta) {
  int newOffset = (int)termScrollOffset + delta;
  uint8_t maxOffset = maxTermScrollOffset();
  if (newOffset < 0) newOffset = 0;
  if (newOffset > maxOffset) newOffset = maxOffset;
  if ((uint8_t)newOffset != termScrollOffset) {
    termScrollOffset = (uint8_t)newOffset;
    drawScrollback();
  }
}

// Called from the main loop while a finger drags over the scrollback
// area (uiMode == UI_CLOSED only -- see UJICoreM5.ino). deltaY is the
// raw per-frame pixel delta from M5Unified's touch_detail_t; dragging
// up (negative deltaY) reveals older lines, same convention as a chat
// app's scrollback.
void handleTerminalDrag(int16_t deltaY) {
  scrollDragAccum += deltaY;
  while (scrollDragAccum <= -LINE_H) { termScrollBy(1); scrollDragAccum += LINE_H; }
  while (scrollDragAccum >= LINE_H) { termScrollBy(-1); scrollDragAccum -= LINE_H; }
}

void resetTerminalDrag() { scrollDragAccum = 0; }

void drawScrollback() {
  int16_t areaH = SCR_H - BAR_H;
  M5.Display.fillRect(0, 0, SCR_W, areaH, COLOR_BG);
  M5.Display.setTextDatum(top_left);

  uint8_t indices[SCROLLBACK_CAPACITY];
  uint8_t toShow;
  computeVisibleLines(areaH, indices, toShow);

  int16_t y = 2;
  for (uint8_t i = 0; i < toShow; i++) {
    ScrollLine& e = scrollBuf[indices[i]];
    M5.Display.setTextSize(e.scale);
    M5.Display.setTextColor(e.color, COLOR_BG);
    M5.Display.setCursor(4, y);
    M5.Display.print(e.text);
    y += lineHeightForScale(e.scale);
  }
}

// Repaints exactly one already-visible line in place -- just its own row,
// not the whole scrollback area. drawScrollback() clears and redraws
// everything every time, which is fine for normal output (new content is
// the common case anyway) but was visibly flashing/strobing when the
// idle-mode widget called it once a second just to tick the clock. This
// is what ticking content (Idle.h) should call instead.
//
// Clears down to the *next* line's top (or the bottom of the visible
// area, if this is the last line) rather than just this line's own new
// height -- a line's scale can change between redraws now (the idle
// widget's flourish line rotates between differently-sized faces and
// jokes), and clearing only the new, possibly-smaller height left a
// strip of the previous, taller glyph behind every time it shrank.
void redrawVisibleLine(uint8_t idx) {
  int16_t areaH = SCR_H - BAR_H;
  uint8_t indices[SCROLLBACK_CAPACITY];
  uint8_t count;
  computeVisibleLines(areaH, indices, count);

  int16_t y = 2;
  for (uint8_t i = 0; i < count; i++) {
    ScrollLine& e = scrollBuf[indices[i]];
    int16_t h = lineHeightForScale(e.scale);
    if (indices[i] == idx) {
      int16_t clearBottom = (i + 1 < count) ? (y + h) : areaH;
      M5.Display.fillRect(0, y, SCR_W, clearBottom - y, COLOR_BG);
      M5.Display.setTextDatum(top_left);
      M5.Display.setTextSize(e.scale);
      M5.Display.setTextColor(e.color, COLOR_BG);
      M5.Display.setCursor(4, y);
      M5.Display.print(e.text);
      return;
    }
    y += h;
  }
}

// Repaints a visible line *and every line below it*, down to the bottom of
// the scrollback area. redrawVisibleLine() assumes the lines beneath the
// one it redraws haven't moved -- true only when the redrawn line's height
// is fixed. The idle widget's flourish line changes height as it rotates
// between differently-sized faces/jokes, which shifts every line below it
// (the battery line), so a plain redrawVisibleLine() there left the
// battery stranded at its old position (looking "covered up"). This
// redraws the whole stack from idx down so the lines below land at their
// new positions.
void redrawVisibleLinesFrom(uint8_t idx) {
  int16_t areaH = SCR_H - BAR_H;
  uint8_t indices[SCROLLBACK_CAPACITY];
  uint8_t count;
  computeVisibleLines(areaH, indices, count);

  int16_t y = 2;
  uint8_t start = count;
  for (uint8_t i = 0; i < count; i++) {
    if (indices[i] == idx) { start = i; break; }
    y += lineHeightForScale(scrollBuf[indices[i]].scale);
  }
  if (start == count) return; // idx isn't currently on screen

  M5.Display.fillRect(0, y, SCR_W, areaH - y, COLOR_BG);
  M5.Display.setTextDatum(top_left);
  for (uint8_t i = start; i < count; i++) {
    ScrollLine& e = scrollBuf[indices[i]];
    M5.Display.setTextSize(e.scale);
    M5.Display.setTextColor(e.color, COLOR_BG);
    M5.Display.setCursor(4, y);
    M5.Display.print(e.text);
    y += lineHeightForScale(e.scale);
  }
}

// Re-draws the current scrollback + bar into an offscreen sprite instead
// of the live display -- used by MatrixEffect.h to get a "background"
// layer to composite rain over. This redraws from our own known state
// rather than reading pixels back off the panel: CoreS3 shares GPIO35
// between SPI MISO and the LCD's D/C pin, so display readback isn't
// something to rely on without testing on the real board.
//
// opacityPercent dims every color used (via dimColorTowardBlack(), which
// operates on logical RGB565 values, not a sprite's raw buffer bytes --
// see Globals.h) toward black, matching how the original layers the
// matrix rain on a canvas at CSS opacity:0.78 *over* the terminal --
// a black overlay, not a gray one.
void captureTerminalSnapshot(M5Canvas& dst, uint8_t opacityPercent) {
  uint16_t bg = dimColorTowardBlack(COLOR_BG, opacityPercent);
  int16_t areaH = SCR_H - BAR_H;
  dst.fillRect(0, 0, SCR_W, areaH, bg);
  dst.setTextDatum(top_left);

  uint8_t indices[SCROLLBACK_CAPACITY];
  uint8_t toShow;
  computeVisibleLines(areaH, indices, toShow); // match whatever's currently scrolled into view

  int16_t y = 2;
  for (uint8_t i = 0; i < toShow; i++) {
    ScrollLine& e = scrollBuf[indices[i]];
    dst.setTextSize(e.scale);
    dst.setTextColor(dimColorTowardBlack(e.color, opacityPercent), bg);
    dst.setCursor(4, y);
    dst.print(e.text);
    y += lineHeightForScale(e.scale);
  }

  int16_t y0 = SCR_H - BAR_H;
  dst.fillRect(0, y0, SCR_W, BAR_H, bg);
  dst.drawFastHLine(0, y0, SCR_W, dimColorTowardBlack(COLOR_DIM, opacityPercent));
  dst.setTextSize(1); // the loop above may have left this at a bigger face scale
  dst.setTextColor(dimColorTowardBlack(colorAccent(), opacityPercent), bg);
  dst.setCursor(4, y0 + (BAR_H - 8) / 2);
  dst.print("> " + composedLine);
}

// Splits on embedded '\n' first, then word-wraps any line too wide for
// one row into multiple stored rows -- breaks on the last space within
// the row's width so words don't get cut in half, only hard-cutting at
// the exact width as a fallback when a single word is wider than a whole
// row. Keeps one stored ScrollLine == exactly one rendered row, always.
//
// Redraws once at the end, *unless* a batch is active (beginScrollBatch/
// endScrollBatch) -- without batching, two termPrintln calls back to back
// (e.g. dispatchLine's echo + a command's own output) each force a full
// redraw of the scrollback area, which looks like the screen blinking on
// real hardware.
void termPrintln(const String& text, uint16_t color, float scale) {
  int maxLen = maxCharsForScale(scale);
  int start = 0;
  int totalLen = text.length();
  do {
    int nl = text.indexOf('\n', start);
    String line = (nl == -1) ? text.substring(start) : text.substring(start, nl);
    int lineLen = line.length();

    int pos = 0;
    do {
      int remaining = lineLen - pos;
      if (remaining <= maxLen) {
        pushScrollLine(line.substring(pos), color, scale);
        pos = lineLen;
      } else {
        int breakAt = -1;
        for (int j = pos + maxLen; j > pos; j--) {
          if (line[j] == ' ') { breakAt = j; break; }
        }
        if (breakAt == -1) {
          pushScrollLine(line.substring(pos, pos + maxLen), color, scale);
          pos += maxLen;
        } else {
          pushScrollLine(line.substring(pos, breakAt), color, scale);
          pos = breakAt + 1; // skip the space itself
        }
      }
    } while (pos < lineLen);

    if (nl == -1) break;
    start = nl + 1;
  } while (start <= totalLen);
  if (!scrollBatchActive) drawScrollback();
}

void termEcho(const String& line) {
  termPrintln("> " + line, COLOR_DIM);
}

// Suppresses the per-call redraw across several termPrintln calls, then
// does exactly one redraw covering all of them -- unless something else
// (the confirm overlay, the touch menu) took over the screen while the
// batch was open, in which case redrawing the scrollback here would just
// immediately wipe out whatever that overlay just drew.
void beginScrollBatch() { scrollBatchActive = true; }
void endScrollBatch() {
  scrollBatchActive = false;
  if (uiMode == UI_CLOSED) drawScrollback();
}

void termClear() {
  scrollCount = 0;
  scrollNext = 0;
  termScrollOffset = 0;
  drawScrollback();
}

// For ephemeral content that should vanish later instead of becoming
// permanent history -- the idle-mode widget (Idle.h) pastes its block in
// while idle, then on wake rewinds back to exactly how things looked
// beforehand. Safe as long as nothing else pushes a line in between
// taking the mark and rewinding to it (true for idle mode: it only runs
// while uiMode == UI_CLOSED, and the tap that wakes it is consumed
// before any command can dispatch and print something new).
void termGetMark(uint8_t& count, uint8_t& next) {
  count = scrollCount;
  next = scrollNext;
}

void termRewindToMark(uint8_t count, uint8_t next) {
  scrollCount = count;
  scrollNext = next;
  termScrollOffset = 0;
  drawScrollback();
}

void barAppendText(const String& s) {
  composedLine += s;
  drawBar();
}

void barBackspace() {
  if (composedLine.length() > 0) composedLine.remove(composedLine.length() - 1);
  drawBar();
}

void barClearText() {
  composedLine = "";
  drawBar();
}

void drawBar() {
  int16_t y0 = SCR_H - BAR_H;
  M5.Display.fillRect(0, y0, SCR_W, BAR_H, COLOR_BG);
  M5.Display.drawFastHLine(0, y0, SCR_W, COLOR_DIM);

  const int16_t tileW = 36;
  enterBtn     = { (int16_t)(SCR_W - tileW),     (int16_t)(y0 + 1), tileW, (int16_t)(BAR_H - 1), "OK",  0 };
  backspaceBtn = { (int16_t)(SCR_W - tileW * 2), (int16_t)(y0 + 1), tileW, (int16_t)(BAR_H - 1), "DEL", 1 };
  drawButton(enterBtn);
  drawButton(backspaceBtn);

  int16_t textAreaW = SCR_W - tileW * 2 - 6;
  int16_t maxChars = textAreaW / CHAR_W - 2; // leave room for "> "
  String shown = composedLine;
  if ((int)shown.length() > maxChars && maxChars > 3) {
    shown = "..." + shown.substring(shown.length() - (maxChars - 3));
  }

  M5.Display.setTextSize(1);
  M5.Display.setTextColor(colorAccent(), COLOR_BG);
  M5.Display.setCursor(4, y0 + (BAR_H - 8) / 2);
  M5.Display.print("> " + shown);
}

const Button* barEnterButton() { return &enterBtn; }
const Button* barBackspaceButton() { return &backspaceBtn; }

// Boot-time welcome banner — the M5 analogue of UJICore.js's printWelcome():
// app name, a divider, a hint line (reworded for no-keyboard input), a
// short command preview, another divider. Plain ASCII dividers since the
// builtin font has no box-drawing glyphs. Batched into one redraw.
void termInit() {
  scrollCount = 0;
  scrollNext = 0;
  composedLine = "";

  beginScrollBatch();
  termPrintln("UJI M5", colorAccent());
  termPrintln(TERM_DIVIDER, COLOR_DIM);
  termPrintln("tap the screen to browse commands", COLOR_DIM);
  termPrintln(TERM_DIVIDER, COLOR_DIM);

  uint8_t shownCount = (COMMAND_COUNT < 5) ? COMMAND_COUNT : 5;
  for (uint8_t i = 0; i < shownCount; i++) {
    termPrintln(String(COMMANDS[i].name) + " - " + COMMANDS[i].description, COLOR_TEXT);
  }
  termPrintln("help - list everything", COLOR_DIM);
  termPrintln(TERM_DIVIDER, COLOR_DIM);
  endScrollBatch();
}

#endif // UJI_M5_TERMINAL_H
