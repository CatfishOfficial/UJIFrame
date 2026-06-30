/*
 * TouchMenu.h — the soft-keyboard overlay: category -> commands -> that
 * command's argument-builder ("extension"). It only ever draws into the
 * scrollback area (above the bar) and only ever appends text into the
 * bar buffer via barAppendText() -- it never executes anything itself.
 * Enter (in the bar, Terminal.h) is the only thing that runs a line.
 */
#ifndef UJI_M5_TOUCHMENU_H
#define UJI_M5_TOUCHMENU_H
#include "Globals.h"

static int8_t selectedCategoryIdx = -1;
static const CommandDef* armedCommand = nullptr;
static int8_t configStep = 0;          // 0 = picking key, 1 = picking value
static int8_t configSelectedKeyIdx = -1;

static Button backButton;
static Button categoryButtons[CATEGORY_COUNT];
static Button commandButtons[8];
static const CommandDef* categoryCmds[8];
static uint8_t categoryCmdCount = 0;

// Single-column lists (EXT_CHOICES, the config key/value picker) render a
// fixed LIST_PAGE_SIZE rows at a constant, comfortable size no matter how
// many items there actually are -- Prev/Next paging (in the header row,
// next to "X") appears once there's more than one page, instead of
// shrinking every row to cram the whole list onto one screen.
#define LIST_PAGE_SIZE 4
static Button choiceButtons[LIST_PAGE_SIZE];
static Button configPickButtons[LIST_PAGE_SIZE];
static uint8_t pagedListVisibleCount = 0;
static uint8_t listScrollOffset = 0;
static Button pagingPrevButton = { 0, 0, 0, 0, "", 0 };
static Button pagingNextButton = { 0, 0, 0, 0, "", 0 };

static Button confirmButtons[2];
static String confirmLine1, confirmLine2;
static void (*confirmOnConfirm)() = nullptr;

static int16_t overlayHeight() { return SCR_H - BAR_H; }

static void drawOverlayBackdrop() {
  M5.Display.fillRect(0, 0, SCR_W, overlayHeight(), COLOR_BG);
}

static void drawBackButton() {
  backButton = { (int16_t)(SCR_W - 56), 2, 52, 36, "X", 0 };
  drawButton(backButton, true);
}

static void drawCategoryMenu() {
  drawOverlayBackdrop();
  drawBackButton();

  const int16_t top = 42, margin = 6, cols = 2, rows = 2;
  int16_t colW = (SCR_W - margin * (cols + 1)) / cols;
  int16_t rowH = (overlayHeight() - top - margin * (rows + 1)) / rows;

  for (uint8_t i = 0; i < CATEGORY_COUNT; i++) {
    int16_t col = i % cols, row = i / cols;
    int16_t x = margin + col * (colW + margin);
    int16_t y = top + margin + row * (rowH + margin);
    categoryButtons[i] = { x, y, colW, rowH, CATEGORIES[i], i };
    drawButton(categoryButtons[i]);
  }
}

static void drawCommandsMenu() {
  drawOverlayBackdrop();
  drawBackButton();

  categoryCmdCount = commandsInCategory(CATEGORIES[selectedCategoryIdx], categoryCmds, 8);

  const int16_t top = 42, margin = 6;
  int16_t cols = (categoryCmdCount <= 2) ? 1 : 2;
  int16_t rows = (categoryCmdCount + cols - 1) / cols;
  int16_t colW = (SCR_W - margin * (cols + 1)) / cols;
  int16_t rowH = (overlayHeight() - top - margin * (rows + 1)) / rows;

  for (uint8_t i = 0; i < categoryCmdCount; i++) {
    int16_t col = i % cols, row = i / cols;
    int16_t x = margin + col * (colW + margin);
    int16_t y = top + margin + row * (rowH + margin);
    commandButtons[i] = { x, y, colW, rowH, categoryCmds[i]->name, i };
    drawButton(commandButtons[i]);
  }
}

// Renders up to LIST_PAGE_SIZE labels starting at listScrollOffset, at a
// row height fixed by LIST_PAGE_SIZE (not by `count`) -- "four items" is
// always the same size, whether the list has 4 entries or 14. Prev/Next
// buttons go in the header row (next to "X") when there's more than one
// page; left blank (a zero-size, untappable rect) otherwise.
static void drawPagedList(Button* buttons, const char* const* labels, uint8_t count, int16_t top) {
  const int16_t margin = 6;
  int16_t rowH = (overlayHeight() - top - margin * (LIST_PAGE_SIZE + 1)) / LIST_PAGE_SIZE;

  uint8_t visible = count - listScrollOffset;
  if (visible > LIST_PAGE_SIZE) visible = LIST_PAGE_SIZE;
  pagedListVisibleCount = visible;

  for (uint8_t i = 0; i < visible; i++) {
    int16_t y = top + margin + i * (rowH + margin);
    buttons[i] = { margin, y, (int16_t)(SCR_W - margin * 2), rowH, labels[listScrollOffset + i], i };
    drawButton(buttons[i]);
  }

  bool hasPrev = listScrollOffset > 0;
  bool hasNext = (uint16_t)listScrollOffset + LIST_PAGE_SIZE < count;
  if (hasPrev) {
    pagingPrevButton = { 112, 4, 70, 32, "< Prev", 0 };
    drawButton(pagingPrevButton);
  } else {
    pagingPrevButton = { 0, 0, 0, 0, "", 0 };
  }
  if (hasNext) {
    pagingNextButton = { 188, 4, 70, 32, "Next >", 1 };
    drawButton(pagingNextButton);
  } else {
    pagingNextButton = { 0, 0, 0, 0, "", 0 };
  }
}

static bool handlePagingTouch(int16_t x, int16_t y) {
  if (hitTestButtons(&pagingPrevButton, 1, x, y) == 0) {
    playClick();
    listScrollOffset = (listScrollOffset >= LIST_PAGE_SIZE) ? listScrollOffset - LIST_PAGE_SIZE : 0;
    drawOverlay();
    return true;
  }
  if (hitTestButtons(&pagingNextButton, 1, x, y) == 0) {
    playClick();
    listScrollOffset += LIST_PAGE_SIZE;
    drawOverlay();
    return true;
  }
  return false;
}

// Generic across every EXT_CHOICES command -- reads the list straight off
// armedCommand instead of a hardcoded global, so cowsay/wifi/anything
// future just supply their own choices/choiceCount in Registry.h.
static void drawChoicesExtension() {
  if (!armedCommand) return;
  drawPagedList(choiceButtons, armedCommand->choices, armedCommand->choiceCount, 42);
}

static void drawConfigExtension() {
  if (configStep == 0) {
    static const char* keyLabels[CONFIG_KEY_COUNT];
    for (uint8_t i = 0; i < CONFIG_KEY_COUNT; i++) keyLabels[i] = CONFIG_KEYS[i].key;
    drawPagedList(configPickButtons, keyLabels, CONFIG_KEY_COUNT, 42);
  } else {
    const ConfigKeyDef& key = CONFIG_KEYS[configSelectedKeyIdx];
    drawPagedList(configPickButtons, key.values, key.valueCount, 42);
  }
}

static void drawExtensionMenu() {
  drawOverlayBackdrop();
  drawBackButton();
  if (!armedCommand) return;

  switch (armedCommand->ext) {
    case EXT_NONE:
      M5.Display.setTextSize(1);
      M5.Display.setTextColor(COLOR_TEXT, COLOR_BG);
      M5.Display.setCursor(8, 48);
      M5.Display.print("Tap OK to run \"" + String(armedCommand->name) + "\".");
      break;
    case EXT_NUMERIC:
      drawCalcExtension(0, 42, SCR_W, overlayHeight() - 42);
      break;
    case EXT_CHOICES:
      drawChoicesExtension();
      break;
    case EXT_CONFIG:
      drawConfigExtension();
      break;
    case EXT_TIME_ENTRY:
      drawTimeEntryExtension();
      break;
  }
}

// ── EXT_TIME_ENTRY: phone-style numpad for HH:MM (alarm) or duration (timer) ──
// armedCommand->name == "alarm" gets an extra row with repeat/once buttons.
static Button timeEntryButtons[14];
static uint8_t timeEntryButtonCount = 0;

void drawTimeEntryExtension() {
  if (!armedCommand) return;
  bool isAlarm = (String(armedCommand->name) == "alarm");

  int16_t top   = 42;
  int16_t colW  = SCR_W / 3;
  int16_t rows  = isAlarm ? 5 : 4;
  int16_t rowH  = (overlayHeight() - top) / rows;

  const char* labels[] = { "1","2","3","4","5","6","7","8","9","0",":","DEL" };
  for (int i = 0; i < 12; i++) {
    int16_t col = i % 3, row = i / 3;
    timeEntryButtons[i] = {
      (int16_t)(col * colW), (int16_t)(top + row * rowH),
      colW, rowH, labels[i], (uint8_t)i
    };
    drawButton(timeEntryButtons[i]);
  }
  timeEntryButtonCount = 12;

  if (isAlarm) {
    int16_t y5    = top + 4 * rowH;
    int16_t halfW = SCR_W / 2;
    timeEntryButtons[12] = { 0,     y5, halfW,                      rowH, "repeat", 12 };
    timeEntryButtons[13] = { halfW, y5, (int16_t)(SCR_W - halfW),   rowH, "once",   13 };
    drawButton(timeEntryButtons[12]);
    drawButton(timeEntryButtons[13]);
    timeEntryButtonCount = 14;
  }
}

void handleTimeEntryExtensionTouch(int16_t x, int16_t y) {
  int idx = hitTestButtons(timeEntryButtons, timeEntryButtonCount, x, y);
  if (idx < 0) return;
  playClick();
  if (idx == 11) { barBackspace(); return; }         // DEL
  if (idx == 12) { barAppendText(" repeat"); return; }
  if (idx == 13) { barAppendText(" once");   return; }
  const char* inserts[] = { "1","2","3","4","5","6","7","8","9","0",":" };
  if (idx < 11) barAppendText(inserts[idx]);
}

void drawOverlay() {
  switch (uiMode) {
    case UI_CATEGORY:  drawCategoryMenu(); break;
    case UI_COMMANDS:  drawCommandsMenu(); break;
    case UI_EXTENSION: drawExtensionMenu(); break;
    default: break; // UI_CLOSED / UI_CONFIRM are handled elsewhere
  }
}

void openCategoryMenu() {
  uiMode = UI_CATEGORY;
  selectedCategoryIdx = -1;
  armedCommand = nullptr;
  drawOverlay();
}

void closeMenu() {
  uiMode = UI_CLOSED;
  selectedCategoryIdx = -1;
  armedCommand = nullptr;
  renderAll();
}

static void handleBack() {
  switch (uiMode) {
    case UI_CATEGORY:
      closeMenu();
      break;
    case UI_COMMANDS:
      uiMode = UI_CATEGORY;
      drawOverlay();
      break;
    case UI_EXTENSION:
      if (armedCommand && armedCommand->ext == EXT_CONFIG && configStep == 1) {
        configStep = 0; // one step back inside the config picker, not all the way out
        listScrollOffset = 0; // back to the key list -- a different list, fresh page
        drawOverlay();
      } else {
        armedCommand = nullptr;
        uiMode = UI_COMMANDS;
        drawOverlay();
      }
      break;
    default:
      closeMenu();
      break;
  }
}

void touchMenuHandleTouch(int16_t x, int16_t y) {
  if (hitTestButtons(&backButton, 1, x, y) == 0) {
    playClick();
    handleBack();
    return;
  }

  switch (uiMode) {
    case UI_CATEGORY: {
      int idx = hitTestButtons(categoryButtons, CATEGORY_COUNT, x, y);
      if (idx >= 0) {
        playClick();
        selectedCategoryIdx = idx;
        uiMode = UI_COMMANDS;
        drawOverlay();
      }
      break;
    }
    case UI_COMMANDS: {
      int idx = hitTestButtons(commandButtons, categoryCmdCount, x, y);
      if (idx >= 0) {
        playClick();
        armedCommand = categoryCmds[idx];
        configStep = 0;
        configSelectedKeyIdx = -1;
        listScrollOffset = 0; // fresh extension list, fresh page
        calcResetKeypadPage(); // no-op for non-calc commands; back to the arithmetic page for calc
        barAppendText(String(armedCommand->name) + " ");
        uiMode = UI_EXTENSION;
        drawOverlay();
      }
      break;
    }
    case UI_EXTENSION: {
      if (!armedCommand) break;
      switch (armedCommand->ext) {
        case EXT_NONE:
          break; // nothing to tap here besides Back/OK
        case EXT_NUMERIC:
          handleCalcExtensionTouch(x, y);
          break;
        case EXT_TIME_ENTRY:
          handleTimeEntryExtensionTouch(x, y);
          break;
        case EXT_CHOICES: {
          if (handlePagingTouch(x, y)) break;
          int idx = hitTestButtons(choiceButtons, pagedListVisibleCount, x, y);
          if (idx >= 0) {
            playClick();
            barAppendText(armedCommand->choices[listScrollOffset + idx]);
          }
          break;
        }
        case EXT_CONFIG: {
          if (handlePagingTouch(x, y)) break;
          int idx = hitTestButtons(configPickButtons, pagedListVisibleCount, x, y);
          if (idx >= 0) {
            playClick();
            uint8_t actualIdx = listScrollOffset + idx;
            if (configStep == 0) {
              configSelectedKeyIdx = actualIdx;
              barAppendText(String(CONFIG_KEYS[actualIdx].key) + " ");
              configStep = 1;
              listScrollOffset = 0; // values are a different list -- fresh page
              drawOverlay();
            } else {
              barAppendText(CONFIG_KEYS[configSelectedKeyIdx].values[actualIdx]);
            }
          }
          break;
        }
      }
      break;
    }
    default:
      break;
  }
}

// ── generic confirm prompt (its own UiMode, not an "extension") -- used
// by any destructive command (purge, poweroff) instead of duplicating
// this Cancel/Confirm overlay per command ──────────────────────────────
void armConfirm(const char* line1, const char* line2, void (*onConfirm)()) {
  confirmLine1 = line1;
  confirmLine2 = line2;
  confirmOnConfirm = onConfirm;
  uiMode = UI_CONFIRM;
  drawConfirmOverlay();
}

void drawConfirmOverlay() {
  drawOverlayBackdrop();

  M5.Display.setTextSize(1);
  M5.Display.setTextColor(COLOR_TEXT, COLOR_BG);
  M5.Display.setCursor(8, 40);
  M5.Display.print(confirmLine1);
  M5.Display.setCursor(8, 56);
  M5.Display.print(confirmLine2);

  int16_t btnY = overlayHeight() - 60;
  int16_t halfW = SCR_W / 2;
  confirmButtons[0] = { 0, btnY, halfW, 50, "Cancel", 0 };
  confirmButtons[1] = { halfW, btnY, (int16_t)(SCR_W - halfW), 50, "Confirm", 1 };
  drawButton(confirmButtons[0]);
  drawButton(confirmButtons[1], true);
}

void handleConfirmTouch(int16_t x, int16_t y) {
  int idx = hitTestButtons(confirmButtons, 2, x, y);
  if (idx < 0) return;
  playClick();
  uiMode = UI_CLOSED;

  if (idx == 0) {
    termPrintln("Cancelled.", COLOR_DIM);
    drawBar();
  } else if (confirmOnConfirm) {
    confirmOnConfirm(); // responsible for its own output + redraw afterward
  }
}

#endif // UJI_M5_TOUCHMENU_H
