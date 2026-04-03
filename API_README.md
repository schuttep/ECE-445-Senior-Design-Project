# ♟️ ESP32 Smart Chessboard API

## Base URL

```
https://j3zvk9adv0.execute-api.us-east-2.amazonaws.com/api/v1
```

---

## Overview

This API manages a single active chess game (`gameId = 1`).

The backend acts as a **state store**, not a chess engine.

### Client Responsibilities

* Detect moves (e.g., `e2e4`)
* Compute **full FEN string**
* Send both move + FEN to the server

### Server Responsibilities

* Store game state
* Track move history
* Synchronize clients

---

## Game Model

### Game Object

```json
{
  "gameId": "1",
  "status": "ACTIVE",
  "fen": "string",
  "turn": "A | B",
  "version": 0,
  "createdAt": 1234567890,
  "lastMoveAt": 1234567890,
  "moveHistory": []
}
```

### Move Entry

```json
{
  "ply": 1,
  "turn": "A",
  "move": "e2e4",
  "at": 1234567890,
  "fen": "full fen string"
}
```

---

## Endpoints

---

### 🔹 Get Current Game State

```http
GET /games/1
```

#### Response

```json
{
  "gameId": "1",
  "status": "ACTIVE",
  "fen": "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1",
  "turn": "B",
  "version": 1,
  "lastMoveAt": 1771822779
}
```

---

### 🔹 Get Move History

```http
GET /games/1/moves
```

#### Response

```json
{
  "gameId": "1",
  "version": 1,
  "moves": [
    {
      "ply": 1,
      "turn": "A",
      "move": "e2e4",
      "at": 1771822779,
      "fen": "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1"
    }
  ]
}
```

---

### 🔹 Submit Move

```http
POST /games/1/moves
```

#### Request Body

```json
{
  "move": "e2e4",
  "fen": "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1",
  "expectedVersion": 0
}
```

#### Required Fields

* `move` → algebraic move string
* `fen` → **full updated FEN**

#### Optional Fields

* `expectedVersion` → prevents conflicting updates

#### Response

```json
{
  "gameId": "1",
  "status": "ACTIVE",
  "fen": "...",
  "turn": "B",
  "version": 1,
  "lastMoveAt": 1771822779,
  "acceptedMove": {
    "ply": 1,
    "turn": "A",
    "move": "e2e4",
    "at": 1771822779,
    "fen": "..."
  }
}
```

---

### 🔹 Reset Game

```http
POST /games/1/reset
```

#### Response

```json
{
  "gameId": "1",
  "status": "ACTIVE",
  "fen": "startpos",
  "turn": "A",
  "version": 0,
  "lastMoveAt": 1771822779,
  "movesCleared": true
}
```

---

## FEN Format

The API expects **full FEN strings**:

```
<board> <turn> <castling> <en passant> <halfmove> <fullmove>
```

### Example

```
rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1
```

---

## Important Notes

### ⚠️ Backend does NOT:

* Validate moves
* Compute FEN
* Enforce chess rules

### ✅ Backend DOES:

* Store state
* Track moves
* Handle version conflicts

---

## Version Control

To avoid conflicting updates:

* Each move increments `version`
* Client should send `expectedVersion`
* If mismatch → server returns **409 conflict**

---

## Error Responses

### Not Found

```json
{
  "error": {
    "code": "NOT_FOUND",
    "message": "game not found"
  }
}
```

### Missing Fields

```json
{
  "error": {
    "code": "BAD_REQUEST",
    "message": "fen is required"
  }
}
```

### Version Conflict

```json
{
  "error": {
    "code": "VERSION_CONFLICT",
    "message": "state version mismatch"
  },
  "state": { ... }
}
```

---

## Example Usage

### Get Game

```bash
curl https://j3zvk9adv0.execute-api.us-east-2.amazonaws.com/api/v1/games/1
```

### Post Move

```bash
curl -X POST https://j3zvk9adv0.execute-api.us-east-2.amazonaws.com/api/v1/games/1/moves \
  -H "Content-Type: application/json" \
  -d '{
    "move": "e2e4",
    "fen": "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1",
    "expectedVersion": 0
  }'
```

---

## Recommended Improvement

Replace `"startpos"` with full FEN:

```
rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1
```

This keeps the API consistent and simplifies client logic.

---

## Client Flow Summary

1. Fetch current state
2. Detect move
3. Compute new FEN
4. POST move + FEN
5. Other client syncs via GET

---

## Project Context

This API is designed for a **network-connected physical chessboard** using:

* ESP32
* LED board state rendering
* Sensor-based move detection
* Real-time synchronization between boards
