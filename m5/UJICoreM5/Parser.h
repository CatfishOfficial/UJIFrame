/*
 * Parser.h — turns one composed line into a command + args and runs it.
 * "args" is the raw remainder of the line after the first space, passed
 * through whole (not split into argv) — same convention UJICore.js uses,
 * so commands like calc get their whole expression as one string.
 */
#ifndef UJI_M5_PARSER_H
#define UJI_M5_PARSER_H
#include "Globals.h"

void dispatchLine(const String& rawLine) {
  String line = rawLine;
  line.trim();
  if (line.length() == 0) return;

  // Batched so the echo line and the command's own output redraw the
  // scrollback exactly once between them, not twice back to back.
  beginScrollBatch();
  termEcho(line);
  pushHistory(line);

  int sp = line.indexOf(' ');
  String cmdName = (sp == -1) ? line : line.substring(0, sp);
  String args = (sp == -1) ? String("") : line.substring(sp + 1);
  args.trim();

  const CommandDef* cmd = findCommand(cmdName);
  if (!cmd) {
    termPrintln("Unknown command: " + cmdName + " (tap the screen for a list)", COLOR_DANGER);
  } else {
    cmd->run(args);
  }
  endScrollBatch();
}

#endif // UJI_M5_PARSER_H
