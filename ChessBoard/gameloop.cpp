#include <Arduino.h>
#include <WiFi.h>

#include "headers.h"
#include "secrets.h"
#include "gamelogic.h"
#include "api_connect.h"
#include "display_driver.h"
#include "wifi_manager.h"

// ============================================================
// ChessGameFSM.ino
// This file does three main jobs:
// 1. Manages gameplay with a real finite state machine
// 2. Tracks turns and detects when the game ends
// 3. Sends / receives board FEN state through your API
// ============================================================

// ============================================================
// Game configuration
// ============================================================
namespace CGMConfig
{
    const uint32_t POLL_INTERVAL_MS = 2000;
    const uint32_t LOCAL_STABLE_TIME_MS = 600;
    const uint32_t WIFI_RETRY_INTERVAL_MS = 5000;
    const uint32_t STATUS_PRINT_INTERVAL_MS = 1500;

    const uint16_t GAME_ID = 1;
    const uint8_t BOARD_NUMBER = 1;

    // false = local board plays black and waits for white first
    // true  = local board plays white and moves first
    const bool LOCAL_IS_WHITE = true;

    // Default promotion piece when no UI prompt is available
    const char DEFAULT_PROMOTION = 'Q';
}

// ============================================================
// Hooks that other files can call into
// ============================================================
// Your sensor code should call cgm_setPhysicalBoardFEN(...) every
// time it reconstructs the full physical board state.
//
// Touchscreen code can call:
//   cgm_confirmPendingMove();
//   cgm_cancelPendingMove();
//   cgm_requestNewGame();
// ============================================================

static String cgm_physicalBoardFEN = "";
static volatile bool cgm_moveConfirmed = false;
static volatile bool cgm_moveCancelled = false;
static volatile bool cgm_newGameRequested = false;
static volatile char cgm_requestedPromotion = CGMConfig::DEFAULT_PROMOTION;
static volatile bool cgm_promotionSelected = false;

void cgm_setPhysicalBoardFEN(const String &fen)
{
    cgm_physicalBoardFEN = fen;
}

void cgm_confirmPendingMove()
{
    cgm_moveConfirmed = true;
}

void cgm_cancelPendingMove()
{
    cgm_moveCancelled = true;
}

void cgm_requestNewGame()
{
    cgm_newGameRequested = true;
}

void cgm_setPromotionPiece(char piece)
{
    cgm_requestedPromotion = piece;
}

// Called by the touch handler when the player taps a promotion-picker button.
void cgm_selectPromotionPiece(char piece)
{
    cgm_requestedPromotion = piece;
    cgm_promotionSelected = true;
}

// ============================================================
// Small helpers
// ============================================================

String cgm_boardOnlyFen(const String &fen)
{
    int space = fen.indexOf(' ');
    if (space == -1)
    {
        return fen;
    }
    return fen.substring(0, space);
}

String cgm_startFen()
{
    return "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR";
}

bool cgm_sameBoardFen(const String &a, const String &b)
{
    return cgm_boardOnlyFen(a) == cgm_boardOnlyFen(b);
}

bool cgm_wifiConnected()
{
    return WiFi.status() == WL_CONNECTED;
}

void cgm_copyCastle(const bool src[4], bool dst[4])
{
    for (int i = 0; i < 4; i++)
    {
        dst[i] = src[i];
    }
}

void cgm_resetCastle(bool rights[4])
{
    rights[0] = true;
    rights[1] = true;
    rights[2] = true;
    rights[3] = true;
}

bool cgm_sameBoard(char a[8][8], char b[8][8])
{
    for (int r = 0; r < 8; r++)
    {
        for (int c = 0; c < 8; c++)
        {
            if (a[r][c] != b[r][c])
            {
                return false;
            }
        }
    }
    return true;
}

// Recomputes castling rights by looking only at board state.
// This is intentionally conservative.
// Once a king or rook is missing from its original square, the right is gone.
void cgm_rebuildCastlingRightsFromBoard(const String &fen, bool rights[4])
{
    char board[8][8];
    if (!parseFENBoard(fen, board))
    {
        rights[0] = false;
        rights[1] = false;
        rights[2] = false;
        rights[3] = false;
        return;
    }

    rights[0] = (board[7][4] == 'K' && board[7][7] == 'R');
    rights[1] = (board[7][4] == 'K' && board[7][0] == 'R');
    rights[2] = (board[0][4] == 'k' && board[0][7] == 'r');
    rights[3] = (board[0][4] == 'k' && board[0][0] == 'r');
}

String cgm_squareName(int row, int col)
{
    char file = 'a' + col;
    char rank = '8' - row;
    String out = "";
    out += file;
    out += rank;
    return out;
}

// For LEDs or debug messages. Works for ordinary moves and castles.
bool cgm_findMoveSquares(const String &beforeFEN,
                         const String &afterFEN,
                         int &fromRow, int &fromCol,
                         int &toRow, int &toCol)
{
    char before[8][8];
    char after[8][8];

    if (!parseFENBoard(beforeFEN, before) || !parseFENBoard(afterFEN, after))
    {
        return false;
    }

    int sourceCount = 0;
    int destCount = 0;
    fromRow = -1;
    fromCol = -1;
    toRow = -1;
    toCol = -1;

    for (int r = 0; r < 8; r++)
    {
        for (int c = 0; c < 8; c++)
        {
            if (before[r][c] == after[r][c])
            {
                continue;
            }

            if (before[r][c] != '.' && after[r][c] == '.')
            {
                sourceCount++;
                if (fromRow == -1)
                {
                    fromRow = r;
                    fromCol = c;
                }
            }

            if (after[r][c] != '.' && before[r][c] != after[r][c])
            {
                destCount++;
                toRow = r;
                toCol = c;
            }
        }
    }

    if (sourceCount >= 1 && destCount >= 1)
    {
        return true;
    }

    return false;
}

// ============================================================
// Physical ↔ logical FEN translation
// The ADC driver outputs P (N-pole) / p (S-pole) / . (empty).
// The game logic works with real piece characters (K,Q,R,B,N,P / kqrbnp).
// These helpers bridge that gap.
// ============================================================

// Converts a logical piece character to a physical sensor character.
static char cgm_toPhysChar(char logical)
{
    if (logical == '.')
        return '.';
    return (logical >= 'A' && logical <= 'Z') ? 'P' : 'p';
}

// Serialises an 8×8 board array back to a board-only FEN string.
static String cgm_boardToFEN(char board[8][8])
{
    String fen = "";
    for (int r = 0; r < 8; r++)
    {
        int empty = 0;
        for (int c = 0; c < 8; c++)
        {
            if (board[r][c] == '.')
            {
                empty++;
            }
            else
            {
                if (empty > 0)
                {
                    fen += (char)('0' + empty);
                    empty = 0;
                }
                fen += board[r][c];
            }
        }
        if (empty > 0)
            fen += (char)('0' + empty);
        if (r < 7)
            fen += '/';
    }
    return fen;
}

// Collapses a logical chess FEN to a physical P/p/. FEN for sensor comparison.
static String cgm_toPhysicalFEN(const String &logicalFEN)
{
    char board[8][8];
    if (!parseFENBoard(logicalFEN, board))
        return "";
    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 8; c++)
            board[r][c] = cgm_toPhysChar(board[r][c]);
    return cgm_boardToFEN(board);
}

// Given the current committed logical FEN and the raw physical FEN (P/p/. only),
// reconstructs the candidate logical FEN by mapping physical piece changes back
// to the logical identities known from the committed board.
//
// Handles normal moves, captures, castling, and en passant.
// Returns false if the physical state is too ambiguous to interpret
// (e.g. more than 4 squares changed, or no clear source for a moved piece).
static bool cgm_physicalToLogicalFEN(const String &committedFEN,
                                     const String &physicalFEN,
                                     String &candidateOut)
{
    char committed[8][8];
    char physical[8][8];
    if (!parseFENBoard(committedFEN, committed))
        return false;
    if (!parseFENBoard(physicalFEN, physical))
        return false;

    // Build expected physical from committed
    char expectedPhys[8][8];
    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 8; c++)
            expectedPhys[r][c] = cgm_toPhysChar(committed[r][c]);

    // Count changed squares
    int changeCount = 0;
    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 8; c++)
            if (expectedPhys[r][c] != physical[r][c])
                changeCount++;

    if (changeCount == 0 || changeCount > 4)
        return false;

    // ----------------------------------------------------------------
    // Piece-in-air filter: if every changed square is a departure
    // (no square gained a piece or changed polarity), the player is
    // still holding the piece.  Don't try to interpret a half-move.
    // ----------------------------------------------------------------
    {
        bool anyArrived = false;
        for (int r = 0; r < 8; r++)
            for (int c = 0; c < 8; c++)
                if (physical[r][c] != '.' && physical[r][c] != expectedPhys[r][c])
                    anyArrived = true;
        if (!anyArrived)
            return false;
    }

    // ----------------------------------------------------------------
    // Castling special case (exactly 4 squares change)
    // The generic pass below can't distinguish King from Rook because
    // the ADC only sees polarity, not piece identity.  Check all four
    // castle outcomes against the physical board directly.
    // ----------------------------------------------------------------
    if (changeCount == 4)
    {
        struct CastleSpec
        {
            char king, rook;
            int kfr, kfc, ktr, ktc; // king from/to
            int rfr, rfc, rtr, rtc; // rook from/to
        };
        static const CastleSpec specs[4] = {
            {'K', 'R', 7, 4, 7, 6, 7, 7, 7, 5}, // white king-side
            {'K', 'R', 7, 4, 7, 2, 7, 0, 7, 3}, // white queen-side
            {'k', 'r', 0, 4, 0, 6, 0, 7, 0, 5}, // black king-side
            {'k', 'r', 0, 4, 0, 2, 0, 0, 0, 3}, // black queen-side
        };

        for (int i = 0; i < 4; i++)
        {
            const CastleSpec &sp = specs[i];
            if (committed[sp.kfr][sp.kfc] != sp.king)
                continue;
            if (committed[sp.rfr][sp.rfc] != sp.rook)
                continue;

            // Build the logical board after this castle
            char expected[8][8];
            copyBoard(committed, expected);
            expected[sp.kfr][sp.kfc] = '.';
            expected[sp.rfr][sp.rfc] = '.';
            expected[sp.ktr][sp.ktc] = sp.king;
            expected[sp.rtr][sp.rtc] = sp.rook;

            // Check that the resulting physical matches the sensor reading
            bool match = true;
            for (int r = 0; r < 8 && match; r++)
                for (int c = 0; c < 8 && match; c++)
                    if (cgm_toPhysChar(expected[r][c]) != physical[r][c])
                        match = false;

            if (match)
            {
                candidateOut = cgm_boardToFEN(expected);
                return true;
            }
        }
        // 4 squares changed but no castle pattern matched — ambiguous
        return false;
    }

    // Start candidate as a copy of committed
    char candidate[8][8];
    copyBoard(committed, candidate);

    // Pass 1: clear squares where a piece left (expectedPhys had piece, now empty)
    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 8; c++)
            if (expectedPhys[r][c] != '.' && physical[r][c] == '.')
                candidate[r][c] = '.';

    // Pass 2: fill squares where a piece arrived or the polarity changed (capture)
    // Track which committed source squares have already been used so castling
    // (where two pieces of the same colour move) is resolved correctly.
    bool usedAsSource[8][8];
    memset(usedAsSource, 0, sizeof(usedAsSource));

    for (int r = 0; r < 8; r++)
    {
        for (int c = 0; c < 8; c++)
        {
            if (physical[r][c] == '.')
                continue; // empty — handled or unchanged
            if (physical[r][c] == expectedPhys[r][c])
                continue; // unchanged

            // A piece arrived here or a capture changed the polarity
            bool isWhiteArriving = (physical[r][c] == 'P');

            // Find the committed source square whose piece matches the arriving colour
            // and which was cleared in pass 1 (candidate[sr][sc] == '.' but committed != '.')
            char sourcePiece = '.';
            int sourceR = -1, sourceC = -1;

            for (int sr = 0; sr < 8 && sourcePiece == '.'; sr++)
            {
                for (int sc = 0; sc < 8 && sourcePiece == '.'; sc++)
                {
                    if (sr == r && sc == c)
                        continue;
                    if (usedAsSource[sr][sc])
                        continue;
                    if (committed[sr][sc] == '.')
                        continue; // was empty
                    if (candidate[sr][sc] != '.')
                        continue; // didn't leave
                    if (isWhitePiece(committed[sr][sc]) != isWhiteArriving)
                        continue;
                    sourcePiece = committed[sr][sc];
                    sourceR = sr;
                    sourceC = sc;
                }
            }

            if (sourcePiece == '.')
                return false; // can't resolve — ambiguous

            usedAsSource[sourceR][sourceC] = true;
            candidate[r][c] = sourcePiece;
        }
    }

    candidateOut = cgm_boardToFEN(candidate);
    return true;
}

// ============================================================
// Game result type
// ============================================================

enum CGMGameResult
{
    CGM_RESULT_NONE,
    CGM_RESULT_WHITE_WIN,
    CGM_RESULT_BLACK_WIN,
    CGM_RESULT_STALEMATE,
    CGM_RESULT_DRAW_50_MOVE,
    CGM_RESULT_DRAW_MATERIAL
};

// ============================================================
// Finite State Machine types and global state
// ============================================================

enum CGMState
{
    CGM_WAIT_FOR_GAME_START,
    CGM_JOIN_POLLING, // fetching server state before initialising a joined game
    CGM_GAME_INITIALIZATION,
    CGM_BOARD_SYNC, // waiting for physical board to match the committed FEN
    CGM_LOCAL_TURN_WAIT_FOR_BOARD,
    CGM_LOCAL_TURN_PROMOTION, // waiting for player to pick a promotion piece
    CGM_LOCAL_TURN_VALIDATE,
    CGM_LOCAL_TURN_CONFIRM,
    CGM_SEND_STATE,
    CGM_WAIT_FOR_REMOTE_MOVE,
    CGM_APPLY_REMOTE_MOVE,
    CGM_GAME_END,
    CGM_ERROR_STATE
};

struct ChessGameManager
{
    CGMState state;

    bool localIsWhite;
    bool whiteToMove;
    bool gameActive;

    bool castling[4];

    String committedFEN;
    String pendingFEN;
    String remoteIncomingFEN;

    String stableCandidateFEN;
    uint32_t stableSinceMs;

    uint32_t lastPollMs;
    uint32_t lastWifiRetryMs;
    uint32_t lastStatusPrintMs;

    // En passant target square in algebraic notation, e.g. "e3", or "" for none
    char enPassantSquare[3];
    // Half-move clock for 50-move draw rule
    uint16_t halfMoveClock;
    // Ready-banner state (replaces static local in cgm_handleWaitForGameStart)
    bool readyBannerShown;
    // Server send retry
    uint8_t sendRetryCount;
    uint32_t sendStartMs;

    // Last candidate FEN that failed validation — skip re-validating the same
    // illegal position while the player decides where to move next.
    String lastRejectedFEN;

    // Server version counter for optimistic concurrency (tracks what the server holds).
    int serverVersion;

    CGMGameResult result;
};

ChessGameManager cgm;

bool cgm_isChoosingPromotion()
{
    return cgm.state == CGM_LOCAL_TURN_PROMOTION;
}

// forward declaration — defined later in this file
void cgm_beginWaitingForStableBoard();

// ============================================================
// Post-move bookkeeping helpers
// ============================================================

// Updates castling rights after a move using proper move-history logic
// (never re-grants rights once lost).
static void cgm_updateCastlingAfterMove(const String &beforeFEN,
                                        const String &afterFEN,
                                        bool castling[4])
{
    char before[8][8], after[8][8];
    if (!parseFENBoard(beforeFEN, before) || !parseFENBoard(afterFEN, after))
        return;

    // Count changed squares to identify move type
    int diffCount = 0;
    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 8; c++)
            if (before[r][c] != after[r][c])
                diffCount++;

    bool newFlags[4];

    if (diffCount == 2)
    {
        // Normal move or capture
        int fromRow = -1, fromCol = -1, toRow = -1, toCol = -1;
        for (int r = 0; r < 8; r++)
        {
            for (int c = 0; c < 8; c++)
            {
                if (before[r][c] != after[r][c])
                {
                    if (before[r][c] != '.' && after[r][c] == '.')
                    {
                        fromRow = r;
                        fromCol = c;
                    }
                    else if (after[r][c] != '.')
                    {
                        toRow = r;
                        toCol = c;
                    }
                }
            }
        }
        if (fromRow == -1 || toRow == -1)
            return;
        char movedPiece = before[fromRow][fromCol];
        char capturedPiece = before[toRow][toCol];
        updateCastlingFlags(before, fromRow, fromCol, toRow, toCol,
                            movedPiece, capturedPiece, castling, newFlags);
        for (int i = 0; i < 4; i++)
            castling[i] = newFlags[i];
    }
    else if (diffCount == 3)
    {
        // En passant — pawns cannot affect castling rights, nothing to do
    }
    else if (diffCount == 4)
    {
        // Castling move — find the king that left its square and clear its rights
        for (int i = 0; i < 4; i++)
            newFlags[i] = castling[i];
        for (int r = 0; r < 8; r++)
            for (int c = 0; c < 8; c++)
                if (before[r][c] != after[r][c])
                {
                    if (before[r][c] == 'K')
                    {
                        newFlags[0] = false;
                        newFlags[1] = false;
                    }
                    if (before[r][c] == 'k')
                    {
                        newFlags[2] = false;
                        newFlags[3] = false;
                    }
                }
        for (int i = 0; i < 4; i++)
            castling[i] = newFlags[i];
    }
}

// Detects a two-square pawn advance and records the en passant target square,
// or clears it if no such advance occurred.
static void cgm_updateEnPassantSquare(const String &beforeFEN, const String &afterFEN)
{
    cgm.enPassantSquare[0] = '\0';

    char before[8][8], after[8][8];
    if (!parseFENBoard(beforeFEN, before) || !parseFENBoard(afterFEN, after))
        return;

    for (int c = 0; c < 8; c++)
    {
        // White pawn: row 6 (rank 2) → row 4 (rank 4), target = row 5 (rank 3)
        if (before[6][c] == 'P' && after[6][c] == '.' &&
            before[4][c] == '.' && after[4][c] == 'P')
        {
            cgm.enPassantSquare[0] = 'a' + c;
            cgm.enPassantSquare[1] = '3';
            cgm.enPassantSquare[2] = '\0';
            return;
        }
        // Black pawn: row 1 (rank 7) → row 3 (rank 5), target = row 2 (rank 6)
        if (before[1][c] == 'p' && after[1][c] == '.' &&
            before[3][c] == '.' && after[3][c] == 'p')
        {
            cgm.enPassantSquare[0] = 'a' + c;
            cgm.enPassantSquare[1] = '6';
            cgm.enPassantSquare[2] = '\0';
            return;
        }
    }
}

// Increments half-move clock, or resets it on pawn move or capture.
static void cgm_updateHalfMoveClock(const String &beforeFEN, const String &afterFEN)
{
    char before[8][8], after[8][8];
    if (!parseFENBoard(beforeFEN, before) || !parseFENBoard(afterFEN, after))
    {
        cgm.halfMoveClock = 0;
        return;
    }

    bool reset = false;
    for (int r = 0; r < 8; r++)
    {
        for (int c = 0; c < 8; c++)
        {
            if (before[r][c] == after[r][c])
                continue;
            // Any pawn that moved (left its square)
            if (before[r][c] != '.' && after[r][c] == '.' && tolower(before[r][c]) == 'p')
                reset = true;
            // Any capture: a square that had a piece and now holds a different piece
            if (before[r][c] != '.' && after[r][c] != '.' && before[r][c] != after[r][c])
                reset = true;
        }
    }

    if (reset)
        cgm.halfMoveClock = 0;
    else
        cgm.halfMoveClock++;
}

// Returns true when neither side has enough material to force checkmate.
static bool cgm_isInsufficientMaterial(char board[8][8])
{
    int whitePieces = 0, blackPieces = 0;
    int whiteBishops = 0, blackBishops = 0;
    int whiteKnights = 0, blackKnights = 0;

    for (int r = 0; r < 8; r++)
    {
        for (int c = 0; c < 8; c++)
        {
            char p = board[r][c];
            if (p == '.' || p == 'K' || p == 'k')
                continue;
            if (p == 'Q' || p == 'R' || p == 'P' ||
                p == 'q' || p == 'r' || p == 'p')
                return false; // major/pawn = sufficient
            if (isWhitePiece(p))
            {
                whitePieces++;
                if (p == 'B')
                    whiteBishops++;
                if (p == 'N')
                    whiteKnights++;
            }
            else
            {
                blackPieces++;
                if (p == 'b')
                    blackBishops++;
                if (p == 'n')
                    blackKnights++;
            }
        }
    }

    if (whitePieces == 0 && blackPieces == 0)
        return true; // K vs K
    if (whitePieces == 0 && blackPieces == 1 && (blackBishops == 1 || blackKnights == 1))
        return true;
    if (blackPieces == 0 && whitePieces == 1 && (whiteBishops == 1 || whiteKnights == 1))
        return true;
    return false;
}
// Replace these bodies with your real display and LED calls.
// ============================================================

void cgm_uiMessage(const String &line1, const String &line2 = "")
{
    String msg = line1;
    if (line2.length() > 0)
    {
        msg += " | ";
        msg += line2;
    }
    Serial0.print("[UI] ");
    Serial0.println(msg);
    displayStatusBar(msg.c_str(), COLOR_RGB565_BLUE);
}

void cgm_ledClear()
{
    // LEDs no longer used — no-op.
}

void cgm_ledShowMoveSquares(const String &beforeFEN, const String &afterFEN)
{
    // LEDs no longer used.
    // Move squares are shown on the display by ChessBoard.ino via
    // drawGameScreenWithMove() whenever the committed / pending FEN changes.
    (void)beforeFEN;
    (void)afterFEN;
}

// ============================================================
// Networking helpers  (delegate to api_connect / wifi_manager)
// ============================================================

void cgm_connectWiFi()
{
    if (cgm_wifiConnected())
        return;

    cgm_uiMessage("Reconnecting WiFi...");
    if (wmConnect(WIFI_SSID, WIFI_PASS))
        cgm_uiMessage("WiFi reconnected", WiFi.localIP().toString());
    else
        cgm_uiMessage("WiFi not available");
}

bool cgm_sendFenToServer(const String &fen, const String &move, int expectedVersion)
{
    ApiResult r = pushFENState(fen, move, expectedVersion);
    return r.ok;
}

bool cgm_fetchLatestFenFromServer(String &latestFen)
{
    ApiResult r = fetchLatestFEN();
    if (r.ok)
    {
        latestFen = r.data;
        return true;
    }
    return false;
}

// ============================================================
// Game result helpers
// ============================================================

CGMGameResult cgm_getGameResult(const String &fen, bool whiteToMove, const bool castling[4])
{
    char board[8][8];
    if (!parseFENBoard(fen, board))
        return CGM_RESULT_NONE;

    // 50-move rule (100 half-moves = 50 full moves without pawn move or capture)
    if (cgm.halfMoveClock >= 100)
        return CGM_RESULT_DRAW_50_MOVE;

    // Insufficient material
    if (cgm_isInsufficientMaterial(board))
        return CGM_RESULT_DRAW_MATERIAL;

    bool inCheck = isKingInCheck(board, whiteToMove);
    bool hasMove = hasAnyLegalMove(board, whiteToMove, castling);

    if (hasMove)
        return CGM_RESULT_NONE;

    if (inCheck)
        return whiteToMove ? CGM_RESULT_BLACK_WIN : CGM_RESULT_WHITE_WIN;

    return CGM_RESULT_STALEMATE;
}

String cgm_resultToString(CGMGameResult result)
{
    if (result == CGM_RESULT_WHITE_WIN)
        return "White wins by checkmate";
    if (result == CGM_RESULT_BLACK_WIN)
        return "Black wins by checkmate";
    if (result == CGM_RESULT_STALEMATE)
        return "Draw by stalemate";
    if (result == CGM_RESULT_DRAW_50_MOVE)
        return "Draw - 50 move rule";
    if (result == CGM_RESULT_DRAW_MATERIAL)
        return "Draw - insufficient material";
    return "Game active";
}

// ============================================================
// Finite State Machine
// ============================================================

// Holds the FEN and turn info fetched during CGM_JOIN_POLLING so that
// cgm_handleGameInitialization() can inherit an in-progress game.
static String cgm_joinedFEN = "";
static bool cgm_joinedWhiteToMove = true;

// State to enter once the board sync check passes.
static CGMState cgm_nextStateAfterSync = CGM_LOCAL_TURN_WAIT_FOR_BOARD;
// Last sync message — used to avoid redrawing the status bar every tick.
static String cgm_lastSyncMsg = "";

void cgm_resetManager()
{
    cgm.state = CGM_WAIT_FOR_GAME_START;
    cgm.localIsWhite = CGMConfig::LOCAL_IS_WHITE;
    cgm.whiteToMove = true;
    cgm.gameActive = false;
    cgm_resetCastle(cgm.castling);
    cgm.committedFEN = cgm_startFen();
    cgm.pendingFEN = "";
    cgm.remoteIncomingFEN = "";
    cgm.stableCandidateFEN = "";
    cgm.stableSinceMs = 0;
    cgm.lastPollMs = 0;
    cgm.lastWifiRetryMs = 0;
    cgm.lastStatusPrintMs = 0;
    cgm.enPassantSquare[0] = '\0';
    cgm.halfMoveClock = 0;
    cgm.readyBannerShown = false;
    cgm.sendRetryCount = 0;
    cgm.sendStartMs = 0;
    cgm.lastRejectedFEN = "";
    cgm.serverVersion = 0;
    cgm.result = CGM_RESULT_NONE;
}

void cgm_startGameNow()
{
    cgm.state = CGM_GAME_INITIALIZATION;
}

// Inject an arbitrary FEN for edge-case testing.
// Bypasses normal game init: sets up the committed FEN directly and enters
// board-sync → local-turn-wait so the player can set up pieces and make a move.
void cgm_loadEdgeCaseFEN(const String &fen, bool whiteToMove, const bool *castlingRights)
{
    cgm_resetManager();
    cgm.localIsWhite = whiteToMove; // tester plays the side to move
    cgm.gameActive = true;
    cgm.committedFEN = fen;
    cgm.whiteToMove = whiteToMove;
    cgm.pendingFEN = "";
    cgm.remoteIncomingFEN = "";
    cgm.enPassantSquare[0] = '\0';
    cgm.halfMoveClock = 0;
    cgm.sendRetryCount = 0;
    cgm.sendStartMs = 0;
    if (castlingRights)
    {
        for (int i = 0; i < 4; i++)
            cgm.castling[i] = castlingRights[i];
    }
    else
    {
        for (int i = 0; i < 4; i++)
            cgm.castling[i] = true;
    }
    cgm_moveConfirmed = false;
    cgm_moveCancelled = false;
    cgm_promotionSelected = false;
    cgm_requestedPromotion = CGMConfig::DEFAULT_PROMOTION;
    cgm_beginWaitingForStableBoard();
    cgm_nextStateAfterSync = CGM_LOCAL_TURN_WAIT_FOR_BOARD;
    cgm_lastSyncMsg = "";
    cgm.state = CGM_BOARD_SYNC;
}

// Start a fresh game as white. Resets the server state first so whitePlayerId
// is cleared and this board can claim white on its first move.
void cgm_createGameNow()
{
    cgm_resetManager();
    cgm.localIsWhite = true;
    cgm_joinedFEN = "";
    displayStatusBar("Resetting game...", COLOR_RGB565_BLUE);
    resetGame();
    cgm.state = CGM_GAME_INITIALIZATION;
}

// Join an existing game. Polls the server for the current board state and
// derives this board's color by comparing its MAC to the stored whitePlayerId.
// localIsWhite is set to false here as a safe default and overwritten in
// cgm_handleJoinPolling() once the server responds.
void cgm_joinGameNow()
{
    cgm_resetManager();
    cgm.localIsWhite = false;
    cgm_joinedFEN = "";
    cgm_joinedWhiteToMove = true;
    cgm.state = CGM_JOIN_POLLING;
}

void cgm_finishGame(CGMGameResult result)
{
    cgm.result = result;
    cgm.gameActive = false;
    cgm.state = CGM_GAME_END;
    cgm_ledClear();
    // Full game-over screen is drawn by ChessBoard.ino on the next tick
    // (it detects cgm_isGameOver() == true).
    Serial0.print("[FSM] Game over: ");
    Serial0.println(cgm_resultToString(result));
}

bool cgm_isLocalTurn()
{
    return cgm.whiteToMove == cgm.localIsWhite;
}

void cgm_setState(CGMState newState)
{
    cgm.state = newState;
}

void cgm_beginWaitingForStableBoard()
{
    cgm.stableCandidateFEN = "";
    cgm.stableSinceMs = 0;
    cgm.lastRejectedFEN = "";
}

bool cgm_candidateBoardReady(String &candidateFen)
{
    String physFen = cgm_boardOnlyFen(cgm_physicalBoardFEN);
    if (physFen.length() == 0)
        return false;

    // Compare against the expected physical representation of the committed board.
    // If they match, no move has been made yet (or the player put the piece back).
    String expectedPhys = cgm_toPhysicalFEN(cgm.committedFEN);
    if (physFen == expectedPhys)
    {
        cgm.stableCandidateFEN = "";
        cgm.stableSinceMs = 0;
        cgm.lastRejectedFEN = ""; // piece returned to start — allow a fresh attempt
        return false;
    }

    // Translate the physical change back to a logical candidate FEN.
    String logicalCandidate;
    if (!cgm_physicalToLogicalFEN(cgm.committedFEN, physFen, logicalCandidate))
    {
        // Physical state is ambiguous (e.g. multiple pieces picked up simultaneously)
        cgm.stableCandidateFEN = "";
        cgm.stableSinceMs = 0;
        return false;
    }

    // Don't re-validate a position we already rejected — the player needs to
    // change the board before we try again.
    if (logicalCandidate == cgm.lastRejectedFEN)
        return false;

    // Require the board to hold the same candidate for LOCAL_STABLE_TIME_MS
    if (logicalCandidate != cgm.stableCandidateFEN)
    {
        cgm.stableCandidateFEN = logicalCandidate;
        cgm.stableSinceMs = millis();
        return false;
    }

    if (millis() - cgm.stableSinceMs < CGMConfig::LOCAL_STABLE_TIME_MS)
        return false;

    candidateFen = logicalCandidate;
    return true;
}

void cgm_handleWaitForGameStart()
{
    if (!cgm.readyBannerShown)
    {
        cgm_uiMessage("Ready", "Call cgm_startGameNow() or map it to a button");
        cgm.readyBannerShown = true;
    }

    if (cgm_newGameRequested)
    {
        cgm_newGameRequested = false;
        cgm_startGameNow();
        cgm.readyBannerShown = false;
    }
}

void cgm_handleJoinPolling()
{
    cgm_uiMessage("Joining game...", "Fetching board state");

    GameStateResult gs = fetchGameState();
    if (gs.ok)
    {
        cgm_joinedFEN = gs.fen;
        cgm_joinedWhiteToMove = gs.whiteToMove;
        cgm.serverVersion = gs.version;
        cgm.localIsWhite = gs.isWhite; // assigned by server based on MAC address
        String turnLabel = gs.whiteToMove ? "White to move" : "Black to move";
        cgm_uiMessage(cgm.localIsWhite ? "You are White" : "You are Black", turnLabel);
        Serial0.printf("[JOIN] Inherited FEN: %s  whiteToMove=%d  isWhite=%d\n",
                       gs.fen.c_str(), (int)gs.whiteToMove, (int)gs.isWhite);
    }
    else
    {
        // No game in progress — start fresh and wait for white's first move.
        cgm_joinedFEN = "";
        cgm_joinedWhiteToMove = true;
        cgm_uiMessage("No active game", "Waiting for opponent to start");
    }

    cgm_setState(CGM_GAME_INITIALIZATION);
}

void cgm_handleGameInitialization()
{
    cgm.gameActive = true;

    // If we arrived here via cgm_joinGameNow() a fetched FEN may be waiting.
    if (cgm_joinedFEN.length() > 0)
    {
        cgm.committedFEN = cgm_joinedFEN;
        cgm.whiteToMove = cgm_joinedWhiteToMove;
        cgm_rebuildCastlingRightsFromBoard(cgm.committedFEN, cgm.castling);
        cgm_joinedFEN = ""; // consume
        Serial0.println("[INIT] Inherited game state from server");
    }
    else
    {
        cgm.committedFEN = cgm_startFen();
        cgm.whiteToMove = true;
        cgm_resetCastle(cgm.castling);
    }

    cgm.pendingFEN = "";
    cgm.remoteIncomingFEN = "";
    cgm.enPassantSquare[0] = '\0';
    cgm.halfMoveClock = 0;
    cgm.sendRetryCount = 0;
    cgm.sendStartMs = 0;
    cgm_beginWaitingForStableBoard(); // also clears lastRejectedFEN
    cgm_moveConfirmed = false;
    cgm_moveCancelled = false;
    cgm_promotionSelected = false;
    cgm_requestedPromotion = CGMConfig::DEFAULT_PROMOTION;

    String colorLabel = cgm.localIsWhite ? "You are White" : "You are Black";
    String startingLabel = cgm.whiteToMove ? "White starts" : "Black starts";
    cgm_uiMessage(colorLabel, startingLabel);

    // Always go through BOARD_SYNC so the player has to match the physical board
    // before any moves are accepted.  This catches mid-game joins where the
    // physical pieces haven't been set up yet.
    cgm_nextStateAfterSync = cgm_isLocalTurn()
                                 ? CGM_LOCAL_TURN_WAIT_FOR_BOARD
                                 : CGM_WAIT_FOR_REMOTE_MOVE;
    cgm_lastSyncMsg = "";
    cgm_setState(CGM_BOARD_SYNC);
}

void cgm_handleBoardSync()
{
    String physOnly = cgm_boardOnlyFen(cgm_physicalBoardFEN);
    String expectedPhys = cgm_toPhysicalFEN(cgm.committedFEN);

    if (physOnly.length() > 0 && physOnly == expectedPhys)
    {
        cgm_lastSyncMsg = "";
        cgm_uiMessage("Board ready");
        cgm_setState(cgm_nextStateAfterSync);
        return;
    }

    // Count missing and extra squares for a helpful message.
    String msg = "Set board to match screen";
    if (physOnly.length() > 0 && expectedPhys.length() > 0)
    {
        char expected[8][8], physical[8][8];
        if (parseFENBoard(expectedPhys, expected) && parseFENBoard(physOnly, physical))
        {
            int missing = 0, extra = 0;
            for (int r = 0; r < 8; r++)
                for (int c = 0; c < 8; c++)
                {
                    if (expected[r][c] != '.' && physical[r][c] == '.')
                        missing++;
                    if (expected[r][c] == '.' && physical[r][c] != '.')
                        extra++;
                }
            if (missing > 0 || extra > 0)
            {
                msg = "";
                if (missing > 0)
                    msg += String(missing) + " missing";
                if (missing > 0 && extra > 0)
                    msg += ", ";
                if (extra > 0)
                    msg += String(extra) + " extra";
            }
        }
    }

    if (msg != cgm_lastSyncMsg)
    {
        cgm_lastSyncMsg = msg;
        cgm_uiMessage(msg);
    }
}

void cgm_handleLocalTurnWaitForBoard()
{
    String candidateFen;
    if (!cgm_candidateBoardReady(candidateFen))
    {
        return;
    }

    cgm.pendingFEN = candidateFen;

    // Check for pawn promotion: candidate has a pawn on the back rank.
    // White pawn 'P' on row 0 (rank 8), black pawn 'p' on row 7 (rank 1).
    {
        char board[8][8];
        bool isPromotion = false;
        if (parseFENBoard(candidateFen, board))
        {
            for (int c = 0; c < 8 && !isPromotion; c++)
            {
                if (board[0][c] == 'P')
                    isPromotion = true;
                if (board[7][c] == 'p')
                    isPromotion = true;
            }
        }
        if (isPromotion)
        {
            cgm_promotionSelected = false;
            cgm_requestedPromotion = CGMConfig::DEFAULT_PROMOTION;
            cgm_uiMessage("Choose promotion piece");
            cgm_setState(CGM_LOCAL_TURN_PROMOTION);
            return;
        }
    }

    cgm_setState(CGM_LOCAL_TURN_VALIDATE);
}

void cgm_handleLocalTurnPromotion()
{
    if (!cgm_promotionSelected)
        return;
    cgm_promotionSelected = false;
    cgm_setState(CGM_LOCAL_TURN_VALIDATE);
}

void cgm_handleLocalTurnValidate()
{
    String validated = validateMoveAndReturnFEN(
        cgm.committedFEN,
        cgm.pendingFEN,
        cgm.whiteToMove,
        cgm.castling,
        cgm_requestedPromotion,
        cgm.enPassantSquare); // pass current en passant target

    if (validated == "Invalid Move")
    {
        cgm_uiMessage("Illegal move", "Try a different square");
        // Cache the rejected position so we don't loop on the same board state
        cgm.lastRejectedFEN = cgm.pendingFEN;
        cgm_beginWaitingForStableBoard();
        cgm.pendingFEN = "";
        cgm_setState(CGM_LOCAL_TURN_WAIT_FOR_BOARD);
        return;
    }

    cgm_uiMessage("Move valid", "Confirm on touchscreen");
    cgm_ledShowMoveSquares(cgm.committedFEN, validated);
    // Update pendingFEN with the validated result — important for promotions
    // where the pawn must be replaced with the chosen piece before sending.
    cgm.pendingFEN = validated;
    cgm_setState(CGM_LOCAL_TURN_CONFIRM);
}

void cgm_handleLocalTurnConfirm()
{
    if (cgm_moveCancelled)
    {
        cgm_moveCancelled = false;
        cgm_uiMessage("Move cancelled", "Restore board and try again");
        cgm_beginWaitingForStableBoard();
        cgm.pendingFEN = "";
        cgm_ledClear();
        cgm_setState(CGM_LOCAL_TURN_WAIT_FOR_BOARD);
        return;
    }

    if (!cgm_moveConfirmed)
    {
        return;
    }

    cgm_moveConfirmed = false;
    cgm_setState(CGM_SEND_STATE);
}

void cgm_handleSendState()
{
    // Record when we first tried to send so we can give up after a timeout
    if (cgm.sendStartMs == 0)
        cgm.sendStartMs = millis();

    // Derive a UCI move string (e.g. "e2e4") from committed → pending FEN.
    String moveStr = "none";
    {
        int fr, fc, tr, tc;
        if (cgm_findMoveSquares(cgm.committedFEN, cgm.pendingFEN, fr, fc, tr, tc))
        {
            moveStr = String((char)('a' + fc));
            moveStr += String((char)('8' - fr));
            moveStr += String((char)('a' + tc));
            moveStr += String((char)('8' - tr));
        }
    }

    bool sent = cgm_sendFenToServer(cgm.pendingFEN, moveStr, cgm.serverVersion);
    if (!sent)
    {
        cgm.sendRetryCount++;
        if (cgm.sendRetryCount < 5 && millis() - cgm.sendStartMs < 30000UL)
        {
            cgm_uiMessage("Send failed", "Retrying...");
            return; // will retry on next tick
        }
        // Give up after 5 retries or 30 s — commit locally and continue
        cgm_uiMessage("Send failed", "Continuing offline");
    }
    else
    {
        cgm.serverVersion++; // server accepted the move, version advanced
    }

    cgm.sendRetryCount = 0;
    cgm.sendStartMs = 0;

    // Update castling rights using proper move-history logic (never re-grants)
    cgm_updateCastlingAfterMove(cgm.committedFEN, cgm.pendingFEN, cgm.castling);

    // Update en passant target and half-move clock
    cgm_updateEnPassantSquare(cgm.committedFEN, cgm.pendingFEN);
    cgm_updateHalfMoveClock(cgm.committedFEN, cgm.pendingFEN);

    cgm.committedFEN = cgm.pendingFEN;
    cgm.pendingFEN = "";
    cgm_ledClear();

    cgm.whiteToMove = !cgm.whiteToMove;

    CGMGameResult result = cgm_getGameResult(cgm.committedFEN, cgm.whiteToMove, cgm.castling);
    if (result != CGM_RESULT_NONE)
    {
        cgm_finishGame(result);
        return;
    }

    cgm_uiMessage("Move sent", "Waiting for opponent");
    cgm_setState(CGM_WAIT_FOR_REMOTE_MOVE);
}

void cgm_handleWaitForRemoteMove()
{
    uint32_t now = millis();

    if (now - cgm.lastPollMs < CGMConfig::POLL_INTERVAL_MS)
    {
        return;
    }
    cgm.lastPollMs = now;

    GameStateResult gs = fetchGameState();
    if (!gs.ok)
    {
        cgm_uiMessage("Polling failed");
        return;
    }

    String latestFen = cgm_boardOnlyFen(gs.fen);

    if (latestFen.length() == 0)
    {
        return;
    }

    if (cgm_sameBoardFen(latestFen, cgm.committedFEN))
    {
        return;
    }

    cgm.serverVersion = gs.version;
    cgm.remoteIncomingFEN = latestFen;
    cgm_setState(CGM_APPLY_REMOTE_MOVE);
}

void cgm_handleApplyRemoteMove()
{
    // Validate the incoming FEN before trusting it
    char incomingBoard[8][8];
    if (!parseFENBoard(cgm.remoteIncomingFEN, incomingBoard) ||
        !boardHasExactlyOneKingEach(incomingBoard))
    {
        cgm_uiMessage("Invalid remote FEN", "Ignoring bad data");
        cgm.remoteIncomingFEN = "";
        cgm_setState(CGM_WAIT_FOR_REMOTE_MOVE);
        return;
    }

    cgm_ledShowMoveSquares(cgm.committedFEN, cgm.remoteIncomingFEN);
    cgm_uiMessage("Opponent move received", "Replicate move on physical board");

    // Wait until the physical board matches the expected physical representation
    // of the incoming position (not a direct string comparison with logical FEN)
    String physicalNow = cgm_boardOnlyFen(cgm_physicalBoardFEN);
    String expectedPhys = cgm_toPhysicalFEN(cgm.remoteIncomingFEN);
    if (physicalNow != expectedPhys)
        return;

    // Update castling rights, en passant, and half-move clock from this remote move
    cgm_updateCastlingAfterMove(cgm.committedFEN, cgm.remoteIncomingFEN, cgm.castling);
    cgm_updateEnPassantSquare(cgm.committedFEN, cgm.remoteIncomingFEN);
    cgm_updateHalfMoveClock(cgm.committedFEN, cgm.remoteIncomingFEN);

    cgm.committedFEN = cgm.remoteIncomingFEN;
    cgm.remoteIncomingFEN = "";
    cgm_ledClear();

    cgm.whiteToMove = !cgm.whiteToMove;

    CGMGameResult result = cgm_getGameResult(cgm.committedFEN, cgm.whiteToMove, cgm.castling);
    if (result != CGM_RESULT_NONE)
    {
        cgm_finishGame(result);
        return;
    }

    cgm_beginWaitingForStableBoard();

    if (cgm_isLocalTurn())
    {
        cgm_uiMessage("Your turn", "Make your move");
        cgm_setState(CGM_LOCAL_TURN_WAIT_FOR_BOARD);
    }
    else
    {
        cgm_uiMessage("Waiting for opponent");
        cgm_setState(CGM_WAIT_FOR_REMOTE_MOVE);
    }
}

void cgm_handleGameEnd()
{
    if (!cgm_newGameRequested)
    {
        return;
    }

    cgm_newGameRequested = false;
    cgm_setState(CGM_GAME_INITIALIZATION);
}

void cgm_handleErrorState()
{
    cgm_uiMessage("FSM error state");
}

void cgm_tick()
{
    uint32_t now = millis();

    if (!cgm_wifiConnected() && now - cgm.lastWifiRetryMs >= CGMConfig::WIFI_RETRY_INTERVAL_MS)
    {
        cgm.lastWifiRetryMs = now;
        cgm_connectWiFi();
    }

    if (now - cgm.lastStatusPrintMs >= CGMConfig::STATUS_PRINT_INTERVAL_MS)
    {
        cgm.lastStatusPrintMs = now;
        Serial0.print("[FSM] state=");
        Serial0.print((int)cgm.state);
        Serial0.print(" whiteToMove=");
        Serial0.print(cgm.whiteToMove ? "W" : "B");
        Serial0.print(" local=");
        Serial0.print(cgm.localIsWhite ? "W" : "B");
        Serial0.print(" fen=");
        Serial0.println(cgm.committedFEN);
    }

    switch (cgm.state)
    {
    case CGM_WAIT_FOR_GAME_START:
        cgm_handleWaitForGameStart();
        break;

    case CGM_JOIN_POLLING:
        cgm_handleJoinPolling();
        break;

    case CGM_GAME_INITIALIZATION:
        cgm_handleGameInitialization();
        break;

    case CGM_BOARD_SYNC:
        cgm_handleBoardSync();
        break;

    case CGM_LOCAL_TURN_WAIT_FOR_BOARD:
        cgm_handleLocalTurnWaitForBoard();
        break;

    case CGM_LOCAL_TURN_PROMOTION:
        cgm_handleLocalTurnPromotion();
        break;

    case CGM_LOCAL_TURN_VALIDATE:
        cgm_handleLocalTurnValidate();
        break;

    case CGM_LOCAL_TURN_CONFIRM:
        cgm_handleLocalTurnConfirm();
        break;

    case CGM_SEND_STATE:
        cgm_handleSendState();
        break;

    case CGM_WAIT_FOR_REMOTE_MOVE:
        cgm_handleWaitForRemoteMove();
        break;

    case CGM_APPLY_REMOTE_MOVE:
        cgm_handleApplyRemoteMove();
        break;

    case CGM_GAME_END:
        cgm_handleGameEnd();
        break;

    case CGM_ERROR_STATE:
        cgm_handleErrorState();
        break;
    }
}

// ============================================================
// Arduino entry points
// ============================================================
// If you already have another setup() / loop() in your main sketch,
// move the body of these functions into that file and just call:
//
//   cgm_setup();
//   cgm_tick();
//
// from your existing setup() and loop().
// ============================================================

// ============================================================
// Accessor helpers used by ChessBoard.ino
// ============================================================

// Returns true while the board sync check is in progress.
bool cgm_isBoardSyncing()
{
    return cgm.state == CGM_BOARD_SYNC;
}

// Returns true when the local player was assigned the white pieces.
bool cgm_isLocalPlayerWhite()
{
    return cgm.localIsWhite;
}

// Returns a short status string suitable for the bottom status bar, e.g.
//   "Your turn (White)"  or  "Opponent (Black) to move"
const String &cgm_getTurnStatusString()
{
    static String s;
    if (!cgm.gameActive)
    {
        s = "";
        return s;
    }
    const char *side = cgm.whiteToMove ? "White" : "Black";
    if (cgm_isLocalTurn())
        s = String("Your turn (") + side + ")";
    else
        s = String("Opponent (") + side + ") to move";
    return s;
}

// True while the FSM is waiting for the player to confirm or cancel a move.
bool cgm_isConfirming()
{
    return cgm.state == CGM_LOCAL_TURN_CONFIRM;
}

// True while the FSM is waiting for the remote opponent's move.
bool cgm_isWaitingForRemote()
{
    return cgm.state == CGM_WAIT_FOR_REMOTE_MOVE;
}

// The last fully committed board FEN (empty until game starts).
const String &cgm_getCommittedFEN()
{
    return cgm.committedFEN;
}

// The candidate FEN waiting for player confirmation (empty if not confirming).
const String &cgm_getPendingFEN()
{
    return cgm.pendingFEN;
}

// The incoming remote FEN currently being applied (empty otherwise).
const String &cgm_getIncomingFEN()
{
    return cgm.remoteIncomingFEN;
}

bool cgm_isWhiteToMove()
{
    return cgm.whiteToMove;
}

bool cgm_isGameOver()
{
    return cgm.state == CGM_GAME_END;
}

// Returns the human-readable result string for the finished game.
const String &cgm_getGameResultString()
{
    static String s;
    s = cgm_resultToString(cgm.result);
    return s;
}

// Returns true if the current side's king is in check on the committed board.
bool cgm_isInCheck()
{
    if (!cgm.gameActive)
        return false;
    char board[8][8];
    if (!parseFENBoard(cgm.committedFEN, board))
        return false;
    return isKingInCheck(board, cgm.whiteToMove);
}

// Detects a single piece being lifted off the committed board.
// squareName is set to e.g. "e2". Returns true once per lift event.
bool cgm_getPieceLiftSquare(char squareName[3])
{
    if (!cgm.gameActive)
        return false;

    // Only report lifts during the local player's wait-for-board phase
    if (cgm.state != CGM_LOCAL_TURN_WAIT_FOR_BOARD)
        return false;

    String physOnly = cgm_boardOnlyFen(cgm_physicalBoardFEN);
    if (physOnly.length() == 0)
        return false;

    // Build the expected physical board from the committed logical FEN
    char expectedPhys[8][8];
    {
        char logBoard[8][8];
        if (!parseFENBoard(cgm.committedFEN, logBoard))
            return false;
        for (int r = 0; r < 8; r++)
            for (int c = 0; c < 8; c++)
                expectedPhys[r][c] = cgm_toPhysChar(logBoard[r][c]);
    }

    char physical[8][8];
    if (!parseFENBoard(physOnly, physical))
        return false;

    // A single lift: exactly one square that expected to have a piece is now empty
    int liftRow = -1, liftCol = -1, liftCount = 0;
    for (int r = 0; r < 8; r++)
    {
        for (int c = 0; c < 8; c++)
        {
            if (expectedPhys[r][c] != '.' && physical[r][c] == '.')
            {
                liftCount++;
                liftRow = r;
                liftCol = c;
            }
        }
    }

    if (liftCount == 1)
    {
        squareName[0] = 'a' + liftCol;
        squareName[1] = '8' - liftRow;
        squareName[2] = '\0';
        return true;
    }
    return false;
}

// ============================================================
// One-time initialisation called from the main sketch setup()
// ============================================================
void cgm_setup()
{
    cgm_resetManager();
    cgm_uiMessage("Chess FSM ready");
}