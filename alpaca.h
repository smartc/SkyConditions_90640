#ifndef ALPACA_H
#define ALPACA_H

#include <Arduino.h>
#include <WebServer.h>
#include <WiFiUdp.h>
#include "config.h"

extern WebServer alpacaServer;
extern WiFiUDP   alpacaUdp;

void setupAlpacaServer();
void handleAlpacaDiscovery();

#endif // ALPACA_H
