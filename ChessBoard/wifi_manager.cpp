#include "wifi_manager.h"
#include <WiFi.h>
#include <Preferences.h>

static Preferences prefs;

uint8_t wmLoadSaved(SavedNetwork *out, uint8_t maxCount)
{
    prefs.begin("wm_nets", true);
    uint8_t count = prefs.getUChar("count", 0);
    if (count > maxCount)
        count = maxCount;
    for (uint8_t i = 0; i < count; i++)
    {
        char key[8];
        snprintf(key, sizeof(key), "s%d", (int)i);
        prefs.getString(key, out[i].ssid, WM_SSID_LEN);
        snprintf(key, sizeof(key), "p%d", (int)i);
        prefs.getString(key, out[i].pass, WM_PASS_LEN);
    }
    prefs.end();
    return count;
}

void wmSaveNetwork(const char *ssid, const char *pass)
{
    prefs.begin("wm_nets", false);
    uint8_t count = prefs.getUChar("count", 0);

    // Update existing entry if SSID already saved
    for (uint8_t i = 0; i < count; i++)
    {
        char key[8], stored[WM_SSID_LEN];
        snprintf(key, sizeof(key), "s%d", (int)i);
        prefs.getString(key, stored, WM_SSID_LEN);
        if (strcmp(stored, ssid) == 0)
        {
            snprintf(key, sizeof(key), "p%d", (int)i);
            prefs.putString(key, pass);
            prefs.end();
            return;
        }
    }

    // Evict the oldest entry if at capacity (shift everything down by 1)
    if (count >= WM_MAX_SAVED)
    {
        for (uint8_t i = 0; i < count - 1; i++)
        {
            char from[8], to[8], val[WM_PASS_LEN];
            snprintf(from, sizeof(from), "s%d", (int)(i + 1));
            snprintf(to, sizeof(to), "s%d", (int)i);
            prefs.getString(from, val, WM_SSID_LEN);
            prefs.putString(to, val);
            snprintf(from, sizeof(from), "p%d", (int)(i + 1));
            snprintf(to, sizeof(to), "p%d", (int)i);
            prefs.getString(from, val, WM_PASS_LEN);
            prefs.putString(to, val);
        }
        count--;
    }

    char key[8];
    snprintf(key, sizeof(key), "s%d", (int)count);
    prefs.putString(key, ssid);
    snprintf(key, sizeof(key), "p%d", (int)count);
    prefs.putString(key, pass);
    prefs.putUChar("count", count + 1);
    prefs.end();
}

bool wmGetSavedPass(const char *ssid, char passOut[WM_PASS_LEN])
{
    prefs.begin("wm_nets", true);
    uint8_t count = prefs.getUChar("count", 0);
    for (uint8_t i = 0; i < count; i++)
    {
        char key[8], stored[WM_SSID_LEN];
        snprintf(key, sizeof(key), "s%d", (int)i);
        prefs.getString(key, stored, WM_SSID_LEN);
        if (strcmp(stored, ssid) == 0)
        {
            snprintf(key, sizeof(key), "p%d", (int)i);
            prefs.getString(key, passOut, WM_PASS_LEN);
            prefs.end();
            return true;
        }
    }
    prefs.end();
    return false;
}

uint8_t wmScan(ScannedNetwork *out)
{
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
        char pw[WM_PASS_LEN];
        out[i].saved = wmGetSavedPass(out[i].ssid, pw);
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

bool wmConnectBoot()
{
    SavedNetwork nets[WM_MAX_SAVED];
    uint8_t count = wmLoadSaved(nets, WM_MAX_SAVED);
    for (uint8_t i = 0; i < count; i++)
    {
        if (wmConnect(nets[i].ssid, nets[i].pass))
            return true;
    }
    return false;
}
