#ifndef DEBUG_H
#define DEBUG_H

#include <Arduino.h>

// Set to 0 to disable serial output (web console log always active)
#define DEBUG 1

// Append text to the in-RAM console log ring buffer (called by DebugClass).
// Lines are flushed to the ring on each '\n'; consecutive identical lines
// collapse into a single entry with an incrementing repeat count.
void logAppend(const char *text);

// Serialise the current ring buffer contents to a JSON string for /console.json.
void logGetJSON(String &out);

class DebugClass {
public:
  void print(const String& msg) {
    if (DEBUG) Serial.print(msg);
    logAppend(msg.c_str());
  }

  void print(int msg) {
    if (DEBUG) Serial.print(msg);
    char tmp[16];
    snprintf(tmp, sizeof(tmp), "%d", msg);
    logAppend(tmp);
  }

  void println(const String& msg) {
    if (DEBUG) Serial.println(msg);
    logAppend(msg.c_str());
    logAppend("\n");
  }

  void println(int msg) {
    if (DEBUG) Serial.println(msg);
    char tmp[20];
    snprintf(tmp, sizeof(tmp), "%d\n", msg);
    logAppend(tmp);
  }

  void printf(const char *format, ...) {
    char buffer[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    if (DEBUG) Serial.print(buffer);
    logAppend(buffer);
  }
};

extern DebugClass Debug;

#endif // DEBUG_H
