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
#define JOIN_BTN_X 30
#define JOIN_BTN_Y 160
#define JOIN_BTN_W 260
#define JOIN_BTN_H 70

#define CREATE_BTN_X 30
#define CREATE_BTN_Y 245
#define CREATE_BTN_W 260
#define CREATE_BTN_H 70

#define WIFI_BTN_X 30
#define WIFI_BTN_Y 330
#define WIFI_BTN_W 260
#define WIFI_BTN_H 70

// Confirm / Cancel overlay shown when a local move awaits approval
#define CONFIRM_BTN_X 20
#define CONFIRM_BTN_Y 405
#define CONFIRM_BTN_W 130
#define CONFIRM_BTN_H 55

#define CANCEL_BTN_X 170
#define CANCEL_BTN_Y 405
#define CANCEL_BTN_W 130
#define CANCEL_BTN_H 55

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

// ===================== SCREEN HELPERS =====================
void showMenuScreen()
{
  currentScreen = MENU;
  drawMenuScreen(wifiConnected);

  displayButton(JOIN_BTN_X, JOIN_BTN_Y, JOIN_BTN_W, JOIN_BTN_H, COLOR_RGB565_BLUE, "Join Game");
  displayButton(CREATE_BTN_X, CREATE_BTN_Y, CREATE_BTN_W, CREATE_BTN_H, 0xFD20, "Run Tests");
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

  // Show a placeholder board while the FSM initialises
  drawGameScreen(wifiConnected, false, String("Starting game..."));

  // Kick off the FSM — it will drive everything from here
  cgm_startGameNow();
}

// Draw confirm / cancel buttons as an overlay on the game screen.
void drawConfirmOverlay()
{
  displayButton(CONFIRM_BTN_X, CONFIRM_BTN_Y, CONFIRM_BTN_W, CONFIRM_BTN_H,
                COLOR_RGB565_GREEN, "Confirm");
  displayButton(CANCEL_BTN_X, CANCEL_BTN_Y, CANCEL_BTN_W, CANCEL_BTN_H,
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

void handleWifiListTouch(int tx, int ty)
{
  if (ty < 38 && tx < 80)
  {
    showMenuScreen();
    return;
  } // Back
  if (ty < 38 && tx > 238)
  {
    showWifiListScreen();
    return;
  } // Rescan

  if (ty >= WIFLIST_ROW_Y_START && scannedCount > 0)
  {
    int idx = (ty - WIFLIST_ROW_Y_START) / WIFLIST_ROW_H;
    if (idx < 0 || idx >= (int)scannedCount)
      return;
    const ScannedNetwork &net = scannedNets[idx];

    if (net.saved)
    {
      char savedPass[WM_PASS_LEN];
      wmGetSavedPass(net.ssid, savedPass);
      displayStatusBar("Connecting...", COLOR_RGB565_BLUE);
      if (wmConnect(net.ssid, savedPass))
      {
        wifiConnected = true;
        displayStatusBar("Connected!", COLOR_RGB565_GREEN);
        delay(1200);
        showMenuScreen();
      }
      else
      {
        drawErrorScreen("Connection Failed",
                        "Could not connect. The saved password may be wrong.");
        waitForTap();
        showWifiListScreen();
      }
    }
    else
    {
      showWifiPassScreen(net.ssid);
    }
  }
}

void handleWifiPassTouch(int tx, int ty)
{
  if (ty < 38 && tx < 80)
  {
    showWifiListScreen();
    return;
  } // Back

  // Show / Hide password toggle
  if (tx >= 272 && tx <= 316 && ty >= 72 && ty <= 110)
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
      wmSaveNetwork(selectedSSID, passwordBuf);
      wifiConnected = true;
      displayStatusBar("Connected! Network saved.", COLOR_RGB565_GREEN);
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

  uint16_t tx = touch._point.x;
  uint16_t ty = touch._point.y;

  if (currentScreen == MENU)
  {
    if (millis() - menuShownAt < 500)
      return;

    if (tx >= JOIN_BTN_X && tx <= JOIN_BTN_X + JOIN_BTN_W &&
        ty >= JOIN_BTN_Y && ty <= JOIN_BTN_Y + JOIN_BTN_H)
    {
      showGameScreen();
      return;
    }
    if (tx >= CREATE_BTN_X && tx <= CREATE_BTN_X + CREATE_BTN_W &&
        ty >= CREATE_BTN_Y && ty <= CREATE_BTN_Y + CREATE_BTN_H)
    {
      runBoardTests();
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
  calibrateBaselines();

  // Initialise game FSM (idle, no game started)
  cgm_setup();

  drawConnectingScreen(WIFI_SSID);
  // 1. Try all NVS-saved networks first
  wifiConnected = wmConnectBoot();
  // 2. Fall back to secrets.h credentials
  if (!wifiConnected)
  {
    wifiConnected = connectWifi();
    if (wifiConnected)
      wmSaveNetwork(WIFI_SSID, WIFI_PASS); // persist for future boots
  }
  // 3. Nothing worked — let the user pick a network from the scan list
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
void drawBoardTestLive()
{
  clearLEDs();
  showLEDs();

  // Erase and redraw one line per ADC chip showing chip index and ch1 raw value
  static char lineBuf[8][28];
  for (int chip = 0; chip < 8; chip++)
  {
    uint16_t raw = readRawChannel(chip, 1);
    bool active = false;

    if (raw == 0xFFFF)
    {
      snprintf(lineBuf[chip], 28, " ADC%d ch1: NO RESPONSE", chip);
    }
    else
    {
      int diff = (int)raw - (int)getBaseline(chip, 1);
      active = abs(diff) >= 300;
      snprintf(lineBuf[chip], 28, " ADC%d ch1: %4u %s", chip, (unsigned)raw,
               active ? "*" : " ");
    }

    // Light the entire row for this chip if active
    if (active)
    {
      for (int col = 0; col < 8; col++)
        setLEDForSquare(chip, col, 0, 180, 255);
    }

    int lineY = 22 + (2 + chip) * 14;
    screen.fillRect(0, lineY, 320, 14, COLOR_RGB565_BLACK);
    screen.setTextSize(1);
    screen.setTextColor(active ? COLOR_RGB565_GREEN : (uint16_t)0x7BEF);
    screen.setCursor(4, lineY);
    screen.print(lineBuf[chip]);
  }
}

void runBoardTests()
{
  testLEDs();
  currentScreen = BOARD_TEST;
  lastBoardTestUpdate = 0;

  // Draw the static frame once
  static const char *initLines[2] = {
      "Tap to exit  Live Readings",
      "   Ch1 raw values (0-4095)"};
  drawDebugScreen(initLines, 2);

  drawBoardTestLive();
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
    if (now - lastBoardTestUpdate >= 300)
    {
      lastBoardTestUpdate = now;
      drawBoardTestLive();
    }
  }
}