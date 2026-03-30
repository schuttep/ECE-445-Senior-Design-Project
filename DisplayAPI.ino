#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

#include "DFRobot_GDL.h"
#include "DFRobot_Touch.h"
#include "DFRobot_UI.h"

// ===================== WIFI / API =====================
const char* ssid = "405 E Stoughton";
const char* password = "VividBoleGorilla";

const char* apiUrl =
  "https://j3zvk9adv0.execute-api.us-east-2.amazonaws.com/api/v1/games/1/moves";

// ===================== PINS =====================
#define SCR_SCLK      45
#define SCR_MOSI      48
#define SCR_MISO      47
#define SCR_CS        21
#define SCR_RST       14
#define SCR_DC        13
#define SCR_BLK       12

#define SCR_I2C_SCL   11
#define SCR_I2C_SDA   10
#define SCR_INT       9
#define SCR_TCH_RST   8

// ===================== DISPLAY / TOUCH =====================
DFRobot_Touch_GT911_IPS touch(0x5D, SCR_TCH_RST, SCR_INT);
DFRobot_ST7365P_320x480_HW_SPI screen(SCR_DC, SCR_CS, SCR_RST, SCR_BLK);
DFRobot_UI ui(&screen, &touch);

// ===================== UI STATE =====================
DFRobot_UI::sButton_t* fetchBtnPtr = nullptr;
DFRobot_UI::sTextBox_t* callbackTextBox = nullptr;

String currentFEN = "Press button to fetch FEN";
String macAddress = "";
bool wifiConnected = false;

// ===================== WIFI =====================
void connectToWiFi() {
  screen.fillScreen(COLOR_RGB565_WHITE);
  screen.setTextSize(1);
  screen.setTextColor(COLOR_RGB565_BLACK);
  screen.setCursor(20, 20);
  screen.print("Connecting to WiFi...");

  Serial.print("Connecting to WiFi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  int tries = 0;
  while (WiFi.status() != WL_CONNECTED && tries < 40) {
    delay(500);
    Serial.print(".");
    tries++;
  }
  Serial.println();

  macAddress = WiFi.macAddress();
  Serial.print("MAC Address: ");
  Serial.println(macAddress);

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    Serial.println("Connected!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
  } else {
    wifiConnected = false;
    Serial.println("WiFi failed");
  }
}

// ===================== DRAW HELPERS =====================
void drawTitleLine() {
  screen.fillRect(0, 15, 320, 20, COLOR_RGB565_WHITE);
  screen.setCursor(20, 20);
  screen.setTextSize(1);
  screen.setTextColor(COLOR_RGB565_BLACK);
  screen.print("Chessboard FEN");
}

void drawStatusLine() {
  screen.fillRect(0, 45, 320, 20, COLOR_RGB565_WHITE);
  screen.setCursor(20, 50);
  screen.setTextSize(1);
  screen.setTextColor(COLOR_RGB565_BLACK);

  if (wifiConnected) {
    screen.print("WiFi: Connected");
  } else {
    screen.print("WiFi: Not Connected");
  }
}

void drawMacLine() {
  screen.fillRect(0, 65, 320, 20, COLOR_RGB565_WHITE);
  screen.setCursor(20, 70);
  screen.setTextSize(1);
  screen.setTextColor(COLOR_RGB565_BLACK);
  screen.print("MAC: ");
  screen.print(macAddress);
}

void drawFENRows(const String& fen, int startX, int startY) {
  screen.setTextSize(1);
  screen.setTextColor(COLOR_RGB565_BLACK);

  int cursorX = startX;
  int cursorY = startY;

  for (int i = 0; i < fen.length(); i++) {
    char c = fen[i];

    if (c == ' ') {
      break;
    }

    if (c == '/') {
      cursorY += 22;
      cursorX = startX;
      continue;
    }

    if (c >= '1' && c <= '8') {
      int emptyCount = c - '0';
      for (int j = 0; j < emptyCount; j++) {
        screen.setCursor(cursorX, cursorY);
        screen.print(".");
        cursorX += 14;
      }
      continue;
    }

    screen.setCursor(cursorX, cursorY);
    screen.print(c);
    cursorX += 14;
  }
}

void drawFenBox() {
  screen.fillRect(20, 180, 280, 260, COLOR_RGB565_WHITE);
  screen.drawRect(20, 180, 280, 260, COLOR_RGB565_BLACK);

  screen.setCursor(30, 195);
  screen.setTextSize(1);
  screen.setTextColor(COLOR_RGB565_BLACK);
  screen.print("Board:");

  if (currentFEN == "Fetching..." ||
      currentFEN == "WiFi not connected" ||
      currentFEN == "Request failed" ||
      currentFEN == "JSON parse failed" ||
      currentFEN == "No moves found" ||
      currentFEN == "No fen field found") {
    screen.setCursor(30, 220);
    screen.print(currentFEN);
  } else {
    drawFENRows(currentFEN, 35, 220);
  }
}

void redrawScreen() {
  screen.fillScreen(COLOR_RGB565_WHITE);

  drawTitleLine();
  drawStatusLine();
  //  drawMacLine();

  if (fetchBtnPtr) {
    fetchBtnPtr->bgColor = COLOR_RGB565_GREEN;
    fetchBtnPtr->setText("Fetch FEN");
    ui.draw(fetchBtnPtr, 60, 100, 200, 60);
  }

  drawFenBox();
}

// ===================== API =====================
String fetchLatestFEN() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi not connected");
    return "WiFi not connected";
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

  Serial.println("No fen field found");
  return "No fen field found";
}

// ===================== BUTTON CALLBACK =====================
void fetchCallback(DFRobot_UI::sButton_t &btn, DFRobot_UI::sTextBox_t &obj) {
  (void)btn;
  (void)obj;

  currentFEN = "Fetching...";
  drawFenBox();

  currentFEN = fetchLatestFEN();
  drawStatusLine();
  //drawMacLine();
  drawFenBox();
}

// ===================== SETUP =====================
void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(SCR_BLK, OUTPUT);
  digitalWrite(SCR_BLK, HIGH);

  SPI.begin(SCR_SCLK, SCR_MISO, SCR_MOSI, SCR_CS);
  Wire.begin(SCR_I2C_SDA, SCR_I2C_SCL);

  ui.begin();
  screen.setTextWrap(false);

  connectToWiFi();

  DFRobot_UI::sTextBox_t &tb = ui.creatText();
  tb.bgColor = COLOR_RGB565_WHITE;
  ui.draw(&tb, 0, 0, 1, 1);
  callbackTextBox = &tb;

  DFRobot_UI::sButton_t &fetchBtn = ui.creatButton();
  fetchBtn.setText("Fetch FEN");
  fetchBtn.setCallback(fetchCallback);
  fetchBtn.setOutput(&tb);
  fetchBtnPtr = &fetchBtn;

  redrawScreen();
}

// ===================== LOOP =====================
void loop() {
  ui.refresh();
}
