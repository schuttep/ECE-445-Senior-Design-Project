#ifndef DISPLAY_DRIVER_H
#define DISPLAY_DRIVER_H

#include <Arduino.h>
#include "headers.h"
#include "wifi_manager.h"

// ===========================================================================
// Primitives — building blocks for composing custom screens from the main sketch
// ===========================================================================

// Fill the screen white.
void displayClear();

// Draw the standard top header bar (title + WiFi status + divider line).
// showBack=true reserves the top-left hit zone for a visible back button.
void displayHeader(bool wifiConnected, bool showBack = false);

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

// Timer mode selection screen shown after "Create Game" is tapped.
// Three buttons let the player choose: Unlimited, Rapid (10 min), Bullet (5 min).
// title — heading text; defaults to "Select Timer Mode".
void drawTimerModeScreen(bool wifiConnected, const char *title = "Select Timer Mode");

// WiFi settings button — top-right corner of the header (all screens).
#define WIFI_SETTINGS_BTN_X 392
#define WIFI_SETTINGS_BTN_Y 5
#define WIFI_SETTINGS_BTN_W 80
#define WIFI_SETTINGS_BTN_H 26

// Hit-test regions for the timer mode selection buttons (landscape 480×320).
#define TIMER_BTN_X 50
#define TIMER_BTN_W 380
#define TIMER_BTN_H 55
#define TIMER_BTN_UNLIM_Y 80
#define TIMER_BTN_RAPID_Y 150
#define TIMER_BTN_BULLET_Y 220

// Game screen showing the current board state.
// fenOk=true  -> data is a valid FEN and will be rendered as a board grid.
// fenOk=false -> data is an error message shown as plain text.
// localIsWhite=false flips the board so black's pieces are at the bottom.
void drawGameScreen(bool wifiConnected, bool fenOk, const String &data, bool localIsWhite = true, bool aiGame = false);

// Game screen with move highlight:
//   beforeFEN  — the board before the move (source square shown in yellow)
//   afterFEN   — the board after the move  (dest square shown in green)
// Pass empty strings for either to skip highlighting.
void drawGameScreenWithMove(bool wifiConnected,
                            const String &beforeFEN,
                            const String &afterFEN,
                            bool localIsWhite = true,
                            bool aiGame = false);

// Overlay a yellow "in check!" banner over the current game screen.
void drawCheckAlert(bool whiteInCheck);

// Remove the check-alert banner and restore the opponent player label.
// No board pixels are redrawn — only the 22px right-panel strip is touched.
void clearCheckAlert();

// Full-screen game-over panel (checkmate or stalemate).
void drawGameOverScreen(const char *resultLine);

// Small overlay shown as soon as a piece leaves a square
// (piece has been picked up and is in the air).
void drawPiecePickedUp(const char *squareName);

// Remove the piece-lift banner (restores the right-panel strip to white).
// No board pixels are redrawn.
void clearPieceLiftOverlay();

// Remove the confirm/cancel buttons overlay from the right panel.
// No board pixels are redrawn.
void clearConfirmOverlay();

// Overlay mismatch highlights on the already-drawn board:
//   - Extra piece (physical present, logical empty) → red square tint
//   - Missing piece (logical present, physical empty) → piece drawn in dim red
// Pass the board-only logical FEN and the raw physical P/p/. FEN.
// flipped must match the board orientation used when the screen was drawn.
void drawBoardSyncOverlay(const String &logicalFEN, const String &physicalFEN, bool flipped);

// Promotion picker — drawn over the right info panel while the player chooses.
// isWhite: true = local player is white (shows uppercase piece letters).
void drawPromotionPicker(bool isWhite);

// Draw the game clock in both player-label strips of the right info panel.
// Call this after drawGameScreen — it only paints the rightmost 60 px of each
// label strip so it does not disturb other overlays.
// whiteMs / blackMs  — remaining milliseconds for each side (at time of call).
// clockRunning       — true when a clock is actively ticking.
// clockForWhite      — which side's clock is running (only used if clockRunning).
// localIsWhite       — used to decide which strip is "mine" vs "opponent".
void drawTimerDisplay(int32_t whiteMs, int32_t blackMs,
                      bool clockRunning, bool clockForWhite,
                      bool localIsWhite);

// Chat message entry used by the game-screen message panel.
// isMine = true when this board sent the message (right-tinted bubble).
#define CHAT_MAX_DISPLAY 3
struct ChatDisplayMsg
{
    char text[201]; // matches API_MSG_TEXT_LEN (200) + null terminator
    bool isMine;
};

// Live chat panel in the game screen's right panel.
// Shows the last CHAT_MAX_DISPLAY messages and the current draft.
// Tap anywhere in the card bounds to open the composer.
void drawGameMessagePanel(const ChatDisplayMsg *msgs, int count, const char *draft);

// Full-screen message composer overlay for the game screen.
void drawGameMessageComposer(const char *message, bool shifted, bool symbols);

// Redraws only the draft text field inside the composer (cheap partial update).
void drawGameMessageComposerField(const char *message);

// Touch hit-test regions for the promotion picker (landscape 480×320).
// Button order: 0=Queen, 1=Rook, 2=Bishop, 3=Knight.
#define PROMO_BTN_X 276 // RPANEL_X + 4
#define PROMO_BTN_W 192 // RPANEL_W - 8
#define PROMO_BTN_H 52
#define PROMO_BTN_Y0 68 // top of first button (BOARD_Y + 28)
#define PROMO_BTN_GAP 6 // vertical gap between buttons

#define GAME_MSG_CARD_X 276
#define GAME_MSG_CARD_Y 94
#define GAME_MSG_CARD_W 192
#define GAME_MSG_CARD_H 100

// Hint button — sits in the 32 px gap between the opponent label (y=40..62)
// and the chat card (y=94) in the right info panel.
#define HINT_BTN_X 272
#define HINT_BTN_Y 63
#define HINT_BTN_W 196
#define HINT_BTN_H 31 // was 27 — increased for easier touch target

// Draw (or refresh) the hint button with the remaining hint count.
// hintsLeft > 0 → blue button; 0 → dark-grey (exhausted).
// Call this after drawGameScreen() and after any operation that repaints chrome.
void drawHintButton(int hintsLeft);

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

// Force the board-cell cache to be treated as invalid so the next
// drawGameScreen / drawFENBoard call repaints all 64 squares.
// Call this before any drawGameScreen that must clear a board overlay.
void invalidateBoardCache();

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