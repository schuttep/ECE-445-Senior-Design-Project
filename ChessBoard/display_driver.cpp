#include "display_driver.h"
#include "DFRobot_GDL.h"

extern DFRobot_ST7365P_320x480_HW_SPI screen;

static void drawHeader(bool wifiConnected)
{
  screen.setTextSize(1);
  screen.setTextColor(COLOR_RGB565_BLACK);

  // Title
  screen.setCursor(20, 20);
  screen.print("ChessBoard");

  // WiFi status top-right
  screen.setCursor(200, 20);
  if (wifiConnected)
    screen.print("WiFi: OK");
  else
    screen.print("WiFi: --");

  // Divider
  screen.drawFastHLine(0, 38, 320, COLOR_RGB565_BLACK);
}

static void drawFENRows(const String &fen, int startX, int startY)
{
  screen.setTextSize(1);
  screen.setTextColor(COLOR_RGB565_BLACK);

  int x = startX;
  int y = startY;

  for (int i = 0; i < (int)fen.length(); i++)
  {
    char c = fen[i];
    if (c == ' ')
      break;
    if (c == '/')
    {
      y += 22;
      x = startX;
      continue;
    }
    if (c >= '1' && c <= '8')
    {
      int count = c - '0';
      for (int j = 0; j < count; j++)
      {
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

void initDisplay()
{
  screen.begin();
  screen.setTextWrap(false);
}

void drawConnectingScreen()
{
  screen.fillScreen(COLOR_RGB565_WHITE);
  screen.setTextSize(1);
  screen.setTextColor(COLOR_RGB565_BLACK);
  screen.setCursor(80, 230);
  screen.print("Connecting to WiFi...");
}

void drawMenuScreen(bool wifiConnected)
{
  screen.fillScreen(COLOR_RGB565_WHITE);
  drawHeader(wifiConnected);

  // Centered title
  screen.setTextSize(2);
  screen.setTextColor(COLOR_RGB565_BLACK);
  screen.setCursor(70, 120);
  screen.print("Chess Board");
}

void drawGameScreen(bool wifiConnected, bool fenOk, const String &data)
{
  screen.fillScreen(COLOR_RGB565_WHITE);
  drawHeader(wifiConnected);

  screen.setTextSize(1);
  screen.setTextColor(COLOR_RGB565_BLACK);
  screen.setCursor(20, 50);
  screen.print("Board:");

  screen.setCursor(20, 75);
  if (fenOk)
    drawFENRows(data, 20, 75);
  else
    screen.print(data);
}