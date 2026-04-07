#include "display_driver.h"
#include "DFRobot_GDL.h"

extern DFRobot_ST7365P_320x480_HW_SPI screen;

// ── Layout constants ──────────────────────────────────────────────────────────
static constexpr int SCR_W    = 320;
static constexpr int SCR_H    = 480;
static constexpr int STATUS_Y = 458; // y-start of bottom status bar
static constexpr int STATUS_H = 22;  // height of status bar

// ── Internal helpers ──────────────────────────────────────────────────────────
// Render the piece-placement section of a FEN string as a character grid.
static void drawFENRows(const String &fen, int startX, int startY)
{
  screen.setTextSize(1);
  screen.setTextColor(COLOR_RGB565_BLACK);

  int x = startX;
  int y = startY;

  for (int i = 0; i < (int)fen.length(); i++)
  {
    char c = fen[i];
    if (c == ' ') break;
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
        screen.print('.');
        x += 14;
      }
      continue;
    }
    screen.setCursor(x, y);
    screen.print(c);
    x += 14;
  }
}

// ── Primitives ────────────────────────────────────────────────────────────────
void displayClear()
{
  screen.fillScreen(COLOR_RGB565_WHITE);
}

void displayHeader(bool wifiConnected)
{
  screen.setTextSize(1);
  screen.setTextColor(COLOR_RGB565_BLACK);

  screen.setCursor(20, 20);
  screen.print("ChessBoard");

  screen.setCursor(200, 20);
  screen.print(wifiConnected ? "WiFi: OK" : "WiFi: --");

  screen.drawFastHLine(0, 38, SCR_W, COLOR_RGB565_BLACK);
}

void displayCenteredText(const char *text, int y, uint8_t size, uint16_t color)
{
  int textW = (int)strlen(text) * 6 * size;
  int x = (SCR_W - textW) / 2;
  if (x < 0) x = 0;
  screen.setTextSize(size);
  screen.setTextColor(color);
  screen.setCursor(x, y);
  screen.print(text);
}

void displayButton(int x, int y, int w, int h, uint16_t color, const char *label)
{
  screen.fillRect(x, y, w, h, color);
  screen.setTextSize(2);
  screen.setTextColor(COLOR_RGB565_WHITE);
  int textX = x + (w - (int)strlen(label) * 12) / 2;
  int textY = y + (h - 16) / 2;
  screen.setCursor(textX, textY);
  screen.print(label);
}

void displayStatusBar(const char *msg, uint16_t bgColor)
{
  screen.fillRect(0, STATUS_Y, SCR_W, STATUS_H, bgColor);
  screen.setTextSize(1);
  screen.setTextColor(COLOR_RGB565_WHITE);
  screen.setCursor(6, STATUS_Y + 7);
  screen.print(msg);
}

void displayDivider(int y, uint16_t color)
{
  screen.drawFastHLine(0, y, SCR_W, color);
}

// ── Full screens ──────────────────────────────────────────────────────────────
void initDisplay()
{
  screen.begin();
  screen.setTextWrap(false);
}

void drawConnectingScreen(const char *ssid)
{
  displayClear();
  displayCenteredText("ChessBoard", 130, 3, COLOR_RGB565_BLACK);
  displayCenteredText("Connecting to WiFi...", 192, 1, COLOR_RGB565_BLACK);
  displayCenteredText(ssid, 212, 1, COLOR_RGB565_BLUE);
  displayStatusBar("Please wait...", COLOR_RGB565_BLUE);
}

void drawMenuScreen(bool wifiConnected)
{
  displayClear();
  displayHeader(wifiConnected);
  displayCenteredText("Chess Board", 120, 2, COLOR_RGB565_BLACK);
}

void drawGameScreen(bool wifiConnected, bool fenOk, const String &data)
{
  displayClear();
  displayHeader(wifiConnected);

  screen.setTextSize(1);
  screen.setTextColor(COLOR_RGB565_BLACK);
  screen.setCursor(20, 50);
  screen.print("Board:");

  if (fenOk)
    drawFENRows(data, 20, 75);
  else
  {
    screen.setCursor(20, 75);
    screen.print(data);
  }

  displayStatusBar(
    fenOk ? "Live - updates every 5s" : "Fetch error",
    fenOk ? COLOR_RGB565_GREEN : COLOR_RGB565_RED
  );
}

void drawErrorScreen(const char *title, const char *detail)
{
  // Red banner at top
  screen.fillRect(0, 0, SCR_W, 52, COLOR_RGB565_RED);
  screen.setTextSize(2);
  screen.setTextColor(COLOR_RGB565_WHITE);
  screen.setCursor(12, 18);
  screen.print("! ERROR");

  // White body
  screen.fillRect(0, 52, SCR_W, SCR_H - 52 - STATUS_H, COLOR_RGB565_WHITE);

  // Bold title + divider
  displayCenteredText(title, 68, 2, COLOR_RGB565_RED);
  displayDivider(100, COLOR_RGB565_RED);

  // Multi-line detail text (wrap enabled)
  screen.setTextWrap(true);
  screen.setTextSize(1);
  screen.setTextColor(COLOR_RGB565_BLACK);
  screen.setCursor(10, 112);
  screen.print(detail);
  screen.setTextWrap(false);

  displayStatusBar("Tap anywhere to dismiss", COLOR_RGB565_RED);
}

void drawDebugScreen(const char *const lines[], uint8_t count)
{
  screen.fillScreen(COLOR_RGB565_BLACK);

  // Green header bar with black title text
  screen.fillRect(0, 0, SCR_W, 18, COLOR_RGB565_GREEN);
  screen.setTextSize(1);
  screen.setTextColor(COLOR_RGB565_BLACK);
  screen.setCursor(4, 5);
  screen.print("-- DEBUG --");

  // Log lines in green
  screen.setTextColor(COLOR_RGB565_GREEN);
  static constexpr uint8_t MAX_VISIBLE = 30;
  uint8_t n = (count < MAX_VISIBLE) ? count : MAX_VISIBLE;
  for (uint8_t i = 0; i < n; i++)
  {
    if (lines[i] == nullptr) break;
    screen.setCursor(4, 22 + i * 14);
    screen.print(lines[i]);
  }
}

// ── WiFi screens ──────────────────────────────────────────────────────────────

// 4-bar signal strength indicator, 22px wide.
static void drawSignalBars(int x, int y, int8_t rssi)
{
  int bars = (rssi > -60) ? 4 : (rssi > -70) ? 3 : (rssi > -80) ? 2 : 1;
  for (int i = 0; i < 4; i++)
  {
    int bx = x + i * 6;
    int bh = 4 + i * 3;
    int by = y + 12 - bh;
    screen.fillRect(bx, by, 4, bh, (i < bars) ? COLOR_RGB565_BLACK : (uint16_t)0xC618);
  }
}

void drawKeyboard(bool shifted, bool symbols)
{
  // Character lookup: [row][col=0:lower, 1:upper, 2:symbols]
  static const char* LUT[3][3] = {
    { "qwertyuiop", "QWERTYUIOP", "1234567890" },
    { "asdfghjkl",  "ASDFGHJKL",  "@#$%&-_+=" },
    { "zxcvbnm",    "ZXCVBNM",    "!'\"();:"   }
  };
  int col    = symbols ? 2 : (shifted ? 1 : 0);
  const char* row1 = LUT[0][col];
  const char* row2 = LUT[1][col];
  const char* row3 = LUT[2][col];

  // Keyboard background
  screen.fillRect(0, KB_ROW1_Y - 4, SCR_W,
                  KB_ROW4_Y + KB_KEY_H + 6 - (KB_ROW1_Y - 4), 0xDEDB);

  // Row 1 – 10 keys
  for (int i = 0; i < 10; i++)
  {
    int kx = KB_ROW1_X + i * KB_STRIDE;
    screen.fillRect(kx, KB_ROW1_Y, KB_STD_W, KB_KEY_H, (uint16_t)0x8410);
    screen.setTextSize(1); screen.setTextColor(COLOR_RGB565_WHITE);
    screen.setCursor(kx + 10, KB_ROW1_Y + 16); screen.print(row1[i]);
  }
  // Row 2 – 9 keys
  for (int i = 0; row2[i]; i++)
  {
    int kx = KB_ROW2_X + i * KB_STRIDE;
    screen.fillRect(kx, KB_ROW2_Y, KB_STD_W, KB_KEY_H, (uint16_t)0x8410);
    screen.setTextSize(1); screen.setTextColor(COLOR_RGB565_WHITE);
    screen.setCursor(kx + 10, KB_ROW2_Y + 16); screen.print(row2[i]);
  }
  // Row 3 – Shift/ABC + 7 keys + DEL
  screen.fillRect(0, KB_ROW3_Y, KB_SHIFT_W, KB_KEY_H,
                  symbols ? (uint16_t)0x4208
                          : (shifted ? COLOR_RGB565_BLUE : (uint16_t)0x4208));
  screen.setTextSize(1); screen.setTextColor(COLOR_RGB565_WHITE);
  screen.setCursor(4, KB_ROW3_Y + 16);
  screen.print(symbols ? "ABC" : (shifted ? "SFT" : "sft"));

  for (int i = 0; row3[i]; i++)
  {
    int kx = KB_ROW3_LX + i * KB_STRIDE;
    screen.fillRect(kx, KB_ROW3_Y, KB_STD_W, KB_KEY_H, (uint16_t)0x8410);
    screen.setTextSize(1); screen.setTextColor(COLOR_RGB565_WHITE);
    screen.setCursor(kx + 10, KB_ROW3_Y + 16); screen.print(row3[i]);
  }
  screen.fillRect(KB_DEL_X, KB_ROW3_Y, KB_DEL_W, KB_KEY_H, (uint16_t)0x4208);
  screen.setTextSize(1); screen.setTextColor(COLOR_RGB565_WHITE);
  screen.setCursor(KB_DEL_X + 14, KB_ROW3_Y + 16); screen.print("DEL");

  // Row 4 – Sym toggle + Space + Done
  screen.fillRect(0, KB_ROW4_Y, KB_SYM_W, KB_KEY_H, (uint16_t)0x4208);
  screen.setTextSize(1); screen.setTextColor(COLOR_RGB565_WHITE);
  screen.setCursor(8, KB_ROW4_Y + 16);
  screen.print(symbols ? "ABC" : "?123");

  screen.fillRect(KB_SPACE_X, KB_ROW4_Y, KB_SPACE_W, KB_KEY_H, (uint16_t)0x8410);
  screen.setTextColor(COLOR_RGB565_WHITE);
  screen.setCursor(KB_SPACE_X + 60, KB_ROW4_Y + 16); screen.print("SPACE");

  screen.fillRect(KB_DONE_X, KB_ROW4_Y, KB_DONE_W, KB_KEY_H, COLOR_RGB565_GREEN);
  screen.setTextColor(COLOR_RGB565_WHITE);
  screen.setCursor(KB_DONE_X + 10, KB_ROW4_Y + 16); screen.print("DONE");
}

void drawPasswordField(const char* password, bool showChars)
{
  screen.fillRect(8,   72, 260, 38, (uint16_t)0xEF7D);
  screen.drawRect(8,   72, 260, 38, COLOR_RGB565_BLACK);
  screen.setTextSize(1); screen.setTextColor(COLOR_RGB565_BLACK);
  screen.setCursor(12, 84);
  if (showChars) {
    screen.print(password);
  } else {
    for (int i = 0; password[i]; i++) screen.print('*');
  }
  // Show / Hide toggle button
  screen.fillRect(272, 72, 44, 38, (uint16_t)0x6B4D);
  screen.setTextSize(1); screen.setTextColor(COLOR_RGB565_WHITE);
  screen.setCursor(274, 84); screen.print(showChars ? "HIDE" : "SHOW");
}

void drawPasswordScreen(const char* ssid, const char* password,
                        bool showChars, bool shifted, bool symbols)
{
  displayClear();
  screen.fillRect(0, 0, SCR_W, 38, COLOR_RGB565_BLACK);
  screen.setTextSize(1); screen.setTextColor(COLOR_RGB565_WHITE);
  screen.setCursor(6, 15); screen.print("< Back");
  displayCenteredText("Enter Password", 15, 1, COLOR_RGB565_WHITE);
  displayDivider(39, (uint16_t)0xC618);

  screen.setTextSize(1); screen.setTextColor(COLOR_RGB565_BLACK);
  screen.setCursor(10, 48); screen.print("Network: ");
  screen.setTextColor(COLOR_RGB565_BLUE); screen.print(ssid);

  screen.setTextColor(COLOR_RGB565_BLACK);
  screen.setCursor(10, 66); screen.print("Password:");

  drawPasswordField(password, showChars);
  drawKeyboard(shifted, symbols);
}

void drawWifiListScreen(const ScannedNetwork* nets, uint8_t count, bool scanning)
{
  displayClear();
  screen.fillRect(0, 0, SCR_W, 38, COLOR_RGB565_BLACK);
  screen.setTextSize(1); screen.setTextColor(COLOR_RGB565_WHITE);
  screen.setCursor(6, 15); screen.print("< Back");
  displayCenteredText("WiFi Settings", 15, 1, COLOR_RGB565_WHITE);
  screen.fillRect(238, 4, 78, 30, (uint16_t)0x2945); // teal Rescan btn
  screen.setCursor(248, 15); screen.print("Rescan");
  displayDivider(39, (uint16_t)0xC618);

  if (scanning)
  {
    displayCenteredText("Scanning...", 200, 1, COLOR_RGB565_BLACK);
    displayStatusBar("Please wait...", COLOR_RGB565_BLUE);
    return;
  }
  if (count == 0)
  {
    displayCenteredText("No networks found", 190, 1, COLOR_RGB565_BLACK);
    displayCenteredText("Tap Rescan to retry", 210, 1, COLOR_RGB565_BLACK);
    displayStatusBar("No networks in range", (uint16_t)0x8410);
    return;
  }

  for (uint8_t i = 0; i < count && i < 7; i++)
  {
    int ry = WIFLIST_ROW_Y_START + i * WIFLIST_ROW_H;
    screen.fillRect(0, ry, SCR_W, WIFLIST_ROW_H - 1,
                    (i % 2 == 0) ? COLOR_RGB565_WHITE : (uint16_t)0xF7BE);
    drawSignalBars(8, ry + 4, nets[i].rssi);

    screen.setTextSize(1); screen.setTextColor(COLOR_RGB565_BLACK);
    screen.setCursor(36, ry + 8);
    char truncated[22]; strncpy(truncated, nets[i].ssid, 21); truncated[21] = '\0';
    screen.print(truncated);

    screen.setTextColor((uint16_t)0x8410);
    screen.setCursor(36, ry + 26);
    screen.print(nets[i].rssi); screen.print(" dBm");

    if (nets[i].saved)
    {
      screen.fillRect(274, ry + 12, 42, 18, COLOR_RGB565_GREEN);
      screen.setTextSize(1); screen.setTextColor(COLOR_RGB565_WHITE);
      screen.setCursor(276, ry + 17); screen.print("SAVED");
    }
    else
    {
      screen.fillRect(278, ry + 12, 38, 18, COLOR_RGB565_BLUE);
      screen.setTextSize(1); screen.setTextColor(COLOR_RGB565_WHITE);
      screen.setCursor(282, ry + 17); screen.print("NEW");
    }
  }

  char statusMsg[46];
  snprintf(statusMsg, sizeof(statusMsg), "%d network%s found  -  tap to connect",
           count, count == 1 ? "" : "s");
  displayStatusBar(statusMsg, (uint16_t)0x4208);
}

char keyboardHitTest(int tx, int ty, bool symbols)
{
  static const char* ROW_LOWER[3] = { "qwertyuiop", "asdfghjkl", "zxcvbnm"   };
  static const char* ROW_SYM[3]   = { "1234567890", "@#$%&-_+=", "!'\"();:" };

  int row = -1;
  if      (ty >= KB_ROW1_Y && ty < KB_ROW1_Y + KB_KEY_H) row = 0;
  else if (ty >= KB_ROW2_Y && ty < KB_ROW2_Y + KB_KEY_H) row = 1;
  else if (ty >= KB_ROW3_Y && ty < KB_ROW3_Y + KB_KEY_H) row = 2;
  else if (ty >= KB_ROW4_Y && ty < KB_ROW4_Y + KB_KEY_H) row = 3;
  else return 0;

  if (row == 0)
  {
    int idx = (tx - KB_ROW1_X) / KB_STRIDE;
    if (tx < KB_ROW1_X || idx < 0 || idx > 9) return 0;
    return (symbols ? ROW_SYM : ROW_LOWER)[0][idx];
  }
  if (row == 1)
  {
    const char* r = (symbols ? ROW_SYM : ROW_LOWER)[1];
    int idx = (tx - KB_ROW2_X) / KB_STRIDE;
    if (tx < KB_ROW2_X || idx < 0 || idx >= (int)strlen(r)) return 0;
    return r[idx];
  }
  if (row == 2)
  {
    if (tx < KB_SHIFT_W)  return '\t'; // shift / ABC toggle
    if (tx >= KB_DEL_X)   return '\b'; // backspace
    const char* r = (symbols ? ROW_SYM : ROW_LOWER)[2];
    int idx = (tx - KB_ROW3_LX) / KB_STRIDE;
    if (tx < KB_ROW3_LX || idx < 0 || idx >= (int)strlen(r)) return 0;
    return r[idx];
  }
  // row == 3
  if (tx < KB_SYM_W)   return 0x01; // symbols page toggle
  if (tx >= KB_DONE_X) return '\n'; // Done
  return ' ';                        // Space
}
