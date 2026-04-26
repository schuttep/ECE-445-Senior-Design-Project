#ifndef API_CONNECT_H
#define API_CONNECT_H

#include <Arduino.h>

struct ApiResult
{
    bool ok;
    String data;
};

// Fetch the latest FEN from the server (GET most recent move).
ApiResult fetchLatestFEN();

// Fetch the current game state: board FEN, whose turn it is, server version,
// and which color this board is assigned.
// Sends this board's MAC address so the server can return the correct color.
// Returns ok=false when no moves have been recorded yet (game not started).
struct GameStateResult
{
    bool ok;
    String fen;
    bool whiteToMove; // true = white to move next
    int version;      // server-side version counter for optimistic concurrency
    bool isWhite;     // true = this board is the white player
    // Timer fields (populated when timerMode != "none")
    String timerMode;    // "none" | "rapid" | "bullet"
    int32_t whiteTimeMs; // white's remaining time in ms at time of fetch
    int32_t blackTimeMs; // black's remaining time in ms at time of fetch
    bool clockRunning;   // true = one side's clock is ticking
    bool clockForWhite;  // true = white's clock is running (only valid when clockRunning)
};
GameStateResult fetchGameState();

// Post a move as FEN + move notation (legacy simple endpoint).
ApiResult pushLatestFEN(const String &move, const String &fen);

// Post a move to the server.
//   fen             — board-only FEN after the move
//   move            — UCI move string, e.g. "e2e4" or "e1g1" for castling
//   expectedVersion — current server version for optimistic concurrency
ApiResult pushFENState(const String &fen, const String &move, int expectedVersion);

// Reset the server game state back to the starting position.
//   timerMode  — "none" (default), "rapid", or "bullet"
//   boardId    — MAC address of the creating board (registered as white immediately)
//   gameMode   — "pvp" (default) or "ai" (play against Stockfish)
ApiResult resetGame(const char *timerMode = "none", const String &boardId = "", const char *gameMode = "pvp");

// Send a heartbeat so the server knows this board is still connected.
// The server uses heartbeats to start / pause the game clock.
ApiResult sendHeartbeat();

// ---- Messenger ----
#define API_MSG_MAX_COUNT 10
#define API_MSG_TEXT_LEN 96

struct ChatMessage
{
    char boardId[36]; // full MAC address string
    char text[API_MSG_TEXT_LEN + 1];
};

struct FetchMessagesResult
{
    bool ok;
    int count;
    ChatMessage messages[API_MSG_MAX_COUNT];
};

// Post a chat message from this board to the server.
ApiResult sendMessage(const String &text);

// Fetch the last API_MSG_MAX_COUNT chat messages for this game.
FetchMessagesResult fetchMessages();

// ---- Stockfish hint ----

struct HintResult
{
    bool ok;
    String move;     // UCI move string, e.g. "e2e4"
    String afterFen; // board-only FEN after the hint move is applied
    String error;    // non-empty on failure
};

// Ask the server for the best move in the current position (Stockfish depth-5).
// Limited to 3 hints per player per game; returns ok=false when exhausted.
HintResult fetchBestMove(const String &currentFen);

#endif