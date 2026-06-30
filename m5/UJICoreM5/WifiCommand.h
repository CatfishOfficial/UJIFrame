/*
 * WifiCommand.h — WiFi connection management. Credentials are compiled in
 * via WifiSecrets.h (gitignored; copy WifiSecrets.h.example to
 * WifiSecrets.h and fill in your real networks) -- no on-device typing
 * of a WPA2 password, since there's no keyboard yet.
 *
 * Multiple networks are supported: WifiSecrets.h's WIFI_NETWORKS[] is
 * tried in order until one associates, except the most recently connected
 * one (persisted in CONFIG.lastWifiSsid) is always tried first.
 */
#ifndef UJI_M5_WIFICOMMAND_H
#define UJI_M5_WIFICOMMAND_H
#include "Globals.h"
#include <WiFi.h>
#include "WifiSecrets.h"

const char* const WIFI_SUBACTIONS[WIFI_SUBACTION_COUNT] = { "status", "connect", "disconnect", "scan" };

static String wifiStatusText() {
  if (WiFi.status() == WL_CONNECTED) {
    return "Connected to " + WiFi.SSID() + "\nIP: " + WiFi.localIP().toString() +
           "\nSignal: " + String(WiFi.RSSI()) + " dBm";
  }
  return "Not connected.";
}

// The most-recently-connected network is always tried first (persisted in
// CONFIG.lastWifiSsid across reboots); the rest follow in WifiSecrets.h
// order. Manual `wifi connect` walks this order blocking; boot autoconnect
// walks it in the background via wifiTick() so it never holds up boot.
#define WIFI_CONNECT_TIMEOUT_MS 10000UL // per-network association wait

// Fills order[] (length WIFI_NETWORK_COUNT) with indices into
// WIFI_NETWORKS[], the last-connected SSID first, then the rest in order.
static void wifiBuildOrder(uint8_t order[WIFI_NETWORK_COUNT]) {
  uint8_t n = 0;
  int first = -1;
  for (uint8_t i = 0; i < WIFI_NETWORK_COUNT; i++) {
    if (CONFIG.lastWifiSsid.length() && CONFIG.lastWifiSsid == WIFI_NETWORKS[i].ssid) { first = i; break; }
  }
  if (first >= 0) order[n++] = (uint8_t)first;
  for (uint8_t i = 0; i < WIFI_NETWORK_COUNT; i++)
    if ((int)i != first) order[n++] = i;
}

// Records the network we just associated with (so it's first next time)
// and kicks off the NTP sync the rest of the system relies on. Silent --
// safe to call from the background tick without disturbing the screen.
static void wifiNoteConnected() {
  if (CONFIG.lastWifiSsid != WiFi.SSID()) {
    CONFIG.lastWifiSsid = WiFi.SSID();
    saveConfig();
  }
  // Kicked off here so it's likely already done by the time something
  // needs it (the time command, or an HTTPS request's cert check) --
  // Clock.h's ensureTimeSynced() is the fallback if it isn't.
  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  applyTimezone(); // configTime() just reset TZ to UTC internally -- put the chosen zone back
}

// ── background (non-blocking) boot failover, advanced by wifiTick() ──
static bool wifiAutoActive = false;
static uint8_t wifiAutoOrder[WIFI_NETWORK_COUNT];
static uint8_t wifiAutoPos = 0;
static unsigned long wifiAutoAttemptMs = 0;

// Blocking single-network attempt (manual connect path); true on success.
static bool wifiTryNetwork(const WifiNetwork& net) {
  WiFi.begin(net.ssid, net.password);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_TIMEOUT_MS) {
    delay(250);
  }
  return WiFi.status() == WL_CONNECTED;
}

static void wifiConnect() {
  if (WiFi.status() == WL_CONNECTED) {
    termPrintln("Already connected to " + WiFi.SSID() + ".", COLOR_TEXT);
    return;
  }
  wifiAutoActive = false; // an explicit connect supersedes any background attempt
  WiFi.mode(WIFI_STA);

  uint8_t order[WIFI_NETWORK_COUNT];
  wifiBuildOrder(order);

  for (uint8_t i = 0; i < WIFI_NETWORK_COUNT; i++) {
    const WifiNetwork& net = WIFI_NETWORKS[order[i]];
    termPrintln(String("Connecting to ") + net.ssid + "...", COLOR_DIM);
    drawScrollback(); // force this to show now, not after the blocking wait below
    if (wifiTryNetwork(net)) {
      termPrintln("Connected. IP: " + WiFi.localIP().toString(), COLOR_TEXT);
      wifiNoteConnected();
      return;
    }
    termPrintln(String("  couldn't reach ") + net.ssid + ".", COLOR_DIM);
  }
  termPrintln("Failed to connect to any configured network.", COLOR_DANGER);
}

static void wifiDisconnect() {
  wifiAutoActive = false; // don't let the background failover immediately reconnect
  WiFi.disconnect();
  termPrintln("Disconnected.", COLOR_DIM);
}

static void wifiScan() {
  termPrintln("Scanning...", COLOR_DIM);
  drawScrollback(); // same reasoning as wifiConnect() -- show this before the scan blocks

  int n = WiFi.scanNetworks();
  if (n <= 0) {
    termPrintln("No networks found.", COLOR_DIM);
    return;
  }
  String out;
  for (int i = 0; i < n; i++) {
    out += WiFi.SSID(i) + "  " + String(WiFi.RSSI(i)) + "dBm\n";
  }
  termPrintln(out, COLOR_TEXT);
  WiFi.scanDelete();
}

void wifiRun(const String& args) {
  String action = args;
  action.trim();
  action.toLowerCase();

  if (action.length() == 0 || action == "status") termPrintln(wifiStatusText(), COLOR_TEXT);
  else if (action == "connect") wifiConnect();
  else if (action == "disconnect") wifiDisconnect();
  else if (action == "scan") wifiScan();
  else termPrintln("Usage: wifi <status|connect|disconnect|scan>", COLOR_DIM);
}

// Called once from setup() when "config autoconnect on" is set. Kicks off
// a NON-blocking failover: begin the first network now, then wifiTick()
// rolls to the next if it doesn't associate within the timeout -- so boot
// is never held up waiting on WiFi. "wifi status" reports whatever it
// settles on.
void wifiAutoConnectIfEnabled() {
  if (!CONFIG.wifiAutoConnect || WIFI_NETWORK_COUNT == 0) return;
  WiFi.mode(WIFI_STA);
  wifiBuildOrder(wifiAutoOrder);
  wifiAutoPos = 0;
  wifiAutoActive = true;
  wifiAutoAttemptMs = millis();
  WiFi.begin(WIFI_NETWORKS[wifiAutoOrder[0]].ssid, WIFI_NETWORKS[wifiAutoOrder[0]].password);
  configTime(0, 0, "pool.ntp.org", "time.nist.gov"); // SNTP client; syncs once any connection completes
  applyTimezone(); // configTime() just reset TZ to UTC internally -- put the chosen zone back
}

// Called every loop(). No-op unless a background autoconnect is in flight.
// On success it records the network and stops; on a per-network timeout it
// rolls to the next (wrapping around), so it keeps cycling the configured
// networks until one comes up or the user disconnects -- without ever
// blocking the loop. A manual `wifi connect`/`disconnect` clears the flag.
void wifiTick() {
  if (!wifiAutoActive) return;
  if (WiFi.status() == WL_CONNECTED) {
    wifiAutoActive = false;
    wifiNoteConnected();
    return;
  }
  if (millis() - wifiAutoAttemptMs < WIFI_CONNECT_TIMEOUT_MS) return;
  wifiAutoPos = (uint8_t)((wifiAutoPos + 1) % WIFI_NETWORK_COUNT);
  wifiAutoAttemptMs = millis();
  const WifiNetwork& net = WIFI_NETWORKS[wifiAutoOrder[wifiAutoPos]];
  WiFi.begin(net.ssid, net.password);
}

#endif // UJI_M5_WIFICOMMAND_H
