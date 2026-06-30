/*
 * Idle.h — idle-mode widget. Not a separate screen/UI mode like the
 * touch-menu overlays (TouchMenu.h). A tap wakes -- instantly in day mode,
 * but night mode is two-stage (a tap re-powers the screen from sleep / just
 * keeps it on, and it takes a double tap to actually enter the terminal),
 * see idleNotifyInteraction()/idleConsumeWakeTap(), wired into
 * UJICoreM5.ino's loop().
 *
 * Day (7am-10pm) and night (10pm-7am) are genuinely two different
 * rendering models, not just a palette swap:
 *  - Day prints a small block of lines straight into the terminal
 *    scrollback (Terminal.h's printLineGetIndex()/updateScrollLineAt())
 *    and refreshes them in place (clock ticking, weather/face rotating)
 *    rather than spamming a new line every tick. It's ephemeral:
 *    termGetMark()/termRewindToMark() snap the scrollback back to
 *    exactly how it looked right before idle kicked in, both on wake and
 *    when switching to night mid-session -- it never lingers as history.
 *    After 15 idle min it also asks whether to power off (see
 *    drawIdlePowerPromptOverlay() et al below).
 *  - Night takes over the live display directly with a sprite-based
 *    dashboard (date/time/weather/battery/IMU-temp -- NightAnimations.h's
 *    drawNightIdleBackground()) plus a rotating ambient animation (matrix
 *    rain / starfield / fireflies, ~15 min each) meant to be calm to look
 *    at while falling asleep. It never touches
 *    scrollBuf at all, so waking from night is just a normal redraw of
 *    the real (untouched) terminal underneath -- see
 *    idleNotifyInteraction()'s wasNight branch. Dims to
 *    IDLE_NIGHT_BRIGHTNESS, and after IDLE_NIGHT_SLEEP_MS (3h) of total
 *    idle time blanks the screen via M5.Display.sleep() until tapped (a
 *    tap there just re-powers back to the dashboard, not the terminal) --
 *    verified against the installed M5GFX source (LGFXBase.hpp) that
 *    sleep()/wakeup() are real, existing calls; wakeup() restores
 *    whatever brightness was last set (the dim level), so
 *    applyBrightness() (Config.h) is called explicitly on every wake to
 *    restore the user's real configured level, not leave it stuck dim.
 *
 * Deliberately NOT ESP32 deep-sleep with a wake-on-touch interrupt --
 * just a software display sleep while the main loop keeps polling touch
 * normally (and, while awake, keeps stepping night's animation every
 * ~100ms via a non-blocking throttle, not a blocking loop -- that would
 * freeze touch input for hours), since M5's touch controller is a
 * separate I2C peripheral that isn't gated by display power state.
 * Revisit deep-sleep if battery life on the finished build ends up
 * mattering.
 */
#ifndef UJI_M5_IDLE_H
#define UJI_M5_IDLE_H
#include "Globals.h"

#define IDLE_DAY_START_HOUR   7
#define IDLE_NIGHT_START_HOUR 22

#define IDLE_DEBUG_FORCE_NIGHT 0 // set to 1 to force night-idle regardless of clock, for testing

#define IDLE_ENTER_MS          60000UL  // inactivity before the idle widget appears
// IDLE_WEATHER_REFRESH_MS now lives in Globals.h (NightAnimations.h needs it too).
#define IDLE_FLOURISH_ROTATE_MS 20000UL  // face/joke rotation
#define IDLE_CLOCK_TICK_MS      1000UL
#define IDLE_NIGHT_BRIGHTNESS      50    // bumped from 25 -- still dim, but 25 was reportedly too dark even in a fully dark room

// Night-idle: a dashboard (date/time/weather/battery/IMU-temp, drawn into
// a sprite by NightAnimations.h, not the scrollback) plus a rotating
// ambient animation -- matrix rain / starfield / fireflies, ~15 min each,
// cycling. IDLE_NIGHT_SLEEP_MS is total idle time (the same idleFor clock
// day-idle uses), not time-since-night-began, so the rotation segment is
// just computed from it -- 3h / 15min = 12 segments = exactly 4 clean
// trips through all 3 animations.
#define IDLE_NIGHT_FRAME_MS         100UL                // animation frame cadence
#define IDLE_NIGHT_BG_REFRESH_MS   1000UL                 // dashboard text refresh cadence (independent of frame cadence)
#define IDLE_NIGHT_ANIM_SEGMENT_MS (15UL * 60UL * 1000UL) // 15 min per animation
#define IDLE_NIGHT_SLEEP_MS        (3UL * 60UL * 60UL * 1000UL) // 3 hours total before screen sleep

// Day-idle-only "are you still there?" power-save prompt -- after this
// long idle (day hours only; night-idle already has its own sleep path),
// ask whether to power off. No answer for IDLE_POWER_PROMPT_WAIT_MS turns
// the text red as a last warning; no answer for IDLE_POWER_PROMPT_RED_MS
// after *that* and it powers off on its own.
#define IDLE_DAY_POWER_PROMPT_MS (15UL * 60UL * 1000UL) // 15 min
#define IDLE_POWER_PROMPT_WAIT_MS (2UL * 60UL * 1000UL) // 2 min grace period
#define IDLE_POWER_PROMPT_RED_MS  (1UL * 60UL * 1000UL) // 1 min red warning

// IDLE_DATE_SCALE/IDLE_TIME_SCALE/IDLE_WEATHER_SCALE now live in Globals.h
// (shared with NightAnimations.h's dashboard).
//
// The flourish (face-or-joke) line needs its own sizing, not a fixed
// constant: kaomoji are a handful of characters and can take the bigger
// size easily, but some jokes are full sentences that would have to be
// cut off mid-word to fit at the same size. scaleForFlourish() tries
// IDLE_FLOURISH_MAX_SCALE first (deliberately less than IDLE_WEATHER_SCALE,
// per request -- "a little bigger... but not as big as weather"), then
// IDLE_FLOURISH_MID_SCALE for longer ones -- every joke below is short
// enough to guarantee at least the mid tier, so 1.0x is a safety net for
// future content, not something that should normally show up.
#define IDLE_FLOURISH_MAX_SCALE 2.0f
#define IDLE_FLOURISH_MID_SCALE 1.5f

// Same self-deprecating-dev-humor vein as the user's own example; easy to
// edit/extend -- this is content, not architecture. All trimmed to fit
// one row at IDLE_FLOURISH_MID_SCALE (<=32 chars) so scaleForFlourish()
// never has to fall back below that.
static const char* const IDLE_JOKES[] = {
  "day 3 vibecoding: who's json?",
  "works on my machine. not yours.",
  "99 bugs in the code, counting...",
  "i don't test. i just ship it.",
  "semicolon? i barely know-colon.",
  "to mess up big, use a computer.",
  "not lazy, just power-saving.",
  "still compiling... send snacks.",
  "trust stack overflow. it's fine.",
  "ctrl+z: my favorite life skill.",
  "404: motivation not found.",
};
#define IDLE_JOKE_COUNT (sizeof(IDLE_JOKES) / sizeof(IDLE_JOKES[0]))

// Friendlier moods only -- an idle ambient display shouldn't randomly
// flash a frustrated/sad face at you.
static const FaceMood IDLE_FACE_MOODS[] = {
  FACE_NEUTRAL, FACE_HAPPY, FACE_EXCITED, FACE_PLAYFUL, FACE_THINKING,
};
#define IDLE_FACE_MOOD_COUNT (sizeof(IDLE_FACE_MOODS) / sizeof(IDLE_FACE_MOODS[0]))

enum IdleState : uint8_t { IDLE_ACTIVE, IDLE_DAY, IDLE_NIGHT, IDLE_SLEEPING };
static IdleState idleState = IDLE_ACTIVE;
static unsigned long lastInteractionMs = 0;
static bool idleWakeTapPending = false;

static unsigned long idlePowerPromptShownMs = 0;
static bool idlePowerPromptIsRed = false;
// Reference time the day-idle power-save countdown runs from. Pinned to
// "now" every tick while plugged in (so it never matures), so the timer
// effectively (re)starts the moment the device is unplugged.
static unsigned long idleDayPowerBaseMs = 0;
static Button idlePowerPromptButtons[2]; // [0] = Stay On, [1] = Power Off

// Where the scrollback stood right before idle mode first kicked in --
// every open*IdleWidget() call rewinds to this mark before drawing its
// own content (so day<->night transitions replace, not stack), and
// idleNotifyInteraction() rewinds to it one final time on wake, so the
// whole widget vanishes rather than becoming permanent history.
static uint8_t idleEntryMarkCount = 0;
static uint8_t idleEntryMarkNext = 0;

static const uint8_t IDLE_NO_LINE = 255; // sentinel: "this line isn't on screen this session"
static uint8_t idleDateLineIdx = IDLE_NO_LINE;
static uint8_t idleTimeLineIdx = IDLE_NO_LINE;
static uint8_t idleWeatherLineIdx = IDLE_NO_LINE;
static uint8_t idleBatteryLineIdx = IDLE_NO_LINE;
static uint8_t idleFlourishLineIdx = IDLE_NO_LINE;
static bool idleTimeKnown = false; // false => NTP never synced; date/time/weather lines are skipped
static String idleLastTimeStr; // last text actually drawn -- redraw only happens when this changes
static String idleLastDateStr;
static String idleLastBatteryStr;

static unsigned long idleLastClockMs = 0;
static unsigned long idleLastWeatherMs = 0;
static unsigned long idleLastFlourishMs = 0;

// Night-idle's own throttles -- idleNightLastSegment != 255 detects when
// the rotation crosses into a new animation (so matrix rain can reseed
// its columns); starfield/fireflies lazily self-init on first use, so
// they don't need an explicit reset here.
static unsigned long idleNightFrameMs = 0;
static unsigned long idleNightBgMs = 0;
static uint8_t idleNightLastSegment = 255;

// Night-idle wakes are deliberately two-stage so a stray nighttime bump
// turns the screen on without dumping you into the CLI: from sleep a tap
// just re-powers the panel back to the dashboard, and from the dashboard
// it takes a second tap within IDLE_NIGHT_DOUBLE_TAP_MS of the first to
// actually enter the terminal. Day-idle is unaffected (single tap wakes).
#define IDLE_NIGHT_DOUBLE_TAP_MS 600UL
static unsigned long idleNightFirstTapMs = 0;

static bool isNightHour(int hour) {
#if IDLE_DEBUG_FORCE_NIGHT
  (void)hour;
  return true;
#else
  return hour >= IDLE_NIGHT_START_HOUR || hour < IDLE_DAY_START_HOUR;
#endif
}

// "Is the device on external/USB power right now?" Uses VBUS voltage
// rather than isCharging(): charging flips to false once the battery tops
// off, but VBUS stays ~5V the whole time a cable's plugged in, which is
// what "plugged in" should mean here. AXP2101 (CoreS3) reports VBUS in mV;
// -1 means the model can't, in which case fall back to the charging state.
// (M5 Power_Class::getVBUSVoltage(), verified in the installed M5Unified
// source.)
static bool idleOnExternalPower() {
  int16_t vbus = M5.Power.getVBUSVoltage();
  if (vbus < 0) return M5.Power.isCharging() == m5::Power_Class::is_charging;
  return vbus > 4000; // USB bus ~5V when plugged, ~0 when not
}

// Defensive cap for content whose length isn't under our control (the
// weather API's response) -- everything stored here is meant to be
// exactly one row, and printLineGetIndex()/updateScrollLineAt() don't wrap.
static String fitToOneRow(const String& s, float scale) {
  int maxLen = maxCharsForScale(scale);
  if ((int)s.length() <= maxLen) return s;
  return s.substring(0, maxLen);
}

// Tries IDLE_FLOURISH_MAX_SCALE, then IDLE_FLOURISH_MID_SCALE, only
// falling all the way back to normal (1x) for something longer than
// every joke currently in the list -- short kaomoji and the shorter
// jokes get the biggest treatment, longer sentences still get a real
// size bump instead of staying at "normal," and nothing gets cut off
// mid-word.
static float scaleForFlourish(const String& s) {
  int len = s.length();
  if (len <= maxCharsForScale(IDLE_FLOURISH_MAX_SCALE)) return IDLE_FLOURISH_MAX_SCALE;
  if (len <= maxCharsForScale(IDLE_FLOURISH_MID_SCALE)) return IDLE_FLOURISH_MID_SCALE;
  return 1.0f;
}

static String randomIdleFlourish() {
  if (random(2) == 0) return randomFaceString(IDLE_FACE_MOODS[random(IDLE_FACE_MOOD_COUNT)]);
  return String(IDLE_JOKES[random(IDLE_JOKE_COUNT)]);
}

static void openDayIdleWidget() {
  termRewindToMark(idleEntryMarkCount, idleEntryMarkNext); // clear whatever was here before (a previous night-idle block, most likely)
  idleTimeKnown = time(nullptr) > 1700000000;

  beginScrollBatch();
  termPrintln(TERM_DIVIDER, COLOR_DIM);
  if (idleTimeKnown) {
    idleLastDateStr = formatIdleDate();
    idleLastTimeStr = formatIdleTime();
    idleDateLineIdx = printLineGetIndex(idleLastDateStr, COLOR_DIM, IDLE_DATE_SCALE);
    idleTimeLineIdx = printLineGetIndex(idleLastTimeStr, colorAccent(), IDLE_TIME_SCALE);
  } else {
    idleDateLineIdx = printLineGetIndex("(clock not synced -- try \"wifi connect\")", COLOR_DIM, 1);
    idleTimeLineIdx = IDLE_NO_LINE;
  }

  String werr;
  String w = idleTimeKnown ? fetchWeatherShort(werr) : "";
  idleWeatherLineIdx = printLineGetIndex(fitToOneRow(w.length() ? w : "(weather unavailable)", IDLE_WEATHER_SCALE), COLOR_TEXT, IDLE_WEATHER_SCALE);

  String flourish = randomIdleFlourish();
  idleFlourishLineIdx = printLineGetIndex(flourish, COLOR_TEXT, scaleForFlourish(flourish));

  idleLastBatteryStr = formatBatteryStatus();
  idleBatteryLineIdx = printLineGetIndex(idleLastBatteryStr, COLOR_DIM, 1);
  endScrollBatch();

  unsigned long now = millis();
  idleLastClockMs = now;
  idleLastWeatherMs = now;
  idleLastFlourishMs = now;
  idleDayPowerBaseMs = now; // start the power-save countdown from here (paused while plugged in)
}

// Night-idle no longer touches the scrollback at all (see NightAnimations.h) --
// it takes over the live display directly with a sprite-based dashboard +
// rotating animation, so there's nothing here to mark/rewind. idleTimeKnown
// still gets set (idleTick()'s day/night-flip check reads it).
static void openNightIdleWidget() {
  termRewindToMark(idleEntryMarkCount, idleEntryMarkNext); // clear whatever day-idle content was here, if any
  idleTimeKnown = time(nullptr) > 1700000000;
  M5.Display.setBrightness(IDLE_NIGHT_BRIGHTNESS);
  matrixEnsureCanvas();
  matrixEnsureBgCanvas();

  idleNightLastSegment = 255; // force refreshNightIdleWidget()'s first call to treat this as a fresh segment
  idleNightFrameMs = 0;
  idleNightBgMs = 0;
  idleNightFirstTapMs = 0; // a fresh night session always starts the double-tap from scratch
}

// The display only ever shows HH:MM (no seconds), so redrawing every
// time this is *called* would mean 59 out of every 60 calls touch the
// screen for a value that didn't actually change -- that's what was
// still flickering. This only writes anything when the formatted string
// is actually different from what's already on screen, so a real redraw
// happens once a minute, right when the minute changes, not on a blind
// timer. redrawVisibleLine() (vs. drawScrollback()) keeps each of those
// real redraws scoped to just the one row, too.
static void refreshIdleClockLine() {
  if (!idleTimeKnown || idleTimeLineIdx == IDLE_NO_LINE) return;

  String t = formatIdleTime();
  if (t != idleLastTimeStr) {
    idleLastTimeStr = t;
    updateScrollLineAt(idleTimeLineIdx, t, colorAccent(), IDLE_TIME_SCALE);
    redrawVisibleLine(idleTimeLineIdx);
  }

  if (idleDateLineIdx != IDLE_NO_LINE) {
    String d = formatIdleDate();
    if (d != idleLastDateStr) {
      idleLastDateStr = d;
      updateScrollLineAt(idleDateLineIdx, d, COLOR_DIM, IDLE_DATE_SCALE);
      redrawVisibleLine(idleDateLineIdx);
    }
  }
}

static void refreshDayIdleWidget() {
  unsigned long now = millis();

  if (now - idleLastClockMs >= IDLE_CLOCK_TICK_MS) {
    idleLastClockMs = now;
    refreshIdleClockLine();

    // Battery is a cheap I2C poll (unlike weather), so it's fine to check
    // every tick -- only redraws when the percentage/charging state
    // actually changes, same "compare before redraw" idiom as the clock.
    String b = formatBatteryStatus();
    if (b != idleLastBatteryStr) {
      idleLastBatteryStr = b;
      updateScrollLineAt(idleBatteryLineIdx, b, COLOR_DIM, 1);
      redrawVisibleLine(idleBatteryLineIdx);
    }
  }

  if (idleTimeKnown && now - idleLastWeatherMs >= IDLE_WEATHER_REFRESH_MS) {
    idleLastWeatherMs = now;
    String werr;
    String w = fetchWeatherShort(werr);
    updateScrollLineAt(idleWeatherLineIdx, fitToOneRow(w.length() ? w : "(weather unavailable)", IDLE_WEATHER_SCALE), COLOR_TEXT, IDLE_WEATHER_SCALE);
    redrawVisibleLine(idleWeatherLineIdx);
  }

  if (now - idleLastFlourishMs >= IDLE_FLOURISH_ROTATE_MS) {
    idleLastFlourishMs = now;
    String flourish = randomIdleFlourish();
    updateScrollLineAt(idleFlourishLineIdx, flourish, COLOR_TEXT, scaleForFlourish(flourish));
    // The flourish line's height varies per-string, and the battery line
    // sits below it -- so repaint from the flourish down, otherwise a
    // height change leaves the battery stranded/covered (redrawVisibleLine
    // alone only repaints the flourish's own row).
    redrawVisibleLinesFrom(idleFlourishLineIdx);
  }
}

// idleFor (the same "time since last interaction" clock day-idle uses)
// drives the rotation directly -- segment = (idleFor / 15min) % 3 -- so
// there's no separate "time since night began" to track. Runs the
// current segment's animation on its own ~100ms throttle, independent of
// the dashboard text's own ~1s throttle.
//
// Rain (segment 0) draws its own dashboard text directly (reversed
// layering -- see NightAnimations.h's nightMatrixStep()), so the shared
// drawNightIdleBackground()/matrixBgCanvas refresh below is skipped for
// it -- nothing would read it anyway.
static void refreshNightIdleWidget(unsigned long idleFor) {
  unsigned long now = millis();

  uint8_t segment = (uint8_t)((idleFor / IDLE_NIGHT_ANIM_SEGMENT_MS) % 3);
  if (segment != idleNightLastSegment) {
    idleNightLastSegment = segment;
    if (segment == 0) matrixReset(); // (re)seed the rain columns whenever it's rain's turn again
  }

  if (segment != 0 && now - idleNightBgMs >= IDLE_NIGHT_BG_REFRESH_MS) {
    idleNightBgMs = now;
    drawNightIdleBackground(matrixBgCanvas);
  }

  if (now - idleNightFrameMs >= IDLE_NIGHT_FRAME_MS) {
    idleNightFrameMs = now;
    switch (segment) {
      case 0: nightMatrixStep(); break;
      case 1: starfieldStep();   break;
      case 2: firefliesStep();   break;
    }
  }
}

// Day-idle-only "are you still there?" prompt -- M5.Power.powerOff() is
// the same call the "poweroff" command uses (Registry.h); printing a
// short message and a brief delay first just lets it actually paint
// before power cuts, same reasoning as that command's confirm path.
static void doIdlePowerOff() {
  M5.Display.fillRect(0, 0, SCR_W, SCR_H - BAR_H, COLOR_BG);
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(COLOR_TEXT, COLOR_BG);
  M5.Display.setCursor(8, 40);
  M5.Display.print("No answer -- powering off...");
  delay(400);
  M5.Power.powerOff();
}

void drawIdlePowerPromptOverlay() {
  int16_t areaH = SCR_H - BAR_H;
  M5.Display.fillRect(0, 0, SCR_W, areaH, COLOR_BG);

  uint16_t textColor = idlePowerPromptIsRed ? COLOR_DANGER : COLOR_TEXT;
  M5.Display.setTextSize(1);
  M5.Display.setTextColor(textColor, COLOR_BG);
  M5.Display.setCursor(8, 40);
  M5.Display.print("Been idle a while --");
  M5.Display.setCursor(8, 56);
  M5.Display.print("power off to save power?");

  int16_t btnY = areaH - 60;
  int16_t halfW = SCR_W / 2;
  idlePowerPromptButtons[0] = { 0, btnY, halfW, 50, "Stay On", 0 };
  idlePowerPromptButtons[1] = { halfW, btnY, (int16_t)(SCR_W - halfW), 50, "Power Off", 1 };
  drawButton(idlePowerPromptButtons[0]);
  drawButton(idlePowerPromptButtons[1], true);
}

void handleIdlePowerPromptTouch(int16_t x, int16_t y) {
  int idx = hitTestButtons(idlePowerPromptButtons, 2, x, y);
  if (idx < 0) return;
  playClick();
  if (idx == 1) { doIdlePowerOff(); return; }

  // Stay On -- same as any other wake: widget/prompt disappear, back to
  // the normal active terminal, fresh idle countdown from here.
  uiMode = UI_CLOSED;
  lastInteractionMs = millis();
  termRewindToMark(idleEntryMarkCount, idleEntryMarkNext);
  drawBar();
}

// Called once per loop() iteration; a no-op most ticks.
void idleTick() {
  // The prompt drives its own timeout/escalation regardless of the
  // uiMode == UI_CLOSED gate below -- it's the one idle-mode state that
  // needs to keep ticking while uiMode says otherwise.
  if (uiMode == UI_IDLE_POWER_PROMPT) {
    unsigned long elapsed = millis() - idlePowerPromptShownMs;
    bool shouldBeRed = elapsed >= IDLE_POWER_PROMPT_WAIT_MS;
    if (shouldBeRed != idlePowerPromptIsRed) {
      idlePowerPromptIsRed = shouldBeRed; // redraw once, right when it crosses into the red phase -- not every tick
      drawIdlePowerPromptOverlay();
    }
    if (elapsed >= IDLE_POWER_PROMPT_WAIT_MS + IDLE_POWER_PROMPT_RED_MS) doIdlePowerOff();
    return;
  }

  if (uiMode != UI_CLOSED) return; // don't engage underneath an open menu/confirm

  unsigned long now = millis();
  unsigned long idleFor = now - lastInteractionMs;
  bool nowIsNight = idleTimeKnown ? isNightHour(currentLocalHour()) : false;

  switch (idleState) {
    case IDLE_ACTIVE:
      if (idleFor >= IDLE_ENTER_MS) {
        termGetMark(idleEntryMarkCount, idleEntryMarkNext); // remember "before idle" so it can all be undone later
        bool timeKnown = time(nullptr) > 1700000000;
        bool night = timeKnown && isNightHour(currentLocalHour());
        if (night) { openNightIdleWidget(); idleState = IDLE_NIGHT; }
        else       { openDayIdleWidget();   idleState = IDLE_DAY; }
      }
      break;

    case IDLE_DAY:
      if (nowIsNight) { openNightIdleWidget(); idleState = IDLE_NIGHT; }
      else {
        // Plugged in => never power-save: keep the countdown's baseline
        // pinned to now so it can't mature. Unplugging freezes the
        // baseline, so the prompt fires IDLE_DAY_POWER_PROMPT_MS after the
        // unplug (the timer "starts" only when on battery).
        if (idleOnExternalPower()) idleDayPowerBaseMs = now;

        if (now - idleDayPowerBaseMs >= IDLE_DAY_POWER_PROMPT_MS) {
          // Counts as "no longer silently idling" -- it's actively asking
          // something now, not just sitting there -- so the generic
          // any-tap-wakes logic (idleNotifyInteraction()) leaves this tap
          // alone and lets it reach handleIdlePowerPromptTouch() instead.
          idleState = IDLE_ACTIVE;
          idlePowerPromptShownMs = now;
          idlePowerPromptIsRed = false;
          uiMode = UI_IDLE_POWER_PROMPT;
          drawIdlePowerPromptOverlay();
        } else {
          refreshDayIdleWidget();
        }
      }
      break;

    case IDLE_NIGHT:
      if (!nowIsNight) {
        applyBrightness();
        openDayIdleWidget();
        idleState = IDLE_DAY;
      } else {
        refreshNightIdleWidget(idleFor);
        if (idleFor >= IDLE_NIGHT_SLEEP_MS) {
          M5.Display.sleep();
          idleState = IDLE_SLEEPING;
        }
      }
      break;

    case IDLE_SLEEPING:
      break; // nothing to do; waits for idleNotifyInteraction()
  }
}

// Fully wake into the active terminal. Used by day-idle (a single tap)
// and night-idle (the confirming second tap of the double-tap). wasNight
// distinguishes the two cleanup paths: night-idle never touched the
// scrollback (it took over the live display directly), so it just redraws
// the real, untouched terminal; day-idle's lines still need removing via
// the entry mark so the widget doesn't linger as history.
static void idleWakeToActive(bool wasNight) {
  applyBrightness(); // wakeup()/the dim night level may be in effect -- put the real one back
  idleState = IDLE_ACTIVE;
  idleNightFirstTapMs = 0;
  if (wasNight) drawScrollback();
  else termRewindToMark(idleEntryMarkCount, idleEntryMarkNext);
  drawBar();
  idleWakeTapPending = true;
}

// Call whenever a real tap/drag is processed in the main loop. isTap is
// true only for a discrete click/release; a drag produces many false-isTap
// frames, which the night double-tap below must not count as taps.
void idleNotifyInteraction(bool isTap) {
  lastInteractionMs = millis();
  if (idleState == IDLE_ACTIVE) return;

  // Day-idle: any tap wakes straight into the terminal.
  if (idleState == IDLE_DAY) { idleWakeToActive(false); return; }

  // Sleeping (only ever reached from night): the screen was blanked for
  // power-save, so a tap just re-powers the panel and drops back to the
  // night dashboard -- never straight into the terminal. This wake tap
  // doesn't count as the first half of the double-tap.
  if (idleState == IDLE_SLEEPING) {
    M5.Display.wakeup();
    M5.Display.setBrightness(IDLE_NIGHT_BRIGHTNESS); // stay dim -- still night
    idleState = IDLE_NIGHT;
    idleNightLastSegment = 255; // force the next idleTick() to repaint a fresh frame
    idleNightFrameMs = 0;
    idleNightBgMs = 0;
    idleNightFirstTapMs = 0;
    idleWakeTapPending = true;
    return;
  }

  // Night dashboard showing: require a double tap to enter the terminal --
  // the first tap just arms the window (and keeps the screen on), the
  // second within IDLE_NIGHT_DOUBLE_TAP_MS confirms. Drag frames (isTap
  // false) only keep the screen on; they never arm or confirm, so a swipe
  // across the dashboard can't impersonate the second tap.
  if (!isTap) { idleWakeTapPending = true; return; }

  unsigned long now = millis();
  if (idleNightFirstTapMs != 0 && now - idleNightFirstTapMs <= IDLE_NIGHT_DOUBLE_TAP_MS) {
    idleWakeToActive(true);
  } else {
    idleNightFirstTapMs = now;
    idleWakeTapPending = true; // consume this tap; it shouldn't fall through to a menu
  }
}

// Returns true exactly once per wake, so the caller knows to skip normal
// tap dispatch for that touch (the tap that wakes shouldn't also fire
// whatever's underneath it).
bool idleConsumeWakeTap() {
  if (idleWakeTapPending) { idleWakeTapPending = false; return true; }
  return false;
}

#endif // UJI_M5_IDLE_H
