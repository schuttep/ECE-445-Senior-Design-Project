#ifndef API_CONNECT_H
#define API_CONNECT_H

#include <Arduino.h>

String fetchLatestFEN();
String pushLatestFEN(const String& move, const String& fen);

#endif