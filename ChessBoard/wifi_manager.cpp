#include "wifi_manager.h"
#include <WiFi.h>

uint8_t wmScan(ScannedNetwork *out)
{
    // Ensure the radio is in a state that can scan even when not connected.
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);
    delay(150);

    int n = WiFi.scanNetworks();
    if (n < 0)
        n = 0;
    if (n > WM_MAX_SCAN)
        n = WM_MAX_SCAN;
    for (int i = 0; i < n; i++)
    {
        strncpy(out[i].ssid, WiFi.SSID(i).c_str(), WM_SSID_LEN - 1);
        out[i].ssid[WM_SSID_LEN - 1] = '\0';
        out[i].rssi = (int8_t)WiFi.RSSI(i);
    }
    WiFi.scanDelete();
    return (uint8_t)n;
}

bool wmConnect(const char *ssid, const char *pass)
{
    WiFi.disconnect();
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, pass);
    int tries = 0;
    while (WiFi.status() != WL_CONNECTED && tries < 40)
    {
        delay(500);
        tries++;
    }
    return WiFi.status() == WL_CONNECTED;
}
