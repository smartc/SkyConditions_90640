#ifndef HTML_TEMPLATES_H
#define HTML_TEMPLATES_H

#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "sky_sensor.h"

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
  html += "  <div class='stat-box'><div class='stat-label'>Sky Brightness</div>"
          "    <div class='stat-value' style='color:#fdcb6e'>" +
          (skyConditions.hasBrightnessData() ? String(skyConditions.getLux(), 4) + " lux" : "--") +
          "</div></div>\n";
  html += "  <div class='stat-box'><div class='stat-label'>Sky Quality</div>"
          "    <div class='stat-value' style='color:#00cec9'>" +
          (skyConditions.hasBrightnessData() ? String(skyConditions.getSqm(), 2) + " mag/\"²" : "--") +
          "</div></div>\n";
  html += "</div>\n";  // stat-grid
  html += "</div>\n";  // card

  // Thermal snapshot image (auto-refreshes every JPEG_INTERVAL_MS)
  html += "<div class='card'>\n";
  html += "<h2>Thermal Snapshot</h2>\n";
  html += "<p style='font-size:0.8em;color:#636e72'>Refreshes every " +
          String(JPEG_INTERVAL_MS / 1000) + " s &nbsp;&middot;&nbsp; "
          "<a href='/thermal.jpg'>direct link</a></p>\n";
  html += "<img id='thermal-snap' src='/thermal.jpg' "
          "style='display:block;margin:0 auto;image-rendering:pixelated;"
          "width:320px;height:240px;border-radius:4px;'>\n";
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
  html += "<tr><td>Uptime</td><td>"      + String(millis()/60000)   + " min</td></tr>\n";
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

// Jet colormap: maps [0..1] to RGBA
function jet(t) {
  let r = Math.min(1, Math.max(0, 1.5 - Math.abs(4*t - 3)));
  let g = Math.min(1, Math.max(0, 1.5 - Math.abs(4*t - 2)));
  let b = Math.min(1, Math.max(0, 1.5 - Math.abs(4*t - 1)));
  return [Math.round(r*255), Math.round(g*255), Math.round(b*255)];
}

// Pre-compute jet lookup table
const jetLUT = new Array(256);
for (let i = 0; i < 256; i++) { jetLUT[i] = jet(i/255); }

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
      const [r,g,b] = jetLUT[pix[i]];
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
    ctx.strokeStyle = 'rgba(255,255,255,0.7)';
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
  html += "<tr><td>Implemented Sensors</td><td>SkyTemperature, Temperature, SkyBrightness, SkyQuality</td></tr>\n";
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
  html += "</script>\n";
  html += "</div>\n";

  html += "</body></html>";
  return html;
}

#endif // HTML_TEMPLATES_H
