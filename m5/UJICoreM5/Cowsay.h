/*
 * Cowsay.h — ascii cow + a small preset phrase list. TouchMenu's
 * EXT_CHOICES picker taps one of these straight into the bar as the
 * argument; cowsayRun() also accepts free text if a phrase ever gets
 * there some other way (e.g. a future real keyboard).
 */
#ifndef UJI_M5_COWSAY_H
#define UJI_M5_COWSAY_H
#include "Globals.h"

const char* const COWSAY_PRESETS[COWSAY_PRESET_COUNT] = {
  "UJI wakes.",
  "Moo.",
  "Touch to continue.",
  "I have no keyboard.",
  "Ported, not forgotten.",
};

static String cowsayBubble(const String& phrase) {
  String border = "_";
  for (size_t i = 0; i < phrase.length() + 2; i++) border += "_";

  String out;
  out += " " + border + "\n";
  out += "< " + phrase + " >\n";
  out += " ";
  for (size_t i = 0; i < border.length(); i++) out += "-";
  out += "\n";
  out += "        \\   ^__^\n";
  out += "         \\  (oo)\\_______\n";
  out += "            (__)\\       )\\/\\\n";
  out += "                ||----w |\n";
  out += "                ||     ||\n";
  return out;
}

void cowsayRun(const String& args) {
  String phrase = (args.length() > 0) ? args : String(COWSAY_PRESETS[0]);
  termPrintln(cowsayBubble(phrase), COLOR_TEXT);
}

#endif // UJI_M5_COWSAY_H
