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
// Returns ok=false when no moves have been recorded yet (game not started),
// but still populates opponentJoined so callers can detect when the second
// board registers before any moves are made.
struct GameStateResult
{
    bool ok;
    String fen;
    bool whiteToMove; // true = white to move next
    int version;      // server-side version counter for optimistic concurrency
    bool isWhite;     // true = this board is the white player
    // Timer fields — server only stores mode and initial budget; boards track clocks locally
    String timerMode;    // "none" | "rapid" | "bullet"
    int32_t timerInitMs; // initial time budget per side in ms (0 when timerMode=="none")
    // Connectivity
    bool opponentJoined; // true when both whitePlayerId and blackPlayerId are registered
    // Game result — non-empty when set by /timeout or similar
    String gameResult; // "" | "white_timeout" | "black_timeout"
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
//   aiDepth    — Stockfish search depth for AI games (1-15, default 5)
ApiResult resetGame(const char *timerMode = "none", const String &boardId = "", const char *gameMode = "pvp", int aiDepth = 5);

// Notify the server that a player ran out of time.
// loserColor: "white" or "black"
// The server records the result so the other board can detect it on its next poll.
ApiResult notifyTimeout(const char *loserColor);

// Send a heartbeat so the server knows this board is still connected.
// The server uses heartbeats to start / pause the game clock.
ApiResult sendHeartbeat();

// ---- Messenger ----
#define API_MSG_MAX_COUNT 10
#define API_MSG_TEXT_LEN 200 // matches server-side 200-char limit

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