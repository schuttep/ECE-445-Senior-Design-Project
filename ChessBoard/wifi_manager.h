#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>

#define WM_MAX_SAVED 5
#define WM_MAX_SCAN 10
#define WM_SSID_LEN 33
#define WM_PASS_LEN 65

struct ScannedNetwork
{
    char ssid[WM_SSID_LEN];
    int8_t rssi;
    bool saved; // true if a password is stored in NVS for this SSID
};

struct SavedNetwork
{
    char ssid[WM_SSID_LEN];
    char pass[WM_PASS_LEN];
};

// Load all saved networks from NVS into out[]. Returns count.
uint8_t wmLoadSaved(SavedNetwork *out, uint8_t maxCount);

// Persist a (ssid, pass) pair. Updates an existing entry or evicts the
// oldest when the store is full.
void wmSaveNetwork(const char *ssid, const char *pass);

// Retrieve a saved password for ssid. Returns false if not found.
bool wmGetSavedPass(const char *ssid, char passOut[WM_PASS_LEN]);

// Scan for nearby networks. Returns count (capped at WM_MAX_SCAN).
uint8_t wmScan(ScannedNetwork *out);

// Connect to a specific network. Returns true on success.
bool wmConnect(const char *ssid, const char *pass);

// Try all NVS-saved networks in order. Returns true when one succeeds.
bool wmConnectBoot();

#endif
