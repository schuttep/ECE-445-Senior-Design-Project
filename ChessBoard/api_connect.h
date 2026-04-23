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
};
GameStateResult fetchGameState();

// Post a move as FEN + move notation (legacy simple endpoint).
ApiResult pushLatestFEN(const String &move, const String &fen);

// Post a move to the server.
//   fen             — board-only FEN after the move
//   move            — UCI move string, e.g. "e2e4" or "e1g1" for castling
//   expectedVersion — current server version for optimistic concurrency
ApiResult pushFENState(const String &fen, const String &move, int expectedVersion);

// Reset the server game state back to the starting position and clear the
// registered white player so a fresh game can begin.
ApiResult resetGame();

#endif