#ifndef DISPLAY_DRIVER_H
#define DISPLAY_DRIVER_H

#include <Arduino.h>

void initDisplay();
void drawConnectingScreen();
void drawMainScreen(bool wifiConnected, const String& fen);
void drawFenBox(const String& fen);

#endif