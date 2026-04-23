# ChessBoard

An ESP32-S3 Arduino project that turns a physical chess board into a networked, two-player game system. Magnetic hall-effect sensors detect piece positions, a 320×480 IPS touchscreen drives the UI and game feedback, and a cloud REST API over HTTPS synchronises moves between two boards in real time.

---

## How It Works

### Hardware

| Component | Details |
|---|---|
| Microcontroller | ESP32-S3 |
| Piece detection | 8× ADS7128 I²C ADC chips, one per rank, reading hall-effect sensors under each square |
| Display | 320×480 IPS (DFRobot ST7365P), SPI |
| Touch | GT911 capacitive controller, I²C |
| LEDs | 64× WS2812B NeoPixel strip (wired snake pattern) — present but not used for game events |

### Software Architecture

The firmware is structured around a **Finite State Machine (FSM)** in `gameloop.cpp` that acts as the central game controller. `ChessBoard.ino` owns `setup()` and `loop()` and delegates everything to the FSM.

```
loop()
  │
  ├─ readBoardFEN()          ← ADC driver reads all 64 squares
  ├─ cgm_setPhysicalBoardFEN() ← feeds raw board state into the FSM
  ├─ cgm_tick()              ← advances the FSM
  └─ display update          ← screen redraws driven by FSM state queries
```

### Game Flow

**Creating a game (White)**
1. Tap **Create Game** → FSM resets and starts as white
2. Place pieces in the starting position; the board is validated and ready
3. Make your move, tap **Confirm**, and the FEN is sent to the API
4. FSM polls the API until the opponent's move arrives, then shows it on screen
5. Replicate the opponent's piece move on the physical board to advance to your next turn

**Joining a game (Black)**
1. Tap **Join Game** → FSM polls the API for the current board position
2. If a game is already in progress the current FEN and active turn are inherited; if not, a fresh game is set up waiting for white's first move
3. If it is already your turn, proceed to step 3 of the create-game flow above; otherwise wait for the opponent's move to appear

**Every move**
1. Pick up a piece and place it — ADC reads all 64 squares at ~100 ms intervals
2. Board must be stable for **600 ms** before the move is evaluated
3. Chess rules are checked (legality, castling, en passant, check)
4. **Confirm / Cancel** prompt appears with the move highlighted
5. On confirm the FEN is POSTed (up to 5 retries); on cancel the board is restored

**Game end** — checkmate, stalemate, 50-move rule, or insufficient material triggers a game-over panel. Tap to restart.

---

## File Structure

| File | Purpose |
|---|---|
| `ChessBoard.ino` | `setup()` / `loop()`, touch handling, screen state machine |
| `gameloop.h/.cpp` | Chess game FSM — the main game controller |
| `gamelogic.h/.cpp` | Pure chess rules engine (move validation, check, castling, stalemate) |
| `display_driver.h/.cpp` | All screen drawing functions |
| `ADC_driver.h/.cpp` | ADS7128 I²C ADC driver; reads board FEN |
| `LED_driver.h/.cpp` | WS2812B NeoPixel driver (hardware present; game events use screen only) |
| `api_connect.h/.cpp` | HTTPS REST client for the game server |
| `wifi_driver.h/.cpp` | Boot-time WiFi helper |
| `wifi_manager.h/.cpp` | On-screen WiFi scan / connect UI |
| `headers.h` | Pin definitions and shared colour constants |
| `secrets.h` | `WIFI_SSID` / `WIFI_PASS` — **gitignored, do not commit** |

---

## Pin Definitions (`headers.h`)

| Constant | Pin | Purpose |
|---|---|---|
| `LED_PIN` | 6 | NeoPixel data line |
| `SDA_DAQ` | 38 | I²C SDA for ADC chips |
| `SCL_DAQ` | 39 | I²C SCL for ADC chips |
| `SCR_SCLK` | 45 | Screen SPI clock |
| `SCR_MOSI` | 48 | Screen SPI MOSI |
| `SCR_MISO` | 47 | Screen SPI MISO |
| `SCR_CS` | 21 | Screen SPI chip-select |
| `SCR_RST` | 14 | Screen reset |
| `SCR_DC` | 13 | Screen data/command |
| `SCR_BLK` | 12 | Screen backlight |
| `SCR_I2C_SCL` | 11 | Touchscreen I²C SCL |
| `SCR_I2C_SDA` | 10 | Touchscreen I²C SDA |
| `SCR_INT` | 9 | Touchscreen interrupt |
| `SCR_TCH_RST` | 8 | Touchscreen reset |

---

## Game FSM (`gameloop.h/.cpp`)

The FSM is the heart of the system. `ChessBoard.ino` calls `cgm_tick()` every loop iteration and queries FSM state to decide what to draw.

### States

| State | Description |
|---|---|
| `CGM_WAIT_FOR_GAME_START` | Idle — waiting for a "Create" or "Join" button tap |
| `CGM_JOIN_POLLING` | Fetching the current board position from the server before initialising a joined game |
| `CGM_GAME_INITIALIZATION` | Applies the inherited or fresh starting position; sets whose turn it is |
| `CGM_LOCAL_TURN_WAIT_FOR_BOARD` | Watching the physical board for a stable piece movement |
| `CGM_LOCAL_TURN_VALIDATE` | Validates the detected board change against chess rules |
| `CGM_LOCAL_TURN_CONFIRM` | Paused — waiting for the player to tap **Confirm** or **Cancel** |
| `CGM_SEND_STATE` | POSTing the new FEN to the API (up to 5 retries over 30 s) |
| `CGM_WAIT_FOR_REMOTE_MOVE` | Polling the API every 2 s for the opponent's next FEN |
| `CGM_APPLY_REMOTE_MOVE` | Waiting for the physical board to match the received opponent FEN |
| `CGM_GAME_END` | Game over — tap anywhere to restart |
| `CGM_ERROR_STATE` | Unrecoverable FSM error |

### State Transition Diagram

```
                        ┌─────────────────────┐
               tap      │  WAIT_FOR_GAME_START │ ◄── cgm_resetManager()
          "Create Game"  └──────────┬──────────┘
          ─────────────────────────►│
                                    │ tap "Join Game"
                    ┌───────────────┼──────────────────┐
                    ▼               │                   ▼
        ┌───────────────────┐       │     ┌──────────────────────┐
        │ GAME_INITIALIZATION│◄──── ┘     │    JOIN_POLLING       │
        │  (white, fresh)   │ always      │ (GET /moves → FEN)   │
        └─────────┬─────────┘ next        └──────────┬───────────┘
                  │                                   │
         local turn?                       inherits FEN + turn
         ┌────────┴────────┐
         ▼ yes             ▼ no
┌──────────────────┐   ┌────────────────────┐
│LOCAL_TURN_WAIT_  │   │ WAIT_FOR_REMOTE_   │◄─────────────────────────┐
│  FOR_BOARD       │   │      MOVE          │ (polls API every 2 s)    │
└────────┬─────────┘   └────────┬───────────┘                          │
         │ stable 600 ms              │ new FEN received                │
         ▼                      ▼                                       │
┌──────────────────┐   ┌────────────────────┐                          │
│LOCAL_TURN_       │   │ APPLY_REMOTE_MOVE  │                          │
│  VALIDATE        │   │ (show move on      │                          │
└────────┬─────────┘   │  screen; wait for  │                          │
         │ legal        │  physical board    │                          │
         ▼              │  to match)         │                          │
┌──────────────────┐   └────────┬───────────┘                          │
│LOCAL_TURN_CONFIRM│            │ board matches                         │
│ (tap Confirm /   │            │                                       │
│  Cancel)         │            │ ←── local turn? ──────────────────────┘ no
└────────┬─────────┘            │ yes
         │ Confirm              ▼
         ▼           LOCAL_TURN_WAIT_FOR_BOARD
┌──────────────────┐
│   SEND_STATE     │──────────► WAIT_FOR_REMOTE_MOVE  (after POST succeeds)
│ (POST /moves)    │                    │
└──────────────────┘                    │ checkmate / stalemate / draw
                                        ▼
                               ┌─────────────────┐
                               │    GAME_END      │◄── any terminal result
                               └────────┬────────┘
                                        │ tap to restart
                                        ▼
                               GAME_INITIALIZATION
```

### Step-by-step: Creating a game

1. **Boot** — the board connects to WiFi using credentials from `secrets.h` and shows the main menu.
2. **Tap "Create Game"** — calls `cgm_createGameNow()`:
   - FSM resets, `localIsWhite = true`.
   - Skips `JOIN_POLLING` and goes straight to `GAME_INITIALIZATION`.
3. **Initialisation** — committed FEN set to the standard starting position; white to move; castling rights fully enabled.
4. **Your turn (White)** — FSM enters `LOCAL_TURN_WAIT_FOR_BOARD`. Make a move on the physical board.

### Step-by-step: Joining a game

1. **Boot** — same as above.
2. **Tap "Join Game"** — calls `cgm_joinGameNow()`:
   - FSM resets, `localIsWhite = false`.
   - Enters `JOIN_POLLING`.
3. **Polling the server** — `fetchGameState()` GETs the moves list:
   - If a game is in progress, the latest FEN and whose turn it is are stored.
   - If no game exists yet, defaults to the starting position and white to move.
4. **Initialisation** — `GAME_INITIALIZATION` inherits whatever the poll returned. If white has already moved, the board is shown mid-game and the FSM immediately enters `WAIT_FOR_REMOTE_MOVE` or `LOCAL_TURN_WAIT_FOR_BOARD` depending on whose turn it is.
5. **Status bar** shows `"Your turn (Black)"` or `"Opponent (White) to move"`.

### Step-by-step: Making a local move

1. **FSM is in `LOCAL_TURN_WAIT_FOR_BOARD`.**  
   Pick up a piece and place it on the target square.
2. **Stability check** — the ADC driver reads all 64 squares every ~100 ms. The FSM compares the physical P/p/. board against the expected physical representation of the committed FEN. A change must hold stable for **600 ms** before it is accepted.
3. **Validation (`LOCAL_TURN_VALIDATE`)** — `validateMoveAndReturnFEN()` checks the move is legal (including castling, en passant, promotion, and not leaving own king in check). If illegal the board is rejected and the player must try again.
4. **Confirm prompt (`LOCAL_TURN_CONFIRM`)** — the display highlights the source square (yellow) and destination square (green) and shows **Confirm / Cancel** buttons.
   - **Confirm** → FSM advances to `SEND_STATE`.
   - **Cancel** → FSM returns to `LOCAL_TURN_WAIT_FOR_BOARD`; player must restore the piece.
5. **Send (`SEND_STATE`)** — the new FEN is POSTed to the API. Up to 5 retries over 30 s; if all fail the move is committed locally anyway. After a successful commit the FSM transitions to `WAIT_FOR_REMOTE_MOVE`.

### Step-by-step: Receiving the opponent's move

1. **FSM is in `WAIT_FOR_REMOTE_MOVE`**, polling the API every 2 s.
2. When a FEN arrives that differs from the committed position, the FSM enters `APPLY_REMOTE_MOVE`.
3. **Display** shows the opponent's move highlighted (yellow source, green destination).
4. **Physical replication** — the player moves the piece on the physical board. The FSM checks every `cgm_tick()` whether `readBoardFEN()` matches the expected physical representation of the incoming FEN.
5. Once the physical board matches, castling rights, en passant, and the half-move clock are all updated, then the committed FEN is updated and the FSM returns to `LOCAL_TURN_WAIT_FOR_BOARD` (or `WAIT_FOR_REMOTE_MOVE` again if it's a relay board watching both sides).

### Step-by-step: Game end

- After every committed move the FSM checks for **checkmate**, **stalemate**, **50-move rule**, and **insufficient material**.
- When a terminal condition is found, `cgm_finishGame()` is called, setting state to `CGM_GAME_END`.
- `ChessBoard.ino` detects `cgm_isGameOver() == true` and draws the game-over panel.
- **Tap anywhere** calls `cgm_requestNewGame()`, which restarts from `GAME_INITIALIZATION` with the same colour assignment.

### Key Configuration (`CGMConfig` namespace)

| Constant | Default | Description |
|---|---|---|
| `POLL_INTERVAL_MS` | 2000 | How often to poll the API for opponent moves |
| `LOCAL_STABLE_TIME_MS` | 600 | How long a board position must be stable before it is accepted |
| `WIFI_RETRY_INTERVAL_MS` | 5000 | WiFi reconnect attempt interval |
| `GAME_ID` | 1 | API game identifier |
| `BOARD_NUMBER` | 1 | Physical board number sent to the API |
| `LOCAL_IS_WHITE` | `true` | Set to `false` if this board plays black |
| `DEFAULT_PROMOTION` | `'Q'` | Promotion piece used when no UI prompt is available |

### Control Functions

```cpp
cgm_setup();                  // Call once in setup() after hardware init
cgm_tick();                   // Call every loop() while GAME screen is active
cgm_setPhysicalBoardFEN(fen); // Feed ADC board reading into the FSM
cgm_createGameNow();          // Start a new game as white
cgm_joinGameNow();            // Join an existing game as black (polls server first)
cgm_resetManager();           // Reset FSM to idle
cgm_requestNewGame();         // Restart from game-over state
cgm_confirmPendingMove();     // Confirm the move awaiting approval
cgm_cancelPendingMove();      // Cancel the move awaiting approval
cgm_setPromotionPiece('Q');   // Choose promotion piece
```

### State Query Functions

```cpp
cgm_isConfirming()            // true while waiting for Confirm/Cancel tap
cgm_isWaitingForRemote()      // true while polling for opponent move
cgm_isGameOver()              // true when in GAME_END state
cgm_isLocalPlayerWhite()      // true if local board was assigned white
cgm_isWhiteToMove()           // whose turn it currently is
cgm_isInCheck()               // true if the side to move is in check
cgm_getTurnStatusString()     // e.g. "Your turn (White)" / "Opponent (Black) to move"
cgm_getCommittedFEN()         // last fully accepted board FEN
cgm_getPendingFEN()           // candidate FEN awaiting confirmation
cgm_getIncomingFEN()          // remote move FEN while being applied
cgm_getGameResultString()     // e.g. "White wins by checkmate"
cgm_getPieceLiftSquare(sq)    // detects single piece lift; fills sq e.g. "e2"
```

---

## Display Driver (`display_driver.h/.cpp`)

### Screen Layout

```
y=0  ┌─────────────────────────────┐
     │  Header: title + WiFi status │  38 px
y=38 ├─────────────────────────────┤
     │                             │
     │   Main content area         │
     │   (board grid / menus)      │
     │                             │
y=458├─────────────────────────────┤
     │  Status bar                 │  22 px
y=480└─────────────────────────────┘
```

### Game Screen Variants

#### `drawGameScreen(wifiConnected, fenOk, data)`
Base board view. `fenOk=true` renders `data` as a FEN character grid; `fenOk=false` shows `data` as a message.

#### `drawGameScreenWithMove(wifiConnected, beforeFEN, afterFEN)`
Same as above but highlights the move:
- **Yellow** background on the source square (piece lifted from here)
- **Green** background on the destination square (piece placed here)

Called automatically by `ChessBoard.ino` whenever a local or remote move changes the board.

#### `drawCheckAlert(whiteInCheck)`
Draws a **yellow banner** between the header and the board reading `!! WHITE IS IN CHECK !!` or `!! BLACK IS IN CHECK !!`. Overlaid without redrawing the full board.

#### `drawGameOverScreen(resultLine)`
Full black-background panel shown when the game ends:
- Red banner for checkmate, grey for draws/stalemate
- Large **GAME OVER** heading
- Result text centred on screen (e.g. "White wins by checkmate")
- Status bar: "Tap to play again"

Tapping anywhere restarts the game.

#### `drawPiecePickedUp(squareName)`
**Orange banner** shown the instant the ADC detects a piece lifted off the board (e.g. `Piece lifted: e2`). Clears automatically when the piece is placed back down or on a new square.

### Other Screens

| Function | Description |
|---|---|
| `drawMenuScreen(wifiConnected)` | Main menu background; add buttons with `displayButton()` |
| `drawConnectingScreen(ssid)` | WiFi connecting splash |
| `drawErrorScreen(title, detail)` | Full-screen red error panel |
| `drawDebugScreen(lines, count)` | Black terminal-style diagnostic screen |
| `drawWifiListScreen(nets, count, scanning)` | Scanned network list |
| `drawPasswordScreen(...)` | On-screen QWERTY password entry |

### Primitives

| Function | Description |
|---|---|
| `displayClear()` | Fill screen white |
| `displayHeader(wifiConnected)` | Title bar + WiFi status + divider |
| `displayButton(x, y, w, h, color, label)` | Filled labelled button |
| `displayStatusBar(msg, bgColor)` | 22 px bar at bottom of screen |
| `displayCenteredText(text, y, size, color)` | Horizontally centred text |
| `displayDivider(y, color)` | Full-width horizontal line |

---

## ADC Driver (`ADC_driver.h/.cpp`)

Drives 8× **ADS7128** I²C ADC chips (one per rank, addresses `0x10`–`0x17`). Each chip reads 8 hall-effect sensor channels, giving 64 measurements total.

```cpp
initADCs();           // Call once in setup()
calibrateBaselines(); // Call with NO pieces on board; stores resting values
readBoardFEN(fenOut); // Reads all 64 squares, writes Modified FEN into fenOut (72 bytes)
```

**Modified FEN** (output of `readBoardFEN`):

| Char | Meaning |
|---|---|
| `P` | N-pole magnet detected |
| `p` | S-pole magnet detected |
| `1`–`8` | Run of empty squares |
| `/` | Rank separator |

Detection threshold: ±300 ADC counts from baseline.

---

## API Connect (`api_connect.h/.cpp`)

HTTPS REST client. TLS is validated with the Amazon Root CA 1 certificate bundled in the source (valid until 2038-01-17).

**Endpoint:** `https://j3zvk9adv0.execute-api.us-east-2.amazonaws.com/api/v1/games/1/moves`

```cpp
struct ApiResult { bool ok; String data; };

ApiResult fetchLatestFEN();
// GET — returns FEN from the last entry in the `moves` array

ApiResult pushFENState(fen, isWhite, gameId, boardNum);
// POST — sends { game_id, board_number, fen, player_color, mac_address }
```

---

## Game Logic (`gamelogic.h/.cpp`)

Pure C++ chess rules engine. Stateless — all functions take boards/arrays by pointer.

**Board representation:** `char[8][8]`, row 0 = rank 8, col 0 = file a, `'.'` = empty, uppercase = white, lowercase = black.

**Castling rights:** `bool castling[4]` — indices 0=white K-side, 1=white Q-side, 2=black K-side, 3=black Q-side.

### Main Entry Point

```cpp
String validateMoveAndReturnFEN(
    beforeFEN, afterFEN, whiteToMove, castling, promotionPiece);
// Returns afterFEN if the move is legal, "Invalid Move" otherwise.
// Handles ordinary moves, pawn promotion, and all four castling variants.
```

### Other Key Functions

```cpp
parseFENBoard(fen, board)         // Parse FEN string into char[8][8]
isKingInCheck(board, whiteKing)  // Check detection
hasAnyLegalMove(board, white, castling) // Checkmate/stalemate detection
canCastle(board, white, kingSide, castling)
```

---

## WiFi Manager (`wifi_manager.h/.cpp`)

Stores up to **5 networks** in ESP32 NVS. Credentials are never hardcoded.

```cpp
wmConnectBoot()                     // Try all saved networks; returns true on success
wmConnect(ssid, pass)               // Connect to a specific network
wmScan(nets)                        // Scan nearby networks (~2–3 s blocking)
wmSaveNetwork(ssid, pass)           // Persist credentials to NVS
wmGetSavedPass(ssid, passOut)       // Retrieve saved password
```

Boot sequence in `setup()`:
1. Try all NVS-saved networks (`wmConnectBoot`)
2. Fall back to `secrets.h` credentials; save on success
3. If still unconnected, show the WiFi scan/selection screen

---

## LED Driver (`LED_driver.h/.cpp`)

Controls a 64-pixel WS2812B strip. The hardware is present and initialised but **game events (check, win, move) are shown on the touchscreen only**.

```cpp
initLEDs();       // Initialise strip, brightness 40/255
demoSequence();   // Startup wiring test (even then odd pixels)
testLEDs();       // Flash all green ~800 ms; returns pixel count
clearLEDs();      // Clear buffer (call showLEDs() to push)
showLEDs();       // Push buffer to strip
```

---

## Secrets

Create `secrets.h` in the project root (it is gitignored):

```cpp
#ifndef SECRETS_H
#define SECRETS_H
#define WIFI_SSID "your_network_name"
#define WIFI_PASS "your_password"
#endif
```

Alternatively, leave the file with placeholder values and use the on-screen WiFi settings menu to connect — credentials will be saved to NVS automatically.


---

## Table of Contents

1. [Pin Definitions](#pin-definitions)
2. [ADC Driver](#adc-driver)
3. [LED Driver](#led-driver)
4. [Display Driver](#display-driver)
5. [WiFi Driver](#wifi-driver)
6. [WiFi Manager](#wifi-manager)
7. [API Connect](#api-connect)
8. [Game Logic](#game-logic)

---

## Pin Definitions

Defined in `headers.h`.

| Constant | Pin | Purpose |
|---|---|---|
| `LED_PIN` | 6 | NeoPixel data line |
| `SDA_DAQ` | 38 | I²C SDA for ADC chips |
| `SCL_DAQ` | 39 | I²C SCL for ADC chips |
| `SCR_SCLK` | 45 | Screen SPI clock |
| `SCR_MOSI` | 48 | Screen SPI MOSI |
| `SCR_MISO` | 47 | Screen SPI MISO |
| `SCR_CS` | 21 | Screen SPI chip-select |
| `SCR_RST` | 14 | Screen reset |
| `SCR_DC` | 13 | Screen data/command |
| `SCR_BLK` | 12 | Screen backlight |
| `SCR_I2C_SCL` | 11 | Touchscreen I²C SCL |
| `SCR_I2C_SDA` | 10 | Touchscreen I²C SDA |
| `SCR_INT` | 9 | Touchscreen interrupt |
| `SCR_TCH_RST` | 8 | Touchscreen reset |

`NUM_LEDS` is `64` (8×8 board).

---

## ADC Driver

**Files:** `ADC_driver.h` / `ADC_driver.cpp`

Drives 8× **ADS7128** I²C ADC chips (addresses `0x10`–`0x17`), one per board rank. Each chip reads 8 channels — one per file — giving 64 hall-effect measurements total. The threshold for registering a piece is ±300 ADC counts from the calibrated baseline.

### Setup

```cpp
initADCs();
```

Call once in `setup()`. Initialises the I²C bus on `SDA_DAQ`/`SCL_DAQ` at 100 kHz and puts every chip into manual channel-select mode.

---

### `calibrateBaselines()`

Samples every channel 20 times and stores the average as the resting baseline. Call with **no pieces on the board**.

```cpp
calibrateBaselines();   // prints per-channel values over Serial
```

Baselines are stored in a static array inside `ADC_driver.cpp` and persist until the next call or power cycle.

---

### `readBoardFEN(char *fenOut)`

Reads all 64 squares, drives the LED strip to reflect polarity (see LED Driver), and writes a **Modified FEN** string into `fenOut`.

```cpp
char fen[72];
readBoardFEN(fen);
```

`fenOut` must be at least 72 bytes.

**Modified FEN encoding:**

| Character | Meaning |
|---|---|
| `P` | N-pole magnet present (LED = red) |
| `p` | S-pole magnet present (LED = white) |
| `1`–`8` | Run of empty squares |
| `/` | Rank separator |

Row 0 of the FEN is rank 8 (the far side from white), column 0 is file a.

---

### `testADCs()` → `ADCTestResult`

Probes all 8 chips and reads every channel. Returns a struct:

```cpp
ADCTestResult r = testADCs();
// r.chipMask   — bit N set if chip N responded on I²C
// r.chanMask   — bit N set if all 8 channels on chip N returned valid data
// r.totalValid — total channels (0–64) that did not return 0xFFFF
```

---

## LED Driver

**Files:** `LED_driver.h` / `LED_driver.cpp`

Controls a **64-pixel WS2812B NeoPixel** strip wired in a snake pattern under the board. Brightness is initialised to 40/255.

### Setup

```cpp
initLEDs();
```

Call once in `setup()`. Begins the strip, sets brightness to 40, clears all pixels, and pushes the update.

---

### `lightFEN(const char *fen)`

Parses a standard FEN board string and sets each pixel to the colour for its piece. Calls `strip.show()` internally.

```cpp
lightFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR");
```

**Piece colours:**

| Piece | Color |
|---|---|
| White pawn `P` | White (255, 255, 255) |
| White rook `R` | Red (255, 0, 0) |
| White knight `N` | Blue (0, 0, 255) |
| White bishop `B` | Green (0, 255, 0) |
| White queen `Q` | Magenta (255, 0, 255) |
| White king `K` | Yellow (255, 255, 0) |
| Black pawn `p` | Gray (120, 120, 120) |
| Black rook `r` | Dark red (120, 0, 0) |
| Black knight `n` | Cyan (0, 120, 120) |
| Black bishop `b` | Dark green (0, 120, 0) |
| Black queen `q` | Purple (120, 0, 120) |
| Black king `k` | Orange (255, 140, 0) |
| Empty `.` | Off |

---

### `clearLEDs()`

Clears the pixel buffer. Does **not** push to the strip; call `showLEDs()` afterwards if you need an immediate update.

---

### `showLEDs()`

Pushes the current pixel buffer to the strip (calls `strip.show()`).

---

### `testLEDs()` → `int`

Flashes all 64 pixels dim green for ~800 ms, then clears. Returns the pixel count (always 64).

```cpp
int n = testLEDs();
```

---

### `demoSequence()`

Sequentially lights even-indexed pixels green, turns them off, then does the same for odd-indexed pixels. Useful for wiring verification.

---

## Display Driver

**Files:** `display_driver.h` / `display_driver.cpp`

Drives a **320×480 IPS screen** (DFRobot ST7365P) over SPI and a **GT911 capacitive touch controller** over I²C. Provides two API layers: low-level primitives and pre-built full-screen layouts.

### Setup

```cpp
initDisplay();
```

Call once in `setup()` before drawing anything. Calls `screen.begin()` and disables text wrap.

---

### Primitives

#### `displayClear()`

Fills the entire screen white.

```cpp
displayClear();
```

---

#### `displayHeader(bool wifiConnected)`

Draws the standard top bar: "ChessBoard" title on the left, "WiFi: OK" / "WiFi: --" on the right, and a full-width divider line at y = 38.

```cpp
displayHeader(true);   // shows "WiFi: OK"
displayHeader(false);  // shows "WiFi: --"
```

---

#### `displayCenteredText(const char *text, int y, uint8_t size, uint16_t color)`

Prints text horizontally centered at the given y pixel position.

| Parameter | Description |
|---|---|
| `text` | Null-terminated string |
| `y` | Top-of-text y coordinate in pixels |
| `size` | Scale factor (1 = 6×8 px/char, 2 = 12×16 px/char, …) |
| `color` | RGB565 constant, e.g. `COLOR_RGB565_BLACK` |

---

#### `displayButton(int x, int y, int w, int h, uint16_t color, const char *label)`

Draws a filled rectangle with a centered white label. Use for interactive buttons in `ChessBoard.ino`.

```cpp
displayButton(30, 160, 260, 70, COLOR_RGB565_BLUE, "Join Game");
```

---

#### `displayStatusBar(const char *msg, uint16_t bgColor)`

Draws a 22-pixel status bar at the very bottom of the screen.

```cpp
displayStatusBar("Connecting...", COLOR_RGB565_RED);
```

---

#### `displayDivider(int y, uint16_t color)`

Draws a full-width 1-pixel horizontal line.

```cpp
displayDivider(38, COLOR_RGB565_BLACK);
```

---

### Full-Screen Layouts

#### `drawConnectingScreen(const char *ssid)`

Splash shown while connecting to WiFi. Displays the SSID name.

```cpp
drawConnectingScreen("MyNetwork");
```

---

#### `drawMenuScreen(bool wifiConnected)`

Draws the main menu background (header + title). Add buttons separately with `displayButton()`.

```cpp
drawMenuScreen(wifiConnected);
displayButton(30, 160, 260, 70, COLOR_RGB565_BLUE, "Join Game");
```

---

#### `drawGameScreen(bool wifiConnected, bool fenOk, const String &data)`

Game screen. When `fenOk` is true, `data` is treated as a FEN string and rendered as a board grid. When `fenOk` is false, `data` is shown as an error message.

```cpp
drawGameScreen(true, true, "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR");
drawGameScreen(true, false, "Request failed");
```

---

#### `drawErrorScreen(const char *title, const char *detail)`

Full-screen red banner error with bold title and wrapped detail text.

```cpp
drawErrorScreen("Connection Error", "Could not reach the API server.");
```

---

#### `drawDebugScreen(const char *const lines[], uint8_t count)`

Black terminal-style screen. Pass an array of C-strings.

```cpp
const char *dbg[] = { "ADC chips: 8/8", "LEDs: 64", "WiFi: OK" };
drawDebugScreen(dbg, 3);
```

---

### WiFi UI Screens

#### `drawWifiListScreen(const ScannedNetwork *nets, uint8_t count, bool scanning)`

Network list screen. Pass `(nullptr, 0, true)` to show a "Scanning…" placeholder while `wmScan()` runs.

```cpp
drawWifiListScreen(nullptr, 0, true);            // placeholder
drawWifiListScreen(scannedNets, count, false);   // results
```

---

#### `drawPasswordScreen(const char *ssid, const char *password, bool showChars, bool shifted, bool symbols)`

Full password-entry screen with an on-screen QWERTY keyboard.

---

#### `drawPasswordField(const char *password, bool showChars)`

Partial update — redraws only the password field. Call after every keypress to avoid redrawing the entire screen.

---

#### `drawKeyboard(bool shifted, bool symbols)`

Redraws only the keyboard area, e.g. after a shift or symbol-page toggle.

---

#### `keyboardHitTest(int tx, int ty, bool symbols)` → `char`

Maps a touch coordinate to a keyboard action.

| Return value | Meaning |
|---|---|
| Printable char | Key character to append |
| `'\b'` | Backspace / delete |
| `'\n'` | Done / confirm |
| `'\t'` | Toggle shift |
| `0x01` | Toggle symbol page |
| `0` | No hit |

---

### Keyboard Layout Constants

Exposed in `display_driver.h` for touch hit-testing in `ChessBoard.ino`.

| Constant | Value | Description |
|---|---|---|
| `WIFLIST_ROW_Y_START` | 75 | Y of first network row |
| `WIFLIST_ROW_H` | 50 | Height of each network row |
| `KB_ROW1_Y` – `KB_ROW4_Y` | 116 / 162 / 208 / 254 | Top-y of each keyboard row |
| `KB_KEY_H` | 44 | Key height in pixels |
| `KB_STD_W` | 28 | Standard key width |

---

## WiFi Driver

**Files:** `wifi_driver.h` / `wifi_driver.cpp`

Thin wrapper used during boot.

### `connectWifi()` → `bool`

Attempts to connect using all networks saved in NVS (delegates to `wmConnectBoot()`). Returns `true` on success.

```cpp
bool ok = connectWifi();
```

---

## WiFi Manager

**Files:** `wifi_manager.h` / `wifi_manager.cpp`

Manages up to **5 saved networks** in ESP32 NVS (non-volatile storage) and provides scan/connect helpers. Credentials are never hardcoded; `secrets.h` is gitignored.

### Constants

| Constant | Value | Meaning |
|---|---|---|
| `WM_MAX_SAVED` | 5 | Max stored networks |
| `WM_MAX_SCAN` | 10 | Max scan results |
| `WM_SSID_LEN` | 33 | SSID buffer size (including null) |
| `WM_PASS_LEN` | 65 | Password buffer size (including null) |

### Structs

```cpp
struct ScannedNetwork {
    char ssid[33];
    int8_t rssi;
    bool saved;   // true if a password is stored in NVS for this SSID
};

struct SavedNetwork {
    char ssid[33];
    char pass[65];
};
```

---

### `wmLoadSaved(SavedNetwork *out, uint8_t maxCount)` → `uint8_t`

Loads all saved credentials from NVS into `out[]`. Returns the count.

```cpp
SavedNetwork saved[WM_MAX_SAVED];
uint8_t n = wmLoadSaved(saved, WM_MAX_SAVED);
```

---

### `wmSaveNetwork(const char *ssid, const char *pass)`

Persists a network. Updates an existing entry for the same SSID, or evicts the oldest entry when the store is full.

```cpp
wmSaveNetwork("MyNetwork", "hunter2");
```

---

### `wmGetSavedPass(const char *ssid, char passOut[WM_PASS_LEN])` → `bool`

Retrieves the saved password for an SSID. Returns `false` if not found.

```cpp
char pass[WM_PASS_LEN];
if (wmGetSavedPass("MyNetwork", pass)) { /* use pass */ }
```

---

### `wmScan(ScannedNetwork *out)` → `uint8_t`

Blocks for ~2–3 seconds, scans for nearby networks, and returns the count (capped at `WM_MAX_SCAN`). Each result has its `saved` flag set if a password exists in NVS.

```cpp
ScannedNetwork nets[WM_MAX_SCAN];
uint8_t count = wmScan(nets);
```

---

### `wmConnect(const char *ssid, const char *pass)` → `bool`

Connects to a specific SSID. Returns `true` on success.

```cpp
bool ok = wmConnect("MyNetwork", "hunter2");
```

---

### `wmConnectBoot()` → `bool`

Tries every saved network in NVS order. Returns `true` as soon as one succeeds. Used by `connectWifi()` during boot.

```cpp
bool ok = wmConnectBoot();
```

---

## API Connect

**Files:** `api_connect.h` / `api_connect.cpp`

HTTPS REST client targeting an AWS API Gateway endpoint. TLS is validated with the Amazon Root CA 1 certificate bundled in the source (valid until 2038-01-17).

Endpoint: `POST/GET https://j3zvk9adv0.execute-api.us-east-2.amazonaws.com/api/v1/games/1/moves`

### Struct

```cpp
struct ApiResult {
    bool ok;
    String data;   // FEN string on success, error message on failure
};
```

---

### `fetchLatestFEN()` → `ApiResult`

Issues a GET request and returns the FEN from the last entry in the `moves` array.

```cpp
ApiResult r = fetchLatestFEN();
if (r.ok) {
    Serial.println(r.data);  // FEN string
} else {
    Serial.println(r.data);  // error description
}
```

Possible failure strings: `"WiFi not connected"`, `"Request failed"`, `"JSON parse failed"`, `"No moves found"`, `"No fen field found"`.

---

### `pushLatestFEN(const String &move, const String &fen)` → `ApiResult`

Issues a POST with `{ "move": "…", "fen": "…" }` JSON body. Returns `ok = true` on a successful HTTP response.

```cpp
ApiResult r = pushLatestFEN("e2e4", "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR");
```

---

## Game Logic

**Files:** `gamelogic.h` / `gamelogic.cpp`

Pure C++ chess rules engine with no external dependencies. All functions are stateless — pass boards by value/pointer.

Boards are represented as `char[8][8]`. Row 0 = rank 8, column 0 = file a. Empty squares are `'.'`. White pieces are uppercase (`K R Q B N P`), black are lowercase (`k r q b n p`).

Castling rights are tracked as `bool castling[4]`:

| Index | Right |
|---|---|
| `[0]` | White king-side |
| `[1]` | White queen-side |
| `[2]` | Black king-side |
| `[3]` | Black queen-side |

---

### Piece Helpers

#### `isWhitePiece(char p)` → `bool`
Returns true if `p` is `'A'`–`'Z'`.

#### `isPiece(char p)` → `bool`
Returns true if `p` is not `'.'`.

#### `sameColor(char a, char b)` → `bool`
Returns true if both are pieces of the same color.

#### `isValidPromotionPiece(char p)` → `bool`
Returns true if `tolower(p)` is `q`, `r`, `b`, or `n`.

#### `normalizePromotionPiece(char promotionPiece, bool white)` → `char`
Returns the promotion piece in the correct case for the moving side.

#### `copyBoard(char src[8][8], char dst[8][8])`
Deep-copies one board into another.

---

### FEN Parsing

#### `parseFENBoard(const String &fen, char board[8][8])` → `bool`

Parses the board portion of a FEN string. Accepts both a bare board string and a full FEN (space-separated fields). Returns `false` if the string is malformed.

```cpp
char board[8][8];
bool ok = parseFENBoard("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR", board);
```

---

### Board Validation

#### `boardHasExactlyOneKingEach(char board[8][8])` → `bool`
Returns true only if the board contains exactly one `K` and one `k`.

#### `findKing(char board[8][8], bool whiteKing, int &kingRow, int &kingCol)` → `bool`
Locates the king. Returns false if not found.

#### `sameBoard(char a[8][8], char b[8][8])` → `bool`
Returns true if both boards are identical.

---

### Movement Helpers

#### `clearStraight(board, r1, c1, r2, c2)` → `bool`
Returns true if all squares between the two endpoints on the same rank or file are empty.

#### `clearDiagonal(board, r1, c1, r2, c2)` → `bool`
Returns true if all squares on the diagonal between the two endpoints are empty.

#### `validPawnMove(board, r1, c1, r2, c2, piece)` → `bool`
Checks standard pawn movement and captures. Does not handle promotion or en passant.

#### `validKnightMove(r1, c1, r2, c2)` → `bool`
Returns true for an L-shaped knight jump.

#### `validKingMove(r1, c1, r2, c2)` → `bool`
Returns true for a one-square king step (not castling).

#### `applyMove(board, r1, c1, r2, c2)`
Moves the piece at `(r1,c1)` to `(r2,c2)` in place.

#### `applyPromotion(board, r1, c1, r2, c2, promotionPiece)`
Moves a pawn and replaces it with `promotionPiece`.

---

### Attack & Check Helpers

#### `pawnAttacksSquare(board, r, c, targetRow, targetCol)` → `bool`
Returns true if the pawn on `(r,c)` attacks the target square.

#### `pieceAttacksSquare(board, r, c, targetRow, targetCol)` → `bool`
Returns true if the piece on `(r,c)` attacks the target square (any piece type).

#### `isSquareAttacked(board, targetRow, targetCol, byWhite)` → `bool`
Returns true if any piece of the given color attacks the target square.

#### `isKingInCheck(board, whiteKing)` → `bool`
Returns true if the specified king is currently in check.

---

### Castling Helpers

#### `castleFlagIndex(bool white, bool kingSide)` → `int`
Returns the index into the `castling[4]` array for the given side and direction.

#### `canCastle(board, white, kingSide, castling)` → `bool`
Returns true if the specified castle is legal: flag set, king and rook on original squares, path clear, king not moving through check.

#### `applyCastle(board, white, kingSide)`
Moves the king and rook for the specified castle in place.

---

### Move Legality

#### `basicMoveIsLegal(board, r1, c1, r2, c2)` → `bool`
Checks movement rules for the piece at `(r1,c1)`. Does **not** check for leaving the king in check, castling, or promotion legality.

#### `updateCastlingFlags(before, r1, c1, r2, c2, movedPiece, capturedPiece, oldFlags, newFlags)`
Computes updated castling rights after a move — revokes rights when kings or rooks move or are captured.

#### `hasAnyLegalMove(board, whiteToMove, castling)` → `bool`
Returns true if the side to move has at least one fully legal move (including promotions and castling). Used to detect checkmate and stalemate.

---

### Main Validator

#### `validateMoveAndReturnFEN(beforeFEN, afterFEN, whiteToMove, castling, promotionPiece)` → `String`

The top-level validator. Pass the board FEN before and after a physical move. Returns `afterFEN` if the move is legal, or `"Invalid Move"` otherwise.

```cpp
bool castling[4] = { true, true, true, true };
String result = validateMoveAndReturnFEN(
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR",
    "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR",
    true,        // white to move
    castling,
    '\0'         // no promotion
);
// result == afterFEN  →  legal move
// result == "Invalid Move"  →  illegal
```

Handles ordinary moves, pawn promotion (pass the promotion piece character, e.g. `'Q'`), and all four castling variants. After validation it also internally computes whether the opponent is in check, checkmate, or stalemate.

```cpp
displayCenteredText("Game Over", 200, 3, COLOR_RGB565_RED);
```

---

### `displayButton(int x, int y, int w, int h, uint16_t color, const char* label)`
Draws a filled colored rectangle with white centered label text (size 2).

| Parameter | Description |
|---|---|
| `x`, `y` | Top-left corner |
| `w`, `h` | Width and height in pixels |
| `color` | Button fill color |
| `label` | Button text |

```cpp
displayButton(30, 200, 260, 80, COLOR_RGB565_BLUE, "Join Game");
```

---

### `displayStatusBar(const char* msg, uint16_t bgColor)`
Draws a 22px colored bar at the very bottom of the screen (y=458) with white text.

```cpp
displayStatusBar("Connected", COLOR_RGB565_GREEN);
displayStatusBar("Error — tap to retry", COLOR_RGB565_RED);
```

---

### `displayDivider(int y, uint16_t color)`
Draws a full-width (320px) horizontal line at the given y position.

```cpp
displayDivider(100, COLOR_RGB565_BLACK);
```

---

## Pre-built Screens

Ready-to-use full screen layouts. Each one calls `displayClear()` internally.

---

### `drawConnectingScreen(const char* ssid)`
Shown while connecting to WiFi. Displays the board title, a "Connecting..." message, the SSID name, and a blue status bar.

```cpp
drawConnectingScreen(WIFI_SSID);
```

---

### `drawMenuScreen(bool wifiConnected)`
Draws the main menu background — header bar + centered "Chess Board" title. Call `displayButton()` after to add your own buttons.

```cpp
drawMenuScreen(true);
displayButton(30, 200, 260, 80, COLOR_RGB565_BLUE, "Join Game");
displayButton(30, 310, 260, 80, COLOR_RGB565_GREEN, "Create Game");
```

---

### `drawGameScreen(bool wifiConnected, bool fenOk, const String& data)`
Draws the live board view. When `fenOk` is `true`, `data` is rendered as a FEN character grid. When `false`, `data` is shown as a plain error string. Status bar is green on success, red on failure.

```cpp
// Success
drawGameScreen(true, true, "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR");

// Error
drawGameScreen(true, false, "Request failed");
```

---

### `drawErrorScreen(const char* title, const char* detail)`
Full-screen error layout: red banner with "! ERROR", bold title, word-wrapped detail text, and a red "Tap anywhere to dismiss" status bar.

```cpp
drawErrorScreen("Connection Lost", "Could not reach the game server. Check WiFi and try again.");
```

---

### `drawDebugScreen(const char* const lines[], uint8_t count)`
Black terminal-style screen with a green header bar and up to 30 lines of green monospace text. Useful for printing Serial-style diagnostics to the display.

```cpp
const char* dbg[] = {
  "FEN: rnbqkbnr/pp...",
  "HTTP: 200",
  "Free heap: 142KB",
  "Poll interval: 5000ms"
};
drawDebugScreen(dbg, 4);
```
