#include "api_connect.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

static constexpr const char* API_URL =
  "https://j3zvk9adv0.execute-api.us-east-2.amazonaws.com/api/v1/games/1/moves";

String fetchLatestFEN() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected");
    return "WiFi not connected";
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.begin(client, API_URL);

  int httpCode = http.GET();

  Serial.print("HTTP code: ");
  Serial.println(httpCode);

  if (httpCode <= 0) {
    Serial.println("Request failed");
    http.end();
    return "Request failed";
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
    return "JSON parse failed";
  }

  JsonArray moves = doc["moves"];

  if (moves.isNull() || moves.size() == 0) {
    Serial.println("No moves found");
    http.end();
    return "No moves found";
  }

  const char* fen = moves[moves.size() - 1]["fen"];
  http.end();

  if (fen) {
    Serial.println("Latest FEN:");
    Serial.println(fen);
    return String(fen);
  }

  return "No fen field found";
}

String pushLatestFEN(const String& move, const String& fen) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected");
    return "WiFi not connected";
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.begin(client, API_URL);

  http.addHeader("Content-Type", "application/json");

  // Build JSON body
  DynamicJsonDocument doc(512);
  doc["move"] = move;
  doc["fen"] = fen;

  String requestBody;
  serializeJson(doc, requestBody);

  Serial.println("Sending POST:");
  Serial.println(requestBody);

  int httpCode = http.POST(requestBody);

  Serial.print("HTTP code: ");
  Serial.println(httpCode);

  if (httpCode <= 0) {
    Serial.println("POST failed");
    http.end();
    return "POST failed";
  }

  String payload = http.getString();
  Serial.println("Response:");
  Serial.println(payload);

  // Parse response
  DynamicJsonDocument resDoc(1024);
  DeserializationError error = deserializeJson(resDoc, payload);

  if (error) {
    Serial.print("JSON parse failed: ");
    Serial.println(error.c_str());
    http.end();
    return "JSON parse failed";
  }

  const char* newFen = resDoc["fen"];

  http.end();

  if (newFen) {
    Serial.println("Updated FEN:");
    Serial.println(newFen);
    return String(newFen);
  }

  return "No fen in response";
}