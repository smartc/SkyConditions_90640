#ifndef HTML_TEMPLATES_H
#define HTML_TEMPLATES_H

#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "sky_sensor.h"
#include "config_store.h"
#include "rain_sensor.h"

// ---------------------------------------------------------------------------
// Shared CSS
// ---------------------------------------------------------------------------
inline String getCommonStyles()
{
  return
    "* { box-sizing: border-box; }\n"
    "body { font-family: Arial, sans-serif; background: #1a1a2e; color: #e0e0e0; margin: 0; padding: 16px; }\n"
    "h1, h2 { color: #a0c4ff; }\n"
    "a { color: #74b9ff; text-decoration: none; }\n"
    "a:hover { text-decoration: underline; }\n"
    ".card { background: #16213e; border-radius: 8px; padding: 16px; margin-bottom: 16px;"
             " box-shadow: 0 2px 8px rgba(0,0,0,0.4); }\n"
    "table { border-collapse: collapse; width: 100%; }\n"
    "th, td { border: 1px solid #2d3561; padding: 8px; text-align: left; }\n"
    "th { background: #0f3460; }\n"
    ".nav { margin-bottom: 16px; }\n"
    ".nav a { display: inline-block; background: #0f3460; color: #e0e0e0; padding: 8px 14px;"
              " border-radius: 4px; margin-right: 8px; }\n"
    ".nav a:hover { background: #16213e; text-decoration: none; }\n"
    ".nav a.warn { background: #7f4f00; }\n"
    ".tbtn { display:inline-block; padding:6px 12px; margin:2px; border-radius:4px;"
             " background:#0f3460; color:#e0e0e0; cursor:pointer; border:none;"
             " font-size:0.85em; }\n"
    ".tbtn:hover { background:#1a4a8a; }\n"
    ".tbtn.act { background:#4a90d9; color:#fff; }\n"
    "#hist-status { font-size:0.8em; color:#636e72; margin-top:8px; }\n"
    ".btn { display: inline-block; padding: 8px 16px; border-radius: 4px; cursor: pointer;"
            " border: none; color: white; }\n"
    ".btn-danger { background: #c0392b; }\n"
    ".btn-danger:hover { background: #96281b; }\n"
    ".stat-grid { display: grid; grid-template-columns: repeat(auto-fit, minmax(140px,1fr));"
                 " gap: 12px; }\n"
    ".stat-box { background: #0f3460; border-radius: 6px; padding: 12px; text-align: center; }\n"
    ".stat-label { font-size: 0.75em; color: #74b9ff; text-transform: uppercase; letter-spacing: 1px; }\n"
    ".stat-value { font-size: 1.6em; font-weight: bold; margin-top: 4px; }\n"
    ".sky-temp  { color: #a29bfe; }\n"
    ".min-temp  { color: #74b9ff; }\n"
    ".max-temp  { color: #fd79a8; }\n"
    ".med-temp  { color: #55efc4; }\n"
    ".amb-temp  { color: #ffeaa7; }\n"
    "#thermal-canvas { display: block; margin: 0 auto; image-rendering: pixelated; border-radius: 4px; }\n"
    "#ws-status { font-size: 0.8em; color: #636e72; margin-top: 6px; text-align: center; }\n";
}

// ---------------------------------------------------------------------------
// Shared page header
// ---------------------------------------------------------------------------
inline String getPageHeader(const String& title)
{
  return
    "<!DOCTYPE html><html>\n"
    "<head><title>" + title + "</title>\n"
    "<meta charset='UTF-8'>\n"
    "<meta name='viewport' content='width=device-width, initial-scale=1'>\n"
    "<style>" + getCommonStyles() + "</style>\n"
    "</head>\n<body>\n";
}

// ---------------------------------------------------------------------------
// Navigation bar
// ---------------------------------------------------------------------------
inline String getNavBar()
{
  return
    "<div class='nav'>\n"
    "<a href='/'>Home</a>\n"
    "<a href='/setup'>Setup</a>\n"
    "<a href='/trends'>Trends</a>\n"
    "<a href='/update' class='warn'>Update</a>\n"
    "</div>\n";
}

// ---------------------------------------------------------------------------
// Home page – live thermal view + reading cards
// ---------------------------------------------------------------------------
inline String getHomePage()
{
  String ip   = WiFi.localIP().toString();
  String host = "ws://" + ip + ":81";

  String html = getPageHeader("Sky Conditions Monitor");
  html += "<h1>Sky Conditions Monitor</h1>\n";
  html += getNavBar();

  // Live thermal canvas
  html += "<div class='card'>\n";
  html += "<h2>Live Thermal View</h2>\n";
  html += "<canvas id='thermal-canvas' width='640' height='480'></canvas>\n";
  html += "<div id='ws-status'>Connecting...</div>\n";
  html += "</div>\n";

  // Temperature statistics
  html += "<div class='card'>\n";
  html += "<h2>Current Readings</h2>\n";
  html += "<div class='stat-grid'>\n";
  html += "  <div class='stat-box'><div class='stat-label'>Sky Temp<br>(center 50% FOV)</div>"
          "    <div class='stat-value sky-temp' id='sky-temp'>--</div></div>\n";
  html += "  <div class='stat-box'><div class='stat-label'>Frame Min</div>"
          "    <div class='stat-value min-temp' id='min-temp'>--</div></div>\n";
  html += "  <div class='stat-box'><div class='stat-label'>Frame Max</div>"
          "    <div class='stat-value max-temp' id='max-temp'>--</div></div>\n";
  html += "  <div class='stat-box'><div class='stat-label'>Frame Median</div>"
          "    <div class='stat-value med-temp' id='med-temp'>--</div></div>\n";
  html += "  <div class='stat-box'><div class='stat-label'>Ambient</div>"
          "    <div class='stat-value amb-temp' id='amb-temp'>" +
          (skyConditions.hasData() ? String(skyConditions.getAmbientTemperature(), 1) + "°C" : "--") +
          "</div></div>\n";
  html += "  <div class='stat-box'><div class='stat-label'>Cloud Cover<br><span style='font-size:0.8em;color:#74b9ff'>Mean</span></div>"
          "    <div class='stat-value' style='color:#dfe6e9'>" +
          (skyConditions.hasData() ? String(skyConditions.getCloudCoverMean(), 0) + "%" : "--") +
          "</div></div>\n";
  html += "  <div class='stat-box'><div class='stat-label'>Cloud Cover<br><span style='font-size:0.8em;color:#74b9ff'>Per-Pixel</span></div>"
          "    <div class='stat-value' style='color:#b2bec3'>" +
          (skyConditions.hasData() ? String(skyConditions.getCloudCoverPixel(), 0) + "%" : "--") +
          "</div></div>\n";
  html += "  <div class='stat-box'><div class='stat-label'>Sky Brightness</div>"
          "    <div class='stat-value' style='color:#fdcb6e'>" +
          (skyConditions.hasBrightnessData() ? String(skyConditions.getLux(), 4) + " lux" : "--") +
          "</div></div>\n";
  html += "  <div class='stat-box'><div class='stat-label'>Sky Quality</div>"
          "    <div class='stat-value' style='color:#00cec9'>" +
          (skyConditions.hasBrightnessData() ? String(skyConditions.getSqm(), 2) + " mag/\"²" : "--") +
          "</div></div>\n";
  {
    bool wet      = rainIsWet();
    bool relayWet = rainData.relayWet;
    bool mbOk     = rainData.modbusOk;
    bool mbWet    = rainData.modbusWet;
    String mainColor  = wet ? "#74b9ff" : "#00b894";
    String mainLabel  = wet ? "WET"     : "DRY";
    String relayTxt   = relayWet                   ? "Relay: WET"   : "Relay: DRY";
    String modbusTxt  = mbOk ? (mbWet ? "RS485: WET" : "RS485: DRY") : "RS485: --";
    html += "  <div class='stat-box'><div class='stat-label'>Rain / Snow</div>"
            "    <div class='stat-value' style='color:" + mainColor + "'>" + mainLabel + "</div>"
            "    <div style='font-size:0.65em;color:#b2bec3;margin-top:4px'>" +
            relayTxt + " &bull; " + modbusTxt + "</div></div>\n";
  }
  html += "</div>\n";  // stat-grid
  html += "</div>\n";  // card

  // Thermal snapshot image (auto-refreshes every JPEG_INTERVAL_MS)
  html += "<div class='card'>\n";
  html += "<h2>Thermal Snapshot</h2>\n";
  html += "<p style='font-size:0.8em;color:#636e72'>Refreshes every " +
          String(JPEG_INTERVAL_MS / 1000) + " s &nbsp;&middot;&nbsp; "
          "<a href='/thermal.jpg'>direct link</a></p>\n";
  html += "<img id='thermal-snap' src='/thermal.jpg' "
          "style='display:block;margin:0 auto;max-width:100%;border-radius:4px;'>\n";
  html += "</div>\n";

  // Device info
  html += "<div class='card'>\n";
  html += "<h2>Device Information</h2>\n";
  html += "<table><tr><th>Property</th><th>Value</th></tr>\n";
  html += "<tr><td>Device Name</td><td>" + String(DEVICE_NAME)    + "</td></tr>\n";
  html += "<tr><td>Firmware</td><td>"    + String(MANUFACTURER_V) + "</td></tr>\n";
  html += "<tr><td>IP Address</td><td>"  + ip + "</td></tr>\n";
  html += "<tr><td>MAC Address</td><td>" + WiFi.macAddress()        + "</td></tr>\n";
  html += "<tr><td>SSID</td><td>"        + WiFi.SSID()              + "</td></tr>\n";
  html += "<tr><td>Signal</td><td>"      + String(WiFi.RSSI())      + " dBm</td></tr>\n";
  html += "<tr><td>ASCOM Port</td><td>"  + String(ALPACA_PORT)      + "</td></tr>\n";
  html += "<tr><td>Uptime</td><td><span id='uptime' data-s='" +
          String(millis()/1000) + "'></span></td></tr>\n";
  html += "<tr><td>Free Heap</td><td>"   + String(ESP.getFreeHeap()) + " bytes</td></tr>\n";
  html += "</table>\n";
  html += "</div>\n";

  // ---------------------------------------------------------------------------
  // JavaScript – WebSocket client + jet colormap renderer
  // ---------------------------------------------------------------------------
  html += R"rawjs(
<script>
const COLS = 32, ROWS = 24, PIXELS = 768;
const HEADER = 16;  // bytes: 4×float32 (min, max, median, skyTemp)
const canvas = document.getElementById('thermal-canvas');
const ctx    = canvas.getContext('2d');
const imgData = ctx.createImageData(COLS, ROWS);
const status  = document.getElementById('ws-status');

// ClearDarkSky colormap: t=0 (coldest/clear) → dark navy, t=1 (warmest/overcast) → white
const CDS_STOPS = [
  [0,62,126],[19,83,147],[38,102,166],[78,142,206],[98,162,226],
  [118,182,246],[153,217,217],[173,237,237],[193,193,193],[233,233,233],[250,250,250]
];
function cds(t) {
  if (t <= 0) return CDS_STOPS[0];
  if (t >= 1) return CDS_STOPS[10];
  const lo = Math.floor(t * 10);
  const f  = t * 10 - lo;
  return CDS_STOPS[lo].map((c, i) => Math.round(c + f * (CDS_STOPS[lo+1][i] - c)));
}

// Pre-compute colormap lookup table
const skyLT = new Array(256);
for (let i = 0; i < 256; i++) { skyLT[i] = cds(i/255); }

let ws, fpsCount = 0, lastFpsTime = Date.now(), fps = 0;

function connect() {
  ws = new WebSocket(')rawjs" + host + R"rawjs(');
  ws.binaryType = 'arraybuffer';

  ws.onopen    = () => { status.textContent = 'Connected'; };
  ws.onclose   = () => { status.textContent = 'Disconnected – reconnecting…';
                          setTimeout(connect, 1500); };
  ws.onerror   = () => { ws.close(); };

  ws.onmessage = (evt) => {
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

    // Scale 32×24 → 640×480 using an offscreen canvas
    const tmp = new OffscreenCanvas(COLS, ROWS);
    tmp.getContext('2d').putImageData(imgData, 0, 0);
    ctx.imageSmoothingEnabled = false;
    ctx.drawImage(tmp, 0, 0, canvas.width, canvas.height);

    // Draw center-FOV box
    const ccs = )rawjs" + String(CENTER_COL_START) + R"rawjs(,
          cce = )rawjs" + String(CENTER_COL_END + 1) + R"rawjs(,
          crs = )rawjs" + String(CENTER_ROW_START) + R"rawjs(,
          cre = )rawjs" + String(CENTER_ROW_END + 1) + R"rawjs(;
    const scaleX = canvas.width  / COLS;
    const scaleY = canvas.height / ROWS;
    ctx.strokeStyle = 'rgba(255,60,60,0.85)';
    ctx.lineWidth   = 2;
    ctx.strokeRect(ccs*scaleX, crs*scaleY,
                   (cce-ccs)*scaleX, (cre-crs)*scaleY);

    // Update stat readouts
    document.getElementById('sky-temp').textContent = skyT.toFixed(1) + '°C';
    document.getElementById('min-temp').textContent = minT.toFixed(1) + '°C';
    document.getElementById('max-temp').textContent = maxT.toFixed(1) + '°C';
    document.getElementById('med-temp').textContent = medT.toFixed(1) + '°C';

    // FPS
    fpsCount++;
    const now = Date.now();
    if (now - lastFpsTime >= 1000) {
      fps = fpsCount;
      fpsCount = 0;
      lastFpsTime = now;
    }
    status.textContent =
      'Live – ' + fps + ' fps | Sky: ' + skyT.toFixed(1) +
      '°C | Min: ' + minT.toFixed(1) + '°C | Max: ' + maxT.toFixed(1) + '°C';
  };
}

connect();

// Real-time uptime counter.
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
// Setup page
// ---------------------------------------------------------------------------
inline String getSetupPage()
{
  String html = getPageHeader("Sky Conditions Setup");
  html += "<h1>Sky Conditions Setup</h1>\n";
  html += getNavBar();

  // Current readings snapshot
  html += "<div class='card'>\n";
  html += "<h2>Current Status</h2>\n";
  html += "<table><tr><th>Property</th><th>Value</th></tr>\n";
  if (skyConditions.hasData()) {
    html += "<tr><td>Sky Temperature (center 50% FOV)</td><td>" +
            String(skyConditions.getSkyTemperature(), 2) + " °C</td></tr>\n";
    html += "<tr><td>Frame Minimum</td><td>" +
            String(skyConditions.getMinTemperature(), 2) + " °C</td></tr>\n";
    html += "<tr><td>Frame Maximum</td><td>" +
            String(skyConditions.getMaxTemperature(), 2) + " °C</td></tr>\n";
    html += "<tr><td>Frame Median</td><td>" +
            String(skyConditions.getMedianTemperature(), 2) + " °C</td></tr>\n";
    html += "<tr><td>Ambient (die temp)</td><td>" +
            String(skyConditions.getAmbientTemperature(), 2) + " °C</td></tr>\n";
    unsigned long ageSec = (millis() - skyConditions.getLastUpdateMillis()) / 1000;
    html += "<tr><td>Last Update (thermal)</td><td>" + String(ageSec) + " s ago</td></tr>\n";
  } else {
    html += "<tr><td colspan='2'>No sensor data yet</td></tr>\n";
  }
  if (skyConditions.hasBrightnessData()) {
    html += "<tr><td>Sky Brightness</td><td>" +
            String(skyConditions.getLux(), 4) + " lux</td></tr>\n";
    html += "<tr><td>Sky Quality</td><td>" +
            String(skyConditions.getSqm(), 2) + " mag/arcsec\u00B2</td></tr>\n";
    unsigned long bAgeSec = (millis() - skyConditions.getLastBrightnessMillis()) / 1000;
    html += "<tr><td>Last Update (brightness)</td><td>" + String(bAgeSec) + " s ago</td></tr>\n";
  } else {
    html += "<tr><td>Sky Brightness</td><td>No TSL2591 data yet</td></tr>\n";
  }
  html += "</table>\n";
  html += "</div>\n";

  // ASCOM information
  html += "<div class='card'>\n";
  html += "<h2>ASCOM Alpaca</h2>\n";
  html += "<table><tr><th>Property</th><th>Value</th></tr>\n";
  html += "<tr><td>Device Type</td><td>ObservingConditions</td></tr>\n";
  html += "<tr><td>Device Number</td><td>0</td></tr>\n";
  html += "<tr><td>Alpaca Port</td><td>" + String(ALPACA_PORT) + "</td></tr>\n";
  html += "<tr><td>Discovery Port</td><td>" + String(ALPACA_DISCOVERY_PORT) + "</td></tr>\n";
  html += "<tr><td>API Base URL</td><td>http://" + WiFi.localIP().toString() +
          ":" + String(ALPACA_PORT) + "/api/v1/observingconditions/0</td></tr>\n";
  html += "<tr><td>Implemented Sensors</td><td>SkyTemperature, Temperature, SkyBrightness, SkyQuality, CloudCover</td></tr>\n";
  html += "</table>\n";
  html += "</div>\n";

  // Hardware information
  html += "<div class='card'>\n";
  html += "<h2>Hardware</h2>\n";
  html += "<table><tr><th>Property</th><th>Value</th></tr>\n";
  html += "<tr><td>Sensor</td><td>MLX90640 (" +
          String(SENSOR_COLS) + "×" + String(SENSOR_ROWS) + " pixels)</td></tr>\n";
  html += "<tr><td>I2C SDA Pin</td><td>" + String(THERMAL_SDA_PIN) + "</td></tr>\n";
  html += "<tr><td>I2C SCL Pin</td><td>" + String(THERMAL_SCL_PIN) + "</td></tr>\n";
  html += "<tr><td>I2C Frequency</td><td>" + String(THERMAL_I2C_FREQ/1000) + " kHz</td></tr>\n";
  html += "<tr><td>Frame Rate</td><td>" + String(1000/FRAME_INTERVAL_MS) + " Hz</td></tr>\n";
  html += "<tr><td>Center FOV Region</td><td>Cols " +
          String(CENTER_COL_START) + "–" + String(CENTER_COL_END) + ", Rows " +
          String(CENTER_ROW_START) + "–" + String(CENTER_ROW_END) + " (" +
          String(CENTER_PIXEL_COUNT) + " pixels)</td></tr>\n";
  html += "<tr><td>ESP32 Free Heap</td><td>" + String(ESP.getFreeHeap()) + " bytes</td></tr>\n";
  html += "</table>\n";
  html += "</div>\n";

  // Calibration / config
  html += "<div class='card'>\n";
  html += "<h2>Calibration</h2>\n";
  html += "<p style='font-size:0.85em;color:#8899aa'>Settings are saved persistently in NVS flash.</p>\n";
  html += "<form action='/save_config' method='POST'>\n";
  html += "<table><tr><th>Parameter</th><th>Value</th><th>Description</th></tr>\n";

  String inpStyle = "width:90px;background:#1a1a2e;color:#e0e0e0;"
                    "border:1px solid #2d3561;border-radius:4px;padding:4px;";

  // ── Calibration ─────────────────────────────────────────────────────────
  html += "<tr><th colspan='3' style='background:#0a2a50'>Calibration</th></tr>\n";

  html += "<tr><td>SQM Offset (mag/arcsec\u00B2)</td>"
          "<td><input type='number' name='sqmOffset' step='0.01' value='" +
          String(deviceConfig.sqmOffset, 2) + "' style='" + inpStyle + "'></td>"
          "<td>Additive correction applied to the calculated SQM value</td></tr>\n";

  html += "<tr><td>SQM Reference (lux)</td>"
          "<td><input type='number' name='sqmRef' step='1' min='1' value='" +
          String(deviceConfig.sqmReference, 0) + "' style='" + inpStyle + "'></td>"
          "<td>Lux level that maps to SQM\u202F0.0 (default\u202F108000)</td></tr>\n";

  html += "<tr><td>Cloud-Clear Delta (\u00B0C)</td>"
          "<td><input type='number' name='cloudClear' step='0.5' value='" +
          String(deviceConfig.cloudClearDelta, 1) + "' style='" + inpStyle + "'></td>"
          "<td>Ambient \u2212 Sky \u2265 this \u2192 0\u202F% cloud cover (clear)</td></tr>\n";

  html += "<tr><td>Cloud-Overcast Delta (\u00B0C)</td>"
          "<td><input type='number' name='cloudOvercast' step='0.5' value='" +
          String(deviceConfig.cloudOvercastDelta, 1) + "' style='" + inpStyle + "'></td>"
          "<td>Ambient \u2212 Sky \u2264 this \u2192 100\u202F% cloud cover (overcast)</td></tr>\n";

  // ── Imaging ──────────────────────────────────────────────────────────────
  html += "<tr><th colspan='3' style='background:#0a2a50'>Imaging</th></tr>\n";

  html += "<tr><td>Snapshot Interval (s)</td>"
          "<td><input type='number' name='snapSec' min='5' max='3600' step='5' value='" +
          String(deviceConfig.snapshotIntervalSec) + "' style='" + inpStyle + "'></td>"
          "<td>How often /thermal.jpg is regenerated (seconds)</td></tr>\n";

  html += "<tr><td>JPEG Quality (0\u2013100)</td>"
          "<td><input type='number' name='jpegQuality' min='1' max='100' step='1' value='" +
          String(deviceConfig.jpegQuality) + "' style='" + inpStyle + "'></td>"
          "<td>Higher = better image, larger file</td></tr>\n";

  // ── Brightness sensor ─────────────────────────────────────────────────────
  html += "<tr><th colspan='3' style='background:#0a2a50'>Brightness Sensor (TSL2591)</th></tr>\n";

  {
    const char* timingLabels[] = {"100 ms","200 ms","300 ms (default)","400 ms","500 ms","600 ms"};
    html += "<tr><td>Integration Time</td><td>";
    html += "<select name='tslInteg' style='" + inpStyle + "'>";
    for (int i = 0; i < 6; i++) {
      html += "<option value='" + String(i) + "'";
      if (deviceConfig.tsl2591Integration == i) html += " selected";
      html += ">" + String(timingLabels[i]) + "</option>";
    }
    html += "</select></td><td>Longer integration improves dark-sky sensitivity</td></tr>\n";
  }

  // ── ASCOM ─────────────────────────────────────────────────────────────────
  html += "<tr><th colspan='3' style='background:#0a2a50'>ASCOM</th></tr>\n";

  html += "<tr><td>Average Period (s)</td>"
          "<td><input type='number' name='avgPeriod' min='0' step='0.1' value='" +
          String(deviceConfig.averagePeriod, 1) + "' style='" + inpStyle + "'></td>"
          "<td>ObservingConditions AveragePeriod; persisted across reboots</td></tr>\n";

  html += "<tr><td>Cloud Cover Method<br><span style='font-size:0.8em;color:#8899aa'>(Alpaca reporting)</span></td><td>";
  html += "<select name='cloudMethod' style='" + inpStyle + "width:130px;'>";
  html += "<option value='0'" + String(deviceConfig.cloudCoverMethod == 0 ? " selected" : "") +
          ">Mean (FOV avg)</option>";
  html += "<option value='1'" + String(deviceConfig.cloudCoverMethod == 1 ? " selected" : "") +
          ">Per-pixel</option>";
  html += "</select></td>"
          "<td>Which calculation to report via ASCOM Alpaca CloudCover. "
          "Both are always recorded in history and shown on the home page.</td></tr>\n";

  html += "<tr><td>Per-Pixel Region</td><td>";
  html += "<select name='cloudPxRgn' id='cloudPxRgn' style='" + inpStyle + "width:130px;'"
          " onchange='updateEdge()'>";
  html += "<option value='0'" + String(deviceConfig.cloudPixelRegion == 0 ? " selected" : "") +
          ">Centre FOV</option>";
  html += "<option value='1'" + String(deviceConfig.cloudPixelRegion == 1 ? " selected" : "") +
          ">Full Frame</option>";
  html += "</select></td>"
          "<td>Pixel region used for the per-pixel cloud cover calculation</td></tr>\n";

  html += "<tr><td>Edge Exclusion (pixels)<br><span style='font-size:0.8em;color:#8899aa'>(Full Frame only)</span></td><td>";
  html += "<input type='number' name='cloudEdge' id='cloudEdge' min='0' max='10' step='1' value='" +
          String(deviceConfig.cloudEdgeExclude) + "' style='" + inpStyle + "'"  +
          String(deviceConfig.cloudPixelRegion == 0 ? " disabled" : "") + "></td>"
          "<td>Pixels to ignore on each side of the frame (0\u201310). "
          "With default 2: uses " +
          String((SENSOR_COLS - 4)) + "\u00D7" + String((SENSOR_ROWS - 4)) + " = " +
          String((SENSOR_COLS - 4) * (SENSOR_ROWS - 4)) + " pixels</td></tr>\n";

  html += R"rawsetup(
<script>
function updateEdge() {
  var rgn = document.getElementById('cloudPxRgn').value;
  document.getElementById('cloudEdge').disabled = (rgn === '0');
}
</script>
)rawsetup";

  // ── Identity ──────────────────────────────────────────────────────────────
  html += "<tr><th colspan='3' style='background:#0a2a50'>Identity</th></tr>\n";

  html += "<tr><td>Location</td>"
          "<td><input type='text' name='location' maxlength='31' value='" +
          String(deviceConfig.location) + "' style='" + inpStyle + "width:180px;'></td>"
          "<td>Appears in Alpaca management API discovery response</td></tr>\n";

  // ── Network ────────────────────────────────────────────────────────────────
  html += "<tr><th colspan='3' style='background:#0a2a50'>Network</th></tr>\n";

  html += "<tr><td>NTP Server</td>"
          "<td><input type='text' name='ntpServer' maxlength='63' value='" +
          String(deviceConfig.ntpServer) + "' style='" + inpStyle + "width:180px;' placeholder='e.g. 192.168.1.1'></td>"
          "<td>Preferred NTP server (blank = use pool.ntp.org / time.nist.gov). "
          "Takes effect after reboot.</td></tr>\n";

  // ── MQTT / Home Assistant ──────────────────────────────────────────────────
  html += "<tr><th colspan='3' style='background:#0a2a50'>MQTT / Home Assistant"
          " <span style='font-weight:normal;font-size:0.8em;color:#74b9ff'>"
          "(takes effect after reboot)</span></th></tr>\n";

  html += "<tr><td>Enable MQTT</td>"
          "<td><input type='checkbox' name='mqttEnabled' value='1'" +
          String(deviceConfig.mqttEnabled ? " checked" : "") + "></td>"
          "<td>Publish sensor data and Home Assistant autodiscovery</td></tr>\n";

  html += "<tr><td>Broker Address</td>"
          "<td><input type='text' name='mqttServer' maxlength='63' value='" +
          String(deviceConfig.mqttServer) + "' style='" + inpStyle + "width:180px;' placeholder='192.168.1.x'></td>"
          "<td>MQTT broker hostname or IP</td></tr>\n";

  html += "<tr><td>Broker Port</td>"
          "<td><input type='number' name='mqttPort' min='1' max='65535' value='" +
          String(deviceConfig.mqttPort) + "' style='" + inpStyle + "width:80px;'></td>"
          "<td>Default: 1883</td></tr>\n";

  html += "<tr><td>Username</td>"
          "<td><input type='text' name='mqttUser' maxlength='31' value='" +
          String(deviceConfig.mqttUser) + "' style='" + inpStyle + "width:160px;' placeholder='(optional)'></td>"
          "<td>Leave blank if broker requires no auth</td></tr>\n";

  html += "<tr><td>Password</td>"
          "<td><input type='password' name='mqttPass' maxlength='63' value='" +
          String(deviceConfig.mqttPassword) + "' style='" + inpStyle + "width:160px;' placeholder='(optional)'></td>"
          "<td></td></tr>\n";

  html += "<tr><td>Topic Prefix</td>"
          "<td><input type='text' name='mqttTopicPfx' maxlength='63' value='" +
          String(deviceConfig.mqttTopicPrefix) + "' style='" + inpStyle + "width:200px;'></td>"
          "<td>Default '<code>skyconditions</code>' auto-appends MAC suffix. "
          "State topic: <code>&lt;prefix&gt;/state</code></td></tr>\n";

  html += "</table>\n";
  html += "<br><input type='submit' value='Save Configuration' class='btn' style='background:#0f3460'>\n";
  html += "</form>\n";
  html += "</div>\n";

  // WiFi reset
  html += "<div class='card'>\n";
  html += "<h2>WiFi</h2>\n";
  html += "<p>SSID: <strong>" + WiFi.SSID() + "</strong> &nbsp; IP: <strong>" +
          WiFi.localIP().toString() + "</strong></p>\n";
  html += "<button class='btn btn-danger' onclick='resetWifi()'>Reset WiFi Settings</button>\n";
  html += "<script>\n";
  html += "function resetWifi() {\n";
  html += "  if (!confirm('Reset WiFi? The device will restart and create a setup AP.')) return;\n";
  html += "  fetch('/reset_wifi', { method:'POST' })\n";
  html += "    .then(() => alert('Resetting. Connect to WiFi AP: SkyCond-Setup'))\n";
  html += "    .catch(() => alert('Request sent'));\n";
  html += "}\n";
  html += "function rebootDevice() {\n";
  html += "  if (!confirm('Reboot the device?')) return;\n";
  html += "  fetch('/reboot', { method:'POST' })\n";
  html += "    .then(() => alert('Rebooting. Page will reload in 10 seconds.'))\n";
  html += "    .catch(() => alert('Request sent'));\n";
  html += "  setTimeout(() => location.reload(), 10000);\n";
  html += "}\n";
  html += "</script>\n";
  html += "</div>\n";

  // Reboot
  html += "<div class='card'>\n";
  html += "<h2>Device</h2>\n";
  html += "<button class='btn btn-danger' onclick='rebootDevice()'>Reboot Device</button>\n";
  html += "</div>\n";

  html += "</body></html>";
  return html;
}

// ---------------------------------------------------------------------------
// Trends page – time-series charts for temperature and brightness
// ---------------------------------------------------------------------------
inline String getTrendsPage()
{
  String html = getPageHeader("Sky Conditions – Trends");
  html += "<h1>Sky Conditions – Trends</h1>\n";
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
  html += "<div class='card'><h2>Temperature (\u00B0C)</h2>"
          "<canvas id='cT' height='210' style='width:100%;display:block'></canvas></div>\n";
  html += "<div class='card'><h2>Cloud Cover (%)</h2>"
          "<canvas id='cC' height='170' style='width:100%;display:block'></canvas></div>\n";
  html += "<div class='card'><h2>Sky Brightness (lux)</h2>"
          "<canvas id='cL' height='170' style='width:100%;display:block'></canvas></div>\n";
  html += "<div class='card'><h2>Sky Quality (mag/arcsec\u00B2)</h2>"
          "<canvas id='cQ' height='170' style='width:100%;display:block'></canvas></div>\n";

  html += R"rawjs(
<script>
// ── State ──────────────────────────────────────────────────────────────────
let curMinutes = 60;

function setRange(m, btn) {
  curMinutes = m;
  document.querySelectorAll('.tbtn').forEach(b => b.classList.remove('act'));
  btn.classList.add('act');
  loadData();
}

// ── Data fetch ─────────────────────────────────────────────────────────────
function loadData() {
  document.getElementById('hist-status').textContent = 'Loading…';
  fetch('/history.json?minutes=' + curMinutes)
    .then(r => r.json())
    .then(d => {
      const resTxt = d.res < 60 ? d.res + ' s' : (d.res / 60) + ' min';
      document.getElementById('hist-status').textContent =
        d.count + ' samples · ' + resTxt + ' resolution';
      renderAll(d);
    })
    .catch(e => {
      document.getElementById('hist-status').textContent = 'Error: ' + e;
    });
}

// ── Render all charts ──────────────────────────────────────────────────────
function renderAll(d) {
  drawChart('cT', [
    { label:'Sky',     color:'#a29bfe', data:d.sky,  lo:d.sky_lo,  hi:d.sky_hi  },
    { label:'Min',     color:'#74b9ff', data:d.fmin, lo:d.fmin_lo, hi:d.fmin_hi },
    { label:'Max',     color:'#fd79a8', data:d.fmax, lo:d.fmax_lo, hi:d.fmax_hi },
    { label:'Median',  color:'#55efc4', data:d.med,  lo:d.med_lo,  hi:d.med_hi  },
    { label:'Ambient', color:'#ffeaa7', data:d.amb,  lo:d.amb_lo,  hi:d.amb_hi  }
  ], d.t, d.now, '°C');

  drawChart('cC', [
    { label:'Cloud Mean',     color:'#dfe6e9', data:d.cloud,   lo:d.cloud_lo,   hi:d.cloud_hi   },
    { label:'Cloud Per-Pixel',color:'#b2bec3', data:d.cloudpx, lo:d.cloudpx_lo, hi:d.cloudpx_hi }
  ], d.t, d.now, '%');

  drawChart('cL', [
    { label:'Lux', color:'#fdcb6e', data:d.lux, lo:d.lux_lo, hi:d.lux_hi }
  ], d.t, d.now, 'lux');

  drawChart('cQ', [
    { label:'SQM', color:'#00cec9', data:d.sqm, lo:d.sqm_lo, hi:d.sqm_hi }
  ], d.t, d.now, 'mag/sq"');
}

// ── Chart renderer ─────────────────────────────────────────────────────────
function drawChart(id, series, timestamps, nowMs, yUnit) {
  const canvas = document.getElementById(id);
  if (!canvas) return;
  canvas.width = canvas.parentElement.clientWidth - 32;
  const W = canvas.width, H = canvas.height;
  const ctx = canvas.getContext('2d');
  const P = { t: 12, r: 14, b: 38, l: 54 };
  const cw = W - P.l - P.r, ch = H - P.t - P.b;

  // Background
  ctx.fillStyle = '#0f3460';
  ctx.fillRect(0, 0, W, H);

  const n = timestamps ? timestamps.length : 0;
  if (n === 0) {
    ctx.fillStyle = '#888'; ctx.textAlign = 'center';
    ctx.font = '13px sans-serif';
    ctx.fillText('No data yet – wait for the first 30-second bucket to close.',
                 W / 2, H / 2);
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
    ctx.fillStyle = '#ccc';
    ctx.fillText(s.label, lx + 17, ly);
    lx += 17 + ctx.measureText(s.label).width + 14;
  }
}

// ── Boot & auto-refresh ────────────────────────────────────────────────────
loadData();
setInterval(loadData, 30000);
</script>
)rawjs";

  html += "</body></html>";
  return html;
}

#endif // HTML_TEMPLATES_H
