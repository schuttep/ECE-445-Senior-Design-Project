#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

const char* ssid = "Payton34";
const char* password = "Awesome101";

const char* apiUrl = "https://j3zvk9adv0.execute-api.us-east-2.amazonaws.com/api/v1/games/1/moves";

void connectToWiFi() {
  Serial.print("Connecting to WiFi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("Connected!");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
}

void fetchFEN() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected");
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.begin(client, apiUrl);

  int httpCode = http.GET();

  Serial.print("HTTP code: ");
  Serial.println(httpCode);

  if (httpCode <= 0) {
    Serial.println("Request failed");
    http.end();
    return;
  }

  String payload = http.getString();
  Serial.println("Raw response:");
  Serial.println(payload);

  DynamicJsonDocument doc(4096);
  DeserializationError error = deserializeJson(doc, payload);

  if (error) {
    Serial.print("JSON parse failed: ");
    Serial.println(error.c_str());
    http.end();
    return;
  }

  JsonArray moves = doc["moves"];

  if (moves.isNull() || moves.size() == 0) {
    Serial.println("No moves found");
    http.end();
    return;
  }

  const char* fen = moves[moves.size() - 1]["fen"];

  if (fen) {
    Serial.println("Latest FEN:");
    Serial.println(fen);
  } else {
    Serial.println("No fen field found");
  }

  http.end();
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  connectToWiFi();
  fetchFEN();
}

void loop() {
  delay(5000);
  fetchFEN();
}
