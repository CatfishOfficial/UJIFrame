/*
 * Http.h — one shared HTTPS GET helper so future API-calling commands
 * don't each reimplement this. Validates the server's certificate
 * against the ESP32CertBundle library's embedded Mozilla root-CA store
 * (setCACertBundle) instead of WiFiClientSecure::setInsecure() --
 * setInsecure() encrypts the connection but never checks who's on the
 * other end, which isn't acceptable once real API keys go over this
 * path. Requires the "ESP32CertBundle" library (Arduino Library
 * Manager, or github.com/tanakamasayuki/ESP32CertBundle) to be
 * installed -- that's a one-time setup step, same idea as WifiSecrets.h.
 *
 * Currently sends Accept: application/json unconditionally since every
 * API hooked up so far wants that -- if a future API needs different
 * headers, this is the place to add a headers parameter rather than
 * each caller rolling its own HTTPClient setup.
 *
 * Real certificate validation also needs the device's clock to be
 * roughly correct (mbedTLS checks a cert's valid-date range against
 * system time) -- a freshly booted ESP32 thinks it's 1970, which makes
 * every certificate look "not valid yet" and fails the handshake before
 * any HTTP-level error can even form (HTTPClient just reports that as a
 * generic "connection refused"). ensureTimeSynced() (Clock.h) runs an
 * NTP sync once, the first time it's actually needed.
 */
#ifndef UJI_M5_HTTP_H
#define UJI_M5_HTTP_H
#include "Globals.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "esp32_cert_bundle.h"

String httpGet(const String& url, String& errorOut) {
  errorOut = "";
  if (WiFi.status() != WL_CONNECTED) {
    errorOut = "Not connected to WiFi.";
    return "";
  }
  if (!ensureTimeSynced()) {
    errorOut = "Couldn't sync time (needed to check the server's certificate).";
    return "";
  }

  // Heap-allocated, not a local WiFiClientSecure -- it embeds a sizeable
  // mbedTLS context internally, too large to risk on the (small, fixed)
  // task stack.
  WiFiClientSecure* client = new WiFiClientSecure();
  client->setCACertBundle(x509_crt_bundle, x509_crt_bundle_len);

  HTTPClient http;
  http.setTimeout(8000);
  if (!http.begin(*client, url)) {
    errorOut = "Could not start request.";
    delete client;
    return "";
  }
  http.addHeader("Accept", "application/json");
  http.addHeader("User-Agent", "UJI-M5 (github.com/CatfishOfficial/UJIFrame)");

  int code = http.GET();
  String body;
  if (code <= 0) {
    errorOut = "Request failed: " + http.errorToString(code) + ".";
  } else if (code != 200) {
    errorOut = "HTTP " + String(code) + ".";
  } else {
    body = http.getString();
  }

  http.end();
  delete client;
  return body;
}

#endif // UJI_M5_HTTP_H
