#include "api_connect.h"

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Amazon Root CA 1 — valid until 2038-01-17
// Source: https://www.amazontrust.com/repository/AmazonRootCA1.pem
static const char *AWS_ROOT_CA = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0BAQsF
ADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6
b24gUm9vdCBDQSAxMB4XDTE1MDUyNjAwMDAwMFoXDTM4MDExNzAwMDAwMFowOTEL
MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJv
b3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALJ4gHHKeNXj
ca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrAIthtOgQ3pOsqTQNroBvo3bSMgHFzZM
9O6II8c+6zf1tRn4SWiw3te5djgdYZ6k/oI2peVKVuRF4fn9tBb6dNqcmzU5L/qw
IFAGbHrQgLKm+a/sRxmPUDgH3KKHOVj4utWp+UhnMJbulHheb4mjUcAwhmahRWa6
VOujw5H5SNz/0egwLX0tdHA114gk957EWW67c4cX8jJGKLhD+rcdqsq08p8kDi1L
93FcXmn/6pUCyziKrlA4b9v7LWIbxcceVOF34GfID5yHI9Y/QCB/IIDEgEw+OyQm
jgSubJrIqg0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMC
AYYwHQYDVR0OBBYEFIQYzIU07LwMlJQuCFmcx7IQTgoIMA0GCSqGSIb3DQEBCwUA
A4IBAQCY8jdaQZChGsV2USggNiMOruYou6r4lK5IpDB/G/wkjUu0yKGX9rbxenDI
U5PMCCjjmCXPI6T53iHTfIUJrU6adTrCC2qJeHZERxhlbI1Bjjt/msv0tadQ1wUs
N+gDS63pYaACbvXy8MWy7Vu33PqUXHeeE6V/Uq2V8viTO96LXFvKWlJbYK8U90vv
o/ufQJVtMVT8QtPHRh8jrdkPSHCa2XV4cdFyQzR1bldZwgJcJmApzyMZFo6IQ6XU
5MsI+yMRQ+hDKXJioaldXgjUkK642M4UwtBV8ob2xJNDd2ZhwLnoQdeXeGADbkpy
rqXRfboQnoZsG4q5WTP468SQvvG5
-----END CERTIFICATE-----
)EOF";

static constexpr const char *API_URL =
    "https://j3zvk9adv0.execute-api.us-east-2.amazonaws.com/api/v1/games/1/moves";

ApiResult fetchLatestFEN()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial0.println("WiFi not connected");
    return {false, "WiFi not connected"};
  }

  WiFiClientSecure client;
  client.setCACert(AWS_ROOT_CA);

  HTTPClient http;
  http.begin(client, API_URL);

  int httpCode = http.GET();

  Serial0.print("HTTP code: ");
  Serial0.println(httpCode);

  if (httpCode <= 0)
  {
    Serial0.println("Request failed");
    http.end();
    return {false, "Request failed"};
  }

  String payload = http.getString();
  Serial0.println("Raw response:");
  Serial0.println(payload);

  JsonDocument doc;
  DeserializationError error = deserializeJson(doc, payload);

  if (error)
  {
    Serial0.print("JSON parse failed: ");
    Serial0.println(error.c_str());
    http.end();
    return {false, "JSON parse failed"};
  }

  JsonArray moves = doc["moves"];

  if (moves.isNull() || moves.size() == 0)
  {
    Serial0.println("No moves found");
    http.end();
    return {false, "No moves found"};
  }

  const char *fen = moves[moves.size() - 1]["fen"];
  http.end();

  if (fen)
  {
    Serial0.println("Latest FEN:");
    Serial0.println(fen);
    return {true, String(fen)};
  }

  return {false, "No fen field found"};
}

// Returns the latest board FEN and derives whose turn it is from the move
// count: even number of moves recorded = white to move, odd = black to move.
GameStateResult fetchGameState()
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial0.println("WiFi not connected");
    return {false, "", true};
  }

  WiFiClientSecure client;
  client.setCACert(AWS_ROOT_CA);

  HTTPClient http;
  http.begin(client, API_URL);

  int httpCode = http.GET();
  if (httpCode <= 0)
  {
    Serial0.println("fetchGameState: request failed");
    http.end();
    return {false, "", true};
  }

  String payload = http.getString();
  http.end();

  JsonDocument doc;
  if (deserializeJson(doc, payload))
    return {false, "", true};

  JsonArray moves = doc["moves"];
  if (moves.isNull() || moves.size() == 0)
    return {false, "", true}; // no moves yet

  size_t moveCount = moves.size();
  const char *fen = moves[moveCount - 1]["fen"];
  if (!fen)
    return {false, "", true};

  // Even move count means white has had as many turns as black → white to move.
  bool whiteToMove = (moveCount % 2 == 0);
  Serial0.printf("fetchGameState: %u moves, fen=%s, whiteToMove=%d\n",
                 (unsigned)moveCount, fen, (int)whiteToMove);
  return {true, String(fen), whiteToMove};
}

ApiResult pushLatestFEN(const String &move, const String &fen)
{
  if (WiFi.status() != WL_CONNECTED)
  {
    Serial0.println("WiFi not connected");
    return {false, "WiFi not connected"};
  }

  WiFiClientSecure client;
  client.setCACert(AWS_ROOT_CA);

  HTTPClient http;
  http.begin(client, API_URL);

  http.addHeader("Content-Type", "application/json");

  JsonDocument doc;
  doc["move"] = move;
  doc["fen"] = fen;

  String requestBody;
  serializeJson(doc, requestBody);

  Serial0.println("Sending POST:");
  Serial0.println(requestBody);

  int httpCode = http.POST(requestBody);

  Serial0.print("HTTP code: ");
  Serial0.println(httpCode);

  if (httpCode <= 0)
  {
    Serial0.println("POST failed");
    http.end();
    return {false, "POST failed"};
  }

  String payload = http.getString();
  Serial0.println("Response:");
  Serial0.println(payload);

  JsonDocument resDoc;
  DeserializationError error = deserializeJson(resDoc, payload);

  if (error)
  {
    Serial0.print("JSON parse failed: ");
    Serial0.println(error.c_str());
    http.end();
    return {false, "JSON parse failed"};
  }

  const char *newFen = resDoc["fen"];

  http.end();

  if (newFen)
  {
    Serial0.println("Updated FEN:");
    Serial0.println(newFen);
    return {true, String(newFen)};
  }

  return {false, "No fen in response"};
}

// ---------------------------------------------------------------------------
// pushFENState — richer POST used by the game FSM
// ---------------------------------------------------------------------------
ApiResult pushFENState(const String &fen, bool isWhite,
                       uint16_t gameId, uint8_t boardNum)
{
  if (WiFi.status() != WL_CONNECTED)
    return {false, "WiFi not connected"};

  WiFiClientSecure client;
  client.setCACert(AWS_ROOT_CA);

  HTTPClient http;
  http.begin(client, API_URL);
  http.addHeader("Content-Type", "application/json");

  JsonDocument doc;
  doc["game_id"] = gameId;
  doc["board_number"] = boardNum;
  doc["fen"] = fen;
  doc["player_color"] = isWhite ? "white" : "black";
  doc["mac_address"] = WiFi.macAddress();

  String body;
  serializeJson(doc, body);

  Serial0.print("[API] pushFENState POST: ");
  Serial0.println(body);

  int code = http.POST(body);
  String response = http.getString();
  http.end();

  Serial0.print("[API] pushFENState code: ");
  Serial0.println(code);
  if (response.length() > 0)
  {
    Serial0.print("[API] pushFENState body: ");
    Serial0.println(response);
  }

  if (code < 200 || code >= 300)
    return {false, "HTTP " + String(code)};

  return {true, response};
}