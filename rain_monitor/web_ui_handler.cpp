/*
 * ESP32 ASCOM Alpaca Rain Sensor
 * Web UI Handler Implementation
 */

#include "web_ui_handler.h"
#include "html_templates.h"
#include <WiFi.h>

// Web Server instance for UI on port 80
WebServer webUiServer(80);

// Initialize Web UI
void initWebUI() {
  // Handle root page
  webUiServer.on("/", HTTP_GET, handleRoot);
  
  // Handle setup page
  webUiServer.on("/setup", HTTP_GET, handleSetup);
  webUiServer.on("/setup", HTTP_POST, handleSetupPost);
  
  // Handle WiFi reset
  webUiServer.on("/reset_wifi", HTTP_POST, handleResetWifi);
  
  // Handle 404 errors
  webUiServer.onNotFound(handleWebUINotFound);
  
  // Start server
  webUiServer.begin();
  Debug.println("Web UI server started on port 80");
}

// Handle Web UI requests in the main loop
void handleWebUI() {
  webUiServer.handleClient();
}

// Handle the root page - shows the status dashboard
void handleRoot() {
  String html = getHomePage(isRaining, rainLevel);
  webUiServer.send(200, "text/html", html);
}

// Handle setup page
void handleSetup() {
  String html = getSetupPage();
  webUiServer.send(200, "text/html", html);
}

// Handle setup form submission
void handleSetupPost() {
  bool settingsChanged = false;
  
  // Process detection mode
  if (webUiServer.hasArg("detectionMode")) {
    bool newUseAnalogMode = (webUiServer.arg("detectionMode") == "analog");
    if (newUseAnalogMode != useAnalogMode) {
      useAnalogMode = newUseAnalogMode;
      settingsChanged = true;
      Debug.print("Detection mode changed to: ");
      Debug.println(useAnalogMode ? "Analog" : "Digital");
    }
  }
  
  // Process analog threshold
  if (webUiServer.hasArg("analogThreshold")) {
    int newThreshold = webUiServer.arg("analogThreshold").toInt();
    if (newThreshold >= 0 && newThreshold <= 4095 && newThreshold != analogThreshold) {
      analogThreshold = newThreshold;
      settingsChanged = true;
      Debug.print("Analog threshold changed to: ");
      Debug.println(analogThreshold);
    }
  }
  
  // Process hold time
  if (webUiServer.hasArg("holdTime")) {
    int newHoldTime = webUiServer.arg("holdTime").toInt();
    if (newHoldTime >= 0 && newHoldTime != holdTimeMinutes) {
      holdTimeMinutes = newHoldTime;
      settingsChanged = true;
      Debug.print("Hold time changed to: ");
      Debug.print(holdTimeMinutes);
      Debug.println(" minutes");
    }
  }
  
  // Save settings if changed
  if (settingsChanged) {
    saveSettings();
    webUiServer.send(200, "text/html", 
      "<html><head><meta http-equiv='refresh' content='3;url=/setup'></head>"
      "<body><h2>Settings saved successfully!</h2>"
      "<p>Redirecting back to setup page in 3 seconds...</p></body></html>"
    );
  } else {
    webUiServer.send(200, "text/html", 
      "<html><head><meta http-equiv='refresh' content='3;url=/setup'></head>"
      "<body><h2>No changes detected</h2>"
      "<p>Redirecting back to setup page in 3 seconds...</p></body></html>"
    );
  }
}

// Handle 404 not found
void handleWebUINotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += webUiServer.uri();
  message += "\nMethod: ";
  message += (webUiServer.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += webUiServer.args();
  message += "\n";
  
  for (uint8_t i = 0; i < webUiServer.args(); i++) {
    message += " " + webUiServer.argName(i) + ": " + webUiServer.arg(i) + "\n";
  }
  
  webUiServer.send(404, "text/plain", message);
}

// Handle WiFi reset
void handleResetWifi() {
  webUiServer.send(200, "text/plain", "Resetting WiFi settings and restarting...");
  Debug.println("WiFi settings reset requested, restarting device...");
  
  // Small delay to allow the response to be sent
  delay(1000);
  
  // Reset WiFi settings
  WiFiManager wifiManager;
  wifiManager.resetSettings();
  
  // Restart the device
  ESP.restart();
}