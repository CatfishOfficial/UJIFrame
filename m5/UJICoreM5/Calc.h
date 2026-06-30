/*
 * Calc.h — the core arithmetic engine ported from UJICore.js's
 * evalMathExpr(): tokenizer + recursive-descent parser with real
 * precedence (+ - | * / % | unary | ^ right-assoc | postfix ! | primary),
 * functions, constants, and "ans" memory. Algebra-solve and unit
 * conversion are deliberately not ported -- both need free-text
 * variable/unit names that don't fit a keypad-only input (see the plan).
 *
 * No C++ exceptions (Arduino toolchains often build with them off) --
 * errors are tracked with a flag instead, checked between parse steps.
 */
#ifndef UJI_M5_CALC_H
#define UJI_M5_CALC_H
#include "Globals.h"

enum CalcTokType { CT_NUM, CT_IDENT, CT_OP };
struct CalcToken {
  CalcTokType type;
  double num;
  String ident;
  char op;
};

#define CALC_MAX_TOKENS 64
static CalcToken calcTokens[CALC_MAX_TOKENS];
static uint8_t calcTokenCount = 0;
static uint8_t calcPos = 0;
static bool calcErr = false;
static String calcErrMsg = "";
static double lastCalcResult = 0;
static bool hasLastCalcResult = false;

static void calcFail(const String& msg) {
  if (!calcErr) { calcErr = true; calcErrMsg = msg; }
}

static void calcTokenize(const String& str) {
  calcTokenCount = 0;
  calcErr = false;
  calcErrMsg = "";
  int i = 0, n = str.length();
  while (i < n && calcTokenCount < CALC_MAX_TOKENS) {
    char c = str[i];
    if (isspace(c)) { i++; continue; }
    if (isdigit(c) || c == '.') {
      int j = i;
      while (j < n && (isdigit(str[j]) || str[j] == '.')) j++;
      if (j < n && (str[j] == 'e' || str[j] == 'E')) {
        j++;
        if (j < n && (str[j] == '+' || str[j] == '-')) j++;
        while (j < n && isdigit(str[j])) j++;
      }
      calcTokens[calcTokenCount++] = { CT_NUM, str.substring(i, j).toDouble(), "", 0 };
      i = j;
      continue;
    }
    if (isalpha(c) || c == '_') {
      int j = i;
      while (j < n && (isalnum(str[j]) || str[j] == '_')) j++;
      String ident = str.substring(i, j);
      ident.toLowerCase();
      calcTokens[calcTokenCount++] = { CT_IDENT, 0, ident, 0 };
      i = j;
      continue;
    }
    if (String("()+-*/%^!,").indexOf(c) != -1) {
      calcTokens[calcTokenCount++] = { CT_OP, 0, "", c };
      i++;
      continue;
    }
    calcFail("Unexpected character '" + String(c) + "'");
    return;
  }
}

static CalcToken* calcPeek() { return (calcPos < calcTokenCount) ? &calcTokens[calcPos] : nullptr; }
static CalcToken* calcNext() { return (calcPos < calcTokenCount) ? &calcTokens[calcPos++] : nullptr; }
static bool calcIsOp(CalcToken* t, char op) { return t && t->type == CT_OP && t->op == op; }
static void calcExpectOp(char op) {
  CalcToken* t = calcNext();
  if (!calcIsOp(t, op)) calcFail(String("Expected '") + op + "'");
}

static double calcParseExpression();

static double calcFactorial(double n) {
  if (n < 0 || floor(n) != n) { calcFail("! needs a non-negative integer"); return 0; }
  double result = 1;
  for (int i = 2; i <= (int)n; i++) result *= i;
  return result;
}

static double calcCallFn(const String& name, double* args, int argc) {
  if (name == "sqrt" && argc == 1) return sqrt(args[0]);
  if (name == "sin" && argc == 1) return sin(args[0]);
  if (name == "cos" && argc == 1) return cos(args[0]);
  if (name == "tan" && argc == 1) return tan(args[0]);
  if (name == "asin" && argc == 1) return asin(args[0]);
  if (name == "acos" && argc == 1) return acos(args[0]);
  if (name == "atan" && argc == 1) return atan(args[0]);
  if (name == "abs" && argc == 1) return fabs(args[0]);
  if (name == "floor" && argc == 1) return floor(args[0]);
  if (name == "ceil" && argc == 1) return ceil(args[0]);
  if (name == "round" && argc == 1) return round(args[0]);
  if (name == "exp" && argc == 1) return exp(args[0]);
  if (name == "ln" && argc == 1) return log(args[0]);
  if (name == "log" && argc == 1) return log10(args[0]);
  if (name == "max" && argc == 2) return args[0] > args[1] ? args[0] : args[1];
  if (name == "min" && argc == 2) return args[0] < args[1] ? args[0] : args[1];
  if (name == "pow" && argc == 2) return pow(args[0], args[1]);
  calcFail("Unknown function: " + name);
  return 0;
}

static double calcParsePrimary() {
  CalcToken* t = calcPeek();
  if (!t) { calcFail("Unexpected end of expression"); return 0; }
  if (t->type == CT_NUM) { calcNext(); return t->num; }
  if (calcIsOp(t, '(')) {
    calcNext();
    double val = calcParseExpression();
    if (calcErr) return 0;
    calcExpectOp(')');
    return val;
  }
  if (t->type == CT_IDENT) {
    calcNext();
    String name = t->ident;
    if (calcIsOp(calcPeek(), '(')) {
      calcNext();
      double args[4];
      int argc = 0;
      if (!calcIsOp(calcPeek(), ')')) {
        args[argc++] = calcParseExpression();
        while (!calcErr && calcIsOp(calcPeek(), ',')) {
          calcNext();
          double v = calcParseExpression();
          if (argc < 4) args[argc++] = v;
        }
      }
      if (calcErr) return 0;
      calcExpectOp(')');
      if (calcErr) return 0;
      return calcCallFn(name, args, argc);
    }
    if (name == "ans") {
      if (!hasLastCalcResult) { calcFail("No previous result (ans)"); return 0; }
      return lastCalcResult;
    }
    if (name == "pi") return PI; // Arduino core defines PI
    if (name == "e") return 2.718281828459045;
    calcFail("Unknown identifier: " + name);
    return 0;
  }
  calcFail("Unexpected token");
  return 0;
}

static double calcParsePostfix() {
  double val = calcParsePrimary();
  while (!calcErr && calcIsOp(calcPeek(), '!')) {
    calcNext();
    val = calcFactorial(val);
  }
  return val;
}

static double calcParseUnary();

static double calcParsePow() {
  double base = calcParsePostfix();
  if (calcErr) return 0;
  if (calcIsOp(calcPeek(), '^')) {
    calcNext();
    double exponent = calcParseUnary(); // right-assoc: 2^3^2 == 2^(3^2)
    if (calcErr) return 0;
    return pow(base, exponent);
  }
  return base;
}

static double calcParseUnary() {
  if (calcIsOp(calcPeek(), '-') || calcIsOp(calcPeek(), '+')) {
    char op = calcNext()->op;
    double val = calcParseUnary();
    if (calcErr) return 0;
    return (op == '-') ? -val : val;
  }
  return calcParsePow();
}

static double calcParseMulDiv() {
  double left = calcParseUnary();
  while (!calcErr && (calcIsOp(calcPeek(), '*') || calcIsOp(calcPeek(), '/') || calcIsOp(calcPeek(), '%'))) {
    char op = calcNext()->op;
    double right = calcParseUnary();
    if (calcErr) return 0;
    if (op == '*') left *= right;
    else if (op == '/') { if (right == 0) { calcFail("Division by zero"); return 0; } left /= right; }
    else left = fmod(left, right);
  }
  return left;
}

static double calcParseAddSub() {
  double left = calcParseMulDiv();
  while (!calcErr && (calcIsOp(calcPeek(), '+') || calcIsOp(calcPeek(), '-'))) {
    char op = calcNext()->op;
    double right = calcParseMulDiv();
    if (calcErr) return 0;
    left = (op == '+') ? left + right : left - right;
  }
  return left;
}

static double calcParseExpression() { return calcParseAddSub(); }

static String calcFormatResult(double n) {
  if (isnan(n)) return "Err (not a number)";
  if (isinf(n)) return (n > 0) ? "Infinity" : "-Infinity";
  if (fabs(n - round(n)) < 1e-9 && fabs(n) < 1e15) return String((long long)round(n));
  String s = String(n, 8);
  while (s.endsWith("0")) s.remove(s.length() - 1);
  if (s.endsWith(".")) s.remove(s.length() - 1);
  return s;
}

String evalMathExprToString(const String& expr) {
  calcTokenize(expr);
  if (!calcErr) {
    calcPos = 0;
    double result = calcParseExpression();
    if (!calcErr && calcPos < calcTokenCount) calcFail("Unexpected trailing input");
    if (!calcErr) {
      lastCalcResult = result;
      hasLastCalcResult = true;
      return calcFormatResult(result);
    }
  }
  return "Error: " + calcErrMsg;
}

void calcRun(const String& args) {
  if (args.length() == 0) {
    termPrintln("Usage: calc <expression>", COLOR_DIM);
    return;
  }
  String result = evalMathExprToString(args);
  bool isErr = result.startsWith("Error");
  termPrintln(args + " = " + result, isErr ? COLOR_DANGER : COLOR_TEXT);
}

// ── calc's keypad extension (touch builds the expression directly
// into the bar -- the keypad has no state of its own) ─────────────────
struct CalcKey { const char* label; const char* insert; };
static const CalcKey CALC_KEYS[25] = {
  { "7", "7" }, { "8", "8" }, { "9", "9" }, { "(", "(" }, { ")", ")" },
  { "4", "4" }, { "5", "5" }, { "6", "6" }, { "^", "^" }, { "!", "!" },
  { "1", "1" }, { "2", "2" }, { "3", "3" }, { "*", "*" }, { "/", "/" },
  { "0", "0" }, { ".", "." }, { "-", "-" }, { "+", "+" }, { "%", "%" },
  { "sqrt", "sqrt(" }, { "sin", "sin(" }, { "cos", "cos(" }, { "pi", "pi" }, { "ans", "ans" },
};
static Button calcKeyButtons[25];

void drawCalcExtension(int16_t x, int16_t y, int16_t w, int16_t h) {
  const int16_t cols = 5, rows = 5;
  int16_t colW = w / cols;
  int16_t rowH = h / rows;
  for (int i = 0; i < 25; i++) {
    int16_t col = i % cols, row = i / cols;
    calcKeyButtons[i] = { (int16_t)(x + col * colW), (int16_t)(y + row * rowH), colW, rowH, CALC_KEYS[i].label, (uint8_t)i };
    drawButton(calcKeyButtons[i]);
  }
}

void handleCalcExtensionTouch(int16_t x, int16_t y) {
  int idx = hitTestButtons(calcKeyButtons, 25, x, y);
  if (idx < 0) return;
  playClick();
  barAppendText(CALC_KEYS[idx].insert);
}

#endif // UJI_M5_CALC_H
