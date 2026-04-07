#ifndef DISPLAY_DRIVER_H
#define DISPLAY_DRIVER_H

#include <Arduino.h>

void initDisplay();
void drawConnectingScreen();
void drawMenuScreen(bool wifiConnected);
void drawGameScreen(bool wifiConnected, bool fenOk, const String &data);

#endif