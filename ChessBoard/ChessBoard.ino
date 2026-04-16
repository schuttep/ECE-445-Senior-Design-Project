#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>

#include "headers.h"
#include "display_driver.h"
#include "wifi_driver.h"
#include "api_connect.h"
#include "LED_driver.h"
#include "ADC_driver.h"
#include "wifi_manager.h"

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

// ===================== STATE =====================
enum ScreenState
{
  MENU,
  GAME,
  WIFI_LIST,
  WIFI_PASS_SCREEN
};
ScreenState currentScreen = MENU;
String currentFEN = "";
bool wifiConnected = false;
unsigned long menuShownAt = 0;
unsigned long lastFENPoll = 0;
const unsigned long FEN_POLL_INTERVAL = 5000;

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
void showWifiListScreen();
void showWifiPassScreen(const char *ssid);
void handleWifiListTouch(int tx, int ty);
void handleWifiPassTouch(int tx, int ty);
void runBoardTests();
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
  Serial.begin(115200);
  delay(1000);

  setupDisplayHardware();
  initDisplay();
  initLEDs();
  initADCs();
  calibrateBaselines();

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
void runBoardTests()
{
  displayStatusBar("Running tests...", COLOR_RGB565_BLUE);

  static char buf[21][44];
  static const char *lines[21];
  uint8_t n = 0;

  // 1. Display — visible if we got here
  snprintf(buf[n], 44, "[PASS] Display:  visible");
  lines[n] = buf[n];
  n++;

  // 2. Touch — just fired to reach this function
  snprintf(buf[n], 44, "[PASS] Touch:    responded");
  lines[n] = buf[n];
  n++;

  // 3. WiFi
  snprintf(buf[n], 44, "[%s] WiFi:     %s",
           wifiConnected ? "PASS" : "FAIL",
           wifiConnected ? "connected" : "not connected");
  lines[n] = buf[n];
  n++;

  // 4. API
  if (wifiConnected)
  {
    ApiResult r = fetchLatestFEN();
    if (r.ok)
      snprintf(buf[n], 44, "[PASS] API:      FEN received");
    else
    {
      char err[18];
      strncpy(err, r.data.c_str(), 17);
      err[17] = '\0';
      snprintf(buf[n], 44, "[FAIL] API:      %s", err);
    }
  }
  else
    snprintf(buf[n], 44, "[SKIP] API:      no WiFi");
  lines[n] = buf[n];
  n++;

  // 5. LEDs — flashes all green briefly
  int ledCount = testLEDs();
  snprintf(buf[n], 44, "[PASS] LEDs:     %d pixels lit", ledCount);
  lines[n] = buf[n];
  n++;

  // 6. ADCs — probe presence + read all 64 channels
  ADCTestResult adc = testADCs();
  snprintf(buf[n], 44, "ADC chips (%d/8 found, %d/64 ch OK):",
           __builtin_popcount(adc.chipMask), adc.totalValid);
  lines[n] = buf[n];
  n++;

  // One line per chip: presence + channel validity
  for (int i = 0; i < 8; i++)
  {
    bool chipOk = (adc.chipMask >> i) & 1;
    bool chanOk = (adc.chanMask >> i) & 1;
    if (!chipOk)
      snprintf(buf[n], 44, "  ADC%d: [FAIL] no I2C response", i);
    else if (!chanOk)
      snprintf(buf[n], 44, "  ADC%d: [WARN] chip OK, ch errors", i);
    else
      snprintf(buf[n], 44, "  ADC%d: [PASS] 8/8 channels valid", i);
    lines[n] = buf[n];
    n++;
  }

  drawDebugScreen(lines, n);
  waitForTap();

  // Light the LEDs based on live ADC readings before returning to menu
  char fenBuf[72];
  readBoardFEN(fenBuf);

  showMenuScreen();
}
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