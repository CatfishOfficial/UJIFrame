/*
 * Faces.h — Stack-chan ASCII kaomoji. The GLCD builtin font only covers
 * ASCII 0x20-0x7E (see Globals.h), so these are the classic ASCII-art
 * kaomoji genre that predates unicode entirely (orz, m(_ _)m, (^_^) and
 * friends were all typed on plain ASCII Japanese BBSes) rather than the
 * unicode ones that lean on Greek omega/CJK punctuation (・ω・) -- this
 * device's font literally can't draw those glyphs.
 *
 * This is just the inventory + a "face" command to browse it; nothing
 * wires these into idle mode or other commands yet -- that comes later.
 * randomFaceString() is the building block future call sites should use
 * (returns a String to drop into other output) rather than re-picking
 * from these arrays directly.
 */
#ifndef UJI_M5_FACES_H
#define UJI_M5_FACES_H
#include "Globals.h"

const char* const FACE_MOOD_NAMES[FACE_MOOD_COUNT] = {
  "neutral", "happy", "excited", "sad", "frustrated",
  "surprised", "sleepy", "thinking", "embarrassed", "apologetic", "playful",
};

static const char* const FACES_NEUTRAL[] = {
  "(^_^)", "(-_-)", "('_')", "(._.)", "(~_~)", "(o_o)",
};
static const char* const FACES_HAPPY[] = {
  "(^o^)", "(^_^)/", "\\(^o^)/", "(^_^)v", "(n_n)", "(^L^)",
};
static const char* const FACES_EXCITED[] = {
  "(>w<)", "(OwO)", "(^w^)", "(*^_^*)", "\\o/",
};
static const char* const FACES_SAD[] = {
  "(T_T)", "(;_;)", "(Q_Q)", "(ToT)", "('_')",
};
static const char* const FACES_FRUSTRATED[] = {
  "(>_<)", "(-_-#)", "(#_#)", "(>:()",
};
static const char* const FACES_SURPRISED[] = {
  "(o_O)", "(O_O)", "(0_0)", "(O.O)", "(?_?)",
};
static const char* const FACES_SLEEPY[] = {
  "(-.-)Zzz", "(u_u)", "(=_=)zzZ", "(~_~)zzz",
};
static const char* const FACES_THINKING[] = {
  "( . _ .)", "(-_-)?", "...", "(o_-)", "('-')?",
};
static const char* const FACES_EMBARRASSED[] = {
  "(^_^;)", "(;^_^)", "(>_<;)", "(#^.^#)",
};
static const char* const FACES_APOLOGETIC[] = {
  "m(_ _)m", "orz", "(m_m)", "(T_T)/",
};
static const char* const FACES_PLAYFUL[] = {
  "(^_-)", "(-_^)", "(^_~)",
};

struct FaceMoodEntry {
  const char* const* faces;
  uint8_t count;
};

#define FACE_ARR(a) { a, sizeof(a) / sizeof(a[0]) }
static const FaceMoodEntry FACE_TABLE[FACE_MOOD_COUNT] = {
  FACE_ARR(FACES_NEUTRAL), FACE_ARR(FACES_HAPPY), FACE_ARR(FACES_EXCITED),
  FACE_ARR(FACES_SAD), FACE_ARR(FACES_FRUSTRATED), FACE_ARR(FACES_SURPRISED),
  FACE_ARR(FACES_SLEEPY), FACE_ARR(FACES_THINKING), FACE_ARR(FACES_EMBARRASSED),
  FACE_ARR(FACES_APOLOGETIC), FACE_ARR(FACES_PLAYFUL),
};
#undef FACE_ARR

String randomFaceString(FaceMood mood) {
  const FaceMoodEntry& entry = FACE_TABLE[mood];
  return String(entry.faces[random(entry.count)]);
}

// Picks the largest builtin-font text size that still fits the whole
// face on one row -- "(-.-)Zzz" needs more room than "...", so this
// checks rather than assuming a single fixed size for every face.
// Capped at FACE_MAX_SCALE so even a 3-character face doesn't blow up
// to something silly; every face here comfortably fits well under that
// cap's width limit regardless.
static const uint8_t FACE_MAX_SCALE = 2;
static uint8_t scaleForFace(const String& face) {
  for (uint8_t s = FACE_MAX_SCALE; s >= 1; s--) {
    if ((int)face.length() <= maxCharsForScale(s)) return s;
  }
  return 1;
}

static void printFace(FaceMood mood) {
  String f = randomFaceString(mood);
  termPrintln(f, COLOR_TEXT, scaleForFace(f));
}

void faceRun(const String& args) {
  String moodName = args;
  moodName.trim();
  moodName.toLowerCase();

  if (moodName.length() == 0) {
    printFace((FaceMood)random(FACE_MOOD_COUNT));
    return;
  }

  for (uint8_t i = 0; i < FACE_MOOD_COUNT; i++) {
    if (moodName == FACE_MOOD_NAMES[i]) {
      printFace((FaceMood)i);
      return;
    }
  }
  termPrintln("Unknown mood. Try: face <mood> -- see the picker for the list.", COLOR_DIM);
}

#endif // UJI_M5_FACES_H
