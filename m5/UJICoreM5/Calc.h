/*
 * Calc.h — the core arithmetic engine ported from UJICore.js's
 * evalMathExpr(): tokenizer + recursive-descent parser with real
 * precedence (+ - | * / % | unary | ^ right-assoc | postfix ! | primary),
 * functions, constants, and "ans" memory. Unit conversion is still not
 * ported (needs free-text unit names, not a great keypad fit). Algebra
 * solve (calc solve <eqn>[, <eqn2>]) *is* now ported -- see the "algebra
 * solver" section below, a hand-rolled (no <regex> on this toolchain)
 * equivalent of UJICore.js's parsePolyTerms()/solveSingleEquation()/
 * solveSystem(). The calc keypad gained a second page (x/y/=/, and a
 * "solve" key) so equations can actually be typed without a keyboard --
 * see CALC_KEYS_NUM/CALC_KEYS_ALG below.
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

// ── algebra solver (calc solve), ported from UJICore.js's parsePolyTerms()/
// equationToPoly()/solveSingleEquation()/solveSystem(). Same restricted
// grammar as the original -- plain polynomial terms only (3x^2, -5x, 7),
// not the full expression grammar above -- hand-rolled instead of regex
// since this toolchain doesn't carry <regex>.
struct PolyTerm { int sign; double coeff; char varName; int exp; }; // varName == 0 means "no variable, this is a constant term"

#define POLY_MAX_TERMS 16

// Parses one side of an equation (no "=") into polynomial terms, e.g.
// "3x^2-5x+6" -> [{+1,3,'x',2},{-1,5,'x',1},{+1,6,0,0}]. Sets calcErr (via
// calcFail()) and returns 0 on anything it can't make sense of.
static uint8_t calcParsePolyTerms(String side, PolyTerm* out, uint8_t maxOut) {
  side.trim();
  uint8_t count = 0;
  int i = 0, n = side.length();
  while (i < n) {
    while (i < n && isspace(side[i])) i++;
    if (i >= n) break;

    int sign = 1;
    if (side[i] == '+' || side[i] == '-') { sign = (side[i] == '-') ? -1 : 1; i++; }
    while (i < n && isspace(side[i])) i++;

    int coeffStart = i;
    while (i < n && (isdigit(side[i]) || side[i] == '.')) i++;
    bool hasCoeff = i > coeffStart;
    double coeff = hasCoeff ? side.substring(coeffStart, i).toDouble() : 1.0;

    while (i < n && isspace(side[i])) i++;
    if (i < n && side[i] == '*') { i++; while (i < n && isspace(side[i])) i++; }

    char varName = 0;
    if (i < n && isalpha(side[i])) { varName = (char)tolower(side[i]); i++; }

    if (!hasCoeff && varName == 0) { calcFail("Couldn't parse \"" + side.substring(coeffStart) + "\" as a term"); return 0; }

    while (i < n && isspace(side[i])) i++;
    int exp = varName ? 1 : 0;
    if (i < n && side[i] == '^') {
      i++;
      while (i < n && isspace(side[i])) i++;
      int expStart = i;
      while (i < n && isdigit(side[i])) i++;
      if (i == expStart) { calcFail("Couldn't parse \"" + side.substring(coeffStart) + "\" as a term"); return 0; }
      exp = side.substring(expStart, i).toInt();
    }
    while (i < n && isspace(side[i])) i++;

    if (count >= maxOut) { calcFail("Too many terms"); return 0; }
    out[count++] = PolyTerm{ sign, coeff, varName, exp };

    if (i < n && side[i] != '+' && side[i] != '-') { calcFail("Couldn't parse \"" + side.substring(i) + "\" as a term"); return 0; }
  }
  return count;
}

// Combines both sides (RHS negated) into a single term list, equivalent to
// moving everything to "lhs - rhs = 0" form.
static uint8_t calcEquationToPoly(const String& eqStr, PolyTerm* out, uint8_t maxOut) {
  int eq = eqStr.indexOf('=');
  if (eq == -1) { calcFail("Equation must contain \"=\""); return 0; }
  String lhs = eqStr.substring(0, eq);
  String rhs = eqStr.substring(eq + 1);

  PolyTerm lhsTerms[POLY_MAX_TERMS];
  uint8_t lhsCount = calcParsePolyTerms(lhs, lhsTerms, POLY_MAX_TERMS);
  if (calcErr) return 0;
  PolyTerm rhsTerms[POLY_MAX_TERMS];
  uint8_t rhsCount = calcParsePolyTerms(rhs, rhsTerms, POLY_MAX_TERMS);
  if (calcErr) return 0;

  uint8_t count = 0;
  for (uint8_t i = 0; i < lhsCount && count < maxOut; i++) out[count++] = lhsTerms[i];
  for (uint8_t i = 0; i < rhsCount && count < maxOut; i++) {
    PolyTerm t = rhsTerms[i];
    t.sign *= -1;
    out[count++] = t;
  }
  return count;
}

enum SolveResultType { SOLVE_IDENTITY, SOLVE_NONE, SOLVE_LINEAR, SOLVE_REAL, SOLVE_COMPLEX, SOLVE_SYSTEM };
struct SolveResult {
  SolveResultType type;
  char varName;
  double value;        // SOLVE_LINEAR
  double roots[2];      // SOLVE_REAL
  uint8_t rootCount;
  double reIm[2][2];    // SOLVE_COMPLEX: [root][re=0/im=1]
  char sysVar[2];        // SOLVE_SYSTEM
  double sysVal[2];
};

static SolveResult calcSolveSingleEquation(const String& eqStr) {
  SolveResult r = {};
  PolyTerm terms[POLY_MAX_TERMS];
  uint8_t count = calcEquationToPoly(eqStr, terms, POLY_MAX_TERMS);
  if (calcErr) return r;

  char v = 0;
  bool multipleVars = false;
  for (uint8_t i = 0; i < count; i++) {
    if (terms[i].varName) {
      if (v == 0) v = terms[i].varName;
      else if (v != terms[i].varName) multipleVars = true;
    }
  }
  if (v == 0) { calcFail("No variable found to solve for"); return r; }
  if (multipleVars) { calcFail("solve only supports one variable per equation (use a comma-separated pair for a 2-variable system)"); return r; }

  double a = 0, b = 0, c = 0;
  for (uint8_t i = 0; i < count; i++) {
    double val = terms[i].sign * terms[i].coeff;
    if (terms[i].varName == 0) c += val;
    else if (terms[i].exp == 1) b += val;
    else if (terms[i].exp == 2) a += val;
    else { calcFail("Only linear and quadratic (degree <= 2) terms are supported"); return r; }
  }

  r.varName = v;
  if (a == 0) {
    if (b == 0) { r.type = (c == 0) ? SOLVE_IDENTITY : SOLVE_NONE; return r; }
    r.type = SOLVE_LINEAR;
    r.value = -c / b;
    return r;
  }
  double disc = b * b - 4 * a * c;
  if (disc > 0) {
    double sq = sqrt(disc);
    r.type = SOLVE_REAL;
    r.roots[0] = (-b + sq) / (2 * a);
    r.roots[1] = (-b - sq) / (2 * a);
    r.rootCount = 2;
  } else if (disc == 0) {
    r.type = SOLVE_REAL;
    r.roots[0] = -b / (2 * a);
    r.rootCount = 1;
  } else {
    double sq = sqrt(-disc);
    r.type = SOLVE_COMPLEX;
    double re = -b / (2 * a), im = sq / (2 * a);
    r.reIm[0][0] = re; r.reIm[0][1] = im;
    r.reIm[1][0] = re; r.reIm[1][1] = -im;
  }
  return r;
}

struct LinearCoeffs { char vars[2]; double coeffs[2]; uint8_t varCount; double constant; };

static LinearCoeffs calcEquationToLinearCoeffs(const String& eqStr) {
  LinearCoeffs lc = {};
  PolyTerm terms[POLY_MAX_TERMS];
  uint8_t count = calcEquationToPoly(eqStr, terms, POLY_MAX_TERMS);
  if (calcErr) return lc;
  for (uint8_t i = 0; i < count; i++) {
    if (terms[i].exp > 1) { calcFail("Only linear and quadratic (degree <= 2) terms are supported"); return lc; }
    double val = terms[i].sign * terms[i].coeff;
    if (terms[i].varName == 0) { lc.constant += val; continue; }
    int idx = -1;
    for (uint8_t k = 0; k < lc.varCount; k++) if (lc.vars[k] == terms[i].varName) { idx = k; break; }
    if (idx == -1) {
      if (lc.varCount >= 2) { calcFail("A 2-equation system must use exactly two variables total"); return lc; }
      idx = lc.varCount++;
      lc.vars[idx] = terms[i].varName;
      lc.coeffs[idx] = 0;
    }
    lc.coeffs[idx] += val;
  }
  return lc;
}

static double calcCoeffOf(const LinearCoeffs& e, char v) {
  for (uint8_t i = 0; i < e.varCount; i++) if (e.vars[i] == v) return e.coeffs[i];
  return 0;
}

static SolveResult calcSolveSystem(const String& eq1Str, const String& eq2Str) {
  SolveResult r = {};
  r.type = SOLVE_SYSTEM;
  LinearCoeffs e1 = calcEquationToLinearCoeffs(eq1Str);
  if (calcErr) return r;
  LinearCoeffs e2 = calcEquationToLinearCoeffs(eq2Str);
  if (calcErr) return r;

  char vars[2];
  uint8_t varCount = 0;
  for (uint8_t i = 0; i < e1.varCount; i++) vars[varCount++] = e1.vars[i];
  for (uint8_t i = 0; i < e2.varCount; i++) {
    bool found = false;
    for (uint8_t k = 0; k < varCount; k++) if (vars[k] == e2.vars[i]) found = true;
    if (!found) {
      if (varCount >= 2) { calcFail("A 2-equation system must use exactly two variables total"); return r; }
      vars[varCount++] = e2.vars[i];
    }
  }
  if (varCount != 2) { calcFail("A 2-equation system must use exactly two variables total"); return r; }

  char v1 = vars[0], v2 = vars[1];
  double a1 = calcCoeffOf(e1, v1), b1 = calcCoeffOf(e1, v2), k1 = -e1.constant;
  double a2 = calcCoeffOf(e2, v1), b2 = calcCoeffOf(e2, v2), k2 = -e2.constant;
  double det = a1 * b2 - a2 * b1;
  if (det == 0) { calcFail("This system has no unique solution (equations are parallel or identical)"); return r; }

  r.sysVar[0] = v1; r.sysVar[1] = v2;
  r.sysVal[0] = (k1 * b2 - k2 * b1) / det;
  r.sysVal[1] = (a1 * k2 - a2 * k1) / det;
  return r;
}

static String calcFormatComplex(double re, double im) {
  return calcFormatResult(re) + (im < 0 ? " - " : " + ") + calcFormatResult(fabs(im)) + "i";
}

static void calcPrintSolveResult(const SolveResult& r) {
  if (calcErr) { termPrintln("Error: " + calcErrMsg, COLOR_DANGER); return; }
  switch (r.type) {
    case SOLVE_IDENTITY:
      termPrintln("True for all values (identity)", COLOR_TEXT);
      return;
    case SOLVE_NONE:
      termPrintln("No solution", COLOR_TEXT);
      return;
    case SOLVE_LINEAR:
      termPrintln(String(r.varName) + " = " + calcFormatResult(r.value), COLOR_TEXT);
      return;
    case SOLVE_REAL: {
      String text = (r.rootCount == 1)
        ? calcFormatResult(r.roots[0])
        : calcFormatResult(r.roots[0]) + " or " + calcFormatResult(r.roots[1]);
      termPrintln(String(r.varName) + " = " + text, COLOR_TEXT);
      return;
    }
    case SOLVE_COMPLEX: {
      String text = calcFormatComplex(r.reIm[0][0], r.reIm[0][1]) + " or " + calcFormatComplex(r.reIm[1][0], r.reIm[1][1]);
      termPrintln(String(r.varName) + " = " + text, COLOR_TEXT);
      return;
    }
    case SOLVE_SYSTEM:
      termPrintln(String(r.sysVar[0]) + " = " + calcFormatResult(r.sysVal[0]) + ", " +
                  String(r.sysVar[1]) + " = " + calcFormatResult(r.sysVal[1]), COLOR_TEXT);
      return;
  }
}

// Splits "eq1" or "eq1, eq2" (at most one comma, same limit as UJICore.js's
// cmdCalcSolve()) and dispatches to the single-equation or 2-variable
// system solver.
static void calcSolveRun(const String& rest) {
  calcErr = false;
  calcErrMsg = "";

  int comma = rest.indexOf(',');
  if (comma == -1) {
    String eq = rest;
    eq.trim();
    if (eq.length() == 0) { termPrintln("Usage: calc solve <equation>[, <equation2>]", COLOR_DIM); return; }
    calcPrintSolveResult(calcSolveSingleEquation(eq));
    return;
  }
  String eq1 = rest.substring(0, comma);
  eq1.trim();
  String remainder = rest.substring(comma + 1);
  if (remainder.indexOf(',') != -1) {
    termPrintln("solve supports at most two comma-separated equations", COLOR_DANGER);
    return;
  }
  String eq2 = remainder;
  eq2.trim();
  calcPrintSolveResult(calcSolveSystem(eq1, eq2));
}

void calcRun(const String& args) {
  if (args.length() == 0) {
    termPrintln("Usage: calc <expression> or calc solve <equation>[, <equation2>]", COLOR_DIM);
    return;
  }
  String lower = args;
  lower.toLowerCase();
  if (lower.startsWith("solve ") || lower == "solve") {
    String rest = args.substring(5);
    rest.trim();
    calcSolveRun(rest);
    return;
  }
  String result = evalMathExprToString(args);
  bool isErr = result.startsWith("Error");
  termPrintln(args + " = " + result, isErr ? COLOR_DANGER : COLOR_TEXT);
}

// ── calc's keypad extension (touch builds the expression directly
// into the bar -- the keypad has no state of its own besides which page
// is showing). Two pages: arithmetic (the original 25 keys, minus "cos"
// to make room for a page toggle) and algebra (x/y/=/, plus "solve ",
// for typing equations -- there's no keyboard, so calc solve needs its
// own keypad just like plain calc does).
struct CalcKey { const char* label; const char* insert; bool toggle; };
static const CalcKey CALC_KEYS_NUM[25] = {
  { "7", "7", false }, { "8", "8", false }, { "9", "9", false }, { "(", "(", false }, { ")", ")", false },
  { "4", "4", false }, { "5", "5", false }, { "6", "6", false }, { "^", "^", false }, { "!", "!", false },
  { "1", "1", false }, { "2", "2", false }, { "3", "3", false }, { "*", "*", false }, { "/", "/", false },
  { "0", "0", false }, { ".", ".", false }, { "-", "-", false }, { "+", "+", false }, { "%", "%", false },
  { "sqrt", "sqrt(", false }, { "sin", "sin(", false }, { "pi", "pi", false }, { "ans", "ans", false }, { "ALG", nullptr, true },
};
static const CalcKey CALC_KEYS_ALG[25] = {
  { "7", "7", false }, { "8", "8", false }, { "9", "9", false }, { "x", "x", false }, { "y", "y", false },
  { "4", "4", false }, { "5", "5", false }, { "6", "6", false }, { "^", "^", false }, { "=", "=", false },
  { "1", "1", false }, { "2", "2", false }, { "3", "3", false }, { "(", "(", false }, { ")", ")", false },
  { "0", "0", false }, { ".", ".", false }, { "-", "-", false }, { "+", "+", false }, { ",", ",", false },
  { "solve", "solve ", false }, { "ans", "ans", false }, { "pi", "pi", false }, { "sqrt", "sqrt(", false }, { "123", nullptr, true },
};
static Button calcKeyButtons[25];
static uint8_t calcKeypadPage = 0; // 0 = arithmetic (CALC_KEYS_NUM), 1 = algebra (CALC_KEYS_ALG)

// Called whenever the calc extension is freshly armed (TouchMenu.h) so a
// previous session's algebra page doesn't carry over silently.
void calcResetKeypadPage() {
  calcKeypadPage = 0;
}

static const CalcKey* calcActiveKeys() {
  return (calcKeypadPage == 0) ? CALC_KEYS_NUM : CALC_KEYS_ALG;
}

void drawCalcExtension(int16_t x, int16_t y, int16_t w, int16_t h) {
  const CalcKey* keys = calcActiveKeys();
  const int16_t cols = 5, rows = 5;
  int16_t colW = w / cols;
  int16_t rowH = h / rows;
  for (int i = 0; i < 25; i++) {
    int16_t col = i % cols, row = i / cols;
    calcKeyButtons[i] = { (int16_t)(x + col * colW), (int16_t)(y + row * rowH), colW, rowH, keys[i].label, (uint8_t)i };
    drawButton(calcKeyButtons[i], keys[i].toggle);
  }
}

void handleCalcExtensionTouch(int16_t x, int16_t y) {
  int idx = hitTestButtons(calcKeyButtons, 25, x, y);
  if (idx < 0) return;
  playClick();
  const CalcKey& key = calcActiveKeys()[idx];
  if (key.toggle) {
    calcKeypadPage = (calcKeypadPage == 0) ? 1 : 0;
    drawOverlay(); // repaint the keypad with the other page's labels
    return;
  }
  barAppendText(key.insert);
}

#endif // UJI_M5_CALC_H
