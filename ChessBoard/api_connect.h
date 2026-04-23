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

// Fetch the current game state: latest board FEN and whose turn it is.
// whiteToMove is derived from the number of moves recorded (even = white, odd = black).
// Returns ok=false with an empty FEN when no moves have been recorded yet.
struct GameStateResult
{
    bool ok;
    String fen;
    bool whiteToMove; // true = white to move next
};
GameStateResult fetchGameState();

// Post a move as FEN + move notation (legacy simple endpoint).
ApiResult pushLatestFEN(const String &move, const String &fen);

// Post the full board state used by the FSM:
//   fen         — board-only FEN after the move
//   isWhite     — true if the local player is white
//   gameId      — game identifier
//   boardNum    — physical board number
ApiResult pushFENState(const String &fen, bool isWhite,
                       uint16_t gameId, uint8_t boardNum);

#endif