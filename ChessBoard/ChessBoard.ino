#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <WiFi.h>

#include "headers.h"
#include "display_driver.h"
#include "wifi_driver.h"
#include "api_connect.h"
#include "LED_driver.h"
#include "ADC_driver.h"
#include "wifi_manager.h"
#include "gameloop.h"

#include "DFRobot_GDL.h"
#include "DFRobot_Touch.h"

// ===================== GLOBAL OBJECTS =====================
DFRobot_Touch_GT911_IPS touch(0x5D, SCR_TCH_RST, SCR_INT);
DFRobot_ST7365P_320x480_HW_SPI screen(SCR_DC, SCR_CS, SCR_RST, SCR_BLK);

// ===================== BUTTON REGIONS =====================
// Menu buttons (landscape 480×320)
#define JOIN_BTN_X 50
#define JOIN_BTN_Y 75
#define JOIN_BTN_W 380
#define JOIN_BTN_H 55

#define CREATE_BTN_X 50
#define CREATE_BTN_Y 145
#define CREATE_BTN_W 380
#define CREATE_BTN_H 55

#define WIFI_BTN_X 50
#define WIFI_BTN_Y 215
#define WIFI_BTN_W 380
#define WIFI_BTN_H 55

// Confirm / Cancel in right info panel of the game screen
#define CONFIRM_BTN_X 272
#define CONFIRM_BTN_Y 200
#define CONFIRM_BTN_W 192
#define CONFIRM_BTN_H 44

#define CANCEL_BTN_X 272
#define CANCEL_BTN_Y 252
#define CANCEL_BTN_W 192
#define CANCEL_BTN_H 44

// ===================== STATE =====================
enum ScreenState
{
  MENU,
  GAME,
  BOARD_TEST,
  WIFI_LIST,
  WIFI_PASS_SCREEN
};
ScreenState currentScreen = MENU;
bool wifiConnected = false;
unsigned long menuShownAt = 0;
unsigned long lastBoardTestUpdate = 0;

// Game screen display tracking
static String gs_lastRenderedFEN;
static bool gs_lastConfirmState = false;
static bool gs_lastGameOver = false;
static bool gs_lastCheckState = false;
static String gs_lastIncomingFEN;
static String gs_lastPendingFEN;
static char gs_liftSquare[3] = {0};
static bool gs_liftShown = false;
static String gs_lastTurnStatus; // tracks last status bar turn message

// Physical board FEN buffer (72 bytes = worst-case FEN board + null)
static char gs_boardFENBuf[72];

// WiFi manager UI state
ScannedNetwork scannedNets[WM_MAX_SCAN];
uint8_t scannedCount = 0;
char selectedSSID[WM_SSID_LEN];
char passwordBuf[WM_PASS_LEN];
bool kbShifted = false;
bool kbSymbols = false;
bool kbShowChars = false;

// ===================== FORWARD DECLARATIONS =====================
void showMenuScreen();
void showGameScreen();
void drawConfirmOverlay();
void showWifiListScreen();
void showWifiPassScreen(const char *ssid);
void handleWifiListTouch(int tx, int ty);
void handleWifiPassTouch(int tx, int ty);
void runBoardTests();
void drawBoardTestLive();
static void waitForTap();
static void readTouchPoint(uint16_t &tx, uint16_t &ty);

// ===================== SCREEN HELPERS =====================
void showMenuScreen()
{
  currentScreen = MENU;
  drawMenuScreen(wifiConnected);

  displayButton(JOIN_BTN_X, JOIN_BTN_Y, JOIN_BTN_W, JOIN_BTN_H, COLOR_RGB565_BLUE, "Join Game");
  displayButton(CREATE_BTN_X, CREATE_BTN_Y, CREATE_BTN_W, CREATE_BTN_H, 0xFD20, "Create Game");
  displayButton(WIFI_BTN_X, WIFI_BTN_Y, WIFI_BTN_W, WIFI_BTN_H, 0x7BEF, "WiFi Settings");

  menuShownAt = millis();
}

void showGameScreen()
{
  currentScreen = GAME;
  gs_lastRenderedFEN = "";
  gs_lastConfirmState = false;
  gs_lastGameOver = false;
  gs_lastCheckState = false;
  gs_lastIncomingFEN = "";
  gs_lastPendingFEN = "";
  gs_liftSquare[0] = 0;
  gs_liftShown = false;
  gs_lastTurnStatus = "";

  // Show a placeholder board while the FSM initialises
  drawGameScreen(wifiConnected, false, String("Starting game..."));
  // Note: the caller (touch handler) must have already called cgm_createGameNow()
  // or cgm_joinGameNow() before calling showGameScreen().
}

// Draw confirm / cancel buttons as an overlay on the game screen.
void drawConfirmOverlay()
{
  displayButton(CONFIRM_BTN_X, CONFIRM_BTN_Y, CONFIRM_BTN_W, CONFIRM_BTN_H,
                COLOR_RGB565_GREEN, "Confirm");
  displayButton(CANCEL_BTN_X, CANCEL_BTN_Y, CANCEL_BTN_W, CONFIRM_BTN_H,
                COLOR_RGB565_RED, "Cancel");
}

void showWifiListScreen()
{
  currentScreen = WIFI_LIST;
  drawWifiListScreen(nullptr, 0, true); // show "Scanning..." placeholder
  scannedCount = wmScan(scannedNets);   // blocking scan ~2-3 s
  drawWifiListScreen(scannedNets, scannedCount, false);
}

void showWifiPassScreen(const char *ssid)
{
  currentScreen = WIFI_PASS_SCREEN;
  strncpy(selectedSSID, ssid, WM_SSID_LEN - 1);
  selectedSSID[WM_SSID_LEN - 1] = '\0';
  memset(passwordBuf, 0, sizeof(passwordBuf));
  kbShifted = false;
  kbSymbols = false;
  kbShowChars = false;
  drawPasswordScreen(selectedSSID, passwordBuf, kbShowChars, kbShifted, kbSymbols);
}

// ===================== TOUCH HANDLING =====================
static bool lastTouched = false;

static void waitForTap()
{
  delay(300);
  while (true)
  {
    touch.scan();
    if (touch._pNum > 0)
      break;
    delay(50);
  }
  delay(200);
  lastTouched = true; // prevent immediate double-trigger
}

static void readTouchPoint(uint16_t &tx, uint16_t &ty)
{
  uint16_t rawX = touch._point.x;
  uint16_t rawY = touch._point.y;

  // Display rotation is flipped 180°, so mirror touch coordinates too.
  tx = 479 - rawX;
  ty = 319 - rawY;
}

void handleWifiListTouch(int tx, int ty)
{
  if (ty < 38 && tx < 80)
  {
    showMenuScreen();
    return;
  } // Back
  if (ty < 38 && tx > 360)
  {
    showWifiListScreen();
    return;
  } // Rescan

  if (ty >= WIFLIST_ROW_Y_START && scannedCount > 0)
  {
    int idx = (ty - WIFLIST_ROW_Y_START) / WIFLIST_ROW_H;
    if (idx < 0 || idx >= (int)scannedCount)
      return;
    showWifiPassScreen(scannedNets[idx].ssid);
  }
}

void handleWifiPassTouch(int tx, int ty)
{
  if (ty < 38 && tx < 80)
  {
    showWifiListScreen();
    return;
  } // Back

  // Show / Hide password toggle (landscape: x 396–474)
  if (tx >= 396 && tx <= 474 && ty >= 72 && ty <= 110)
  {
    kbShowChars = !kbShowChars;
    drawPasswordField(passwordBuf, kbShowChars);
    return;
  }

  if (ty < KB_ROW1_Y)
    return;
  char key = keyboardHitTest(tx, ty, kbSymbols);
  if (key == 0)
    return;

  if (key == '\t') // shift toggle
  {
    kbShifted = !kbShifted;
    drawKeyboard(kbShifted, kbSymbols);
  }
  else if (key == 0x01) // symbols page toggle
  {
    kbSymbols = !kbSymbols;
    kbShifted = false;
    drawKeyboard(kbShifted, kbSymbols);
  }
  else if (key == '\b') // backspace
  {
    int len = strlen(passwordBuf);
    if (len > 0)
    {
      passwordBuf[len - 1] = '\0';
      drawPasswordField(passwordBuf, kbShowChars);
    }
  }
  else if (key == '\n') // Done
  {
    if (strlen(passwordBuf) == 0)
      return;
    displayStatusBar("Connecting...", COLOR_RGB565_BLUE);
    if (wmConnect(selectedSSID, passwordBuf))
    {
      wifiConnected = true;
      displayStatusBar("Connected!", COLOR_RGB565_GREEN);
      delay(1200);
      showMenuScreen();
    }
    else
    {
      displayStatusBar("Connection failed - check password", COLOR_RGB565_RED);
    }
  }
  else // printable character
  {
    int len = strlen(passwordBuf);
    if (len < WM_PASS_LEN - 1)
    {
      char ch = (!kbSymbols && kbShifted) ? (char)toupper(key) : key;
      passwordBuf[len] = ch;
      passwordBuf[len + 1] = '\0';
      drawPasswordField(passwordBuf, kbShowChars);
      if (kbShifted && !kbSymbols) // one-shot shift
      {
        kbShifted = false;
        drawKeyboard(kbShifted, kbSymbols);
      }
    }
  }
}

void handleTouch()
{
  touch.scan();
  bool touched = touch._pNum > 0;

  if (!touched)
  {
    lastTouched = false;
    return;
  }
  if (lastTouched)
    return;
  lastTouched = true;

  uint16_t tx = 0;
  uint16_t ty = 0;
  readTouchPoint(tx, ty);

  if (currentScreen == MENU)
  {
    if (millis() - menuShownAt < 500)
      return;

    if (tx >= JOIN_BTN_X && tx <= JOIN_BTN_X + JOIN_BTN_W &&
        ty >= JOIN_BTN_Y && ty <= JOIN_BTN_Y + JOIN_BTN_H)
    {
      cgm_joinGameNow();
      showGameScreen();
      return;
    }
    if (tx >= CREATE_BTN_X && tx <= CREATE_BTN_X + CREATE_BTN_W &&
        ty >= CREATE_BTN_Y && ty <= CREATE_BTN_Y + CREATE_BTN_H)
    {
      cgm_createGameNow();
      showGameScreen();
      return;
    }
    if (tx >= WIFI_BTN_X && tx <= WIFI_BTN_X + WIFI_BTN_W &&
        ty >= WIFI_BTN_Y && ty <= WIFI_BTN_Y + WIFI_BTN_H)
    {
      showWifiListScreen();
      return;
    }
  }
  else if (currentScreen == GAME)
  {
    // Back button (top-left header area)
    if (ty < 38 && tx < 80)
    {
      cgm_resetManager();
      showMenuScreen();
      return;
    }

    // If the game-over screen is up, any tap requests a new game
    if (gs_lastGameOver)
    {
      cgm_requestNewGame();
      gs_lastGameOver = false;
      showGameScreen();
      return;
    }

    if (cgm_isConfirming())
    {
      if (tx >= CONFIRM_BTN_X && tx <= CONFIRM_BTN_X + CONFIRM_BTN_W &&
          ty >= CONFIRM_BTN_Y && ty <= CONFIRM_BTN_Y + CONFIRM_BTN_H)
      {
        cgm_confirmPendingMove();
        return;
      }
      if (tx >= CANCEL_BTN_X && tx <= CANCEL_BTN_X + CANCEL_BTN_W &&
          ty >= CANCEL_BTN_Y && ty <= CANCEL_BTN_Y + CANCEL_BTN_H)
      {
        cgm_cancelPendingMove();
        return;
      }
    }
  }
  else if (currentScreen == BOARD_TEST)
  {
    showMenuScreen();
  }
  else if (currentScreen == WIFI_LIST)
  {
    handleWifiListTouch(tx, ty);
  }
  else if (currentScreen == WIFI_PASS_SCREEN)
  {
    handleWifiPassTouch(tx, ty);
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
  Serial0.begin(115200);
  delay(1000);

  setupDisplayHardware();
  initDisplay();
  initLEDs();
  initADCs();

  // Initialise game FSM (idle, no game started)
  cgm_setup();

  drawConnectingScreen(WIFI_SSID);
  // Connect using secrets.h credentials directly
  wifiConnected = connectWifi();
  // If that fails let the user pick a network
  if (!wifiConnected)
  {
    displayStatusBar("No network found - select WiFi", COLOR_RGB565_RED);
    delay(1200);
    demoSequence();
    showWifiListScreen();
    return;
  }

  demoSequence();
  showMenuScreen();
}

// ===================== BOARD SELF-TEST =====================
// Layout constants that mirror display_driver.cpp (landscape 480×320)
static constexpr int BT_BOARD_X = 8;     // left edge of the grid
static constexpr int BT_BOARD_Y = 40;    // top edge of the grid
static constexpr int BT_CELL = 32;       // cell size in pixels
static constexpr int BT_THRESHOLD = 300; // diff magnitude to call a piece present

// Persistent state for the test screen
static int16_t bt_diff[8][8]; // last ADC diff for each square
static int bt_lastChip = -1;  // last square that crossed threshold
static int bt_lastCh = -1;
static char bt_lastLabel[32] = "";

// Draw the static frame (header + grid outline + legend) — call once.
static void bt_drawFrame()
{
  screen.fillScreen(COLOR_RGB565_BLACK);

  // Header bar
  screen.fillRect(0, 0, 480, 36, (uint16_t)0x2945);
  screen.setTextSize(1);
  screen.setTextColor(COLOR_RGB565_WHITE);
  screen.setCursor(6, 14);
  screen.print("ADC Board Test  ");
  screen.setTextColor((uint16_t)0x07FF); // cyan
  screen.print("Tap anywhere to exit");

  // Grid outline
  screen.drawRect(BT_BOARD_X - 1, BT_BOARD_Y - 1,
                  8 * BT_CELL + 2, 8 * BT_CELL + 2, (uint16_t)0x7BEF);

  // Column labels  a-h
  screen.setTextSize(1);
  screen.setTextColor((uint16_t)0x7BEF);
  for (int c = 0; c < 8; c++)
  {
    screen.setCursor(BT_BOARD_X + c * BT_CELL + 12, BT_BOARD_Y + 8 * BT_CELL + 4);
    screen.print((char)('a' + c));
  }

  // Row labels  8-1
  for (int r = 0; r < 8; r++)
  {
    screen.setCursor(BT_BOARD_X - 8, BT_BOARD_Y + r * BT_CELL + 12);
    screen.print((char)('8' - r));
  }

  // Right-panel legend
  int lx = BT_BOARD_X + 8 * BT_CELL + 12;
  screen.setTextSize(1);
  screen.setTextColor(COLOR_RGB565_WHITE);
  screen.setCursor(lx, 44);
  screen.print("Legend:");
  screen.fillRect(lx, 58, 14, 12, COLOR_RGB565_GREEN);
  screen.setTextColor(COLOR_RGB565_WHITE);
  screen.setCursor(lx + 18, 58);
  screen.print("N-pole (+)");
  screen.fillRect(lx, 76, 14, 12, (uint16_t)0xFD20);
  screen.setCursor(lx + 18, 76);
  screen.print("S-pole (-)");
  screen.fillRect(lx, 94, 14, 12, (uint16_t)0x4208);
  screen.setTextColor((uint16_t)0x7BEF);
  screen.setCursor(lx + 18, 94);
  screen.print("Empty");

  // "Last event" label
  screen.setTextColor(COLOR_RGB565_WHITE);
  screen.setCursor(lx, 124);
  screen.print("Last event:");
}

static inline int bt_channelToRow(int ch)
{
  // ch0 -> rank 8
  // ch1 -> rank 7
  // ch2 -> rank 6
  // ch3 -> rank 5
  // ch4 -> rank 4
  // ch5 -> rank 3
  // ch6 -> rank 2
  // ch7 -> rank 1
  return ch;
}

// Redraw all 64 cells based on the current bt_diff table.
static void bt_redrawGrid()
{
  for (int adc = 0; adc < 8; adc++)
  {
    for (int ch = 0; ch < 8; ch++)
    {
      int row = bt_channelToRow(ch);   // channel decides rank
      int col = adc;                   // ADC decides file

      int px = BT_BOARD_X + col * BT_CELL;
      int py = BT_BOARD_Y + row * BT_CELL;

      int16_t d = bt_diff[row][col];
      uint16_t col565;
      char label = '.';

      if (d >= BT_THRESHOLD)
      {
        col565 = COLOR_RGB565_GREEN;
        label = 'P';
      }
      else if (d <= -BT_THRESHOLD)
      {
        col565 = (uint16_t)0xFD20;
        label = 'p';
      }
      else
      {
        col565 = (uint16_t)0x4208;
      }

      screen.fillRect(px + 1, py + 1, BT_CELL - 2, BT_CELL - 2, col565);

      if (label != '.')
      {
        screen.setTextSize(1);
        screen.setTextColor(COLOR_RGB565_BLACK);
        screen.setCursor(px + (BT_CELL - 6) / 2, py + (BT_CELL - 8) / 2);
        screen.print(label);
      }
    }
  }
}

void drawBoardTestLive()
{
  bool anyChange = false;

  for (int adc = 0; adc < 8; adc++)
  {
    for (int ch = 0; ch < 8; ch++)
    {
      uint16_t raw = readRawChannel(adc, ch);
      int16_t d;

      if (raw == 0xFFFF)
        d = 0;
      else
        d = (int16_t)raw - 2048;

      int row = bt_channelToRow(ch);
      int col = adc;

      int16_t prev = bt_diff[row][col];
      bt_diff[row][col] = d;

      bool wasActive = (abs((int)prev) >= BT_THRESHOLD);
      bool isActive = (abs((int)d) >= BT_THRESHOLD);

      if (isActive != wasActive || (isActive && (d > 0) != (prev > 0)))
      {
        anyChange = true;

        if (isActive)
        {
          bt_lastChip = adc;
          bt_lastCh = ch;

          char file = 'a' + adc;
          char rank = '8' - row;
          const char *polarity = (d > 0) ? "N-pole" : "S-pole";

          snprintf(bt_lastLabel, sizeof(bt_lastLabel),
                   "%c%c  ADC%d ch%d  %s", file, rank, adc, ch, polarity);
        }
      }
    }
  }

  if (anyChange)
  {
    bt_redrawGrid();

    int lx = BT_BOARD_X + 8 * BT_CELL + 12;
    screen.fillRect(lx, 138, 480 - lx - 4, 14, COLOR_RGB565_BLACK);
    screen.setTextSize(1);
    screen.setTextColor((uint16_t)0x07FF);
    screen.setCursor(lx, 138);
    screen.print(bt_lastLabel[0] ? bt_lastLabel : "(none)");
  }
}

void runBoardTests()
{
  currentScreen = BOARD_TEST;
  lastBoardTestUpdate = 0;

  // Clear per-test state
  memset(bt_diff, 0, sizeof(bt_diff));
  bt_lastChip = -1;
  bt_lastCh = -1;
  bt_lastLabel[0] = '\0';

  bt_drawFrame();
  bt_redrawGrid();
}

// ===================== LOOP =====================
void loop()
{
  handleTouch();

  unsigned long now = millis();

  if (currentScreen == GAME)
  {
    // Refresh WiFi status each frame so display reflects reconnects
    bool wifiNow = (WiFi.status() == WL_CONNECTED);

    // 1. Read physical board state and feed it to the FSM
    readBoardFEN(gs_boardFENBuf);
    cgm_setPhysicalBoardFEN(String(gs_boardFENBuf));

    // 2. Tick the game FSM
    cgm_tick();

    // ----------------------------------------------------------------
    // 3. Game-over screen (highest priority — drawn once, stays up)
    // ----------------------------------------------------------------
    if (cgm_isGameOver() && !gs_lastGameOver)
    {
      gs_lastGameOver = true;
      drawGameOverScreen(cgm_getGameResultString().c_str());
      return; // nothing else to draw this frame
    }
    if (gs_lastGameOver)
      return; // keep showing the game-over screen

    // ----------------------------------------------------------------
    // 4. Detect piece lift (piece picked up, not yet placed)
    // ----------------------------------------------------------------
    char liftSq[3] = {0};
    bool liftDetected = cgm_getPieceLiftSquare(liftSq);
    if (liftDetected && (liftSq[0] != gs_liftSquare[0] ||
                         liftSq[1] != gs_liftSquare[1]))
    {
      gs_liftSquare[0] = liftSq[0];
      gs_liftSquare[1] = liftSq[1];
      gs_liftSquare[2] = 0;
      gs_liftShown = false; // force redraw of overlay
    }

    // ----------------------------------------------------------------
    // 5. Redraw full board when committed FEN changes
    // ----------------------------------------------------------------
    const String &committedFEN = cgm_getCommittedFEN();
    const String &incomingFEN = cgm_getIncomingFEN();
    const String &pendingFEN = cgm_getPendingFEN();
    bool isConfirming = cgm_isConfirming();
    bool inCheck = cgm_isInCheck();

    bool boardChanged = (committedFEN != gs_lastRenderedFEN);
    bool incomingChanged = (incomingFEN != gs_lastIncomingFEN);
    bool pendingChanged = (pendingFEN != gs_lastPendingFEN);

    if (boardChanged || incomingChanged || pendingChanged)
    {
      gs_lastRenderedFEN = committedFEN;
      gs_lastIncomingFEN = incomingFEN;
      gs_lastPendingFEN = pendingFEN;
      gs_lastConfirmState = false;
      gs_lastCheckState = false;
      gs_liftShown = false;
      gs_lastTurnStatus = ""; // force status bar refresh after redraw

      // Show move highlight if we have a before/after pair
      if (incomingFEN.length() > 0)
      {
        // Remote move being applied — highlight on screen
        drawGameScreenWithMove(wifiNow, committedFEN, incomingFEN);
      }
      else if (pendingFEN.length() > 0 && committedFEN.length() > 0)
      {
        // Local move validated, awaiting confirmation
        drawGameScreenWithMove(wifiNow, committedFEN, pendingFEN);
      }
      else
      {
        bool fenValid = committedFEN.length() > 0;
        drawGameScreen(wifiNow, fenValid,
                       fenValid ? committedFEN : String("Waiting for game..."));
      }
    }

    // ----------------------------------------------------------------
    // 5b. Refresh turn status in the status bar whenever it changes
    // ----------------------------------------------------------------
    if (!isConfirming && !cgm_isGameOver() && !cgm_isBoardSyncing())
    {
      const String &turnStatus = cgm_getTurnStatusString();
      if (turnStatus.length() > 0 && turnStatus != gs_lastTurnStatus)
      {
        gs_lastTurnStatus = turnStatus;
        displayStatusBar(turnStatus.c_str(), COLOR_RGB565_BLUE);
      }
    }

    // ----------------------------------------------------------------
    // 6. Piece-lift overlay (drawn on top of the board without full redraw)
    // ----------------------------------------------------------------
    if (liftDetected && !gs_liftShown)
    {
      gs_liftShown = true;
      drawPiecePickedUp(gs_liftSquare);
    }
    else if (!liftDetected && gs_liftShown)
    {
      // Piece was placed — clear the overlay by redrawing the board
      gs_liftShown = false;
      gs_liftSquare[0] = 0;
      bool fenValid = committedFEN.length() > 0;
      drawGameScreen(wifiNow, fenValid,
                     fenValid ? committedFEN : String("Waiting for game..."));
    }

    // ----------------------------------------------------------------
    // 7. Check alert banner (drawn on top of board)
    // ----------------------------------------------------------------
    if (inCheck && !gs_lastCheckState)
    {
      gs_lastCheckState = true;
      drawCheckAlert(cgm_isWhiteToMove()); // the side to move is in check
    }
    else if (!inCheck && gs_lastCheckState)
    {
      // Check resolved — redraw without banner
      gs_lastCheckState = false;
      bool fenValid = committedFEN.length() > 0;
      drawGameScreen(wifiNow, fenValid,
                     fenValid ? committedFEN : String("Waiting for game..."));
    }

    // ----------------------------------------------------------------
    // 8. Confirm / Cancel overlay
    // ----------------------------------------------------------------
    if (isConfirming && !gs_lastConfirmState)
    {
      gs_lastConfirmState = true;
      drawConfirmOverlay();
    }
    else if (!isConfirming && gs_lastConfirmState)
    {
      gs_lastConfirmState = false;
      bool fenValid = committedFEN.length() > 0;
      drawGameScreen(wifiNow, fenValid,
                     fenValid ? committedFEN : String("Waiting for game..."));
    }
  }

  if (currentScreen == BOARD_TEST)
  {
    if (now - lastBoardTestUpdate >= 100)
    {
      lastBoardTestUpdate = now;
      drawBoardTestLive();
    }
  }
}
