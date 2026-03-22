#ifndef HTML_TEMPLATES_H
#define HTML_TEMPLATES_H

#include <Arduino.h>
#include "config.h"
#include <WiFi.h>
#include "alpaca.h"

// External references to variables needed in template functions
extern bool isRaining;
extern bool isActuallyRaining;
extern int rainLevel;
extern bool useAnalogMode;
extern int analogThreshold;
extern int holdTimeMinutes;
extern bool isConnected;

// Function declarations
String getPageHeader(String pageTitle);
String getNavBar();
String getCommonStyles();
String getStatusDisplay(bool isRaining, int rainLevel);
String getHomePage(bool isRaining, int rainLevel);
String getSetupPage();

// Common CSS styles used across pages
inline String getCommonStyles() {
  String styles = 
    "body { font-family: Arial, sans-serif; margin: 20px; }\n"
    "h1, h2 { color: #2c3e50; }\n"
    "a { color: #3498db; text-decoration: none; }\n"
    "a:hover { text-decoration: underline; }\n"
    ".card { background: #f8f9fa; border-radius: 4px; padding: 15px; margin-bottom: 20px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }\n"
    "label { display: block; margin-bottom: 5px; font-weight: bold; }\n"
    "input[type=text], input[type=password], input[type=number] { width: 100%; padding: 8px; margin-bottom: 15px; border: 1px solid #ddd; border-radius: 4px; }\n"
    "input[type=submit] { background: #3498db; color: white; border: none; padding: 10px 15px; border-radius: 4px; cursor: pointer; }\n"
    "input[type=submit]:hover { background: #2980b9; }\n"
    "button { background-color: #3498db; color: white; border: none; padding: 10px 15px; margin: 5px; border-radius: 4px; cursor: pointer; }\n"
    "button:hover { background-color: #2980b9; }\n"
    "table { border-collapse: collapse; width: 100%; }\n"
    "table, th, td { border: 1px solid #ddd; }\n"
    "th, td { padding: 8px; text-align: left; }\n"
    "th { background-color: #f2f2f2; }\n"
    ".status-dry { color: green; font-weight: bold; }\n"
    ".status-wet { color: blue; font-weight: bold; }\n"
    ".button-row { display: flex; flex-wrap: wrap; gap: 10px; margin-top: 15px; }\n"
    ".button-primary { background-color: #3498db; }\n"
    ".button-warning { background-color: #f39c12; }\n"
    ".button-danger { background-color: #e74c3c; }\n"
    // Add new styles for rain status indicator
    ".rain-status { display: inline-block; margin: 20px auto; text-align: center; }\n"
    ".status-indicator { display: inline-block; width: 24px; height: 24px; border-radius: 50%; margin-right: 10px; vertical-align: middle; }\n"
    ".status-text { display: inline-block; font-weight: bold; vertical-align: middle; }\n"
    ".indicator-blue { background-color: #3498db; }\n" // Wet
    ".indicator-green { background-color: #2ecc71; }\n" // Dry
    ".indicator-blink { animation: blink 1s infinite alternate; }\n"
    "@keyframes blink { from { opacity: 0.6; } to { opacity: 1; } }\n"
    // Gauge style for rain level
    ".gauge-container { margin: 20px auto; text-align: center; }\n"
    ".gauge { width: 100%; max-width: 300px; height: 30px; margin: 0 auto; background-color: #ecf0f1; border-radius: 15px; overflow: hidden; }\n"
    ".gauge-level { height: 100%; background-color: #3498db; border-radius: 15px; transition: width 0.5s ease-in-out; }\n"
    ".gauge-labels { display: flex; justify-content: space-between; max-width: 300px; margin: 5px auto; }\n"
    ".gauge-value { font-size: 24px; font-weight: bold; margin: 10px 0; }\n";
  
  return styles;
}

// Common HTML page header (title, meta tags, styles)
inline String getPageHeader(String pageTitle) {
  String header = "<!DOCTYPE html><html>\n"
    "<head><title>" + pageTitle + "</title>\n"
    "<meta name='viewport' content='width=device-width, initial-scale=1'>\n"
    "<style>\n" + 
    getCommonStyles() + 
    "</style>\n"
    "</head>\n"
    "<body>\n";
  
  return header;
}

// Navigation links 
inline String getNavBar() {
  String navbar = 
    "<div style='margin-bottom: 20px; padding: 10px; background-color: #f8f9fa; border-radius: 4px; box-shadow: 0 2px 4px rgba(0,0,0,0.1);'>\n"
    "<a href='/' style='margin-right: 10px; padding: 8px 12px; background-color: #3498db; color: white; border-radius: 4px; text-decoration: none;'>Home</a>\n"
    "<a href='/setup' style='margin-right: 10px; padding: 8px 12px; background-color: #3498db; color: white; border-radius: 4px; text-decoration: none;'>Setup</a>\n"
    "<a href='/update' style='margin-right: 10px; padding: 8px 12px; background-color: #f39c12; color: white; border-radius: 4px; text-decoration: none;'>Update</a>\n"
    "</div>\n";
  
  return navbar;
}

// Generate status display with color coding
inline String getStatusDisplay(bool isRaining, int rainLevel) {
  String statusString = isRaining ? "WET" : "DRY";
  String statusClass = isRaining ? "status-wet" : "status-dry";
  String indicatorClass = isRaining ? "indicator-blue" : "indicator-green";
  
  // Calculate percentage (inverted - lower analog value means more water)
  int percentage = map(rainLevel, 0, 4095, 100, 0);
  
  String html = "<div class='status " + statusClass + "' style='font-size: 24px; margin: 20px 0; padding: 15px; border-radius: 8px; text-align: center;'>";
  html += "<span class='status-indicator " + indicatorClass + "'></span> ";
  html += "Rain Status: " + statusString;
  html += "</div>";
  
  // Add gauge for rain level
  html += "<div class='gauge-container'>";
  html += "<h2>Rain Level</h2>";
  html += "<div class='gauge-value'>" + String(percentage) + "%</div>";
  html += "<div class='gauge'>";
  html += "<div class='gauge-level' style='width: " + String(percentage) + "%;'></div>";
  html += "</div>";
  html += "<div class='gauge-labels'>";
  html += "<span>Dry (0%)</span>";
  html += "<span>Wet (100%)</span>";
  html += "</div>";
  html += "<div style='margin-top: 5px; font-size: 0.8em; color: #7f8c8d;'>Raw Analog Value: " + String(rainLevel) + "</div>";
  html += "</div>";
  
  return html;
}

// Generate the home page HTML
inline String getHomePage(bool isRaining, int rainLevel) {
  String html = getPageHeader("ESP32 Rain Sensor Status");
  
  // Add custom styles for home page
  html += "<style>\n"
          "body { text-align: center; }\n"
          ".status-card { background-color: #f8f9fa; border-radius: 8px; padding: 20px; margin: 15px 0; box-shadow: 0 2px 4px rgba(0,0,0,0.1); text-align: left; }\n"
          ".dry { background-color: #e6ffe6; color: green; }\n"
          ".wet { background-color: #e6f2ff; color: blue; }\n"
          ".status-header { font-size: 24px; margin: 20px 0; padding: 15px; border-radius: 8px; text-align: center; }\n"
          ".status-table { width: 100%; margin-bottom: 15px; }\n"
          ".status-table th { text-align: left; width: 40%; padding: 8px; background-color: #f2f2f2; }\n"
          ".status-table td { padding: 8px; }\n"
          "</style>\n";
          
  html += "<meta http-equiv='refresh' content='5'>\n"; // Auto-refresh every 5 seconds
  
  html += "<h1>ESP32 Rain Sensor Monitor</h1>\n"
          "<p>Version: " + String(MANUFACTURER_V) + "</p>\n";
  
  // Status header
  String statusClass = isRaining ? "wet" : "dry";
  html += "<div class='status-header " + statusClass + "'>\n";
  html += getStatusDisplay(isRaining, rainLevel);
  html += "</div>\n";
  
  // Navigation buttons
  html += "<div style='margin: 20px 0;'>\n";
  html += "<a href='/setup' class='nav-button' style='background-color: #3498db; padding: 8px 15px; border-radius: 4px; color: white; text-decoration: none; display: inline-block;'>Device Setup</a>\n";
  html += "<a href='/update' class='nav-button' style='background-color: #f39c12; padding: 8px 15px; border-radius: 4px; color: white; text-decoration: none; display: inline-block; margin-left: 10px;'>Update Firmware</a>\n";
  html += "<a onclick='resetWifi()' class='nav-button' style='background-color: #e74c3c; padding: 8px 15px; border-radius: 4px; color: white; text-decoration: none; display: inline-block; margin-left: 10px; cursor: pointer;'>Reset WiFi</a>\n";
  html += "</div>\n";
  
  // Device Information Card
  html += "<div class='status-card'>\n";
  html += "<h2>Device Information</h2>\n";
  html += "<table class='status-table'>\n";
  html += "<tr><th>Device Name</th><td>" + String(DEVICE_NAME) + "</td></tr>\n";
  html += "<tr><th>Device Type</th><td>" + String(DEVICE_TYPE) + "</td></tr>\n";
  html += "<tr><th>Unique ID</th><td>" + getUniqueID() + "</td></tr>\n";
  html += "<tr><th>IP Address</th><td>" + WiFi.localIP().toString() + "</td></tr>\n";
  html += "<tr><th>MAC Address</th><td>" + WiFi.macAddress() + "</td></tr>\n";
  html += "<tr><th>SSID</th><td>" + WiFi.SSID() + "</td></tr>\n";
  html += "<tr><th>Signal Strength</th><td>" + String(WiFi.RSSI()) + " dBm</td></tr>\n";
  html += "<tr><th>Manufacturer</th><td>" + String(MANUFACTURER) + "</td></tr>\n";
  html += "<tr><th>Firmware Version</th><td>" + String(MANUFACTURER_V) + "</td></tr>\n";
  html += "<tr><th>Uptime</th><td>" + String(millis() / 1000 / 60) + " minutes</td></tr>\n";
  html += "</table>\n";
  html += "</div>\n";
  
  // ASCOM Information Card
  html += "<div class='status-card'>\n";
  html += "<h2>ASCOM Information</h2>\n";
  html += "<table class='status-table'>\n";
  html += "<tr><th>ASCOM Device Type</th><td>SafetyMonitor</td></tr>\n";
  html += "<tr><th>ASCOM Port</th><td>" + String(ALPACA_PORT) + "</td></tr>\n";
  html += "<tr><th>Discovery Port</th><td>" + String(ALPACA_DISCOVERY) + "</td></tr>\n";
  html += "<tr><th>Connected Clients</th><td>" + String(isConnected ? "Yes" : "No") + "</td></tr>\n";
  html += "<tr><th>IsSafe Value</th><td>" + String(!isRaining ? "true (Safe)" : "false (Unsafe)") + "</td></tr>\n";
  html += "</table>\n";
  html += "</div>\n";
  
  // Hardware Information Card
  html += "<div class='status-card'>\n";
  html += "<h2>Hardware Information</h2>\n";
  html += "<table class='status-table'>\n";
  html += "<tr><th>ESP32 Chip ID</th><td>" + String((uint32_t)(ESP.getEfuseMac() >> 32)) + "</td></tr>\n";
  html += "<tr><th>Free Heap</th><td>" + String(ESP.getFreeHeap()) + " bytes</td></tr>\n";
  html += "<tr><th>Rain Sensor DO Pin</th><td>" + String(RAIN_DO_PIN) + "</td></tr>\n";
  html += "<tr><th>Rain Sensor AO Pin</th><td>" + String(RAIN_AO_PIN) + "</td></tr>\n";
  html += "<tr><th>Rain Sensor Power Pin</th><td>" + String(RAIN_POWER_PIN) + "</td></tr>\n";
  html += "</table>\n";
  html += "</div>\n";
  
  // Simple JavaScript for animation and function
  html += "<script>\n";
  html += "document.addEventListener('DOMContentLoaded', function() {\n";
  html += "  console.log('Page loaded, auto-refresh enabled');\n";
  html += "});\n";
  html += "function resetWifi() {\n";
  html += "  if (confirm('Are you sure you want to reset WiFi settings? The device will restart and create an access point.')) {\n";
  html += "    fetch('/reset_wifi', { method: 'POST' })\n";
  html += "      .then(response => {\n";
  html += "        alert('WiFi settings reset. The device will now restart. Connect to the \"RainSensor-Setup\" network.');\n";
  html += "      })\n";
  html += "      .catch(error => {\n";
  html += "        console.error('Error:', error);\n";
  html += "        alert('Error resetting WiFi settings');\n";
  html += "      });\n";
  html += "  }\n";
  html += "}\n";
  html += "</script>\n";
  
  html += "</body></html>";
  
  return html;
}

// Setup page
inline String getSetupPage() {
  String html = getPageHeader("ESP32 Rain Sensor Setup");
  
  html += "<style>\n"
          ".toggle-switch { position: relative; display: inline-block; width: 60px; height: 34px; margin-right: 10px; }\n"
          ".toggle-switch input { opacity: 0; width: 0; height: 0; }\n"
          ".toggle-slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background-color: #ccc; transition: .4s; border-radius: 34px; }\n"
          ".toggle-slider:before { position: absolute; content: \"\"; height: 26px; width: 26px; left: 4px; bottom: 4px; background-color: white; transition: .4s; border-radius: 50%; }\n"
          "input:checked + .toggle-slider { background-color: #2196F3; }\n"
          "input:focus + .toggle-slider { box-shadow: 0 0 1px #2196F3; }\n"
          "input:checked + .toggle-slider:before { transform: translateX(26px); }\n"
          ".toggle-label { font-weight: bold; vertical-align: middle; display: inline-block; }\n"
          ".setting-row { margin-bottom: 20px; }\n"
          ".threshold-container { margin-top: 20px; display: none; }\n"
          "#threshold-display { font-weight: bold; margin-left: 10px; }\n"
          "#analogThresholdSlider { width: 100%; max-width: 400px; }\n"
          "</style>\n";
  
  html += "<h1>ESP32 Rain Sensor Setup</h1>";
  html += "<p>Version: " + String(MANUFACTURER_V) + "</p>";
  
  // Add navigation
  html += getNavBar();
  
  // Status info
  html += "<div class='card'>";
  html += "<h2>Current Status</h2>";
  html += "<p>Sensor Reading: <strong class='" + String(isActuallyRaining ? "status-wet" : "status-dry") + "'>" + 
          String(isActuallyRaining ? "WET" : "DRY") + "</strong></p>";
  html += "<p>Reported Status: <strong class='" + String(isRaining ? "status-wet" : "status-dry") + "'>" + 
          String(isRaining ? "RAINING" : "DRY") + "</strong>";
  
  // If there's a hold time active and we're reporting rain but sensor is actually dry
  if (holdTimeMinutes > 0 && isRaining && !isActuallyRaining) {
    html += " <span style='font-size: 0.8em; color: #e67e22;'>(hold time active)</span>";
  }
  
  html += "</p>";
  html += "<p>Rain Level: <strong>" + String(rainLevel) + "</strong> (" + 
          String(map(rainLevel, 0, 4095, 100, 0)) + "%)</p>";
  html += "<p>Detection Mode: <strong>" + String(useAnalogMode ? "Analog" : "Digital") + "</strong></p>";
  if (useAnalogMode) {
    html += "<p>Analog Threshold: <strong>" + String(analogThreshold) + "</strong> (rain if reading < threshold)</p>";
  }
  html += "<p>Hold Time: <strong>" + String(holdTimeMinutes) + " minutes</strong></p>";
  html += "</div>";
  
  // Settings form
  html += "<div class='card'>";
  html += "<h2>Rain Detection Settings</h2>";
  html += "<form method='post' action='/setup'>";
  
  // Detection mode
  html += "<div class='setting-row'>";
  html += "<label class='toggle-switch'>";
  html += "<input type='checkbox' name='detectionModeToggle' id='detectionModeToggle' " + 
          String(useAnalogMode ? "checked" : "") + " onchange='updateDetectionMode()'>";
  html += "<span class='toggle-slider'></span>";
  html += "</label>";
  html += "<span class='toggle-label'>Detection Mode: <span id='modeText'>" + 
          String(useAnalogMode ? "Analog (threshold-based)" : "Digital (sensor logic)") + "</span></span>";
  html += "<input type='hidden' name='detectionMode' id='detectionMode' value='" + 
          String(useAnalogMode ? "analog" : "digital") + "'>";
  html += "</div>";
  
  // Analog threshold (slider)
  html += "<div id='threshold-container' class='threshold-container' " + 
          String(useAnalogMode ? "style='display:block;'" : "") + ">";
  html += "<label for='analogThresholdSlider'>Analog Threshold: <span id='threshold-display'>" + 
          String(analogThreshold) + "</span></label><br>";
  html += "<input type='range' min='0' max='4095' step='10' value='" + String(analogThreshold) + 
          "' id='analogThresholdSlider' oninput='updateThreshold(this.value)'>";
  html += "<input type='hidden' name='analogThreshold' id='analogThreshold' value='" + 
          String(analogThreshold) + "'>";
  // Add hold time dropdown
  html += "<div class='setting-row'>";
  html += "<label for='holdTime'>Hold Rain Status For:</label>";
  html += "<select id='holdTime' name='holdTime' style='width:100%; max-width:400px; padding:8px; margin-top:5px; margin-bottom:15px;'>";
  html += "<option value='0'" + String(holdTimeMinutes == 0 ? " selected" : "") + ">No Hold Time (0 minutes)</option>";
  html += "<option value='5'" + String(holdTimeMinutes == 5 ? " selected" : "") + ">5 Minutes</option>";
  html += "<option value='10'" + String(holdTimeMinutes == 10 ? " selected" : "") + ">10 Minutes</option>";
  html += "<option value='15'" + String(holdTimeMinutes == 15 ? " selected" : "") + ">15 Minutes</option>";
  html += "<option value='30'" + String(holdTimeMinutes == 30 ? " selected" : "") + ">30 Minutes</option>";
  html += "</select>";
  html += "<p><small>After detecting rain, the system will maintain the \"raining\" state for this duration after the sensor returns to dry.</small></p>";
  html += "</div>";
  
  html += "<input type='submit' value='Save Settings'>";
  html += "</form>";
  html += "</div>";
  
  // Device info (same as before)
  html += "<div class='card'>";
  html += "<h2>Device Information</h2>";
  html += "<p>Device is configured as follows:</p>";
  html += "<ul>";
  html += "<li>Device Name: " + String(DEVICE_NAME) + "</li>";
  html += "<li>ASCOM Device Type: " + String(DEVICE_TYPE) + "</li>";
  html += "<li>Firmware Version: " + String(MANUFACTURER_V) + "</li>";
  html += "</ul>";
  html += "</div>";
  
  // JavaScript for interactive form
  html += "<script>";
  html += "function updateDetectionMode() {";
  html += "  var toggle = document.getElementById('detectionModeToggle');";
  html += "  var modeText = document.getElementById('modeText');";
  html += "  var modeInput = document.getElementById('detectionMode');";
  html += "  var thresholdContainer = document.getElementById('threshold-container');";
  html += "  if (toggle.checked) {";
  html += "    modeText.textContent = 'Analog (threshold-based)';";
  html += "    modeInput.value = 'analog';";
  html += "    thresholdContainer.style.display = 'block';";
  html += "  } else {";
  html += "    modeText.textContent = 'Digital (sensor logic)';";
  html += "    modeInput.value = 'digital';";
  html += "    thresholdContainer.style.display = 'none';";
  html += "  }";
  html += "}";
  html += "function updateThreshold(value) {";
  html += "  document.getElementById('threshold-display').textContent = value;";
  html += "  document.getElementById('analogThreshold').value = value;";
  html += "}";
  html += "</script>";
  
  html += "</body></html>";
  
  return html;
}

#endif // HTML_TEMPLATES_H