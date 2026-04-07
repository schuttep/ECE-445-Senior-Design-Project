#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>

#include "headers.h"
#include "display_driver.h"
#include "wifi_driver.h"
#include "api_connect.h"
#include "LED_driver.h"

#include "DFRobot_GDL.h"
#include "DFRobot_Touch.h"

// ===================== GLOBAL OBJECTS =====================
DFRobot_Touch_GT911_IPS touch(0x5D, SCR_TCH_RST, SCR_INT);
DFRobot_ST7365P_320x480_HW_SPI screen(SCR_DC, SCR_CS, SCR_RST, SCR_BLK);

// ===================== BUTTON REGIONS =====================
#define JOIN_BTN_X 30
#define JOIN_BTN_Y 200
#define JOIN_BTN_W 260
#define JOIN_BTN_H 80

#define CREATE_BTN_X 30
#define CREATE_BTN_Y 310
#define CREATE_BTN_W 260
#define CREATE_BTN_H 80

// ===================== STATE =====================
enum ScreenState
{
  MENU,
  GAME
};
ScreenState currentScreen = MENU;
String currentFEN = "";
bool wifiConnected = false;
unsigned long menuShownAt = 0;
unsigned long lastFENPoll = 0;
const unsigned long FEN_POLL_INTERVAL = 5000;

// ===================== FORWARD DECLARATIONS =====================
void showMenuScreen();
void showGameScreen();

// ===================== DRAW BUTTON HELPERS =====================
static void drawFilledButton(int x, int y, int w, int h, uint16_t color, const char *label)
{
  screen.fillRect(x, y, w, h, color);
  screen.setTextSize(2);
  screen.setTextColor(COLOR_RGB565_WHITE);
  // Center the text roughly
  int textX = x + (w - (int)strlen(label) * 12) / 2;
  int textY = y + (h - 16) / 2;
  screen.setCursor(textX, textY);
  screen.print(label);
}

// ===================== SCREEN HELPERS =====================
void showMenuScreen()
{
  currentScreen = MENU;
  screen.fillScreen(COLOR_RGB565_WHITE);
  drawMenuScreen(wifiConnected);

  drawFilledButton(JOIN_BTN_X, JOIN_BTN_Y, JOIN_BTN_W, JOIN_BTN_H, COLOR_RGB565_BLUE, "Join Game");
  drawFilledButton(CREATE_BTN_X, CREATE_BTN_Y, CREATE_BTN_W, CREATE_BTN_H, COLOR_RGB565_GREEN, "Create Game");

  menuShownAt = millis();
}

void showGameScreen()
{
  currentScreen = GAME;

  screen.fillScreen(COLOR_RGB565_WHITE);
  screen.setTextSize(1);
  screen.setTextColor(COLOR_RGB565_BLACK);
  screen.setCursor(60, 230);
  screen.print("Connecting to game...");

  lastFENPoll = millis();
  ApiResult result = fetchLatestFEN();
  if (result.ok)
    currentFEN = result.data;
  lightFEN(currentFEN.c_str());
  drawGameScreen(wifiConnected, result.ok, result.ok ? currentFEN : result.data);
}

// ===================== TOUCH HANDLING =====================
static bool lastTouched = false;

void handleTouch()
{
  touch.scan();
  bool touched = touch._pNum > 0;

  // Only trigger on new press (not hold)
  if (!touched)
  {
    lastTouched = false;
    return;
  }
  if (lastTouched)
    return;
  lastTouched = true;

  if (currentScreen != MENU)
    return;
  if (millis() - menuShownAt < 500)
    return;

  uint16_t tx = touch._point.x;
  uint16_t ty = touch._point.y;

  // Join Game
  if (tx >= JOIN_BTN_X && tx <= JOIN_BTN_X + JOIN_BTN_W &&
      ty >= JOIN_BTN_Y && ty <= JOIN_BTN_Y + JOIN_BTN_H)
  {
    showGameScreen();
    return;
  }

  // Create Game
  if (tx >= CREATE_BTN_X && tx <= CREATE_BTN_X + CREATE_BTN_W &&
      ty >= CREATE_BTN_Y && ty <= CREATE_BTN_Y + CREATE_BTN_H)
  {
    drawFilledButton(CREATE_BTN_X, CREATE_BTN_Y, CREATE_BTN_W, CREATE_BTN_H, COLOR_RGB565_RED, "Coming Soon");
    delay(1000);
    showMenuScreen();
  }
}

// ===================== HARDWARE SETUP =====================
void setupDisplayHardware()
{
  pinMode(SCR_BLK, OUTPUT);
  digitalWrite(SCR_BLK, HIGH);

  SPI.begin(SCR_SCLK, SCR_MISO, SCR_MOSI, SCR_CS);
  Wire.begin(SCR_I2C_SDA, SCR_I2C_SCL);

  touch.begin();
}

// ===================== SETUP =====================
void setup()
{
  Serial.begin(115200);
  delay(1000);

  setupDisplayHardware();
  initDisplay();
  initLEDs();

  drawConnectingScreen();
  wifiConnected = connectWifi();

  demoSequence();
  showMenuScreen();
}

// ===================== LOOP =====================
void loop()
{
  handleTouch();

  if (currentScreen == GAME)
  {
    unsigned long now = millis();
    if (now - lastFENPoll >= FEN_POLL_INTERVAL)
    {
      lastFENPoll = now;
      ApiResult result = fetchLatestFEN();
      if (result.ok && result.data != currentFEN)
      {
        currentFEN = result.data;
        lightFEN(currentFEN.c_str());
        drawGameScreen(wifiConnected, true, currentFEN);
      }
    }
  }
}