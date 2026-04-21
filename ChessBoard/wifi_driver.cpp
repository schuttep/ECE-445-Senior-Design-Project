#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "headers.h"
#include "wifi_driver.h"

// ===================== WIFI / API =====================
bool connectWifi()
{
  Serial0.print("Connecting to WiFi");

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 40)
  {
    delay(500);
    Serial0.print(".");
    tries++;
  }

  Serial0.println();

  if (WiFi.status() == WL_CONNECTED)
  {
    Serial0.println("Connected!");
    Serial0.print("IP: ");
    Serial0.println(WiFi.localIP());
    return true;
  }

  Serial0.println("WiFi failed");
  return false;
}