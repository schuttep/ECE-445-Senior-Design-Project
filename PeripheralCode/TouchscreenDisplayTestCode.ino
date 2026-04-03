#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include "DFRobot_GDL.h"
#include "DFRobot_Touch.h"
#include "DFRobot_UI.h"

// ===== Your pins =====
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

// ===== Touch + screen objects =====
// The official ST7365P touch-button example uses GT911 touch and the
// ST7365P_320x480 HW SPI screen constructor. :contentReference[oaicite:1]{index=1}
DFRobot_Touch_GT911_IPS touch(0x5D, SCR_TCH_RST, SCR_INT);
DFRobot_ST7365P_320x480_HW_SPI screen(SCR_DC, SCR_CS, SCR_RST, SCR_BLK);
DFRobot_UI ui(&screen, &touch);

// ===== UI state =====
bool greenMode = false;

DFRobot_UI::sButton_t* toggleBtnPtr = nullptr;
const uint16_t BTN_X = 90;
const uint16_t BTN_Y = 180;
const uint16_t BTN_W = 140;
const uint16_t BTN_H = 70;

void drawBackground() {
  screen.fillScreen(greenMode ? COLOR_RGB565_GREEN : COLOR_RGB565_WHITE);
}

void drawButton() {
  if (!toggleBtnPtr) return;

  // Make button easy to see against current background
  toggleBtnPtr->bgColor = greenMode ? COLOR_RGB565_WHITE : COLOR_RGB565_GREEN;
  toggleBtnPtr->setText("TOGGLE");

  ui.draw(toggleBtnPtr, BTN_X, BTN_Y, BTN_W, BTN_H);

  // Simple title text
  screen.setTextSize(2);
  screen.setTextColor(greenMode ? COLOR_RGB565_WHITE : COLOR_RGB565_BLACK);
  screen.setCursor(70, 90);
  screen.print(greenMode ? "SCREEN: GREEN" : "SCREEN: WHITE");
}

void toggleCallback(DFRobot_UI::sButton_t &btn, DFRobot_UI::sTextBox_t &obj) {
  (void)btn;
  (void)obj;

  greenMode = !greenMode;
  drawBackground();
  drawButton();
}

void setup() {
  Serial.begin(115200);

  // Custom buses for your ESP32-S3 pinout
  SPI.begin(SCR_SCLK, SCR_MISO, SCR_MOSI, SCR_CS);
  Wire.begin(SCR_I2C_SDA, SCR_I2C_SCL);

  // Init UI system
  ui.begin();

  // Keep default rotation first.
  // There is a repo issue noting touch mapping trouble after setRotation(1),
  // so this avoids that for now. :contentReference[oaicite:2]{index=2}

  drawBackground();

  // Dummy textbox because the callback signature expects one output object
  DFRobot_UI::sTextBox_t &tb = ui.creatText();
  tb.bgColor = greenMode ? COLOR_RGB565_GREEN : COLOR_RGB565_WHITE;
  ui.draw(&tb, 0, 0, 1, 1);  // tiny hidden placeholder

  // Create the button
  DFRobot_UI::sButton_t &toggleBtn = ui.creatButton();
  toggleBtn.setText("TOGGLE");
  toggleBtn.setCallback(toggleCallback);
  toggleBtn.setOutput(&tb);
  toggleBtnPtr = &toggleBtn;

  drawButton();
} 

void loop() {
  ui.refresh();
}
