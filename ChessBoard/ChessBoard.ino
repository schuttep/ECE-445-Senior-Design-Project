#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>

#include "headers.h"
#include "display_driver.h"
#include "wifi_driver.h"
#include "api_connect.h"

#include "DFRobot_GDL.h"
#include "DFRobot_Touch.h"
#include "DFRobot_UI.h"

// ===================== GLOBAL OBJECTS =====================
DFRobot_Touch_GT911_IPS touch(0x5D, SCR_TCH_RST, SCR_INT);
DFRobot_ST7365P_320x480_HW_SPI screen(SCR_DC, SCR_CS, SCR_RST, SCR_BLK);
DFRobot_UI ui(&screen, &touch);

// ===================== UI OBJECTS =====================
DFRobot_UI::sButton_t* fetchBtnPtr = nullptr;
DFRobot_UI::sTextBox_t* callbackTextBox = nullptr;

// ===================== STATE =====================
String currentFEN = "Press button to fetch FEN";
bool wifiConnected = false;

// ===================== BUTTON CALLBACK =====================
void fetchCallback(DFRobot_UI::sButton_t &btn, DFRobot_UI::sTextBox_t &obj) {
  (void)btn;
  (void)obj;

  currentFEN = "Fetching...";
  drawFenBox(currentFEN);

  currentFEN = fetchLatestFEN();
  drawMainScreen(wifiConnected, currentFEN);

  if (fetchBtnPtr) {
    fetchBtnPtr->bgColor = COLOR_RGB565_GREEN;
    fetchBtnPtr->setText("Fetch FEN");
    ui.draw(fetchBtnPtr, 60, 100, 200, 60);
  }
}

// ===================== HARDWARE SETUP =====================
void setupDisplayHardware() {
  pinMode(SCR_BLK, OUTPUT);
  digitalWrite(SCR_BLK, HIGH);

  SPI.begin(SCR_SCLK, SCR_MISO, SCR_MOSI, SCR_CS);
  Wire.begin(SCR_I2C_SDA, SCR_I2C_SCL);

  ui.begin();
}

// ===================== SETUP =====================
void setup() {
  Serial.begin(115200);
  delay(1000);

  setupDisplayHardware();
  initDisplay();

  drawConnectingScreen();

  wifiConnected = connectWifi();

  DFRobot_UI::sTextBox_t &tb = ui.creatText();
  tb.bgColor = COLOR_RGB565_WHITE;
  ui.draw(&tb, 0, 0, 1, 1);
  callbackTextBox = &tb;

  DFRobot_UI::sButton_t &fetchBtn = ui.creatButton();
  fetchBtn.setText("Fetch FEN");
  fetchBtn.setCallback(fetchCallback);
  fetchBtn.setOutput(&tb);
  fetchBtnPtr = &fetchBtn;

  drawMainScreen(wifiConnected, currentFEN);

  if (fetchBtnPtr) {
    fetchBtnPtr->bgColor = COLOR_RGB565_GREEN;
    fetchBtnPtr->setText("Fetch FEN");
    ui.draw(fetchBtnPtr, 60, 100, 200, 60);
  }
}

// ===================== LOOP =====================
void loop() {
  ui.refresh();
}