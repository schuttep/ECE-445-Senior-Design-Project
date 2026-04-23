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
// localIsWhite=false flips the board so black's pieces are at the bottom.
void drawGameScreen(bool wifiConnected, bool fenOk, const String &data, bool localIsWhite = true);

// Game screen with move highlight:
//   beforeFEN  — the board before the move (source square shown in yellow)
//   afterFEN   — the board after the move  (dest square shown in green)
// Pass empty strings for either to skip highlighting.
void drawGameScreenWithMove(bool wifiConnected,
                            const String &beforeFEN,
                            const String &afterFEN,
                            bool localIsWhite = true);

// Overlay a yellow "in check!" banner over the current game screen.
void drawCheckAlert(bool whiteInCheck);

// Full-screen game-over panel (checkmate or stalemate).
void drawGameOverScreen(const char *resultLine);

// Small overlay shown as soon as a piece leaves a square
// (piece has been picked up and is in the air).
void drawPiecePickedUp(const char *squareName);

// Overlay mismatch highlights on the already-drawn board:
//   - Extra piece (physical present, logical empty) → red square tint
//   - Missing piece (logical present, physical empty) → piece drawn in dim red
// Pass the board-only logical FEN and the raw physical P/p/. FEN.
// flipped must match the board orientation used when the screen was drawn.
void drawBoardSyncOverlay(const String &logicalFEN, const String &physicalFEN, bool flipped);

// Promotion picker — drawn over the right info panel while the player chooses.
// isWhite: true = local player is white (shows uppercase piece letters).
void drawPromotionPicker(bool isWhite);

// Touch hit-test regions for the promotion picker (landscape 480×320).
// Button order: 0=Queen, 1=Rook, 2=Bishop, 3=Knight.
#define PROMO_BTN_X 276 // RPANEL_X + 4
#define PROMO_BTN_W 192 // RPANEL_W - 8
#define PROMO_BTN_H 52
#define PROMO_BTN_Y0 68 // top of first button (BOARD_Y + 28)
#define PROMO_BTN_GAP 6 // vertical gap between buttons

// Edge-case test scenario menu (full screen).
// Labels is an array of 'count' C-strings. Selected index is highlighted.
void drawEdgeCaseMenuScreen(const char *const labels[], uint8_t count, int8_t selectedIdx,
                            int8_t scrollOffset = 0);

// Status overlay for the edge-case test while the player performs a move.
// Shows the scenario name, a short instruction line, and a PASS/FAIL badge
// once the result is known.  result: 0=pending, 1=pass, -1=fail.
void drawEdgeCaseStatus(const char *scenarioName, const char *instruction, int8_t result);

// Full-screen error display: red banner, bold title, wrapped detail text.
void drawErrorScreen(const char *title, const char *detail);

// Black terminal-style debug screen. Pass an array of count C-strings.
void drawDebugScreen(const char *const lines[], uint8_t count);

// ===========================================================================
// WiFi screens
// ===========================================================================

// Row height / start-y for the network list — used for touch hit-testing.
#define WIFLIST_ROW_Y_START 44
#define WIFLIST_ROW_H 48

// Keyboard layout constants — used by ChessBoard.ino for touch hit-testing.
// These match the landscape (480×320) keyboard drawn by drawKeyboard().
#define KB_ROW1_Y 104
#define KB_ROW2_Y 150
#define KB_ROW3_Y 196
#define KB_ROW4_Y 242
#define KB_KEY_H 42
#define KB_STD_W 40
#define KB_STRIDE 44
#define KB_ROW1_X 20
#define KB_ROW2_X 42
#define KB_ROW3_LX 68
#define KB_SHIFT_W 62
#define KB_DEL_X 378
#define KB_DEL_W 102
#define KB_SYM_W 78
#define KB_SPACE_X 82
#define KB_SPACE_W 290
#define KB_DONE_X 376
#define KB_DONE_W 104

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