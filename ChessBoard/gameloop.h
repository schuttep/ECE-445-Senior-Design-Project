#ifndef GAMELOOP_H
#define GAMELOOP_H

#include <Arduino.h>

// ---------------------------------------------------------------------------
// Timer mode — select before calling cgm_createGameNow().
// ---------------------------------------------------------------------------
enum TimerMode
{
    TIMER_NONE = 0,
    TIMER_RAPID = 600000,  // 10 min per side in ms
    TIMER_BULLET = 300000, //  5 min per side in ms
};

// Convert a TimerMode value to the string expected by the server API.
inline const char *timerModeStr(TimerMode m)
{
    switch (m)
    {
    case TIMER_RAPID:
        return "rapid";
    case TIMER_BULLET:
        return "bullet";
    default:
        return "none";
    }
}

// ---------------------------------------------------------------------------
// Initialisation — call once from setup() after hardware and WiFi are ready.
// ---------------------------------------------------------------------------
void cgm_setup();

// ---------------------------------------------------------------------------
// Main tick — call every loop() iteration while in the GAME screen.
// ---------------------------------------------------------------------------
void cgm_tick();

// ---------------------------------------------------------------------------
// Feed the physical board state (board-only FEN string) to the FSM.
// Call this before cgm_tick() every iteration.
// ---------------------------------------------------------------------------
void cgm_setPhysicalBoardFEN(const String &fen);

// ---------------------------------------------------------------------------
// Game control — map these to UI events (touch buttons, menu items).
// ---------------------------------------------------------------------------
void cgm_startGameNow(); // Begin a fresh game as white (legacy / new-game restart)
// Set the timer mode BEFORE calling cgm_createGameNow().
void cgm_setTimerMode(TimerMode mode);
void cgm_createGameNow(bool aiMode = false); // Begin a fresh game as white (Create Game button)
void cgm_joinGameNow();                      // Join an in-progress game as black (Join Game button)
void cgm_resetManager();                     // Reset FSM back to idle (call before startGameNow)
void cgm_requestNewGame();                   // Request restart from the GAME_END state
void cgm_confirmPendingMove();               // Confirm the move currently awaiting approval
void cgm_cancelPendingMove();                // Cancel/undo the move awaiting approval
void cgm_setPromotionPiece(char piece);      // Set promotion piece (default 'Q')
void cgm_selectPromotionPiece(char piece);   // Called by touch handler to pick a promotion piece

// Load an arbitrary FEN as the committed position and enter local-turn wait.
// Designed for edge-case testing — bypasses normal game init and server polling.
// castlingRights is a 4-element bool array [WK, WQ, BK, BQ]; pass nullptr for all-true.
void cgm_loadEdgeCaseFEN(const String &fen, bool whiteToMove, const bool *castlingRights = nullptr, const char *enPassantSquare = nullptr);

// ---------------------------------------------------------------------------
// State queries — used by ChessBoard.ino to drive the display overlay.
// ---------------------------------------------------------------------------
bool cgm_isConfirming();                 // True while waiting for confirm/cancel
bool cgm_isChoosingPromotion();          // True while the promotion picker is active
bool cgm_isWaitingForRemote();           // True while polling for opponent move
bool cgm_isGameOver();                   // True when in GAME_END state
bool cgm_isBoardSyncing();               // True while waiting for physical board to match screen
bool cgm_isWhiteToMove();                // Whose turn it currently is
bool cgm_isLocalPlayerWhite();           // True if the local player holds the white pieces
bool cgm_isInCheck();                    // Is the current player's king in check?
const String &cgm_getCommittedFEN();     // Last accepted board FEN
const String &cgm_getPendingFEN();       // Candidate FEN awaiting confirmation
const String &cgm_getIncomingFEN();      // Remote move FEN being applied
const String &cgm_getGameResultString(); // Human-readable result string
const String &cgm_getTurnStatusString(); // e.g. "Your turn (White)" for the status bar

// Fills squareName[3] (e.g. "e2\0") and returns true when exactly one piece
// has been lifted off the committed board (local turn wait state only).
bool cgm_getPieceLiftSquare(char squareName[3]);

// ---------------------------------------------------------------------------
// Timer queries — return 0 / false when timerMode == TIMER_NONE.
// ---------------------------------------------------------------------------
TimerMode cgm_getTimerMode();
int32_t cgm_getWhiteTimeMs(); // current remaining time for white (interpolated)
int32_t cgm_getBlackTimeMs(); // current remaining time for black (interpolated)
bool cgm_isTimerRunning();    // true when a clock is actively ticking
bool cgm_isTimerForWhite();   // true when white's clock is running

#endif
