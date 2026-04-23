#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>

#define WM_MAX_SCAN 10
#define WM_SSID_LEN 33
#define WM_PASS_LEN 65

struct ScannedNetwork
{
    char ssid[WM_SSID_LEN];
    int8_t rssi;
};

// Scan for nearby networks. Returns count (capped at WM_MAX_SCAN).
uint8_t wmScan(ScannedNetwork *out);

// Connect to a specific network. Returns true on success.
bool wmConnect(const char *ssid, const char *pass);

#endif
