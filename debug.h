#ifndef DEBUG_H
#define DEBUG_H

#include <Arduino.h>

// Set to 0 to disable debug output
#define DEBUG 1

class DebugClass {
public:
  void print(const String& msg) {
    if (DEBUG) Serial.print(msg);
  }

  void print(int msg) {
    if (DEBUG) Serial.print(msg);
  }

  void println(const String& msg) {
    if (DEBUG) Serial.println(msg);
  }

  void println(int msg) {
    if (DEBUG) Serial.println(msg);
  }

  void printf(const char *format, ...) {
    if (DEBUG) {
      char buffer[256];
      va_list args;
      va_start(args, format);
      vsnprintf(buffer, sizeof(buffer), format, args);
      va_end(args);
      Serial.print(buffer);
    }
  }
};

extern DebugClass Debug;

#endif // DEBUG_H
