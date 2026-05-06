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

---

## AWS Setup Guide

This section documents how to deploy the backend from scratch in AWS.

### Prerequisites

* AWS account with console and CLI access
* [AWS CLI](https://docs.aws.amazon.com/cli/latest/userguide/install-cliv2.html) installed and configured (`aws configure`)
* Python 3.12 available locally for packaging

---

### Step 1 — Create the DynamoDB Table

1. Open **AWS Console → DynamoDB → Tables → Create table**
2. Configure:
   - **Table name**: `chess-game` (or any name — you will pass it to Lambda as an env var)
   - **Partition key**: `gameId` (String)
   - **Sort key**: none
   - **Billing mode**: On-demand (pay-per-request)
3. Leave all other settings as default and click **Create table**.

The table schema is managed entirely by the Lambda code (`ensure_single_game()`). No additional index or attribute setup is needed.

---

### Step 2 — Create the Stockfish Lambda Layer

The `/hint` and AI move endpoints require Stockfish compiled for Amazon Linux 2 (the Lambda runtime environment). A pre-built binary is available from the community:

1. Download the Amazon Linux 2 build of Stockfish:
   ```bash
   # Example — check for the latest release at https://github.com/official-stockfish/Stockfish
   wget https://github.com/official-stockfish/Stockfish/releases/download/sf_16/stockfish-ubuntu-x86-64.tar
   ```
2. Create the required directory structure and zip it:
   ```bash
   mkdir -p layer/bin
   cp stockfish layer/bin/stockfish
   chmod +x layer/bin/stockfish
   cd layer && zip -r ../stockfish-layer.zip bin/
   ```
3. In the AWS Console, go to **Lambda → Layers → Create layer**:
   - **Name**: `stockfish`
   - **Upload**: `stockfish-layer.zip`
   - **Compatible runtimes**: Python 3.12
4. Note the **Layer ARN** — you will attach it to the Lambda function in Step 4.

The code expects the binary at `/opt/bin/stockfish` (the `/opt/` prefix is how Lambda exposes layer contents).

---

### Step 3 — Create the Lambda Function

1. Go to **Lambda → Functions → Create function**
2. Configure:
   - **Author from scratch**
   - **Function name**: `chess-api`
   - **Runtime**: Python 3.12
   - **Architecture**: x86_64
3. Click **Create function**.

#### Deploy the code

Package and upload `api.py`:

```bash
cd "API Lambda Code"
zip function.zip api.py
aws lambda update-function-code \
  --function-name chess-api \
  --zip-file fileb://function.zip
```

#### Set the handler

In **Configuration → General configuration → Edit**:
- **Handler**: `api.lambda_handler`

#### Set environment variables

In **Configuration → Environment variables → Edit**, add:

| Key | Value |
|---|---|
| `TABLE_NAME` | `chess-game` (your DynamoDB table name) |

#### Attach the Stockfish layer

In **Configuration → Layers → Add a layer**:
- Select **Custom layers**, choose `stockfish`, and select the latest version.

#### Set memory and timeout

In **Configuration → General configuration → Edit**:
- **Memory**: 512 MB (Stockfish needs headroom; the code caps its hash table at 16 MB)
- **Timeout**: 10 seconds (Stockfish at depth 12 can take a few seconds)

---

### Step 4 — IAM Permissions

The Lambda execution role needs DynamoDB access. Attach the following policy to the role (found under **Configuration → Permissions → Execution role**):

```json
{
  "Version": "2012-10-17",
  "Statement": [
    {
      "Effect": "Allow",
      "Action": [
        "dynamodb:GetItem",
        "dynamodb:PutItem",
        "dynamodb:UpdateItem",
        "dynamodb:DeleteItem"
      ],
      "Resource": "arn:aws:dynamodb:us-east-2:YOUR_ACCOUNT_ID:table/chess-game"
    }
  ]
}
```

Replace `YOUR_ACCOUNT_ID` and `chess-game` with your actual values. The AWS managed policy `AmazonDynamoDBFullAccess` also works for development but is overly broad for production.

---

### Step 5 — Create the API Gateway

The Lambda handler is written for **API Gateway HTTP API (v2)** — it reads `event["requestContext"]["http"]["method"]` and `event["requestContext"]["http"]["path"]`.

1. Go to **API Gateway → Create API → HTTP API → Build**
2. Under **Integrations**, click **Add integration**:
   - **Integration type**: Lambda
   - **Lambda function**: `chess-api`
3. **API name**: `chess-api`
4. Click **Next** through routes and stages — the Lambda function handles all routing internally, so use a catch-all route:
   - **Method**: `ANY`
   - **Resource path**: `/{proxy+}`
5. **Stage name**: `api` (this becomes the first path segment)
6. Click **Create**.

After creation, the console shows your **Invoke URL** in the format:
```
https://<id>.execute-api.<region>.amazonaws.com/api
```

The full base URL used by the firmware and this document is:
```
https://<id>.execute-api.<region>.amazonaws.com/api/v1
```

Update `BASE_URL` in `ChessBoard/api_connect.cpp` if you redeploy to a new endpoint.

---

### Step 6 — Verify the Deployment

```bash
# Should return the initial game state (creates the game record on first call)
curl https://<your-invoke-url>/api/v1/games/1

# Reset to a clean game
curl -X POST https://<your-invoke-url>/api/v1/games/1/reset \
  -H "Content-Type: application/json" \
  -d '{"boardId": "test-board-1", "timerMode": "none"}'

# Submit a move
curl -X POST https://<your-invoke-url>/api/v1/games/1/moves \
  -H "Content-Type: application/json" \
  -d '{
    "move": "e2e4",
    "fen": "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1",
    "boardId": "test-board-1",
    "expectedVersion": 0
  }'
```

---

### Architecture Summary

```
ESP32-S3 Boards (HTTPS)
        │
        ▼
API Gateway HTTP API  (ANY /{proxy+})
        │
        ▼
Lambda  chess-api  (Python 3.12, 512 MB, 10 s timeout)
  │  └── Layer: stockfish  (/opt/bin/stockfish)
  │     Env:   TABLE_NAME=chess-game
        │
        ▼
DynamoDB  chess-game  (partition key: gameId)
```

---

### Redeployment Checklist

| Step | Command / Action |
|---|---|
| Update Lambda code | `zip function.zip api.py` then `aws lambda update-function-code ...` |
| Update env vars | AWS Console → Lambda → Configuration → Environment variables |
| Update Stockfish layer | Upload new layer version, update Lambda to point to it |
| Change base URL on boards | Update `BASE_URL` in `ChessBoard/api_connect.cpp` and re-flash |
