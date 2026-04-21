#ifndef DISPLAY_DRIVER_H
#define DISPLAY_DRIVER_H

#include <Arduino.h>
#include "wifi_manager.h"

// ===========================================================================
// Primitives — building blocks for composing custom screens from the main sketch
// ===========================================================================

// Fill the screen white.
void displayClear();

// Draw the standard top header bar (title + WiFi status + divider line).
void displayHeader(bool wifiConnected);

// Print text horizontally centered at the given y pixel position.
void displayCenteredText(const char *text, int y, uint8_t size, uint16_t color);

// Draw a filled, labeled button rectangle.
void displayButton(int x, int y, int w, int h, uint16_t color, const char *label);

// Draw a 22-pixel status bar at the very bottom of the screen.
void displayStatusBar(const char *msg, uint16_t bgColor);

// Draw a full-width horizontal divider line at y.
void displayDivider(int y, uint16_t color);

// ===========================================================================
// Full screens — pre-built screen layouts
// ===========================================================================

// Initialize the display — must be called once in setup().
void initDisplay();

// WiFi connecting splash. Pass the SSID name so it is shown on screen.
void drawConnectingScreen(const char *ssid);

// Main menu background (header + title). Add buttons with displayButton().
void drawMenuScreen(bool wifiConnected);

// Game screen showing the current board state.
// fenOk=true  -> data is a valid FEN and will be rendered as a board grid.
// fenOk=false -> data is an error message shown as plain text.
void drawGameScreen(bool wifiConnected, bool fenOk, const String &data);

// Game screen with move highlight:
//   beforeFEN  — the board before the move (source square shown in yellow)
//   afterFEN   — the board after the move  (dest square shown in green)
// Pass empty strings for either to skip highlighting.
void drawGameScreenWithMove(bool wifiConnected,
                            const String &beforeFEN,
                            const String &afterFEN);

// Overlay a yellow "in check!" banner over the current game screen.
void drawCheckAlert(bool whiteInCheck);

// Full-screen game-over panel (checkmate or stalemate).
void drawGameOverScreen(const char *resultLine);

// Small overlay shown as soon as a piece leaves a square
// (piece has been picked up and is in the air).
void drawPiecePickedUp(const char *squareName);

// Full-screen error display: red banner, bold title, wrapped detail text.
void drawErrorScreen(const char *title, const char *detail);

// Black terminal-style debug screen. Pass an array of count C-strings.
void drawDebugScreen(const char *const lines[], uint8_t count);

// ===========================================================================
// WiFi screens
// ===========================================================================

// Row height / start-y for the network list — used for touch hit-testing.
#define WIFLIST_ROW_Y_START 75
#define WIFLIST_ROW_H 50

// Keyboard layout constants — used by ChessBoard.ino for touch hit-testing.
#define KB_ROW1_Y 116
#define KB_ROW2_Y 162
#define KB_ROW3_Y 208
#define KB_ROW4_Y 254
#define KB_KEY_H 44
#define KB_STD_W 28
#define KB_STRIDE 30
#define KB_ROW1_X 10
#define KB_ROW2_X 25
#define KB_ROW3_LX 46
#define KB_SHIFT_W 44
#define KB_DEL_X 256
#define KB_DEL_W 64
#define KB_SYM_W 68
#define KB_SPACE_X 70
#define KB_SPACE_W 178
#define KB_DONE_X 250
#define KB_DONE_W 70

// Network list screen.
// Pass (nullptr, 0, true) to show a "Scanning..." placeholder while scanning.
void drawWifiListScreen(const ScannedNetwork *nets, uint8_t count, bool scanning);

// Full password-entry screen with on-screen QWERTY keyboard.
void drawPasswordScreen(const char *ssid, const char *password,
                        bool showChars, bool shifted, bool symbols);

// Redraw only the password field (efficient partial update after each keypress).
void drawPasswordField(const char *password, bool showChars);

// Redraw only the keyboard area (e.g. after shift or symbol-page toggle).
void drawKeyboard(bool shifted, bool symbols);

// Hit-test the keyboard at touch point (tx, ty).
// Returns: printable char | '\b' DEL | '\n' Done | '\t' shift-toggle |
//          0x01 sym-toggle | 0 no-hit.
char keyboardHitTest(int tx, int ty, bool symbols);

#endif