#include "display_driver.h"
#include "DFRobot_GDL.h"

// screen object defined in .ino
extern DFRobot_ST7365P_320x480_HW_SPI screen;

static void drawTitle() {
  screen.fillRect(0, 15, 320, 20, COLOR_RGB565_WHITE);
  screen.setCursor(20, 20);
  screen.setTextSize(1);
  screen.setTextColor(COLOR_RGB565_BLACK);
  screen.print("Chessboard FEN");
}

static void drawStatusLine(bool wifiConnected) {
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

static void drawFENRows(const String& fen, int startX, int startY) {
  screen.setTextSize(1);
  screen.setTextColor(COLOR_RGB565_BLACK);

  int x = startX;
  int y = startY;

  for (int i = 0; i < fen.length(); i++) {
    char c = fen[i];

    if (c == ' ') break;

    if (c == '/') {
      y += 22;
      x = startX;
      continue;
    }

    if (c >= '1' && c <= '8') {
      int count = c - '0';
      for (int j = 0; j < count; j++) {
        screen.setCursor(x, y);
        screen.print(".");
        x += 14;
      }
      continue;
    }

    screen.setCursor(x, y);
    screen.print(c);
    x += 14;
  }
}

void initDisplay() {
  screen.fillScreen(COLOR_RGB565_WHITE);
  screen.setTextWrap(false);
}

void drawConnectingScreen() {
  screen.fillScreen(COLOR_RGB565_WHITE);
  screen.setTextSize(1);
  screen.setTextColor(COLOR_RGB565_BLACK);
  screen.setCursor(20, 20);
  screen.print("Connecting to WiFi...");
}

void drawFenBox(const String& fen) {
  screen.fillRect(20, 180, 280, 260, COLOR_RGB565_WHITE);
  screen.drawRect(20, 180, 280, 260, COLOR_RGB565_BLACK);

  screen.setCursor(30, 195);
  screen.setTextSize(1);
  screen.setTextColor(COLOR_RGB565_BLACK);
  screen.print("Board:");

  if (fen == "Fetching..." ||
      fen == "WiFi not connected" ||
      fen == "Request failed" ||
      fen == "JSON parse failed" ||
      fen == "No moves found" ||
      fen == "No fen field found") {
    screen.setCursor(30, 220);
    screen.print(fen);
  } else {
    drawFENRows(fen, 35, 220);
  }
}

void drawMainScreen(bool wifiConnected, const String& fen) {
  screen.fillScreen(COLOR_RGB565_WHITE);
  drawTitle();
  drawStatusLine(wifiConnected);
  drawFenBox(fen);
}