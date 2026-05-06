# ChessBoard — ECE 445 Senior Design Project

A networked, physical chess board built on the ESP32-S3. Two boards communicate through an AWS backend, allowing two players to play chess on real hardware with piece detection, touchscreen UI, a live chat panel, optional chess clocks, and Stockfish AI hints.

---

## Table of Contents

1. [Hardware Overview](#hardware-overview)
2. [Pin Assignments](#pin-assignments)
3. [Dependencies / Libraries](#dependencies--libraries)
4. [Project Setup](#project-setup)
5. [File Reference](#file-reference)
6. [Backend API](#backend-api)
7. [Game Flow](#game-flow)
8. [Testing Utilities](#testing-utilities)

---

## Hardware Overview

| Component | Part | Interface |
|---|---|---|
| Microcontroller | ESP32-S3 | — |
| Display | DFRobot ST7365P 480×320 IPS | Hardware SPI |
| Touchscreen | GT911 capacitive | I2C (`Wire`, 0x5D) |
| Hall-effect ADCs | 8× ADS7128 (addresses 0x10–0x17) | I2C (`Wire1`, 100 kHz) |
| Hall sensors | 64× (one per square) | Analog via ADS7128 |

Each ADS7128 chip handles one column of the board (8 squares = 8 channels). Chips are physically wired out of order; the mapping is handled in firmware — see `ADC_driver.cpp`.

The display is in **landscape** mode (480×320). The touchscreen reports raw portrait coordinates which are converted to landscape in `ChessBoard.ino`:
```
tx = 479 - raw_y
ty = raw_x
```

---

## Pin Assignments

Defined in `headers.h`:

| Pin | GPIO | Function |
|---|---|---|
| `SDA_DAQ` | 38 | ADC I2C data (Wire1) |
| `SCL_DAQ` | 39 | ADC I2C clock (Wire1) |
| `SCR_SCLK` | 45 | Display SPI clock |
| `SCR_MOSI` | 48 | Display SPI data |
| `SCR_MISO` | 47 | Display SPI (unused) |
| `SCR_CS` | 21 | Display chip select |
| `SCR_RST` | 14 | Display reset |
| `SCR_DC` | 13 | Display data/command |
| `SCR_BLK` | 12 | Display backlight |
| `SCR_I2C_SCL` | 11 | Touch I2C clock (Wire) |
| `SCR_I2C_SDA` | 10 | Touch I2C data (Wire) |
| `SCR_INT` | 9 | Touch interrupt |
| `SCR_TCH_RST` | 8 | Touch reset |

---

## Dependencies / Libraries

Install all libraries through the Arduino IDE Library Manager or manually place them in your Arduino `libraries/` folder.

| Library | Purpose |
|---|---|
| `DFRobot_GDL` | ST7365P display driver |
| `DFRobot_Touch` | GT911 touchscreen driver |
| `WiFi` | Built-in ESP32 WiFi |
| `WiFiClientSecure` | TLS/HTTPS connections |
| `HTTPClient` | HTTP requests to AWS |
| `ArduinoJson` | JSON parsing/serialization |
| `Wire` | I2C (built-in) |
| `SPI` | SPI (built-in) |

**Board**: Install **esp32 by Espressif Systems** via the Arduino Boards Manager. Select **ESP32S3 Dev Module** as the target board.

---

## Project Setup

### 1. Clone / copy the sketch

The Arduino IDE requires the sketch folder name to match the `.ino` filename. The folder must be named `ChessBoard` and contain `ChessBoard.ino`.

### 2. Create `secrets.h`

Create `ChessBoard/secrets.h` (not committed to version control):

```cpp
#pragma once
#define WIFI_SSID "YourNetworkName"
#define WIFI_PASS "YourPassword"
```

At startup the board will attempt to connect to these credentials automatically. If the connection fails, the on-screen WiFi selector lets you choose a different network.

### 3. Install libraries

See [Dependencies](#dependencies--libraries) above.

### 4. Select board and port

In the Arduino IDE:
- **Tools → Board → ESP32S3 Dev Module**
- **Tools → Port** → select the correct COM port for your ESP32-S3

### 5. Upload

Press **Upload**. Open **Serial Monitor** at 115200 baud to see FSM state logs.

---

## File Reference

### `ChessBoard.ino`
**Main sketch — hardware init, UI, touch handling, and main loop.**

- Initialises the display, ADCs, game FSM, and WiFi on boot.
- Manages a `ScreenState` enum (`MENU`, `GAME`, `BOARD_TEST`, `EDGE_CASE_TEST`, `WIFI_LIST`, `WIFI_PASS_SCREEN`, `TIMER_MODE_SELECT`, `AI_MODE_SELECT`).
- `setup()` — initialises hardware, connects to WiFi, shows the menu.
- `loop()` — reads the physical board every iteration, feeds it to the FSM via `cgm_setPhysicalBoardFEN()`, calls `cgm_tick()`, then redraws only the parts of the screen that have changed (dirty tracking via `gs_*` static variables).
- Handles all touch events: menu navigation, move confirm/cancel, promotion selection, WiFi picker, chat composer, hint button.
- Chat messages are polled every 5 seconds (`MSG_POLL_INTERVAL_MS`).
- Each game grants 3 Stockfish hints (`gs_hintsLeft`).

---

### `gameloop.cpp` / `gameloop.h`
**Finite state machine (FSM) that drives the game.**

This is the core game controller. It is entirely decoupled from hardware; it receives the physical board state as a string and calls display/network functions only through well-defined helpers.

**FSM states:**

| State | Description |
|---|---|
| `CGM_WAIT_FOR_GAME_START` | Idle on the menu |
| `CGM_JOIN_POLLING` | Polling server until white's first move appears |
| `CGM_GAME_INITIALIZATION` | Fetching initial game state from server |
| `CGM_BOARD_SYNC` | Waiting for physical board to match the expected position |
| `CGM_LOCAL_TURN_WAIT_FOR_BOARD` | Waiting for player to make a physical move |
| `CGM_LOCAL_TURN_PROMOTION` | Waiting for player to choose a promotion piece |
| `CGM_LOCAL_TURN_VALIDATE` | Validating the detected move against chess rules |
| `CGM_LOCAL_TURN_CONFIRM` | Waiting for player to confirm or cancel on touchscreen |
| `CGM_SEND_STATE` | Sending the confirmed move to the server |
| `CGM_WAIT_FOR_REMOTE_MOVE` | Polling server for opponent's move |
| `CGM_APPLY_REMOTE_MOVE` | Waiting for player to replicate opponent's move on board |
| `CGM_GAME_END` | Game over (checkmate, stalemate, timeout) |
| `CGM_ERROR_STATE` | Unrecoverable FSM error |

**Key public API:**

```cpp
void cgm_setup();                            // call once in setup()
void cgm_tick();                             // call every loop()
void cgm_setPhysicalBoardFEN(const String&); // feed sensor reading each loop

void cgm_createGameNow(bool aiMode);         // create game as white
void cgm_joinGameNow();                      // join game as black
void cgm_confirmPendingMove();               // touch: confirm button
void cgm_cancelPendingMove();                // touch: cancel button
void cgm_selectPromotionPiece(char piece);   // touch: promotion picker
void cgm_requestNewGame();                   // touch: play again
void cgm_loadEdgeCaseFEN(...);               // load a test position
void cgm_setTimerMode(TimerMode);            // set before createGame
void cgm_setAiDifficulty(AiDifficulty);      // set before createGame (AI only)
```

**Timer modes** (`TimerMode` enum): `TIMER_NONE` (unlimited), `TIMER_RAPID` (10 min), `TIMER_BULLET` (5 min). Clocks are tracked entirely on the Arduino; the server only stores the mode and initial budget.

**AI difficulty** (`AiDifficulty` enum): `AI_EASY` (depth 2), `AI_MEDIUM` (depth 5), `AI_HARD` (depth 12).

---

### `gamelogic.cpp` / `gamelogic.h`
**Pure chess move validation — no hardware or network dependencies.**

Validates any move against the full set of chess rules including castling, en passant, promotion, check, checkmate, and stalemate. Takes before/after board FEN strings and returns the validated result FEN or `"Invalid Move"`.

**Key functions:**

```cpp
String validateMoveAndReturnFEN(beforeFEN, afterFEN, whiteToMove, castling, promotionPiece, enPassantSquare);
bool hasAnyLegalMove(board, whiteToMove, castling, enPassantSquare);
bool isKingInCheck(board, whiteKing);
bool isSquareAttacked(board, row, col, byWhite);
bool parseFENBoard(fen, board[8][8]);
bool boardHasExactlyOneKingEach(board[8][8]);
```

Board encoding: uppercase = white pieces (`RNBQKP`), lowercase = black (`rnbqkp`), `.` = empty square.

---

### `ADC_driver.cpp` / `ADC_driver.h`
**ADS7128 I2C ADC driver — reads all 64 Hall effect sensors.**

Eight ADS7128 chips at I2C addresses `0x10`–`0x17` on the `Wire1` bus (GPIO 38/39, 100 kHz). Each chip reads one column of 8 squares.

Physical column-to-chip wiring (file a→h maps to chips): `{7, 6, 5, 4, 0, 1, 2, 3}`.

Piece detection threshold: ±300 counts from a 2048 baseline (12-bit ADC midpoint). N-pole deviation (positive) is encoded as `P` (white); S-pole deviation (negative) as `p` (black); no deviation as `.`.

**Public API:**

```cpp
void initADCs();                                  // Wire1.begin + configure all chips
void readBoardFEN(char* fenOut, bool localIsWhite); // fill 72-byte FEN buffer
ADCTestResult testADCs();                          // full self-test (chip + channel probe)
uint16_t readRawChannel(uint8_t chip, uint8_t ch); // single channel read (debugging)
extern const uint8_t ADC_COL_TO_CHIP[8];          // column-to-chip mapping table
```

The `localIsWhite` flag mirrors the board reading when false, so that the row of sensors nearest the black player maps to rank 8.

---

### `display_driver.cpp` / `display_driver.h`
**All screen rendering — board grid, pieces, panels, keyboard, chat.**

Uses a per-square dirty cache (`s_cachedBoard[8][8]` + 64-bit `s_dirtyCells` bitfield) so only changed squares are redrawn each loop. A separate chrome cache (`s_chromeValid`) avoids re-painting static UI chrome.

**Key functions:**

```cpp
void initDisplay();                  // call once in setup()
void drawGameScreen(...);            // full game screen with board + right panel
void drawGameScreenWithMove(...);    // game screen with before/after move highlights
void drawBoardSyncOverlay(...);      // highlights missing/extra pieces during sync
void drawPromotionPicker(bool);      // promotion piece selection overlay
void drawTimerDisplay(...);          // live clock display in right panel
void drawGameMessagePanel(...);      // live chat panel (last 3 messages)
void drawGameMessageComposer(...);   // full-screen message composer + keyboard
void drawCheckAlert(bool);           // "in check!" banner overlay
void drawGameOverScreen(const char*); // checkmate / stalemate / timeout end screen
void drawWifiListScreen(...);        // WiFi network list
void drawPasswordScreen(...);        // WiFi password entry with on-screen keyboard
void invalidateBoardCache();         // force full board repaint on next draw
```

Primitives available for composing custom screens:
```cpp
void displayClear();
void displayHeader(bool wifiConnected, bool showBack);
void displayCenteredText(text, y, size, color);
void displayButton(x, y, w, h, color, label);
void displayStatusBar(msg, bgColor);
void displayDivider(y, color);
```

---

### `api_connect.cpp` / `api_connect.h`
**All HTTPS communication with the AWS API Gateway backend.**

The certificate for Amazon Root CA 1 (valid until 2038) is embedded directly in `api_connect.cpp`. All functions check WiFi connectivity before attempting a request and return an `ok` flag.

**Endpoints and functions:**

```cpp
GameStateResult fetchGameState();                        // GET  /games/1
ApiResult pushFENState(fen, move, expectedVersion);      // POST /games/1/moves
ApiResult resetGame(timerMode, boardId, gameMode, depth);// POST /games/1/reset
ApiResult sendHeartbeat();                               // POST /games/1/heartbeat
ApiResult notifyTimeout(loserColor);                     // POST /games/1/timeout
ApiResult sendMessage(text);                             // POST /games/1/messages
FetchMessagesResult fetchMessages();                     // GET  /games/1/messages
HintResult fetchBestMove(currentFen);                   // POST /games/1/hint
```

`pushFENState` uses optimistic concurrency: it sends the current `serverVersion` and the server rejects the update if the version has changed (i.e. the opponent moved first).

---

### `wifi_manager.cpp` / `wifi_manager.h`
**WiFi scan and connect utilities.**

```cpp
uint8_t wmScan(ScannedNetwork* out);         // scan up to WM_MAX_SCAN=10 networks
bool wmConnect(const char* ssid, const char* pass); // connect, retries 40×500ms
```

`ScannedNetwork` holds `ssid[33]` and `rssi`. Used by both the auto-connect at boot and the on-screen WiFi picker.

---

### `headers.h`
**Shared pin definitions and RGB565 colour constants.**

Included by all files that need pin numbers or draw to the screen. Includes `secrets.h` so WiFi credentials are available project-wide without extra includes.

All colour constants are individually guarded with `#ifndef` so they do not conflict with definitions in the DFRobot GDL library.

---

### `secrets.h`
**WiFi credentials — not committed to version control.**

```cpp
#pragma once
#define WIFI_SSID "YourNetworkName"
#define WIFI_PASS "YourPassword"
```

This file must be created manually before building. See [Project Setup](#project-setup).

---

## Backend API

Base URL: `https://j3zvk9adv0.execute-api.us-east-2.amazonaws.com/api/v1/games/1`

Full endpoint reference: [`backend/api.md`](../backend/api.md)

The Python backend (`backend/api.py`, located in the parent `ECE-445-Senior-Design-Project/` folder) defines the Lambda handler and DynamoDB schema. Game state is a single record keyed by game ID. Boards identify themselves by MAC address; the server assigns white to the first board that calls `/reset` and black to the second board that registers.

The server does **not** run chess clocks — it only stores the timer mode and initial budget. Both boards track remaining time independently and call `/timeout` when their own clock expires.

---

## Game Flow

```
Power on
  └─ setup(): init display → init ADCs → connect WiFi → show Menu

Menu screen
  ├─ "Join Game"        → poll server until game exists → GAME screen (black)
  ├─ "Create Game"      → choose timer mode → reset server → GAME screen (white)
  ├─ "vs Stockfish AI"  → choose difficulty + timer → reset server → GAME screen (white)
  └─ "ADC Board Test"   → live sensor visualisation screen

GAME screen (FSM ticks every loop)
  ├─ Board sync: physical board must match expected position before play begins
  ├─ Local turn:
  │    detect piece lift → detect piece placed → validate move
  │    → show Confirm / Cancel buttons → player confirms → send to server
  ├─ Remote turn:
  │    poll server → receive opponent FEN → player replicates physically
  └─ Game over: checkmate / stalemate / timeout → result overlay → "Play Again"
```

---

## Testing Utilities

### ADC Board Test (`BOARD_TEST` screen)
Accessible from the main menu. Displays a live 8×8 grid showing the Hall-effect sensor differential for every square in real time. Green = N-pole detected, orange = S-pole detected, grey = empty. Useful for verifying sensor wiring and placement.

### Edge Case Test (`EDGE_CASE_TEST` screen)
Loads pre-defined FEN positions to test special chess rules without playing a full game. Available scenarios:
- Castling (kingside / queenside)
- En passant (white and black)
- Pawn promotion (white and black)

### `testADCs()` (ADC_driver)
Returns an `ADCTestResult` struct with a bitmask of which chips responded on I2C, which chips returned valid data on all 8 channels, and a total valid-channel count. Called from `runBoardTests()` in `ChessBoard.ino`.

### `readRawChannel(chip, ch)` (ADC_driver)
Returns the raw 12-bit ADC value for a single square. Useful when debugging a specific sensor.
