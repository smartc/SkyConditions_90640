#ifndef HTML_TEMPLATES_H
#define HTML_TEMPLATES_H

// Page templates for the port-80 Web UI.
//
// Conventions (see WEB_UI_STYLE_GUIDE.md):
//  - Every page: getPageHeader() -> <h1> -> .page-sub version line -> getNavBar()
//    (nav + live status bar) -> one .card per topic.
//  - All styling lives in getCommonStyles(); pages add no colours of their own.
//  - Template strings are ASCII-only; use HTML entities in markup and \uXXXX
//    escapes in JavaScript.
//  - Live values come from the one /api/status poll in the status bar.  Cells
//    marked data-live='<key>' are filled from liveValue(<key>), the same
//    helper that renders them at page load.

#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include "config.h"
#include "../sensors/sky_sensor.h"
#include "config_store.h"
#include "../sensors/rain_sensor.h"
#include "../sensors/dht_sensor.h"
#include "../mqtt/mqtt_handler.h"
#include "web_ui_handler.h"
#include "../time/ntp_sync.h"

// Cloud cover at or above this shows "Overcast" in the status bar; below
// deviceConfig.clearCloudThreshold shows "Clear", in between "Cloudy".
#define STATUS_OVERCAST_PCT   80.0f
// A sensor that has not reported for this long is flagged in the status bar.
#define STATUS_STALE_THERMAL_MS   30000UL
#define STATUS_STALE_BRIGHT_MS    60000UL
#define STATUS_STALE_ENV_MS      120000UL
// Grace period after boot before "never reported" counts as an error.
#define STATUS_BOOT_GRACE_MS      30000UL

// ---------------------------------------------------------------------------
// Shared CSS
// ---------------------------------------------------------------------------
inline String getToggleSwitchStyles()
{
  return
    ".switch-container { display:flex; align-items:center; gap:10px; justify-content:flex-start; cursor:pointer; }\n"
    ".switch { position:relative; display:inline-block; width:46px; height:24px; flex:0 0 auto; }\n"
    ".switch input { opacity:0; width:0; height:0; }\n"
    ".slider { position:absolute; inset:0; background:#2d3561; border-radius:24px; transition:.2s; }\n"
    ".slider:before { content:''; position:absolute; width:18px; height:18px; left:3px; bottom:3px;"
                    " background:#e0e0e0; border-radius:50%; transition:.2s; }\n"
    "input:checked + .slider { background:#81c784; }\n"
    "input.danger:checked + .slider { background:#e57373; }\n"
    "input:checked + .slider:before { transform:translateX(22px); }\n"
    "input:focus-visible + .slider { outline:2px solid #74b9ff; outline-offset:2px; }\n"
    ".switch-label { font-weight:bold; }\n";
}

inline String getCommonStyles()
{
  return String(
    "* { box-sizing: border-box; }\n"
    "body { font-family: Arial, sans-serif; background:#1a1a2e; color:#e0e0e0;"
          " max-width:1040px; margin:20px auto; padding:0 16px; }\n"
    "h1, h2 { color:#a0c4ff; }\n"
    "h1 { margin:0 0 4px 0; }\n"
    "h2 { margin-top:0; }\n"
    "h3 { color:#81c784; margin:22px 0 10px 0; }\n"
    ".page-sub { margin:0 0 14px 0; color:#8899aa; font-size:0.9em; }\n"
    "a { color:#74b9ff; text-decoration:none; }\n"
    "a:hover { color:#a0c4ff; text-decoration:underline; }\n"
    "code { color:#a0c4ff; }\n"
    ".muted { color:#8899aa; font-size:0.85em; }\n"
    ".warn-text { color:#ffb74d; }\n"
    ".bad-text { color:#e57373; }\n"
    ".card { background:#16213e; border-radius:8px; padding:20px; margin-bottom:16px;"
           " box-shadow:0 2px 8px rgba(0,0,0,0.4); }\n"

    // Nav + status bar
    ".nav { display:flex; flex-wrap:wrap; gap:8px; margin-bottom:12px; }\n"
    ".nav a { background:#0f3460; color:#e0e0e0; padding:8px 14px; border-radius:4px; }\n"
    ".nav a:hover { background:#16213e; color:#e0e0e0; text-decoration:none; }\n"
    ".nav a.warn { background:#7f4f00; }\n"
    ".nav a.warn:hover { background:#a06200; }\n"
    ".status-card { padding:10px 16px; }\n"
    ".status-bar { display:flex; flex-wrap:wrap; gap:6px 20px; font-size:0.9em; align-items:center; }\n"
    ".status-label { color:#8899aa; }\n"
    ".status-error { color:#e57373; margin-top:6px; font-size:0.9em; }\n"

    // Tables
    "table { border-collapse:collapse; width:100%; max-width:860px; }\n"
    "table, th, td { border:1px solid #2d3561; }\n"
    "th, td { padding:8px; text-align:left; }\n"
    "th { background-color:#0f3460; }\n"
    "table.status-table { max-width:720px; }\n"
    ".status-table th { width:40%; font-weight:normal; }\n"
    "table.status-table td { position:relative; padding-left:30px; }\n"
    "table.status-table .status-indicator { position:absolute; left:10px; top:50%; margin:-6px 0 0 0; }\n"

    // Status dots
    ".status-indicator { display:inline-block; width:12px; height:12px; border-radius:50%;"
                       " margin-right:6px; vertical-align:middle; }\n"
    ".status-indicator.green  { background:#81c784; }\n"
    ".status-indicator.red    { background:#e57373; }\n"
    ".status-indicator.blue   { background:#64b5f6; }\n"
    ".status-indicator.orange { background:#ffb74d; }\n"
    ".status-indicator.gray   { background:#78909c; }\n"
    ".status-indicator.blink  { animation:blink 1s infinite alternate; }\n"
    "@keyframes blink { from { opacity:1; } to { opacity:0.25; } }\n"

    // Buttons
    "button, input[type=submit], .button { display:inline-block; background:#74b9ff; color:#1a1a2e;"
          " border:none; border-radius:4px; padding:8px 16px; font-size:0.95em; font-weight:bold;"
          " font-family:inherit; cursor:pointer; }\n"
    "button:hover, input[type=submit]:hover, .button:hover { background:#a0c4ff; text-decoration:none; }\n"
    "button:disabled { opacity:0.5; cursor:default; }\n"
    ".button-danger, .button-danger:hover { background:#e57373; }\n"
    ".button-danger:hover { filter:brightness(1.15); }\n"
    ".button-row { display:flex; flex-wrap:wrap; gap:10px; align-items:center; margin-top:16px; }\n"
    // Segmented selectors (trend range, calendar mode, console controls)
    ".tbtn { background:#0f3460; color:#e0e0e0; padding:6px 12px; margin:2px; font-weight:normal;"
           " font-size:0.85em; }\n"
    ".tbtn:hover { background:#2d3561; }\n"
    ".tbtn.act { background:#74b9ff; color:#1a1a2e; }\n"

    // Forms
    "input[type=text], input[type=password], input[type=number], select {"
          " display:block; width:100%; max-width:420px; background:#0f3460; color:#e0e0e0;"
          " border:1px solid #2d3561; border-radius:4px; padding:6px 8px; font-size:0.95em;"
          " font-family:inherit; }\n"
    "input[type=number] { max-width:200px; }\n"
    "input:disabled, select:disabled { opacity:0.5; }\n"
    ".field { margin-bottom:16px; }\n"
    ".field-label { display:block; font-weight:bold; margin-bottom:4px; }\n"
    ".field small { display:block; margin-top:4px; color:#b0b0b0; font-size:0.85em; max-width:680px;"
                  " line-height:1.4; }\n"
    ".field-pair { display:flex; gap:8px; align-items:center; }\n"
    ".field-pair input { max-width:120px; }\n"
    ".saved-banner { background:#16213e; border-left:4px solid #81c784; color:#81c784;"
                   " padding:10px 16px; border-radius:4px; margin-bottom:16px; font-weight:bold; }\n"

    // Tabs
    ".tabs { display:flex; flex-wrap:wrap; gap:4px; border-bottom:2px solid #0f3460; margin-bottom:16px; }\n"
    ".tabs button { margin:0; border-radius:6px 6px 0 0; background:#16213e; color:#b0b0b0;"
                  " padding:10px 14px; font-weight:bold; }\n"
    ".tabs button:hover { background:#0f3460; }\n"
    ".tabs button.active { background:#0f3460; color:#a0c4ff; }\n"
    ".tab { display:none; }\n"
    ".tab.active { display:block; }\n"

    // Home page: reading tiles (value colours match the Trends chart series)
    ".stat-grid { display:grid; grid-template-columns:repeat(auto-fit, minmax(140px,1fr)); gap:12px; }\n"
    ".stat-box { background:#0f3460; border-radius:6px; padding:12px; text-align:center; }\n"
    ".stat-label { font-size:0.75em; color:#74b9ff; text-transform:uppercase; letter-spacing:1px; }\n"
    ".stat-sub { font-size:0.8em; color:#8899aa; text-transform:none; letter-spacing:0; }\n"
    ".stat-value { font-size:1.6em; font-weight:bold; margin-top:4px; }\n"
    ".stat-note { font-size:0.65em; color:#b0b0b0; margin-top:4px; }\n"
    ".sky-temp   { color:#a29bfe; }\n"
    ".min-temp   { color:#74b9ff; }\n"
    ".max-temp   { color:#fd79a8; }\n"
    ".med-temp   { color:#55efc4; }\n"
    ".amb-temp   { color:#ffeaa7; }\n"
    ".env-val    { color:#74b9ff; }\n"
    ".cloud-mean { color:#e0e0e0; }\n"
    ".cloud-px   { color:#b0b0b0; }\n"
    ".lux-val    { color:#fdcb6e; }\n"
    ".sqm-val    { color:#00cec9; }\n"
    ".good    { color:#81c784; }\n"
    ".bad     { color:#e57373; }\n"
    ".unknown { color:#78909c; }\n"

    // Home page: thermal view + colour bar
    ".thermal-wrap { display:flex; justify-content:center; align-items:stretch; gap:8px; }\n"
    "#thermal-canvas { display:block; min-width:0; max-width:calc(100% - 76px); height:auto;"
                     " image-rendering:pixelated; border-radius:4px; }\n"
    ".cbar { display:flex; gap:6px; flex:0 0 auto; }\n"
    ".cbar-col { display:flex; flex-direction:column; gap:4px; }\n"
    ".cbar-hot { height:14px; width:14px; border-radius:3px; background:#ff00ff; }\n"
    ".cbar-grad { flex:1; width:14px; border-radius:3px; border:1px solid #0f3460; }\n"
    ".cbar-hotlbl { height:14px; line-height:14px; font-size:0.72em; color:#b0b0b0; white-space:nowrap; }\n"
    ".cbar-ticks { flex:1; position:relative; width:56px; font-size:0.72em; color:#b0b0b0; }\n"
    ".cbar-ticks span { position:absolute; left:0; white-space:nowrap; transform:translateY(-50%); }\n"
    ".cbar-ticks span:first-child { transform:none; }\n"
    ".cbar-ticks span:last-child  { transform:translateY(-100%); }\n"
    "#ws-status { font-size:0.8em; color:#8899aa; margin-top:6px; text-align:center; }\n"
    "#hist-status { font-size:0.8em; color:#8899aa; margin-top:8px; }\n"

    // Sky History calendar (cell shades are data colours)
    ".cal-nav { display:flex; align-items:center; justify-content:center; gap:16px; margin-bottom:14px; }\n"
    ".cal-nav button { background:#0f3460; color:#e0e0e0; padding:6px 14px; font-size:1em; }\n"
    ".cal-nav button:hover:not(:disabled) { background:#2d3561; }\n"
    ".cal-nav button:disabled { opacity:0.3; }\n"
    ".cal-title { font-size:1.3em; min-width:200px; text-align:center; }\n"
    ".cal-grid { display:grid; grid-template-columns:repeat(7,1fr); gap:6px; }\n"
    ".cal-dow { text-align:center; color:#74b9ff; font-size:0.78em; text-transform:uppercase;"
               " padding-bottom:4px; }\n"
    ".cal-cell { aspect-ratio:1/1; border-radius:6px; background:#0f3460;"
                " display:flex; flex-direction:column; align-items:center; justify-content:center;"
                " font-size:0.85em; }\n"
    ".cal-cell.empty { background:transparent; }\n"
    ".cal-cell.nodata { opacity:0.35; }\n"
    ".cal-cell .d { font-weight:bold; }\n"
    ".cal-cell .dur { font-size:0.72em; margin-top:2px; color:#e0e0e0; }\n"
    ".cal-cell .dur2 { font-size:0.66em; margin-top:1px; color:#b0b0b0; }\n"
    ".cal-cell.wet1 { background:#5a2626; }\n"
    ".cal-cell.wet2 { background:#8e3a2c; }\n"
    ".cal-cell.wet3 { background:#c0392b; }\n"
    ".cal-cell.sqm1 { background:#6b4226; }\n"
    ".cal-cell.sqm2 { background:#5a5a3a; }\n"
    ".cal-cell.sqm3 { background:#2f6690; }\n"
    ".cal-cell.sqm4 { background:#1a5fb4; }\n"
    ".cal-cell.clr1 { background:#2d5016; }\n"
    ".cal-cell.clr2 { background:#3d7a1f; }\n"
    ".cal-cell.clr3 { background:#4caf50; }\n"
    ".cal-cell.today { outline:2px solid #74b9ff; outline-offset:-2px; }\n"
    ".cal-summary { margin-top:14px; text-align:center; color:#e0e0e0; font-size:1.05em; }\n"
    ".cal-legend { margin-top:10px; text-align:center; font-size:0.75em; color:#8899aa; }\n"
    ".cal-legend span { display:inline-block; margin:0 6px; }\n"
    ".cal-legend i { display:inline-block; width:10px; height:10px; border-radius:2px;"
                    " vertical-align:middle; margin-right:3px; }\n"

    // Console
    "#log { height:520px; overflow-y:auto; background:#1a1a2e; border:1px solid #2d3561;"
          " border-radius:4px; padding:10px; font-family:monospace; font-size:0.83em;"
          " line-height:1.55; white-space:pre-wrap; word-break:break-all; }\n"
    "#log .le  { margin:0; }\n"
    "#log .ts  { color:#8899aa; user-select:none; }\n"
    "#log .rep { color:#ffb74d; font-style:italic; }\n")
    + getToggleSwitchStyles();
}

// ---------------------------------------------------------------------------
// Shared page header
// ---------------------------------------------------------------------------
inline String getPageHeader(const String& title)
{
  return
    "<!DOCTYPE html><html>\n"
    "<head><title>" + title + "</title>\n"
    "<meta charset='utf-8'>\n"
    "<meta name='viewport' content='width=device-width, initial-scale=1'>\n"
    "<style>" + getCommonStyles() + "</style>\n"
    "</head>\n<body>\n";
}

// <h1> plus the version line that sits under it on every page.
inline String getPageTitle(const String& h1)
{
  return "<h1>" + h1 + "</h1>\n"
         "<p class='page-sub'>Version: " MANUFACTURER_V "</p>\n";
}

// ---------------------------------------------------------------------------
// Value formatters shared by page renders and /api/status
// ---------------------------------------------------------------------------
inline String statusDot(const char* colour, bool blink = false)
{
  return String("<span class='status-indicator ") + colour + (blink ? " blink" : "") + "'></span>";
}

// "12s ago" / "3m ago" / "2h ago" for a millis() timestamp.
inline String agoText(unsigned long lastMs)
{
  unsigned long s = (millis() - lastMs) / 1000;
  if (s < 120)  return String(s) + "s ago";
  if (s < 7200) return String(s / 60) + "m ago";
  return String(s / 3600) + "h ago";
}

inline String fmtTempC(float t, int dp = 1)
{
  if (isnan(t)) return "&mdash;";
  return String(t, dp) + " &deg;C";
}

// Adaptive lux formatter - 1 dp when >= 1 lux, more below that.
inline String fmtLux(float lux)
{
  if (lux >= 1.0f)    return String(lux, 1)  + " lux";
  if (lux >= 0.1f)    return String(lux, 3)  + " lux";
  if (lux >= 0.001f)  return String(lux, 4)  + " lux";
  char buf[24]; snprintf(buf, sizeof(buf), "%.2e lux", lux); return String(buf);
}

// Name of the configured ambient sensor (dhtType 1-5), or "" when disabled.
inline const char* envSensorName()
{
  static const char* const names[] = {"", "DHT11", "DHT22", "BMP180", "BMP280", "BME280"};
  return deviceConfig.dhtType <= 5 ? names[deviceConfig.dhtType] : "";
}

// UTC time/date from the system clock, or a dash before NTP sync.  The
// status-bar script takes over these displays (data-clock) once it has run.
inline bool clockSynced() { return time(nullptr) >= (time_t)RAIN_HIST_MIN_VALID_EPOCH; }

inline String utcClockText(bool date, int offsetMin = 0)
{
  if (!clockSynced()) return "&mdash;";
  time_t now = time(nullptr) + (time_t)offsetMin * 60;
  struct tm tmv;
  gmtime_r(&now, &tmv);
  char buf[16];
  strftime(buf, sizeof(buf), date ? "%Y-%m-%d" : "%H:%M:%S", &tmv);
  return String(buf);
}

// "UTC" or e.g. "UTC+05:30" for deviceConfig.utcOffsetMin.
inline String utcOffsetText()
{
  int m = deviceConfig.utcOffsetMin;
  if (m == 0) return "UTC";
  char buf[16];
  snprintf(buf, sizeof(buf), "UTC%c%02d:%02d", m < 0 ? '-' : '+', abs(m) / 60, abs(m) % 60);
  return String(buf);
}

// Clock display for the configured offset: data-clock kind plus its server value.
inline String clockSpan(bool date, const char* tag = "strong")
{
  bool local = deviceConfig.utcOffsetMin != 0;
  const char* kind = date ? (local ? "local-date" : "utc-date") : (local ? "local-time" : "utc-time");
  return String("<") + tag + " data-clock='" + kind + "'>" +
         utcClockText(date, deviceConfig.utcOffsetMin) + "</" + tag + ">";
}

// ---------------------------------------------------------------------------
// Sensor / connection states (dot + label).  One helper per state so the
// status bar, the Setup page and /api/status always say the same thing.
// ---------------------------------------------------------------------------
inline String skyStateHtml()
{
  if (!skyConditions.hasData()) return statusDot("gray") + "Not seen";
  float cc = skyConditions.getCloudCover();
  String pct = " (" + String(cc, 0) + "%)";
  if (cc < deviceConfig.clearCloudThreshold) return statusDot("green")  + "Clear"    + pct;
  if (cc < STATUS_OVERCAST_PCT)              return statusDot("orange") + "Cloudy"   + pct;
  return                                            statusDot("red")    + "Overcast" + pct;
}

// Rain safety verdict - the same state the UDP safety broadcast sends.
inline String rainStateHtml()
{
  if (!deviceConfig.rainEnabled) return statusDot("gray") + "No Sensor";
  if (deviceConfig.rainMode == 1 && !rainData.modbusOk)
    return rainData.lastModbusMs ? statusDot("red", true) + "Offline"
                                 : statusDot("gray") + "Not seen";
  return rainIsWet() ? statusDot("red")   + "UNSAFE (wet)"
                     : statusDot("green") + "Safe (dry)";
}

inline String ambientHtml()
{
  return skyConditions.hasData() ? fmtTempC(skyConditions.getAmbientTemperature()) : String("&mdash;");
}

inline String wifiStateHtml()
{
  if (WiFi.status() != WL_CONNECTED) return statusDot("red", true) + "Disconnected";
  int rssi = WiFi.RSSI();
  const char* c = rssi >= -67 ? "green" : (rssi >= -80 ? "orange" : "red");
  return statusDot(c) + String(rssi) + " dBm";
}

inline String ntpStateHtml()
{
  if (!deviceConfig.ntpEnabled)
    return statusDot("gray") + "Disabled" + (clockSynced() ? " (clock set)" : " (clock not set)");
  unsigned long last = ntpLastSyncMs();
  if (last == 0) return statusDot("orange") + "Waiting for first sync";
  // SNTP re-syncs hourly, so two missed rounds mean the sources aren't answering.
  if (millis() - last > 3UL * 3600000UL) return statusDot("orange") + "Last sync " + agoText(last);
  return statusDot("green") + "Synced &middot; " + agoText(last);
}

inline String mqttStateHtml()
{
  if (!deviceConfig.mqttEnabled) return statusDot("gray") + "Disabled";
  return mqttIsConnected() ? statusDot("green") + "Connected"
                           : statusDot("red", true) + "Disconnected";
}

// Shared "OK / Offline / Not seen" cell for a sensor with a last-reading time.
inline String sensorSeenHtml(bool seen, unsigned long lastMs, unsigned long staleMs)
{
  if (!seen) return statusDot("gray") + "Not seen";
  if (millis() - lastMs > staleMs)
    return statusDot("red", true) + "Offline &middot; last reading " + agoText(lastMs);
  return statusDot("green") + "OK &middot; " + agoText(lastMs);
}

inline String thermalSensorHtml()
{
  return sensorSeenHtml(skyConditions.hasData(), skyConditions.getLastUpdateMillis(),
                        STATUS_STALE_THERMAL_MS);
}

inline String brightSensorHtml()
{
  return sensorSeenHtml(skyConditions.hasBrightnessData(), skyConditions.getLastBrightnessMillis(),
                        STATUS_STALE_BRIGHT_MS);
}

inline String envSensorHtml()
{
  if (!envHasTemperature()) return statusDot("gray") + "No Sensor";
  String html = String(envSensorName()) + ": " +
                sensorSeenHtml(dhtData.lastUpdateMs != 0, dhtData.lastUpdateMs, STATUS_STALE_ENV_MS);
  return html;
}

// Red line under the status bar: anything that needs attention now.
inline String statusErrorText()
{
  String e;
  auto add = [&e](const String& s) { if (e.length()) e += " &middot; "; e += s; };
  bool pastGrace = millis() > STATUS_BOOT_GRACE_MS;

  if (!skyConditions.hasData()) {
    if (pastGrace) add("MLX90640: no thermal frames received");
  } else if (millis() - skyConditions.getLastUpdateMillis() > STATUS_STALE_THERMAL_MS) {
    add("MLX90640: last frame " + agoText(skyConditions.getLastUpdateMillis()));
  }
  if (!skyConditions.hasBrightnessData() && pastGrace)
    add("TSL2591: no brightness readings");
  if (envHasTemperature()) {
    if (dhtData.lastUpdateMs == 0) {
      if (pastGrace) add(String(envSensorName()) + ": no valid reading");
    } else if (millis() - dhtData.lastUpdateMs > STATUS_STALE_ENV_MS) {
      add(String(envSensorName()) + ": last reading " + agoText(dhtData.lastUpdateMs));
    }
  }
  if (deviceConfig.rainEnabled && deviceConfig.rainMode == 1 && !rainData.modbusOk && pastGrace)
    add("Rain sensor (RS485): not responding");
  if (deviceConfig.ntpEnabled && !clockSynced() && millis() > 2 * STATUS_BOOT_GRACE_MS)
    add("NTP: clock not set (no answer from the time sources)");
  if (isScaledBufFailed())
    add("Thermal JPEG buffer not allocated (enable PSRAM)");
  return e;
}

inline String cloudEffText()
{
  if (!skyConditions.hasData()) return "&mdash;";
  return "clear &ge; " + String(skyConditions.getCloudClearEff(), 1) +
         " &deg;C, overcast &le; " + String(skyConditions.getCloudOvercastEff(), 1) +
         " &deg;C at cloud ambient " + fmtTempC(skyConditions.getCloudAmbient());
}

// ---------------------------------------------------------------------------
// Status-bar items and data-live cells.  /api/status sends every entry, so a
// page only has to place the element.
// ---------------------------------------------------------------------------
struct StatusItem {
  const char* key;      // /api/status "bar" key; element id is "ss-<key>"
  const char* label;
  String    (*render)();
};

inline const StatusItem* statusItems(size_t& count)
{
  static const StatusItem items[] = {
    {"sky",    "Sky",     skyStateHtml},
    {"rain",   "Rain",    rainStateHtml},
    {"amb",    "Ambient", ambientHtml},
    {"wifi",   "WiFi",    wifiStateHtml},
    {"mqtt",   "MQTT",    mqttStateHtml},
  };
  count = sizeof(items) / sizeof(items[0]);
  return items;
}

inline String heapText()  { return String(ESP.getFreeHeap()) + " bytes"; }
inline String psramText() { return String(ESP.getFreePsram()) + " bytes"; }

inline const StatusItem* liveItems(size_t& count)
{
  static const StatusItem items[] = {
    {"s_thermal", "", thermalSensorHtml},
    {"s_bright",  "", brightSensorHtml},
    {"s_env",     "", envSensorHtml},
    {"s_rain",    "", rainStateHtml},
    {"ntp",       "", ntpStateHtml},
    {"cloud_eff", "", cloudEffText},
    {"heap",      "", heapText},
    {"psram",     "", psramText},
  };
  count = sizeof(items) / sizeof(items[0]);
  return items;
}

// Renders a data-live element with its current value.
inline String liveCell(const char* key, const char* tag = "span")
{
  size_t n;
  const StatusItem* items = liveItems(n);
  String v;
  for (size_t i = 0; i < n; i++)
    if (strcmp(items[i].key, key) == 0) v = items[i].render();
  return String("<") + tag + " data-live='" + key + "'>" + v + "</" + tag + ">";
}

// ---------------------------------------------------------------------------
// Status bar (part of getNavBar(), so it is on every page).  Its script is
// the page's shared poll: status items, the error line, data-live cells and
// data-clock displays all update from one fetch('/api/status') every 2 s.
// ---------------------------------------------------------------------------
inline String getStatusCard()
{
  size_t n;
  const StatusItem* items = statusItems(n);
  String html = "<div class='card status-card'><div class='status-bar'>\n";
  for (size_t i = 0; i < n; i++)
    html += String("<span><span class='status-label'>") + items[i].label +
            "</span> <strong id='ss-" + items[i].key + "'>" + items[i].render() + "</strong></span>\n";
  html += "<span><span class='status-label'>" + utcOffsetText() + "</span> " + clockSpan(false) +
          "</span>\n";
  html += "</div>\n";
  String err = statusErrorText();
  html += "<div class='status-error' id='ss-err'" + String(err.length() ? "" : " style='display:none'") +
          ">" + err + "</div>\n";
  html += "</div>\n";
  html += R"rawjs(<script>
(function () {
  // Clock: run from the browser's clock, corrected by the device's unix_ms.
  // Keep the offset from the shortest round trip (or any within 20 ms of it,
  // or a fresh one once the best is a minute old).
  // Local = UTC + the device's configured offset (total_offset, minutes).
  var off = null, bestRtt = 1e9, bestAt = 0, tzMin = 0;
  function p2(n) { return (n < 10 ? '0' : '') + n; }
  function fmtClock(kind, ms) {
    var local = kind.indexOf('local') === 0;
    var d = new Date(ms + (local ? tzMin * 60000 : 0));
    if (kind === 'utc-time' || kind === 'local-time')
      return p2(d.getUTCHours()) + ':' + p2(d.getUTCMinutes()) + ':' + p2(d.getUTCSeconds());
    if (kind === 'utc-date' || kind === 'local-date')
      return d.getUTCFullYear() + '-' + p2(d.getUTCMonth() + 1) + '-' + p2(d.getUTCDate());
    return '';
  }
  function tick() {
    var now = Date.now() + off;
    document.querySelectorAll('[data-clock]').forEach(function (el) {
      el.textContent = fmtClock(el.getAttribute('data-clock'), now);
    });
    setTimeout(tick, 1000 - now % 1000 + 5);   // just after the next second boundary
  }
  function apply(d) {
    var bar = d.bar || {};
    for (var k in bar) {
      var el = document.getElementById('ss-' + k);
      if (el) el.innerHTML = bar[k];
    }
    var live = d.live || {};
    document.querySelectorAll('[data-live]').forEach(function (el) {
      var v = live[el.getAttribute('data-live')];
      if (v !== undefined) el.innerHTML = v;
    });
    setErr(d.err || '');
  }
  function setErr(msg) {
    var el = document.getElementById('ss-err');
    if (!el) return;
    el.innerHTML = msg;
    el.style.display = msg ? '' : 'none';
  }
  function poll() {
    var t0 = Date.now();
    fetch('/api/status', { cache: 'no-store' })
      .then(function (r) { return r.json(); })
      .then(function (d) {
        var t1 = Date.now(), rtt = t1 - t0, stale = t1 - bestAt > 60000;
        if (typeof d.total_offset === 'number') tzMin = d.total_offset;
        if (d.unix_ms > 0 && (rtt <= bestRtt + 20 || stale)) {
          var first = (off === null);
          off = d.unix_ms + rtt / 2 - t1;
          bestRtt = stale ? rtt : Math.min(bestRtt, rtt);
          bestAt = t1;
          if (first) tick();
        }
        apply(d);
      })
      .catch(function () { setErr('No response from the device'); });
  }
  function start() { poll(); setInterval(poll, 2000); }
  if (document.readyState === 'loading') document.addEventListener('DOMContentLoaded', start);
  else start();
})();
</script>
)rawjs";
  return html;
}

// ---------------------------------------------------------------------------
// Navigation bar + status bar
// ---------------------------------------------------------------------------
inline String getNavBar()
{
  String navbar =
    "<div class='nav'>\n"
    "<a href='/'>Home</a>\n"
    "<a href='/setup'>Setup</a>\n"
    "<a href='/trends'>Trends</a>\n"
    "<a href='/skyhistory'>Sky History</a>\n"
    "<a href='/console'>Console</a>\n"
    "<a href='/firmware' class='warn'>Update</a>\n"   // warn colour: it restarts the device
    "</div>\n";
  return navbar + getStatusCard();
}

// ---------------------------------------------------------------------------
// Home page - live thermal view + reading tiles
// ---------------------------------------------------------------------------
inline String getHomePage()
{
  String ip   = WiFi.localIP().toString();
  String host = "ws://" + ip + ":81";

  String html = getPageHeader("Sky Conditions Monitor");
  html += getPageTitle("Sky Conditions Monitor");
  html += getNavBar();

  // Live thermal canvas
  html += "<div class='card'>\n";
  html += "<h2>Live Thermal View</h2>\n";
  html += "<div class='thermal-wrap'>\n";
  html += "<canvas id='thermal-canvas' width='640' height='480'></canvas>\n";
  // Vertical colour scale.  Floating mode spans ambient - Clear delta (navy) to
  // ambient - Overcast delta (white); fixed mode spans heatmapFixedMin..Max with a
  // warm tail.  With Setup's Hot-Pixel Marker enabled a magenta cap marks the "hot"
  // threshold (e.g. the sun).  Labels are filled in by the script.
  String cbarTip;
  if (deviceConfig.heatmapRangeMode == 1)
    cbarTip = "Fixed colour scale (set in Setup &rarr; Imaging).";
  else
    cbarTip = "Floating colour scale: navy = ambient &minus; Clear &Delta; (clear sky), "
              "white = ambient &minus; Overcast &Delta; (overcast).";
  if (deviceConfig.heatmapHotMarker && deviceConfig.heatmapRangeMode == 1)
    cbarTip += " Magenta = above the top of the scale.";
  else if (deviceConfig.heatmapHotMarker)
    cbarTip += " Magenta = more than " + String(HEATMAP_HOT_MARGIN_C, 0) + " &deg;C above ambient.";
  else
    cbarTip += " Values beyond either end are clamped.";
  html += "<div class='cbar' title='" + cbarTip + "'>"
          "<div class='cbar-col'>" + String(deviceConfig.heatmapHotMarker ? "<div class='cbar-hot'></div>" : "") +
          "<div class='cbar-grad' id='cbar-grad'></div></div>"
          "<div class='cbar-col'>" +
          String(deviceConfig.heatmapHotMarker ? "<div class='cbar-hotlbl' id='cbar-hot'>--</div>" : "") +
          "<div class='cbar-ticks' id='cbar-ticks'></div></div></div>\n";
  html += "</div>\n";
  html += "<div id='ws-status'>Connecting...</div>\n";
  html += "</div>\n";

  // Reading tiles
  bool nd = skyConditions.hasData();
  bool nb = skyConditions.hasBrightnessData();
  auto tile = [](const String& label, const String& sub, const char* cls, const char* id,
                 const String& value) -> String {
    return "  <div class='stat-box'><div class='stat-label'>" + label +
           (sub.length() ? "<br><span class='stat-sub'>" + sub + "</span>" : String("")) +
           "</div><div class='stat-value " + cls + "' id='" + id + "'>" + value + "</div></div>\n";
  };
  html += "<div class='card'>\n";
  html += "<h2>Current Readings</h2>\n";
  html += "<div class='stat-grid'>\n";
  html += tile("Sky Temp", "center 50% FOV", "sky-temp", "sky-temp",
               nd ? String(skyConditions.getSkyTemperature(), 1) + "&deg;C" : "--");
  html += tile("Frame Min", "", "min-temp", "min-temp",
               nd ? String(skyConditions.getMinTemperature(), 1) + "&deg;C" : "--");
  html += tile("Frame Max", "", "max-temp", "max-temp",
               nd ? String(skyConditions.getMaxTemperature(), 1) + "&deg;C" : "--");
  html += tile("Frame Median", "", "med-temp", "med-temp",
               nd ? String(skyConditions.getMedianTemperature(), 1) + "&deg;C" : "--");
  html += tile("Ambient", (envHasTemperature() && dhtData.valid) ? envSensorName() : "MLX die",
               "amb-temp", "amb-temp",
               nd ? String(skyConditions.getAmbientTemperature(), 1) + "&deg;C" : "--");
  if (envHasHumidity())
    html += tile("Humidity", envSensorName(), "env-val", "hum-val",
                 dhtData.valid ? String(dhtData.humidity, 0) + "%" : "--");
  if (envHasPressure())
    html += tile("Pressure", envSensorName(), "env-val", "pres-val",
                 dhtData.valid ? String(dhtData.pressure, 1) + " hPa" : "--");
  html += tile("Cloud Cover", "Mean", "cloud-mean", "cloud-mean",
               nd ? String(skyConditions.getCloudCoverMean(), 0) + "%" : "--");
  html += tile("Cloud Cover", "Per-Pixel", "cloud-px", "cloud-px",
               nd ? String(skyConditions.getCloudCoverPixel(), 0) + "%" : "--");
  // Reported value (Alpaca CloudCover / MQTT cloud_cover) and the effective
  // thresholds behind it, so "why is it 0 %?" can be answered at a glance.
  html += "  <div class='stat-box'><div class='stat-label'>Cloud Cover<br>"
          "<span class='stat-sub' id='cloud-rep-sub'>Reported</span></div>"
          "<div class='stat-value cloud-mean' id='cloud-rep'>" +
          (nd ? String(skyConditions.getCloudCover(), 0) + "%" : "--") + "</div></div>\n";
  html += "  <div class='stat-box'><div class='stat-label'>Cloud &Delta; Clear / Overcast<br>"
          "<span class='stat-sub' id='cloud-thr-sub'>" +
          (nd ? "at ambient " + String(skyConditions.getCloudAmbient(), 1) + "&deg;C" : String("")) +
          "</span></div>"
          "<div class='stat-value cloud-px' id='cloud-thr' style='font-size:1.2em'>" +
          (nd ? String(skyConditions.getCloudClearEff(), 1) + " / " +
                String(skyConditions.getCloudOvercastEff(), 1) + "&deg;C" : "--") +
          "</div></div>\n";
  html += tile("Sky Brightness", "", "lux-val", "lux-val", nb ? fmtLux(skyConditions.getLux()) : "--");
  html += tile("Sky Quality", "mag/arcsec&sup2;", "sqm-val", "sqm-val",
               nb ? String(skyConditions.getSqm(), 2) : "--");
  {
    bool enabled   = deviceConfig.rainEnabled;
    bool wet       = rainIsWet();
    bool relayMode = (deviceConfig.rainMode == 0);
    const char* cls = !enabled ? "unknown" : (wet ? "bad" : "good");
    String mainLabel = !enabled ? "--" : (wet ? "Wet" : "Dry");
    String subTxt;
    if (!enabled)       subTxt = "No Sensor";
    else if (relayMode) subTxt = rainData.relayWet ? "Relay: Wet" : "Relay: Dry";
    else                subTxt = rainData.modbusOk ? (rainData.modbusWet ? "RS485: Wet" : "RS485: Dry")
                                                   : "RS485: Not seen";
    // IDs are used by the WebSocket text-message handler for live relay updates.
    html += "  <div class='stat-box'><div class='stat-label'>Rain / Snow<br>"
            "<span class='stat-sub'>" + String(relayMode ? "Relay" : "RS485") + "</span></div>"
            "<div class='stat-value " + cls + "' id='rain-state'>" + mainLabel + "</div>"
            "<div class='stat-note' id='rain-sub'>" + subTxt + "</div></div>\n";
  }
  html += "</div>\n";  // stat-grid
  html += "</div>\n";  // card

  // Thermal snapshot image (auto-refreshes every JPEG_INTERVAL_MS)
  html += "<div class='card'>\n";
  html += "<h2>Thermal Snapshot</h2>\n";
  html += "<p class='muted'>Refreshes every " + String(JPEG_INTERVAL_MS / 1000) +
          " s &middot; <a href='/thermal.jpg'>direct link</a></p>\n";
  html += "<img id='thermal-snap' src='/thermal.jpg' alt='Thermal snapshot' "
          "style='display:block;margin:0 auto;max-width:100%;border-radius:4px;'>\n";
  html += "</div>\n";

  // Device info
  html += "<div class='card'>\n";
  html += "<h2>Device Information</h2>\n";
  html += "<table class='status-table'>\n";
  html += "<tr><th>Device Name</th><td>"   + String(DEVICE_NAME)     + "</td></tr>\n";
  html += "<tr><th>IP Address</th><td>"    + ip                      + "</td></tr>\n";
  html += "<tr><th>MAC Address</th><td>"   + WiFi.macAddress()       + "</td></tr>\n";
  html += "<tr><th>SSID</th><td>"          + WiFi.SSID()             + "</td></tr>\n";
  html += "<tr><th>ASCOM Port</th><td>"    + String(ALPACA_PORT)     + "</td></tr>\n";
  html += "<tr><th>Date (" + utcOffsetText() + ")</th>" + clockSpan(true, "td") + "</tr>\n";
  html += "<tr><th>Uptime</th><td><span id='uptime' data-s='" +
          String(millis() / 1000) + "'></span></td></tr>\n";
  html += "<tr><th>Free Heap</th><td>" + liveCell("heap") + "</td></tr>\n";
  html += "</table>\n";
  html += "</div>\n";

  // ---------------------------------------------------------------------------
  // JavaScript - WebSocket client + palette renderer
  // ---------------------------------------------------------------------------

  // Compute the box region that matches the per-pixel cloud cover region.
  // Both cloud cover methods are always computed; the box always tracks the
  // per-pixel region (which equals center FOV when cloudPixelRegion == 0).
  int boxCCS, boxCCE, boxCRS, boxCRE;
  if (deviceConfig.cloudPixelRegion == 1) {
    int edge = (int)deviceConfig.cloudEdgeExclude;
    boxCCS = edge;
    boxCCE = SENSOR_COLS - 1 - edge;
    boxCRS = edge;
    boxCRE = SENSOR_ROWS - 1 - edge;
    if (boxCRS > boxCRE || boxCCS > boxCCE) { // degenerate - fall back
      boxCCS = CENTER_COL_START; boxCCE = CENTER_COL_END;
      boxCRS = CENTER_ROW_START; boxCRE = CENTER_ROW_END;
    }
  } else {
    boxCCS = CENTER_COL_START; boxCCE = CENTER_COL_END;
    boxCRS = CENTER_ROW_START; boxCRE = CENTER_ROW_END;
  }

  html += R"rawjs(
<script>
const COLS = 32, ROWS = 24, PIXELS = 768;
const HEADER = 16;  // bytes: 4 x float32 (min, max, median, skyTemp)
const DEG = '\u00B0C';
const canvas = document.getElementById('thermal-canvas');
const ctx    = canvas.getContext('2d');
const imgData = ctx.createImageData(COLS, ROWS);
const wsStatus = document.getElementById('ws-status');

// Palette lookup table, one entry per WebSocket pixel index, generated by
// heatmapColor() on the device so the live view matches /thermal.jpg.
// Index 255 (magenta) marks pixels above the hot threshold.
)rawjs";
  html += "const skyLT = [";
  for (int i = 0; i < 256; i++) {
    uint8_t r, g, b;
    heatmapColor((uint8_t)i, r, g, b);
    char entry[16];
    snprintf(entry, sizeof(entry), "[%u,%u,%u],", r, g, b);
    html += entry;
  }
  html += "];\n";
  html += R"rawjs(

// Vertical colour scale beside the thermal view.  Mirrors
// SkyConditions::getDisplayRange(): fixed mode uses HEAT_MIN..HEAT_MAX;
// floating mode puts t=0 at cloud ambient - effective Clear delta and t=1 at
// cloud ambient - effective Overcast delta (span clamped to >= 1 C), so its
// labels track ambient.  The magenta cap (HOT_MARKER only) is HEAT_MAX in
// fixed mode, raw ambient + HOT_MARGIN floating.
const HEAT_FIXED     = )rawjs" + String(deviceConfig.heatmapRangeMode == 1 ? "true" : "false") + R"rawjs(;
const HEAT_MIN       = )rawjs" + String(deviceConfig.heatmapFixedMin, 2) + R"rawjs(;
const HEAT_MAX       = )rawjs" + String(deviceConfig.heatmapFixedMax, 2) + R"rawjs(;
const CLOUD_METHOD   = ')rawjs" + String(deviceConfig.cloudCoverMethod == 1 ? "per-pixel" : "mean") + R"rawjs(';
const HOT_MARKER     = )rawjs" + String(deviceConfig.heatmapHotMarker ? "true" : "false") + R"rawjs(;
const HOT_MARGIN     = )rawjs" + String(HEATMAP_HOT_MARGIN_C, 1) + R"rawjs(;
const CBAR_TICKS = 5;
{
  const g = [];
  for (let k = 0; k <= 32; k++) {
    const i = Math.round(k * 254 / 32);
    g.push('rgb(' + skyLT[i].join(',') + ') ' + (k * 100 / 32).toFixed(2) + '%');
  }
  document.getElementById('cbar-grad').style.background =
    'linear-gradient(to top,' + g.join(',') + ')';
}
const cbarHot = document.getElementById('cbar-hot');
const cbarTicks = document.getElementById('cbar-ticks');
for (let i = 0; i < CBAR_TICKS; i++) {
  const s = document.createElement('span');
  s.style.top = (i * 100 / (CBAR_TICKS - 1)) + '%';
  s.textContent = '--';
  cbarTicks.appendChild(s);
}
function updateColorBar(amb, ambCloud, clrEff, ovcEff) {
  const cold = HEAT_FIXED ? HEAT_MIN : ambCloud - clrEff;
  const span = Math.max(1, HEAT_FIXED ? HEAT_MAX - HEAT_MIN : clrEff - ovcEff);
  const spans = cbarTicks.children;
  for (let i = 0; i < CBAR_TICKS; i++) {
    const t = 1 - i / (CBAR_TICKS - 1);          // top = warm end
    // With the hot marker, fixed-mode values above the top are magenta rather
    // than clamped, so the top label drops its '>='.
    const pre = (i === 0 && !(HOT_MARKER && HEAT_FIXED)) ? '\u2265' : (i === CBAR_TICKS - 1) ? '\u2264' : '';
    spans[i].textContent = pre + (cold + t * span).toFixed(1) + DEG;
  }
  if (cbarHot)
    cbarHot.textContent = '>' + (HEAT_FIXED ? cold + span : amb + HOT_MARGIN).toFixed(1) + DEG;
}
)rawjs";
  if (deviceConfig.heatmapRangeMode == 1)
    html += "updateColorBar(0, 0, 0, 0);\n";
  else if (skyConditions.hasData())
    html += "updateColorBar(" + String(skyConditions.getAmbientTemperature(), 2) + "," +
            String(skyConditions.getCloudAmbient(), 2) + "," +
            String(skyConditions.getCloudClearEff(), 2) + "," +
            String(skyConditions.getCloudOvercastEff(), 2) + ");\n";
  html += R"rawjs(

function fmtLux(lux) {
  if (lux >= 1)     return lux.toFixed(1)       + ' lux';
  if (lux >= 0.1)   return lux.toFixed(3)       + ' lux';
  if (lux >= 0.001) return lux.toFixed(4)       + ' lux';
  return lux.toExponential(2) + ' lux';
}

// Pre-allocate the offscreen source canvas; reused every frame to avoid GC churn.
const srcCanvas = new OffscreenCanvas(COLS, ROWS);
const srcCtx    = srcCanvas.getContext('2d');

let ws, fpsCount = 0, lastFpsTime = Date.now(), fps = 0;

function setText(id, v) { const el = document.getElementById(id); if (el) el.textContent = v; }

function connect() {
  ws = new WebSocket(')rawjs" + host + R"rawjs(');
  ws.binaryType = 'arraybuffer';

  ws.onopen    = () => { wsStatus.textContent = 'Connected'; };
  ws.onclose   = () => { wsStatus.textContent = 'Disconnected \u2013 reconnecting\u2026';
                          setTimeout(connect, 1500); };
  ws.onerror   = () => { ws.close(); };

  ws.onmessage = (evt) => {
    // Text messages carry JSON status updates.
    if (typeof evt.data === 'string') {
      try {
        const d = JSON.parse(evt.data);
        // Rain state (relay mode live update)
        if (d.rain !== undefined) {
          const wet = d.rain === 'WET';
          const el  = document.getElementById('rain-state');
          if (el) { el.textContent = wet ? 'Wet' : 'Dry'; el.className = 'stat-value ' + (wet ? 'bad' : 'good'); }
          setText('rain-sub', wet ? 'Relay: Wet' : 'Relay: Dry');
        }
        // Sensor state (cloud cover, brightness, ambient)
        if (d.amb !== undefined) {
          const nd = d.has_data, nb = d.has_brightness;
          setText('amb-temp',   nd ? d.amb.toFixed(1) + DEG : '--');
          if (nd) updateColorBar(d.amb, d.amb_cloud, d.clr_eff, d.ovc_eff);
          setText('cloud-mean', nd ? d.cloud_mean.toFixed(0) + '%' : '--');
          setText('cloud-px',   nd ? d.cloud_px.toFixed(0)   + '%' : '--');
          setText('cloud-rep',  nd ? d.cloud.toFixed(0)      + '%' : '--');
          setText('cloud-rep-sub',
            'Reported (' + CLOUD_METHOD + (d.cloud_n > 1 ? ', median of ' + d.cloud_n : '') + ')');
          setText('cloud-thr', nd ? d.clr_eff.toFixed(1) + ' / ' + d.ovc_eff.toFixed(1) + DEG : '--');
          setText('cloud-thr-sub', nd ? 'at ambient ' + d.amb_cloud.toFixed(1) + DEG : '');
          setText('lux-val', nb ? fmtLux(d.lux) : '--');
          setText('sqm-val', nb ? d.sqm.toFixed(2) : '--');
          if (d.dht_hum !== undefined)  setText('hum-val',  d.dht_hum.toFixed(0) + '%');
          if (d.pressure !== undefined) setText('pres-val', d.pressure.toFixed(1) + ' hPa');
        }
      } catch(e) {}
      return;
    }
    // Binary messages are thermal frames.
    const buf  = evt.data;
    if (buf.byteLength < HEADER + PIXELS) return;

    const dv   = new DataView(buf);
    const minT = dv.getFloat32(0,  true);
    const maxT = dv.getFloat32(4,  true);
    const medT = dv.getFloat32(8,  true);
    const skyT = dv.getFloat32(12, true);
    const pix  = new Uint8Array(buf, HEADER, PIXELS);

    // Render pixels
    for (let i = 0; i < PIXELS; i++) {
      const [r,g,b] = skyLT[pix[i]];
      imgData.data[i*4]   = r;
      imgData.data[i*4+1] = g;
      imgData.data[i*4+2] = b;
      imgData.data[i*4+3] = 255;
    }

    // Scale 32x24 -> 640x480 with bicubic interpolation to smooth pixels and
    // reduce MLX90640 row/column banding artifacts in the live view.
    srcCtx.putImageData(imgData, 0, 0);
    ctx.imageSmoothingEnabled = true;
    ctx.imageSmoothingQuality = 'high';
    ctx.drawImage(srcCanvas, 0, 0, canvas.width, canvas.height);

    // Draw cloud cover region box
    const ccs = )rawjs" + String(boxCCS) + R"rawjs(,
          cce = )rawjs" + String(boxCCE + 1) + R"rawjs(,
          crs = )rawjs" + String(boxCRS) + R"rawjs(,
          cre = )rawjs" + String(boxCRE + 1) + R"rawjs(;
    const scaleX = canvas.width  / COLS;
    const scaleY = canvas.height / ROWS;
    ctx.strokeStyle = 'rgba(255,60,60,0.85)';
    ctx.lineWidth   = 2;
    ctx.strokeRect(ccs*scaleX, crs*scaleY,
                   (cce-ccs)*scaleX, (cre-crs)*scaleY);

    // Update stat readouts
    setText('sky-temp', skyT.toFixed(1) + DEG);
    setText('min-temp', minT.toFixed(1) + DEG);
    setText('max-temp', maxT.toFixed(1) + DEG);
    setText('med-temp', medT.toFixed(1) + DEG);

    // FPS
    fpsCount++;
    const now = Date.now();
    if (now - lastFpsTime >= 1000) {
      fps = fpsCount;
      fpsCount = 0;
      lastFpsTime = now;
    }
    wsStatus.textContent =
      'Live \u2013 ' + fps + ' fps | Sky: ' + skyT.toFixed(1) +
      DEG + ' | Min: ' + minT.toFixed(1) + DEG + ' | Max: ' + maxT.toFixed(1) + DEG;
  };
}

connect();

// Uptime counter (counts seconds since boot, so it stays a local counter).
(function() {
  const el = document.getElementById('uptime');
  if (!el) return;
  let sec = parseInt(el.dataset.s, 10);
  function fmt(s) {
    const d = Math.floor(s / 86400);
    const h = Math.floor(s % 86400 / 3600);
    const m = Math.floor(s % 3600 / 60);
    const ss = s % 60;
    if (d > 0) return d + 'd ' + h + 'h ' + m + 'm';
    if (h > 0) return h + 'h ' + m + 'm ' + ss + 's';
    return m + 'm ' + ss + 's';
  }
  el.textContent = fmt(sec);
  setInterval(function() { el.textContent = fmt(++sec); }, 1000);
})();

// Auto-refresh the static JPEG snapshot.
setInterval(() => {
  const img = document.getElementById('thermal-snap');
  if (img) img.src = '/thermal.jpg?' + Date.now();
}, )rawjs" + String(JPEG_INTERVAL_MS) + R"rawjs();
</script>
)rawjs";

  html += "</body></html>";
  return html;
}

// ---------------------------------------------------------------------------
// Setup page form helpers
// ---------------------------------------------------------------------------
// Element ids are "f-<name>" so they can't collide with browser globals.
inline String fieldHelp(const String& help)
{
  return help.length() ? "<small>" + help + "</small>" : String("");
}

inline String inputField(const char* type, const String& label, const char* name,
                         const String& value, const String& attrs, const String& help)
{
  return String("<div class='field'><label class='field-label' for='f-") + name + "'>" + label + "</label>"
         "<input type='" + type + "' id='f-" + name + "' name='" + name + "' value='" + value + "' " +
         attrs + ">" + fieldHelp(help) + "</div>\n";
}

inline String numField(const String& label, const char* name, const String& value,
                       const String& attrs, const String& help)
{
  return inputField("number", label, name, value, attrs, help);
}

inline String textField(const String& label, const char* name, const String& value,
                        const String& attrs, const String& help)
{
  return inputField("text", label, name, value, attrs, help);
}

inline String selectOption(int value, int current, const String& text)
{
  return "<option value='" + String(value) + "'" + (value == current ? " selected" : "") + ">" +
         text + "</option>";
}

inline String selectField(const String& label, const char* name, const String& options,
                          const String& attrs, const String& help)
{
  return String("<div class='field'><label class='field-label' for='f-") + name + "'>" + label + "</label>"
         "<select id='f-" + name + "' name='" + name + "' " + attrs + ">" + options + "</select>" +
         fieldHelp(help) + "</div>\n";
}

// Toggle switch with a "Label: Enabled/Disabled" caption (updated by the
// Setup page script).  danger = turning it on is the risky direction.
inline String toggleField(const String& label, const char* name, bool on, const String& help,
                          bool danger = false)
{
  return String("<div class='field'><label class='switch-container'><span class='switch'>"
                "<input type='checkbox' name='") + name + "' value='1'" +
         (danger ? " class='danger'" : "") + (on ? " checked" : "") +
         "><span class='slider'></span></span><span class='switch-label'>" + label +
         ": <span class='sw-state'>" + (on ? "Enabled" : "Disabled") + "</span></span></label>" +
         fieldHelp(help) + "</div>\n";
}

inline String statusRow(const String& label, const String& value)
{
  return "<tr><th>" + label + "</th><td>" + value + "</td></tr>\n";
}

inline String saveRow()
{
  return "<div class='button-row'><input type='submit' value='Save Settings'></div>\n";
}

// ---------------------------------------------------------------------------
// Setup page - tabbed, streamed one tab at a time through emit(const String&)
// so the ~40 KB page never sits in one String.
// ---------------------------------------------------------------------------
template <typename Emit>
inline void buildSetupPage(Emit emit, bool saved)
{
  static const char* const TAB_IDS[]   = {"cloud", "sky", "imaging", "sensors", "mqtt",
                                          "network", "wifi", "device"};
  static const char* const TAB_NAMES[] = {"Cloud Cover", "Sky Quality", "Imaging", "Sensors", "MQTT",
                                          "Network", "WiFi", "Device"};
  const int TAB_COUNT = sizeof(TAB_IDS) / sizeof(TAB_IDS[0]);

  // The first tab is marked active server-side so the page is usable without JS.
  auto tabOpen = [](const char* id, bool first) -> String {
    return String("<div class='tab") + (first ? " active" : "") + "' id='tab-" + id + "'>\n";
  };

  // -- Header, nav, tab buttons, form start ---------------------------------
  {
    String html = getPageHeader("Setup - Sky Conditions");
    html += getPageTitle("Setup");
    html += getNavBar();
    if (saved) html += "<div class='saved-banner'>Settings saved.</div>\n";
    html += "<div class='tabs'>\n";
    for (int i = 0; i < TAB_COUNT; i++)
      html += String("<button type='button' data-tab='") + TAB_IDS[i] + "'" +
              (i == 0 ? " class='active'" : "") + " onclick=\"showTab('" + TAB_IDS[i] + "')\">" +
              TAB_NAMES[i] + "</button>\n";
    html += "</div>\n";
    html += "<form id='cfg' action='/save_config' method='POST'>\n";
    // form=full tells /save_config that unticked checkboxes mean "off" (see
    // readCheckbox()); tab brings the browser back to the same tab.
    html += "<input type='hidden' name='form' value='full'>\n";
    html += "<input type='hidden' name='tab' id='f-tab' value='cloud'>\n";
    emit(html);
  }

  // -- Cloud Cover -----------------------------------------------------------
  {
    String html = tabOpen("cloud", true);
    html += "<div class='card'>\n<h2>Cloud Cover</h2>\n";
    html += "<p class='muted'>Settings are stored in flash. Save Settings on any tab saves every tab.</p>\n";
    html += "<h3>Thresholds</h3>\n";
    html += numField("Clear Delta (&deg;C)", "cloudClear", String(deviceConfig.cloudClearDelta, 1),
                     "step='0.5'", "Ambient &minus; sky at or above this reads 0% cloud cover (clear).");
    html += numField("Overcast Delta (&deg;C)", "cloudOvercast", String(deviceConfig.cloudOvercastDelta, 1),
                     "step='0.5'", "Ambient &minus; sky at or below this reads 100% cloud cover (overcast).");

    html += "<h3>Ambient Compensation</h3>\n";
    html += numField("Ambient Slope", "cldAmbSlope", String(deviceConfig.cloudAmbSlope, 2),
                     "step='0.01' min='-2' max='2'",
                     "Shifts both thresholds by slope &times; (ambient &minus; reference). A clear sky's "
                     "ambient &minus; sky grows with ambient. 0 (default) keeps the thresholds fixed.");
    html += numField("Ambient Reference (&deg;C)", "cldAmbRef", String(deviceConfig.cloudAmbRef, 1),
                     "step='0.5'", "Ambient at which the Clear and Overcast deltas apply unchanged.");
    html += numField("Dew-Point Slope", "cldDewSlope", String(deviceConfig.cloudDewSlope, 2),
                     "step='0.01' min='-2' max='2'",
                     "Optional extra shift of slope &times; (dew point &minus; reference). 0 (default) "
                     "turns it off." +
                     String(envHasHumidity() ? ""
                            : " <span class='warn-text'>Needs a humidity sensor (DHT11/22, BME280); "
                              "ignored without one.</span>"));
    html += numField("Dew-Point Reference (&deg;C)", "cldDewRef", String(deviceConfig.cloudDewRef, 1),
                     "step='0.5'", "Dew point at which the dew-point term is zero.");
    html += numField("Ambient Smoothing (s)", "cldAmbTau", String(deviceConfig.cloudAmbTau),
                     "step='10' min='0' max='" + String(CLOUD_AMB_TAU_MAX_S) + "'",
                     "Time constant of an exponential average on the ambient used for cloud cover. "
                     "0 (default) uses the raw ambient. Keep it short (60&ndash;180 s) so fronts still "
                     "register; the filter resets on a jump of more than " +
                     String(CLOUD_AMB_RESET_C, 0) + " &deg;C. The reported Ambient is never smoothed.");
    html += "<p class='muted'>Effective thresholds = delta + slope &times; (ambient &minus; ref) + "
            "dew slope &times; (dew point &minus; ref). Now: " + liveCell("cloud_eff") + ".</p>\n";

    html += "<h3>Reporting</h3>\n";
    html += selectField("Cloud Cover Method", "cloudMethod",
                        selectOption(0, deviceConfig.cloudCoverMethod, "Mean (FOV average)") +
                        selectOption(1, deviceConfig.cloudCoverMethod, "Per-pixel"),
                        "",
                        "Which calculation Alpaca CloudCover and MQTT cloud_cover report. Both are always "
                        "recorded in history and shown on the home page.");
    html += numField("Average Period (h)", "avgPeriod", String(deviceConfig.averagePeriod, 4),
                     "min='0' step='any'",
                     "ASCOM AveragePeriod in hours, kept across reboots. Alpaca CloudCover and MQTT "
                     "cloud_cover report the median over this period; 0 reports the latest frame. History "
                     "holds " + String(CLOUD_AVG_SAMPLES) + " frames (about " +
                     String(CLOUD_AVG_SAMPLES * (FRAME_INTERVAL_MS / 1000) / 60) +
                     " min); longer periods use all of it. Example: 0.0167 = 1 min.");
    html += selectField("Per-Pixel Region", "cloudPxRgn",
                        selectOption(0, deviceConfig.cloudPixelRegion, "Centre FOV") +
                        selectOption(1, deviceConfig.cloudPixelRegion, "Full Frame"),
                        "onchange='updateEdge()'",
                        "Pixel region used for the per-pixel cloud cover calculation.");
    html += numField("Edge Exclusion (pixels)", "cloudEdge", String(deviceConfig.cloudEdgeExclude),
                     "min='0' max='10' step='1'" +
                     String(deviceConfig.cloudPixelRegion == 0 ? " disabled" : ""),
                     "Full Frame only. Pixels ignored on each side of the frame (0&ndash;10). The default "
                     "2 uses " + String(SENSOR_COLS - 4) + " &times; " + String(SENSOR_ROWS - 4) + " = " +
                     String((SENSOR_COLS - 4) * (SENSOR_ROWS - 4)) + " pixels.");
    html += saveRow();
    html += "</div>\n</div>\n";
    emit(html);
  }

  // -- Sky Quality ----------------------------------------------------------
  {
    String html = tabOpen("sky", false);
    html += "<div class='card'>\n<h2>Sky Brightness (TSL2591)</h2>\n";
    html += numField("SQM Offset (mag/arcsec&sup2;)", "sqmOffset", String(deviceConfig.sqmOffset, 2),
                     "step='0.01'", "Added to the calculated SQM value.");
    html += numField("SQM Reference (lux)", "sqmRef", String(deviceConfig.sqmReference, 0),
                     "step='1' min='1'", "Lux level that maps to SQM 0.0 (default 108000).");
    {
      static const char* const timingLabels[] = {"100 ms", "200 ms", "300 ms (default)",
                                                 "400 ms", "500 ms", "600 ms"};
      String opts;
      for (int i = 0; i < 6; i++) opts += selectOption(i, deviceConfig.tsl2591Integration, timingLabels[i]);
      html += selectField("Integration Time", "tslInteg", opts, "",
                          "Longer integration improves dark-sky sensitivity.");
    }
    html += "<h3>Sky History</h3>\n";
    html += numField("Night Lux Threshold (lux)", "nightLux", String(deviceConfig.nightLuxThreshold, 1),
                     "step='0.1' min='0'",
                     "Lux below this counts as night for SQM and night clear-sky tracking on the Sky "
                     "History page.");
    html += numField("Clear-Sky Cloud Threshold (%)", "clearCloud", String(deviceConfig.clearCloudThreshold, 0),
                     "step='1' min='0' max='100'",
                     "Cloud cover below this counts as clear for clear-sky hours, and shows as Clear in "
                     "the status bar.");
    html += saveRow();
    html += "</div>\n</div>\n";
    emit(html);
  }

  // -- Imaging --------------------------------------------------------------
  {
    String html = tabOpen("imaging", false);
    html += "<div class='card'>\n<h2>Imaging</h2>\n";
    html += numField("Snapshot Interval (s)", "snapSec", String(deviceConfig.snapshotIntervalSec),
                     "min='5' max='3600' step='5'", "How often /thermal.jpg is regenerated.");
    html += numField("JPEG Quality (1&ndash;100)", "jpegQuality", String(deviceConfig.jpegQuality),
                     "min='1' max='100' step='1'", "Higher gives a better image and a larger file.");
    html += selectField("Heatmap Range", "heatMode",
                        selectOption(0, deviceConfig.heatmapRangeMode, "Floating") +
                        selectOption(1, deviceConfig.heatmapRangeMode, "Fixed"),
                        "onchange='updateHeat()'",
                        "Colour scale for the live view and /thermal.jpg. Floating spans ambient &minus; "
                        "Clear delta to ambient &minus; Overcast delta, so it tracks cloud cover. Fixed uses "
                        "the range below: blue to white up to 0 &deg;C, then a warm tail to red. Display "
                        "only; cloud cover is unaffected.");
    String heatDis = deviceConfig.heatmapRangeMode == 0 ? " disabled" : "";
    html += "<div class='field'><label class='field-label' for='f-heatMin'>Fixed Range Min / Max (&deg;C)</label>"
            "<div class='field-pair'>"
            "<input type='number' id='f-heatMin' name='heatMin' step='0.5' value='" +
            String(deviceConfig.heatmapFixedMin, 1) + "'" + heatDis + "> / "
            "<input type='number' id='f-heatMax' name='heatMax' step='0.5' value='" +
            String(deviceConfig.heatmapFixedMax, 1) + "'" + heatDis + "></div>" +
            fieldHelp("Palette end points in Fixed mode (default &minus;45 / +60). Max must exceed Min "
                      "by at least 1 &deg;C.") + "</div>\n";
    html += toggleField("Hot-Pixel Marker", "heatHot", deviceConfig.heatmapHotMarker,
                        "When enabled, pixels above the top of the fixed range, or more than " +
                        String(HEATMAP_HOT_MARGIN_C, 0) + " &deg;C above ambient when floating (e.g. the "
                        "sun), show magenta. When disabled, they are clamped to the end colour.");
    html += saveRow();
    html += "</div>\n</div>\n";
    emit(html);
  }

  // -- Sensors --------------------------------------------------------------
  {
    String html = tabOpen("sensors", false);
    html += "<div class='card'>\n<h2>Sensors</h2>\n";
    html += "<table class='status-table'>\n";
    html += statusRow("Thermal Camera (MLX90640)", liveCell("s_thermal"));
    html += statusRow("Sky Brightness (TSL2591)",  liveCell("s_bright"));
    html += statusRow("Ambient Sensor",            liveCell("s_env"));
    html += statusRow("Rain Sensor",               liveCell("s_rain"));
    html += "</table>\n";

    html += "<h3>Rain Sensor</h3>\n<p class='muted'>Takes effect after reboot.</p>\n";
    html += toggleField("Rain Sensor", "rainEnabled", deviceConfig.rainEnabled,
                        "When disabled, the rain sensor is ignored and shows No Sensor. Disable it on "
                        "hardware without one, such as the XIAO Sense.");
    html += selectField("Sensor Mode", "rainMode",
                        selectOption(0, deviceConfig.rainMode, "Relay") +
                        selectOption(1, deviceConfig.rainMode, "RS485 Modbus"),
                        "",
                        "Relay senses the relay contact closure on a GPIO. RS485 polls a ZTS-3000 over "
                        "Modbus RTU.");

    html += "<h3>Ambient Sensor</h3>\n<p class='muted'>Takes effect after reboot.</p>\n";
    {
      static const char* const envLabels[] = {"Disabled", "DHT11", "DHT22", "BMP180", "BMP280", "BME280"};
      String opts;
      for (int i = 0; i <= 5; i++) opts += selectOption(i, deviceConfig.dhtType, envLabels[i]);
      html += selectField("Sensor Type", "dhtType", opts, "onchange='updateEnvAddr()'",
                          "Replaces the MLX90640 die temperature with a true external ambient for cloud "
                          "cover. DHT11 (&plusmn;2 &deg;C) and DHT22 (&plusmn;0.5 &deg;C) connect to D7 "
                          "(GPIO44) and add Humidity. BMP180 (I2C 0x77) adds Pressure; BMP280 adds Pressure; "
                          "BME280 adds Pressure and Humidity. BMP/BME sensors share the SDA/SCL bus.");
    }
    bool showAddr = deviceConfig.dhtType == 4 || deviceConfig.dhtType == 5;
    html += "<div id='f-bmpRow'" + String(showAddr ? "" : " style='display:none'") + ">";
    html += selectField("BMP280 / BME280 Address", "bmp280Addr",
                        selectOption(0x76, deviceConfig.bmp280Addr, "0x76 (SDO to GND)") +
                        selectOption(0x77, deviceConfig.bmp280Addr, "0x77 (SDO to VCC)"),
                        "",
                        "SDO tied to GND gives 0x76 (the common default); SDO tied to VCC gives 0x77. "
                        "Check your module's silkscreen or datasheet.");
    html += "</div>\n";
    html += saveRow();
    html += "</div>\n</div>\n";
    emit(html);
  }

  // -- MQTT -----------------------------------------------------------------
  {
    String html = tabOpen("mqtt", false);
    html += "<div class='card'>\n<h2>MQTT / Home Assistant</h2>\n";
    html += "<p class='muted'>Takes effect after reboot.</p>\n";
    html += toggleField("MQTT", "mqttEnabled", deviceConfig.mqttEnabled,
                        "When enabled, publishes sensor data and Home Assistant autodiscovery to the broker "
                        "below. When disabled, nothing is published.");
    html += textField("Broker Address", "mqttServer", deviceConfig.mqttServer,
                      "maxlength='63' placeholder='192.168.1.x'", "MQTT broker hostname or IP address.");
    html += numField("Broker Port", "mqttPort", String(deviceConfig.mqttPort),
                     "min='1' max='65535'", "Default 1883.");
    html += textField("Username", "mqttUser", deviceConfig.mqttUser,
                      "maxlength='31' placeholder='(optional)'", "Leave blank if the broker needs no login.");
    html += inputField("password", "Password", "mqttPass", deviceConfig.mqttPassword,
                       "maxlength='63' placeholder='(optional)'", "");
    html += textField("Topic Prefix", "mqttTopicPfx", deviceConfig.mqttTopicPrefix, "maxlength='63'",
                      "The default <code>skyconditions</code> gets the MAC address appended. State topic: "
                      "<code>&lt;prefix&gt;/state</code>.");
    html += saveRow();
    html += "</div>\n</div>\n";
    emit(html);
  }

  // -- Network --------------------------------------------------------------
  {
    String html = tabOpen("network", false);
    html += "<div class='card'>\n<h2>Network</h2>\n";
    html += textField("Location", "location", deviceConfig.location, "maxlength='31'",
                      "Shown in the Alpaca management API discovery response.");

    html += "<h3>Time</h3>\n<p class='muted'>Takes effect immediately.</p>\n";
    html += "<table class='status-table' style='margin-bottom:16px'>\n";
    html += statusRow("NTP Sync", liveCell("ntp"));
    html += statusRow("Device Time (" + utcOffsetText() + ")", clockSpan(true, "span") + " " +
                      clockSpan(false, "span"));
    html += "</table>\n";
    html += toggleField("NTP Sync", "ntpEnabled", deviceConfig.ntpEnabled,
                        "When enabled, the clock is set from the sources below and re-synced hourly. "
                        "When disabled, the clock is never set after a reboot, so the Sky History "
                        "calendar records nothing and times show as a dash.");
    html += textField("NTP Server 1", "ntpServer", deviceConfig.ntpServer,
                      "maxlength='63' placeholder='" NTP_DEFAULT_SERVER "'",
                      "Tried first. Default " NTP_DEFAULT_SERVER ". A LAN server (e.g. your router) "
                      "also works.");
    html += textField("NTP Server 2", "ntpServer2", deviceConfig.ntpServer2,
                      "maxlength='63' placeholder='(optional) e.g. pool.ntp.org'",
                      "Used if server 1 doesn't answer.");
    html += textField("NTP Server 3", "ntpServer3", deviceConfig.ntpServer3,
                      "maxlength='63' placeholder='(optional) e.g. time.nist.gov'",
                      "Used if servers 1 and 2 don't answer. With all three blank, " NTP_DEFAULT_SERVER
                      " is used.");
    html += numField("UTC Offset (hours)", "utcOffset", String(deviceConfig.utcOffsetMin / 60.0f, 2),
                     "step='0.25' min='-12' max='14'",
                     "Shifts the times shown in the Web UI, e.g. &minus;6 or 5.5. 0 (default) shows UTC. "
                     "The device clock, Alpaca, MQTT and Sky History stay in UTC; no daylight saving "
                     "is applied.");

    html += "<h3>Safety Sensor UDP Broadcast</h3>\n<p class='muted'>Takes effect immediately.</p>\n";
    html += toggleField("Safety Broadcast", "safetyBcastEnabled", deviceConfig.safetyBcastEnabled,
                        "When enabled, broadcasts the rain safety state over UDP (DDA park-sensor protocol), "
                        "so receivers such as a roll-off roof controller find this device automatically. "
                        "When disabled, nothing is broadcast.");
    html += numField("UDP Port", "safetyBcastPort", String(deviceConfig.safetyBcastPort),
                     "min='1024' max='65535'",
                     "Destination port for broadcast packets. Default 23435 (DDA park-sensor port).");
    html += numField("Broadcast Interval (s)", "safetyBcastSec", String(deviceConfig.safetyBcastIntervalSec),
                     "min='5' max='3600'",
                     "Keep-alive interval. A broadcast is also sent at once on any wet/dry change. "
                     "Default 30 s, minimum 5 s.");
    html += saveRow();
    html += "</div>\n</div>\n";
    html += "</form>\n";
    emit(html);
  }

  // -- WiFi (outside the settings form) -------------------------------------
  {
    String html = tabOpen("wifi", false);
    html += "<div class='card'>\n<h2>WiFi</h2>\n";
    html += "<table class='status-table'>\n";
    html += statusRow("SSID", WiFi.SSID());
    html += statusRow("IP Address", WiFi.localIP().toString());
    html += "</table>\n";

    html += "<h3>Change Network</h3>\n";
    html += "<div class='button-row' style='margin-top:0;margin-bottom:16px'>"
            "<button type='button' id='wifi-scan-btn' onclick='wifiScan()'>Scan for Networks</button></div>\n";
    html += "<div class='field' id='wifi-sel-row' style='display:none'>"
            "<label class='field-label' for='wifi-sel'>Nearby Networks</label>"
            "<select id='wifi-sel' onchange='wifiSelChanged()'><option value=''>-- select --</option></select></div>\n";
    html += "<div class='field'><label class='field-label' for='wifi-ssid'>SSID</label>"
            "<input type='text' id='wifi-ssid' placeholder='Network name'></div>\n";
    html += "<div class='field'><label class='field-label' for='wifi-pass'>Password</label>"
            "<input type='password' id='wifi-pass' placeholder='Leave blank if open'></div>\n";
    html += "<div class='button-row'><button type='button' onclick='wifiConnect()'>Change WiFi</button>"
            "<span id='wifi-msg' class='muted'></span></div>\n";

    html += "<h3>Reset</h3>\n";
    html += "<div class='button-row' style='margin-top:0'>"
            "<button type='button' class='button-danger' onclick='resetWifi()'>Reset WiFi Settings</button>"
            "<span class='muted'>Clears the stored credentials and restarts into the setup access point "
            "(SkyCond-Setup).</span></div>\n";
    html += "</div>\n</div>\n";
    emit(html);
  }

  // -- Device ---------------------------------------------------------------
  {
    String html = tabOpen("device", false);

    String sensors = "SkyTemperature, Temperature, CloudCover, SkyBrightness, SkyQuality";
    if (envHasHumidity()) sensors += ", Humidity, DewPoint";
    if (envHasPressure()) sensors += ", Pressure";
    html += "<div class='card'>\n<h2>ASCOM Alpaca</h2>\n<table class='status-table'>\n";
    html += statusRow("Device Type", "ObservingConditions");
    html += statusRow("Device Number", "0");
    html += statusRow("Alpaca Port", String(ALPACA_PORT));
    html += statusRow("Discovery Port", String(ALPACA_DISCOVERY_PORT));
    html += statusRow("API Base URL", "http://" + WiFi.localIP().toString() + ":" + String(ALPACA_PORT) +
                                      "/api/v1/observingconditions/0");
    html += statusRow("Implemented Sensors", sensors);
    html += "</table>\n</div>\n";

    html += "<div class='card'>\n<h2>Hardware</h2>\n<table class='status-table'>\n";
    html += statusRow("Board (Arduino IDE selection)", IDE_BOARD_ID);
#if defined(BOARD_AUTO_FALLBACK)
    html += statusRow("Pinout Profile", statusDot("orange") + BOARD_NAME +
                      " <span class='warn-text'>(board id above not recognised; defaulted to this "
                      "profile. Verify the pins below, or force one in config.h)</span>");
#else
    html += statusRow("Pinout Profile", BOARD_NAME " <span class='muted'>(auto-detected)</span>");
#endif
    html += statusRow("Sensor", "MLX90640 (" + String(SENSOR_COLS) + " &times; " + String(SENSOR_ROWS) +
                                " pixels)");
    html += statusRow("I2C SDA Pin", String(THERMAL_SDA_PIN));
    html += statusRow("I2C SCL Pin", String(THERMAL_SCL_PIN));
    html += statusRow("I2C Frequency", String(THERMAL_I2C_FREQ / 1000) + " kHz");
    html += statusRow("Frame Rate", String(1000 / FRAME_INTERVAL_MS) + " Hz");
    html += statusRow("Center FOV Region", "Cols " + String(CENTER_COL_START) + "&ndash;" +
                      String(CENTER_COL_END) + ", Rows " + String(CENTER_ROW_START) + "&ndash;" +
                      String(CENTER_ROW_END) + " (" + String(CENTER_PIXEL_COUNT) + " pixels)");
    html += statusRow("Free Heap", liveCell("heap"));
    html += statusRow("Free PSRAM", liveCell("psram"));
    const char* bufDot = isScaledBufFailed() ? "red" : (isScaledBufInPsram() ? "green" : "orange");
    html += statusRow("Thermal JPEG Buffer", statusDot(bufDot) + getScaledBufStatus());
    html += "</table>\n</div>\n";

    html += "<div class='card'>\n<h2>Restart</h2>\n";
    html += "<div class='button-row' style='margin-top:0'>"
            "<button type='button' class='button-danger' onclick='rebootDevice()'>Reboot Device</button>"
            "<span class='muted'>Settings are kept. The page reloads after 10 s.</span></div>\n";
    html += "</div>\n</div>\n";
    emit(html);
  }

  // -- Script ---------------------------------------------------------------
  emit(R"rawjs(
<script>
// Tabs: open the tab from the URL hash, else the last one used, else the first.
var TAB_IDS = Array.prototype.map.call(document.querySelectorAll('.tab'),
                                       function (t) { return t.id.slice(4); });
function showTab(id) {
  if (TAB_IDS.indexOf(id) < 0) id = TAB_IDS[0];
  document.querySelectorAll('.tab').forEach(function (t) {
    t.classList.toggle('active', t.id === 'tab-' + id);
  });
  document.querySelectorAll('.tabs button').forEach(function (b) {
    b.classList.toggle('active', b.getAttribute('data-tab') === id);
  });
  document.getElementById('f-tab').value = id;
  try { localStorage.setItem('setupTab', id); } catch (e) {}
  // Also drops ?saved=1, so a reload doesn't show the banner again.
  if (location.search || location.hash !== '#' + id)
    history.replaceState(null, '', location.pathname + '#' + id);
}
(function () {
  var id = location.hash.slice(1);
  if (TAB_IDS.indexOf(id) < 0) { try { id = localStorage.getItem('setupTab'); } catch (e) { id = null; } }
  showTab(id);
  // A field that fails validation on a hidden tab: switch to it so the
  // browser can show its message.
  document.getElementById('cfg').addEventListener('invalid', function (e) {
    var t = e.target.closest('.tab');
    if (t && !t.classList.contains('active')) showTab(t.id.slice(4));
  }, true);
  document.querySelectorAll('.switch input').forEach(function (cb) {
    cb.addEventListener('change', function () {
      var s = cb.closest('.switch-container').querySelector('.sw-state');
      if (s) s.textContent = cb.checked ? 'Enabled' : 'Disabled';
    });
  });
})();

function updateHeat() {
  var fixed = document.getElementById('f-heatMode').value === '1';
  document.getElementById('f-heatMin').disabled = !fixed;
  document.getElementById('f-heatMax').disabled = !fixed;
}
function updateEdge() {
  document.getElementById('f-cloudEdge').disabled = document.getElementById('f-cloudPxRgn').value === '0';
}
function updateEnvAddr() {
  var v = document.getElementById('f-dhtType').value;
  document.getElementById('f-bmpRow').style.display = (v === '4' || v === '5') ? '' : 'none';
}

async function wifiScan() {
  const btn = document.getElementById('wifi-scan-btn');
  btn.textContent = 'Scanning\u2026'; btn.disabled = true;
  try {
    const nets = await fetch('/wifi/scan').then(r => r.json());
    const sel = document.getElementById('wifi-sel');
    sel.innerHTML = '<option value="">-- select --</option>';
    nets.sort((a,b) => b.rssi - a.rssi).forEach(n => {
      const o = document.createElement('option');
      o.value = n.ssid;
      o.textContent = n.ssid + '  (' + n.rssi + ' dBm' + (n.open ? ', open' : '') + ')';
      sel.appendChild(o);
    });
    document.getElementById('wifi-sel-row').style.display = '';
  } catch(e) { alert('Scan failed \u2013 try again'); }
  btn.textContent = 'Scan for Networks'; btn.disabled = false;
}
function wifiSelChanged() {
  const v = document.getElementById('wifi-sel').value;
  if (v) document.getElementById('wifi-ssid').value = v;
}
async function wifiConnect() {
  const ssid = document.getElementById('wifi-ssid').value.trim();
  const pass = document.getElementById('wifi-pass').value;
  const msg  = document.getElementById('wifi-msg');
  if (!ssid) { msg.textContent = 'Enter an SSID.'; return; }
  if (!confirm('Connect to "' + ssid + '"?\nThe device will reconnect \u2013 your browser connection may be briefly interrupted.')) return;
  msg.textContent = 'Connecting\u2026';
  try {
    await fetch('/wifi/connect', {
      method: 'POST',
      headers: {'Content-Type': 'application/x-www-form-urlencoded'},
      body: 'ssid=' + encodeURIComponent(ssid) + '&password=' + encodeURIComponent(pass)
    });
  } catch(e) {}
  msg.textContent = 'Connecting to "' + ssid + '"\u2026 If the IP changes, reconnect at the new address. If it fails, use Reset WiFi Settings below.';
}
function resetWifi() {
  if (!confirm('Reset WiFi? The device will restart and create a setup AP.')) return;
  fetch('/reset_wifi', {method:'POST'})
    .then(() => alert('Resetting. Connect to WiFi AP: SkyCond-Setup'))
    .catch(() => alert('Request sent'));
}
function rebootDevice() {
  if (!confirm('Reboot the device?')) return;
  fetch('/reboot', {method:'POST'})
    .then(() => alert('Rebooting. Page will reload in 10 seconds.'))
    .catch(() => alert('Request sent'));
  setTimeout(() => location.reload(), 10000);
}
</script>
</body></html>
)rawjs");
}

// ---------------------------------------------------------------------------
// Firmware page - frames ElegantOTA's /update in the site layout
// ---------------------------------------------------------------------------
inline String getFirmwarePage()
{
  String html = getPageHeader("Firmware Update - Sky Conditions");
  html += getPageTitle("Firmware Update");
  html += getNavBar();
  html += R"rawhtml(
<div class='card'>
<p class='muted' style='margin-top:0'>Upload the compiled firmware <code>.bin</code>
(Arduino IDE: Sketch &rarr; Export Compiled Binary). The device restarts when the upload finishes.</p>
<iframe id='ota' src='/update' title='Firmware upload'
        style='width:100%;height:520px;border:0;background:transparent;display:block' onload='otaLoaded()'></iframe>
</div>
<script>
try { localStorage.setItem('theme', 'dark'); } catch (e) {}       // ElegantOTA reads this
function otaLoaded() {
  var f = document.getElementById('ota');
  try {
    var doc = f.contentDocument;                                   // same origin, so allowed
    doc.documentElement.classList.add('dark');
    var st = doc.createElement('style');
    st.textContent = 'html,body,body>div{background:transparent!important}';
    doc.head.appendChild(st);                                      // let the card colour show through
  } catch (e) {}
}
setInterval(function () {                                          // grow/shrink with the upload UI
  var f = document.getElementById('ota');
  try {
    var h = f.contentDocument.documentElement.scrollHeight;
    if (h > 100 && Math.abs(h - f.offsetHeight) > 4) f.style.height = h + 'px';
  } catch (e) {}
}, 500);
</script>
</body></html>
)rawhtml";
  return html;
}

// ---------------------------------------------------------------------------
// Trends page - time-series charts for temperature and brightness
// ---------------------------------------------------------------------------
inline String getTrendsPage(bool rainEnabled)
{
  String html = getPageHeader("Trends - Sky Conditions");
  html += getPageTitle("Trends");
  html += getNavBar();

  // Time-range selector card
  html += "<div class='card'>\n";
  html += "<div>\n";
  html += "  <button class='tbtn' onclick='setRange(5,this)'>5 min</button>\n";
  html += "  <button class='tbtn' onclick='setRange(30,this)'>30 min</button>\n";
  html += "  <button class='tbtn act' onclick='setRange(60,this)'>1 hour</button>\n";
  html += "  <button class='tbtn' onclick='setRange(120,this)'>2 hours</button>\n";
  html += "  <button class='tbtn' onclick='setRange(360,this)'>6 hours</button>\n";
  html += "  <button class='tbtn' onclick='setRange(720,this)'>12 hours</button>\n";
  html += "  <button class='tbtn' onclick='setRange(1440,this)'>24 hours</button>\n";
  html += "</div>\n";
  html += "<div id='hist-status'>Loading...</div>\n";
  html += "</div>\n";

  // Chart canvases
  html += "<div class='card'><h2>Temperature (&deg;C)</h2>"
          "<canvas id='cT' height='210' style='width:100%;display:block'></canvas></div>\n";
  html += "<div class='card'><h2>Cloud Cover (%)</h2>"
          "<canvas id='cC' height='170' style='width:100%;display:block'></canvas></div>\n";
  html += "<div class='card'><h2>Sky Brightness (lux)</h2>"
          "<canvas id='cL' height='170' style='width:100%;display:block'></canvas></div>\n";
  html += "<div class='card'><h2>Sky Quality (mag/arcsec&sup2;)</h2>"
          "<canvas id='cQ' height='170' style='width:100%;display:block'></canvas></div>\n";

  if (rainEnabled)
    html += "<div class='card'><h2>Rain / Snow</h2>"
            "<canvas id='cR' height='80' style='width:100%;display:block'></canvas></div>\n";

  html += R"rawjs(
<script>
// --- State ------------------------------------------------------------------
let curMinutes = 60;
const NO_DATA = 'No data yet \u2013 wait for the first 30-second bucket to close.';

function setRange(m, btn) {
  curMinutes = m;
  document.querySelectorAll('.tbtn').forEach(b => b.classList.remove('act'));
  btn.classList.add('act');
  loadData();
}

// --- Data fetch -------------------------------------------------------------
function loadData() {
  document.getElementById('hist-status').textContent = 'Loading\u2026';
  fetch('/history.json?minutes=' + curMinutes)
    .then(r => r.json())
    .then(d => {
      const resTxt = d.res < 60 ? d.res + ' s' : (d.res / 60) + ' min';
      document.getElementById('hist-status').textContent =
        d.count + ' samples \u00B7 ' + resTxt + ' resolution';
      renderAll(d);
    })
    .catch(e => {
      document.getElementById('hist-status').textContent = 'Error: ' + e;
    });
}

// --- Render all charts ------------------------------------------------------
function renderAll(d) {
  drawChart('cT', [
    { label:'Sky',     color:'#a29bfe', data:d.sky,  lo:d.sky_lo,  hi:d.sky_hi  },
    { label:'Min',     color:'#74b9ff', data:d.fmin, lo:d.fmin_lo, hi:d.fmin_hi },
    { label:'Max',     color:'#fd79a8', data:d.fmax, lo:d.fmax_lo, hi:d.fmax_hi },
    { label:'Median',  color:'#55efc4', data:d.med,  lo:d.med_lo,  hi:d.med_hi  },
    { label:'Ambient', color:'#ffeaa7', data:d.amb,  lo:d.amb_lo,  hi:d.amb_hi  }
  ], d.t, d.now, '\u00B0C');

  drawChart('cC', [
    { label:'Cloud Mean',     color:'#e0e0e0', data:d.cloud,   lo:d.cloud_lo,   hi:d.cloud_hi   },
    { label:'Cloud Per-Pixel',color:'#b0b0b0', data:d.cloudpx, lo:d.cloudpx_lo, hi:d.cloudpx_hi }
  ], d.t, d.now, '%');

  drawChart('cL', [
    { label:'Lux', color:'#fdcb6e', data:d.lux, lo:d.lux_lo, hi:d.lux_hi }
  ], d.t, d.now, 'lux');

  drawChart('cQ', [
    { label:'SQM', color:'#00cec9', data:d.sqm, lo:d.sqm_lo, hi:d.sqm_hi }
  ], d.t, d.now, 'mag/sq"');

  if (document.getElementById('cR')) drawRainChart('cR', d.rain, d.t, d.now);
}

// --- Rain / snow block chart ------------------------------------------------
function drawRainChart(id, data, timestamps, nowMs) {
  const canvas = document.getElementById(id);
  if (!canvas) return;
  canvas.width = canvas.parentElement.clientWidth - 40;
  const W = canvas.width, H = canvas.height;
  const ctx = canvas.getContext('2d');
  const P = { t: 8, r: 14, b: 22, l: 54 };
  const cw = W - P.l - P.r, ch = H - P.t - P.b;

  ctx.fillStyle = '#0f3460';
  ctx.fillRect(0, 0, W, H);

  const n = timestamps ? timestamps.length : 0;
  if (n === 0 || !data) {
    ctx.fillStyle = '#8899aa'; ctx.textAlign = 'center'; ctx.font = '13px sans-serif';
    ctx.fillText(NO_DATA, W / 2, H / 2);
    return;
  }

  const t0 = timestamps[0], t1 = timestamps[n - 1];
  const tSpan = (t1 - t0) || 1;
  const tx = i => P.l + (timestamps[i] - t0) / tSpan * cw;

  for (let i = 0; i < n; i++) {
    const v = data[i];
    if (v === null) continue;
    ctx.fillStyle = (v >= 0.5) ? '#e57373' : '#81c784';
    const x1 = tx(i);
    const x2 = (i + 1 < n) ? tx(i + 1) : P.l + cw;
    ctx.fillRect(x1, P.t, Math.max(x2 - x1, 1), ch);
  }

  // Y labels
  ctx.fillStyle = '#8899aa'; ctx.font = '10px monospace'; ctx.textAlign = 'right';
  ctx.fillText('Wet', P.l - 3, P.t + 9);
  ctx.fillText('Dry', P.l - 3, P.t + ch);

  // X-axis time labels
  const xTicks = 6;
  for (let k = 0; k <= xTicks; k++) {
    const xp = P.l + cw * k / xTicks;
    const tAt = t0 + tSpan * k / xTicks;
    const mAgo = (tAt - nowMs) / 60000;
    const lbl = (k === xTicks) ? 'now' :
                (Math.abs(mAgo) < 90 ? Math.round(-mAgo) + 'm' :
                                       (-mAgo / 60).toFixed(1) + 'h');
    ctx.fillStyle = '#8899aa'; ctx.textAlign = 'center'; ctx.font = '10px sans-serif';
    ctx.fillText(lbl, xp, P.t + ch + 14);
  }

  // Legend
  let lx = P.l;
  const ly = H - 2;
  ctx.font = '10px sans-serif'; ctx.textAlign = 'left';
  ctx.fillStyle = '#81c784'; ctx.fillRect(lx, ly - 8, 12, 8);
  ctx.fillStyle = '#e0e0e0'; ctx.fillText('Dry', lx + 15, ly);
  lx += 45;
  ctx.fillStyle = '#e57373'; ctx.fillRect(lx, ly - 8, 12, 8);
  ctx.fillStyle = '#e0e0e0'; ctx.fillText('Wet / Snow', lx + 15, ly);
}

// --- Chart renderer ---------------------------------------------------------
function drawChart(id, series, timestamps, nowMs, yUnit) {
  const canvas = document.getElementById(id);
  if (!canvas) return;
  canvas.width = canvas.parentElement.clientWidth - 40;
  const W = canvas.width, H = canvas.height;
  const ctx = canvas.getContext('2d');
  const P = { t: 12, r: 14, b: 38, l: 54 };
  const cw = W - P.l - P.r, ch = H - P.t - P.b;

  // Background
  ctx.fillStyle = '#0f3460';
  ctx.fillRect(0, 0, W, H);

  const n = timestamps ? timestamps.length : 0;
  if (n === 0) {
    ctx.fillStyle = '#8899aa'; ctx.textAlign = 'center';
    ctx.font = '13px sans-serif';
    ctx.fillText(NO_DATA, W / 2, H / 2);
    return;
  }

  // X span in ms
  const t0 = timestamps[0], t1 = timestamps[n - 1];
  const tSpan = (t1 - t0) || 1;

  // Y range across all values (avg + lo + hi)
  let yMn = Infinity, yMx = -Infinity;
  for (const s of series) {
    for (const arr of [s.data, s.lo || [], s.hi || []]) {
      for (const v of arr) {
        if (v !== null && isFinite(v)) {
          if (v < yMn) yMn = v;
          if (v > yMx) yMx = v;
        }
      }
    }
  }
  if (!isFinite(yMn)) { yMn = 0; yMx = 1; }
  const ySpan = yMx - yMn || 1;
  yMn -= ySpan * 0.05; yMx += ySpan * 0.05;

  // Coordinate transforms
  const tx = i => P.l + (timestamps[i] - t0) / tSpan * cw;
  const ty = v => P.t + (1 - (v - yMn) / (yMx - yMn)) * ch;

  // Grid
  ctx.strokeStyle = '#2d3561'; ctx.lineWidth = 1;
  const yTicks = 4;
  for (let k = 0; k <= yTicks; k++) {
    const yp = P.t + ch * k / yTicks;
    ctx.beginPath(); ctx.moveTo(P.l, yp); ctx.lineTo(P.l + cw, yp); ctx.stroke();
    const val = yMx - (yMx - yMn) * k / yTicks;
    ctx.fillStyle = '#8899aa'; ctx.font = '10px monospace'; ctx.textAlign = 'right';
    ctx.fillText(val.toFixed(ySpan > 10 ? 0 : 1), P.l - 3, yp + 4);
  }

  const xTicks = 6;
  for (let k = 0; k <= xTicks; k++) {
    const xp = P.l + cw * k / xTicks;
    ctx.beginPath(); ctx.moveTo(xp, P.t); ctx.lineTo(xp, P.t + ch); ctx.stroke();
    const tAt  = t0 + tSpan * k / xTicks;
    const mAgo = (tAt - nowMs) / 60000;   // negative = past
    const lbl  = (k === xTicks) ? 'now' :
                 (Math.abs(mAgo) < 90 ? Math.round(-mAgo) + 'm' :
                                        (-mAgo / 60).toFixed(1) + 'h');
    ctx.fillStyle = '#8899aa'; ctx.textAlign = 'center'; ctx.font = '10px sans-serif';
    ctx.fillText(lbl, xp, P.t + ch + 14);
  }

  // Y-axis unit label (rotated)
  ctx.save();
  ctx.translate(11, P.t + ch / 2); ctx.rotate(-Math.PI / 2);
  ctx.fillStyle = '#8899aa'; ctx.font = '10px sans-serif'; ctx.textAlign = 'center';
  ctx.fillText(yUnit, 0, 0);
  ctx.restore();

  // Shaded lo/hi bands
  for (const s of series) {
    if (!s.lo || !s.hi) continue;
    ctx.fillStyle = s.color + '28';
    ctx.beginPath();
    let started = false;
    for (let i = 0; i < n; i++) {
      const v = s.lo[i];
      if (v === null) { started = false; continue; }
      if (!started) { ctx.moveTo(tx(i), ty(v)); started = true; }
      else ctx.lineTo(tx(i), ty(v));
    }
    for (let i = n - 1; i >= 0; i--) {
      const v = s.hi[i]; if (v !== null) ctx.lineTo(tx(i), ty(v));
    }
    ctx.closePath(); ctx.fill();
  }

  // Lines
  for (const s of series) {
    ctx.strokeStyle = s.color; ctx.lineWidth = 1.5;
    ctx.beginPath();
    let started = false;
    for (let i = 0; i < n; i++) {
      const v = s.data[i];
      if (v === null) { started = false; continue; }
      if (!started) { ctx.moveTo(tx(i), ty(v)); started = true; }
      else ctx.lineTo(tx(i), ty(v));
    }
    ctx.stroke();
  }

  // Legend along the bottom
  let lx = P.l;
  const ly = H - 8;
  ctx.font = '10px sans-serif'; ctx.textAlign = 'left';
  for (const s of series) {
    ctx.strokeStyle = s.color; ctx.lineWidth = 2;
    ctx.beginPath(); ctx.moveTo(lx, ly - 4); ctx.lineTo(lx + 14, ly - 4); ctx.stroke();
    ctx.fillStyle = '#e0e0e0';
    ctx.fillText(s.label, lx + 17, ly);
    lx += 17 + ctx.measureText(s.label).width + 14;
  }
}

// --- Boot & auto-refresh ----------------------------------------------------
loadData();
setInterval(loadData, 30000);
</script>
)rawjs";

  html += "</body></html>";
  return html;
}

// ---------------------------------------------------------------------------
// Sky History page - 90-day calendar of rain, night SQM, and clear-sky hours
// ---------------------------------------------------------------------------
inline String getSkyHistoryPage(bool rainEnabled)
{
  String html = getPageHeader("Sky History - Sky Conditions");
  html += getPageTitle("Sky History");
  html += getNavBar();

  html += "<div class='card'>\n";
  html += "<div style='margin-bottom:10px'>\n"
          "  <button class='tbtn mode-btn act' onclick=\"setMode('rain',this)\">Rain</button>\n"
          "  <button class='tbtn mode-btn' onclick=\"setMode('sqm',this)\">SQM (night)</button>\n"
          "  <button class='tbtn mode-btn' onclick=\"setMode('clear',this)\">Clear Sky</button>\n"
          "</div>\n";
  html += "<div class='cal-nav'>\n"
          "  <button id='calPrev' onclick='navMonth(-1)' aria-label='Previous month'>&#8249;</button>\n"
          "  <div class='cal-title' id='calTitle'>&nbsp;</div>\n"
          "  <button id='calNext' onclick='navMonth(1)' aria-label='Next month'>&#8250;</button>\n"
          "</div>\n";
  html += "<div class='cal-grid' id='calDow'></div>\n";
  html += "<div class='cal-grid' id='calGrid' style='margin-top:6px'></div>\n";
  html += "<div class='cal-summary' id='calSummary'>Loading&hellip;</div>\n";
  html += "<div class='cal-legend' id='calLegend'></div>\n";
  html += "<p class='muted' style='margin-top:16px'>Days and durations are shown in your browser's local "
          "time zone. Recording gaps (device powered off or clock not yet synced) appear as no data. "
          "Night and clear-sky thresholds are set on the Setup page (Sky Quality tab).</p>\n";
  html += "<div class='button-row'><button onclick='exportCsv()'>Export CSV</button></div>\n";
  html += "</div>\n";

  html += "<script>const RAIN_SENSOR_ENABLED = ";
  html += (rainEnabled ? "true" : "false");
  html += ";</script>\n";

  html += R"rawjs(
<script>
const DOW = ['Sun','Mon','Tue','Wed','Thu','Fri','Sat'];
const MONTHS = ['January','February','March','April','May','June','July',
                 'August','September','October','November','December'];

let dayStats = {};       // 'YYYY-MM-DD' (local) -> {rain, clearDay, clearNight, sqmAvgs:[], sqmPeaks:[]}
let minDateKey = null;   // earliest local date covered by fetched data
let maxDateKey = null;   // latest local date covered (= today, server clock)
let viewYear, viewMonth; // currently displayed month (0-based month)
let mode = 'rain';       // 'rain' | 'sqm' | 'clear'

function pad2(n) { return n < 10 ? '0' + n : '' + n; }
function dateKey(d) { return d.getFullYear() + '-' + pad2(d.getMonth() + 1) + '-' + pad2(d.getDate()); }

function fmtDur(sec) {
  if (!sec || sec <= 0) return '0m';
  const h = Math.floor(sec / 3600), m = Math.round((sec % 3600) / 60);
  return h > 0 ? (h + 'h ' + m + 'm') : (m + 'm');
}

function wetClass(sec)  { if (sec < 1800)  return 'wet1'; if (sec < 7200)  return 'wet2'; return 'wet3'; }
function clearClass(sec){ if (sec < 7200)  return 'clr1'; if (sec < 21600) return 'clr2'; return 'clr3'; }
function sqmClass(avg)  { if (avg < 18)    return 'sqm1'; if (avg < 20)    return 'sqm2';
                           if (avg < 21.5) return 'sqm3'; return 'sqm4'; }

function setMode(m, btn) {
  mode = m;
  document.querySelectorAll('.mode-btn').forEach(b => b.classList.remove('act'));
  btn.classList.add('act');
  renderMonth();
}

function loadData() {
  fetch('/skyhistory.json?days=90')
    .then(r => r.json())
    .then(d => {
      if (!d.synced || !d.rainSec || d.rainSec.length === 0) {
        document.getElementById('calSummary').textContent =
          'No data yet \u2013 device clock is not synced.';
        return;
      }
      dayStats = {};
      const t0 = d.t0Epoch * 1000;
      for (let i = 0; i < d.rainSec.length; i++) {
        const key = dateKey(new Date(t0 + i * 3600000));
        if (!dayStats[key]) dayStats[key] = { rain: 0, clearDay: 0, clearNight: 0, sqmAvgs: [], sqmPeaks: [] };
        const s = dayStats[key];
        s.rain       += d.rainSec[i];
        s.clearDay   += d.clearDaySec[i];
        s.clearNight += d.clearNightSec[i];
        if (d.sqmAvg[i]  !== null) s.sqmAvgs.push(d.sqmAvg[i]);
        if (d.sqmPeak[i] !== null) s.sqmPeaks.push(d.sqmPeak[i]);
      }
      minDateKey = dateKey(new Date(t0));
      maxDateKey = dateKey(new Date(d.nowEpoch * 1000));
      viewYear  = new Date(d.nowEpoch * 1000).getFullYear();
      viewMonth = new Date(d.nowEpoch * 1000).getMonth();
      renderMonth();
    })
    .catch(e => {
      document.getElementById('calSummary').textContent = 'Error: ' + e;
    });
}

function navMonth(delta) {
  viewMonth += delta;
  if (viewMonth < 0)  { viewMonth = 11; viewYear--; }
  if (viewMonth > 11) { viewMonth = 0;  viewYear++; }
  renderMonth();
}

// Builds one day cell's class + inner HTML for the current mode, and folds
// its contribution into the running month accumulator (accA/accB/accCount).
function buildCell(day, key, todayKey, acc) {
  const inWindow = key >= minDateKey && key <= maxDateKey;
  let cls = 'cal-cell';
  let lines = '';

  if (!inWindow) {
    cls += ' nodata';
  } else {
    const s = dayStats[key] || { rain: 0, clearDay: 0, clearNight: 0, sqmAvgs: [], sqmPeaks: [] };
    if (mode === 'rain') {
      if (s.rain > 0) {
        cls += ' ' + wetClass(s.rain);
        lines = "<div class='dur'>" + fmtDur(s.rain) + '</div>';
        acc.count++; acc.a += s.rain;
      }
    } else if (mode === 'sqm') {
      if (s.sqmAvgs.length) {
        const avg  = s.sqmAvgs.reduce((x, y) => x + y, 0) / s.sqmAvgs.length;
        const peak = Math.max(...s.sqmPeaks);
        cls += ' ' + sqmClass(avg);
        lines = "<div class='dur'>" + avg.toFixed(1) + "</div><div class='dur2'>pk " + peak.toFixed(1) + '</div>';
        acc.count++; acc.a += avg; acc.b = Math.max(acc.b, peak);
      }
    } else { // clear
      if (s.clearDay > 0 || s.clearNight > 0) {
        cls += ' ' + clearClass(s.clearDay + s.clearNight);
        lines = "<div class='dur'>D " + fmtDur(s.clearDay) + "</div><div class='dur2'>N " + fmtDur(s.clearNight) + '</div>';
      }
      acc.a += s.clearDay; acc.b += s.clearNight;
    }
  }
  if (key === todayKey) cls += ' today';

  return "<div class='" + cls + "'><div class='d'>" + day + '</div>' + lines + '</div>';
}

function renderMonth() {
  document.getElementById('calTitle').textContent = MONTHS[viewMonth] + ' ' + viewYear;

  const dowEl = document.getElementById('calDow');
  dowEl.innerHTML = DOW.map(d => "<div class='cal-dow'>" + d + '</div>').join('');

  const firstOfMonth = new Date(viewYear, viewMonth, 1);
  const daysInMonth   = new Date(viewYear, viewMonth + 1, 0).getDate();
  const startDow      = firstOfMonth.getDay();
  const todayKey      = dateKey(new Date());

  let cells = '';
  for (let i = 0; i < startDow; i++) cells += "<div class='cal-cell empty'></div>";

  const acc = { count: 0, a: 0, b: 0 };
  for (let day = 1; day <= daysInMonth; day++) {
    const key = dateKey(new Date(viewYear, viewMonth, day));
    cells += buildCell(day, key, todayKey, acc);
  }

  document.getElementById('calGrid').innerHTML = cells;

  let summary;
  if (mode === 'rain') {
    summary = 'Rain days: ' + acc.count + '     Total: ' + fmtDur(acc.a);
    if (!RAIN_SENSOR_ENABLED) summary += '  (rain sensor disabled in Setup)';
  } else if (mode === 'sqm') {
    summary = acc.count
      ? ('Nights with data: ' + acc.count + '     Avg: ' + (acc.a / acc.count).toFixed(2) + '     Best: ' + acc.b.toFixed(2))
      : 'No night SQM data this month';
  } else {
    summary = 'Clear (day): ' + fmtDur(acc.a) + '     Clear (night): ' + fmtDur(acc.b);
  }
  document.getElementById('calSummary').textContent = summary;

  const legends = {
    rain:  "<span><i style='background:#5a2626'></i>&lt;30m</span><span><i style='background:#8e3a2c'></i>30m&ndash;2h</span><span><i style='background:#c0392b'></i>&gt;2h</span>",
    sqm:   "<span><i style='background:#6b4226'></i>&lt;18</span><span><i style='background:#5a5a3a'></i>18&ndash;20</span><span><i style='background:#2f6690'></i>20&ndash;21.5</span><span><i style='background:#1a5fb4'></i>&#8805;21.5</span>",
    clear: "<span><i style='background:#2d5016'></i>&lt;2h</span><span><i style='background:#3d7a1f'></i>2&ndash;6h</span><span><i style='background:#4caf50'></i>&gt;6h</span>"
  };
  document.getElementById('calLegend').innerHTML = legends[mode];

  document.getElementById('calPrev').disabled =
    (viewYear + '-' + pad2(viewMonth + 1)) <= minDateKey.slice(0, 7);
  document.getElementById('calNext').disabled =
    (viewYear + '-' + pad2(viewMonth + 1)) >= maxDateKey.slice(0, 7);
}

function exportCsv() {
  const keys = Object.keys(dayStats).sort();
  let csv = 'date,rain_seconds,rain_hms,clear_day_seconds,clear_night_seconds,sqm_avg,sqm_peak\n';
  for (const k of keys) {
    const s = dayStats[k];
    const h = Math.floor(s.rain / 3600), m = Math.floor((s.rain % 3600) / 60), sec = s.rain % 60;
    const avg  = s.sqmAvgs.length  ? (s.sqmAvgs.reduce((x, y) => x + y, 0) / s.sqmAvgs.length).toFixed(2) : '';
    const peak = s.sqmPeaks.length ? Math.max(...s.sqmPeaks).toFixed(2) : '';
    csv += [k, s.rain, h + ':' + pad2(m) + ':' + pad2(sec), s.clearDay, s.clearNight, avg, peak].join(',') + '\n';
  }
  const blob = new Blob([csv], { type: 'text/csv' });
  const url  = URL.createObjectURL(blob);
  const a    = document.createElement('a');
  a.href = url; a.download = 'sky_history.csv';
  document.body.appendChild(a); a.click(); document.body.removeChild(a);
  URL.revokeObjectURL(url);
}

loadData();
setInterval(loadData, 300000);
</script>
)rawjs";

  html += "</body></html>";
  return html;
}

// ---------------------------------------------------------------------------
// Console page - live debug log viewer
// ---------------------------------------------------------------------------
inline String getConsolePage()
{
  String html = getPageHeader("Console - Sky Conditions");
  html += getPageTitle("Console");
  html += getNavBar();

  html +=
    "<div class='card'>\n"
    "<div style='margin-bottom:8px;display:flex;align-items:center;gap:8px;flex-wrap:wrap;'>\n"
    "  <button id='btn-pause' class='tbtn'>Pause</button>\n"
    "  <button id='btn-clear' class='tbtn'>Clear</button>\n"
    "  <button id='btn-copy'  class='tbtn'>Copy</button>\n"
    "  <span id='con-status' class='muted'>Polling every 2 s</span>\n"
    "</div>\n"
    "<div id='log'></div>\n"
    "</div>\n";

  html += R"rawjs(
<script>
(function () {
function esc(s){
  return s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;');
}
function fmtMs(ms){
  var h=Math.floor(ms/3600000),
      m=Math.floor((ms%3600000)/60000),
      s=Math.floor((ms%60000)/1000);
  return [h,m,s].map(function(x){return String(x).padStart(2,'0');}).join(':');
}

var lastSeq=-1, paused=false;

function render(entries){
  var el=document.getElementById('log');
  var atBottom=(el.scrollHeight-el.scrollTop-el.clientHeight)<40;
  el.innerHTML='';
  for(var i=0;i<entries.length;i++){
    var e=entries[i];
    var rep=e.n>1?' <span class="rep">(\xd7'+e.n+')</span>':'';
    var d=document.createElement('div');
    d.className='le';
    d.innerHTML='<span class="ts">['+fmtMs(e.ms)+']</span> '+esc(e.msg)+rep;
    el.appendChild(d);
  }
  if(atBottom) el.scrollTop=el.scrollHeight;
}

function poll(){
  if(paused) return;
  fetch('/console.json').then(function(r){return r.json();}).then(function(d){
    if(d.seq!==lastSeq){lastSeq=d.seq;render(d.entries);}
    document.getElementById('con-status').textContent=
      'Last update: '+new Date().toLocaleTimeString()+' \u2022 '+d.entries.length+' lines';
  }).catch(function(){});
}

document.getElementById('btn-pause').onclick=function(){
  paused=!paused;
  this.textContent=paused?'Resume':'Pause';
  this.classList.toggle('act', paused);
};
document.getElementById('btn-clear').onclick=function(){
  document.getElementById('log').innerHTML='';
  lastSeq=-1;
};
document.getElementById('btn-copy').onclick=function(){
  var lines=document.querySelectorAll('#log .le');
  var text=Array.from(lines).map(function(el){return el.innerText;}).join('\n');
  var btn=this;
  navigator.clipboard.writeText(text).then(function(){
    btn.textContent='Copied!';
    setTimeout(function(){btn.textContent='Copy';},1500);
  }).catch(function(){
    btn.textContent='Failed';
    setTimeout(function(){btn.textContent='Copy';},1500);
  });
};

setInterval(poll,2000);
poll();
})();
</script>
)rawjs";

  html += "</body></html>";
  return html;
}

#endif // HTML_TEMPLATES_H
