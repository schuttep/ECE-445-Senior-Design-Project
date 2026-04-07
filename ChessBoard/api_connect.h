#ifndef API_CONNECT_H
#define API_CONNECT_H

#include <Arduino.h>

struct ApiResult
{
    bool ok;
    String data;
};

ApiResult fetchLatestFEN();
ApiResult pushLatestFEN(const String &move, const String &fen);

#endif