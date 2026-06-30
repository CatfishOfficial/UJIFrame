/*
 * Weather.h — Open-Meteo (api.open-meteo.com), free, no API key for
 * non-commercial use, no signup. Replaces an earlier wttr.in-based
 * version that turned out unreliable in practice (occasional fetch
 * failures, inaccurate readings) -- Open-Meteo aggregates real
 * meteorological models (NOAA/DWD/etc.) behind production-grade
 * infrastructure rather than wttr.in's single-host text-formatting layer.
 *
 * Coordinate-based, not IP-geolocated -- LocationSecrets.h (gitignored,
 * see .example) holds a fixed LOCATION_LAT/LOCATION_LON/LOCATION_NAME.
 * This device doesn't move, so a compile-time location is one less
 * thing that can fail at request time, not a downgrade from wttr.in's
 * automatic IP geolocation.
 *
 * Response is parsed with plain string scanning (extractJsonNumber()),
 * not ArduinoJson, matching this project's existing precedent (Http.h's
 * callers) of not pulling in a JSON library for a handful of flat
 * numeric fields. weatherCodeToText() maps Open-Meteo's weather_code
 * (WMO code table 4677) to a short ASCII description.
 */
#ifndef UJI_M5_WEATHER_H
#define UJI_M5_WEATHER_H
#include "Globals.h"
#include <WiFi.h>
#include <math.h>
#include "LocationSecrets.h"

// Scans for "key":<number> in a raw JSON body and returns the number, or
// fallback if the key isn't found -- handles ints and floats alike via
// toFloat(). Good enough for Open-Meteo's flat "current" object; not a
// general JSON parser.
//
// Real bug found here (confirmed with a live request, not assumed):
// Open-Meteo's response has a "current_units" object listed *before*
// "current", and it repeats every field name with a string value, e.g.
// {"current_units":{"temperature_2m":"°F","weather_code":"wmo code"},
//  "current":{"temperature_2m":97.5,"weather_code":2}}
// A plain indexOf("\"key\":") matched the *units* entry first every time
// (a string, not a number), the digit-scan immediately hit the opening
// quote and bailed with 0 chars scanned, and every field silently fell
// back to its fallback value -- weather always showed "Unknown, 0F".
// Fixed by scoping the search to start at "current":{ (current_units
// always precedes current, and nothing meaningful follows current in
// this API's response shape, so slicing from there to the end is safe).
static float extractJsonNumber(const String& body, const char* key, float fallback) {
  int objStart = body.indexOf("\"current\":{");
  String scope = (objStart >= 0) ? body.substring(objStart) : body;
  String needle = String("\"") + key + "\":";
  int idx = scope.indexOf(needle);
  if (idx == -1) return fallback;
  int start = idx + needle.length();
  int end = start;
  while (end < (int)scope.length() && (isDigit(scope[end]) || scope[end] == '-' || scope[end] == '.')) end++;
  if (end == start) return fallback;
  return scope.substring(start, end).toFloat();
}

// Open-Meteo's weather_code follows WMO code table 4677 -- this covers
// the common groupings, not every individual code.
struct WmoCodeEntry { uint8_t code; const char* text; };
static const WmoCodeEntry WMO_CODES[] = {
  { 0, "Clear" }, { 1, "Mainly clear" }, { 2, "Partly cloudy" }, { 3, "Cloudy" },
  { 45, "Fog" }, { 48, "Fog" },
  { 51, "Light drizzle" }, { 53, "Drizzle" }, { 55, "Heavy drizzle" },
  { 56, "Freezing drizzle" }, { 57, "Freezing drizzle" },
  { 61, "Light rain" }, { 63, "Rain" }, { 65, "Heavy rain" },
  { 66, "Freezing rain" }, { 67, "Freezing rain" },
  { 71, "Light snow" }, { 73, "Snow" }, { 75, "Heavy snow" }, { 77, "Snow grains" },
  { 80, "Light showers" }, { 81, "Showers" }, { 82, "Heavy showers" },
  { 85, "Snow showers" }, { 86, "Snow showers" },
  { 95, "Thunderstorm" }, { 96, "Thunderstorm" }, { 99, "Thunderstorm" },
};
#define WMO_CODE_COUNT (sizeof(WMO_CODES) / sizeof(WMO_CODES[0]))

static String weatherCodeToText(int code) {
  for (uint8_t i = 0; i < WMO_CODE_COUNT; i++) {
    if (WMO_CODES[i].code == code) return WMO_CODES[i].text;
  }
  return "Unknown";
}

// Terser one-line format for the idle-mode widgets (just condition + temp,
// e.g. "Clear, 78F") -- the full "weather" command's format below has room
// for more fields.
String fetchWeatherShort(String& errorOut) {
  if (WiFi.status() != WL_CONNECTED) {
    errorOut = "no WiFi";
    return "";
  }
  String url = String("https://api.open-meteo.com/v1/forecast?latitude=") + LOCATION_LAT +
               "&longitude=" + LOCATION_LON +
               "&current=temperature_2m,weather_code&temperature_unit=fahrenheit";
  String body = httpGet(url, errorOut);
  if (body.length() == 0) return "";

  int code = (int)extractJsonNumber(body, "weather_code", -1);
  if (code < 0) { errorOut = "bad response"; return ""; }
  float temp = extractJsonNumber(body, "temperature_2m", 0);
  return weatherCodeToText(code) + ", " + String((int)lroundf(temp)) + "F";
}

void weatherRun(const String& args) {
  if (WiFi.status() != WL_CONNECTED) {
    termPrintln("Not connected to WiFi. Run \"wifi connect\" first.", COLOR_DANGER);
    return;
  }

  termPrintln("Fetching weather...", COLOR_DIM);
  drawScrollback(); // show this before the request blocks, same as WifiCommand.h's wifiConnect()

  String url = String("https://api.open-meteo.com/v1/forecast?latitude=") + LOCATION_LAT +
               "&longitude=" + LOCATION_LON +
               "&current=temperature_2m,apparent_temperature,relative_humidity_2m,wind_speed_10m,weather_code"
               "&temperature_unit=fahrenheit&wind_speed_unit=mph";
  String err;
  String body = httpGet(url, err);
  if (body.length() == 0) {
    termPrintln("Couldn't get weather: " + err, COLOR_DANGER);
    return;
  }

  int code = (int)extractJsonNumber(body, "weather_code", -1);
  float temp = extractJsonNumber(body, "temperature_2m", 0);
  float feels = extractJsonNumber(body, "apparent_temperature", temp);
  float humidity = extractJsonNumber(body, "relative_humidity_2m", -1);
  float wind = extractJsonNumber(body, "wind_speed_10m", -1);

  String out = String(LOCATION_NAME) + ": " + weatherCodeToText(code) +
               ", " + String((int)lroundf(temp)) + "F" +
               ", feels " + String((int)lroundf(feels)) + "F";
  if (humidity >= 0) out += ", humidity " + String((int)lroundf(humidity)) + "%";
  if (wind >= 0) out += ", wind " + String((int)lroundf(wind)) + " mph";
  termPrintln(out, COLOR_TEXT);
}

#endif // UJI_M5_WEATHER_H
