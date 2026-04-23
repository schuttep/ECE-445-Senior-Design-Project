# ChessBoard — Game Loop & Interaction Reference

This document covers exactly how the firmware runs frame-to-frame, how every touch event is handled, and what must be instantiated for the system to compile and run.

---

## Instantiation Checklist

The following objects and buffers are declared globally in `ChessBoard.ino` and must exist before any other code runs.

### Hardware objects (`ChessBoard.ino` top-level)

```cpp
DFRobot_Touch_GT911_IPS touch(0x5D, SCR_TCH_RST, SCR_INT);
DFRobot_ST7365P_320x480_HW_SPI screen(SCR_DC, SCR_CS, SCR_RST, SCR_BLK);
```

Both are declared globally so that `display_driver.cpp` and `ChessBoard.ino` share the same instances via `extern` linkage.

### `setup()` call order

```
Serial0.begin(115200)
setupDisplayHardware()   ← SPI.begin, Wire.begin, touch.begin
initDisplay()            ← screen.begin, setRotation(1), fillScreen
initLEDs()               ← NeoPixel.begin
initADCs()               ← Wire1.begin(38,39); probe all 8 ADS7128 chips
calibrateBaselines()     ← reads all 64 channels; stores baseline[8][8]
cgm_setup()              ← cgm_resetManager(); FSM enters WAIT_FOR_GAME_START
connectWifi()            ← uses WIFI_SSID / WIFI_PASS from secrets.h
demoSequence()           ← LED startup animation
showMenuScreen()
```

If `connectWifi()` returns `false`, the flow is:
```
displayStatusBar("No network found...")
demoSequence()
showWifiListScreen()     ← user must pick a network before reaching the menu
```

### `secrets.h` (must exist, is gitignored)

```cpp
#define WIFI_SSID "your-network-name"
#define WIFI_PASS "your-password"
```

Used by `connectWifi()` at boot and by `cgm_connectWiFi()` for background reconnects.

---

## Screen State Machine

`currentScreen` is the top-level routing variable. It takes one of five values:

| Value | Active screen |
|---|---|
| `MENU` | Main menu (Join / Create / WiFi Settings) |
| `GAME` | Live chess game |
| `BOARD_TEST` | ADC hardware test grid |
| `WIFI_LIST` | Scanned network list |
| `WIFI_PASS_SCREEN` | On-screen keyboard for password entry |

The `loop()` and `handleTouch()` functions both branch on `currentScreen`. Only the `GAME` and `BOARD_TEST` branches do anything on every frame — all other screens are purely reactive to touch.

---

## `loop()` Frame Breakdown

Every call to `loop()` runs in this order:

```
loop()
 │
 ├─ handleTouch()                 ← check GT911, route one tap per press
 │
 ├─ if (currentScreen == GAME)
 │    │
 │    ├─ 1. readBoardFEN(buf)               ADC: read 64 squares → P/p/. FEN
 │    ├─    cgm_setPhysicalBoardFEN(buf)     feed physical state into FSM
 │    │
 │    ├─ 2. cgm_tick()                      advance FSM one step
 │    │
 │    ├─ 3. Game-over guard
 │    │      cgm_isGameOver() → drawGameOverScreen()  [drawn once, frame returns]
 │    │
 │    ├─ 4. Piece-lift detection
 │    │      cgm_getPieceLiftSquare(sq)
 │    │      → tracks which square the piece left; used by step 6
 │    │
 │    ├─ 5. Board redraw (only when FEN changes)
 │    │      committed / incoming / pending FEN compared to last-rendered values
 │    │      incoming FEN set  → drawGameScreenWithMove(committed, incoming)
 │    │      pending FEN set   → drawGameScreenWithMove(committed, pending)
 │    │      otherwise         → drawGameScreen(committed)
 │    │
 │    ├─ 5b. Status bar turn label
 │    │      cgm_getTurnStatusString() → displayStatusBar()  [only when changed]
 │    │
 │    ├─ 6. Piece-lift overlay
 │    │      lift just detected  → drawPiecePickedUp(square)
 │    │      piece placed back   → drawGameScreen() to clear the banner
 │    │
 │    ├─ 7. Check alert banner
 │    │      cgm_isInCheck() just became true  → drawCheckAlert(whiteToMove)
 │    │      check just cleared                → drawGameScreen() to remove banner
 │    │
 │    └─ 8. Confirm/Cancel overlay
 │           cgm_isConfirming() just became true  → drawConfirmOverlay()
 │           confirm/cancel resolved              → drawGameScreen()
 │
 └─ if (currentScreen == BOARD_TEST)
       every 100 ms → drawBoardTestLive()
```

**Key principle**: the display is never redrawn unless something changed. Every overlay (check, lift, confirm) compares a `gs_last*` bool/string against the current FSM state and only draws when a transition is detected.

---

## Touch Routing (`handleTouch`)

Touch is polled every frame via `touch.scan()`. A tap is acted on only **once per press** — the `lastTouched` flag prevents repeats until the finger is lifted.

### Menu screen

| Region | Action |
|---|---|
| Join Game button (x 50–430, y 75–130) | `cgm_joinGameNow()` → `showGameScreen()` |
| Create Game button (x 50–430, y 145–200) | `cgm_createGameNow()` → `showGameScreen()` |
| WiFi Settings button (x 50–430, y 215–270) | `showWifiListScreen()` |

A 500 ms debounce (`menuShownAt`) prevents the tap that opened the menu from immediately triggering a button.

### Game screen

| Region / Condition | Action |
|---|---|
| Top-left header (x < 80, y < 38) | `cgm_resetManager()` → `showMenuScreen()` |
| Any tap while game-over is shown | `cgm_requestNewGame()` → `showGameScreen()` |
| Confirm button (x 272–464, y 200–244) — only while `cgm_isConfirming()` | `cgm_confirmPendingMove()` |
| Cancel button (x 272–464, y 252–296) — only while `cgm_isConfirming()` | `cgm_cancelPendingMove()` |

### Board test screen

Any tap → `showMenuScreen()`.

### WiFi list screen

| Region | Action |
|---|---|
| Top-left (y < 38, x < 80) | `showMenuScreen()` (Back) |
| Top-right (y < 38, x > 360) | `showWifiListScreen()` (Rescan) |
| Network row (y ≥ `WIFLIST_ROW_Y_START`) | `showWifiPassScreen(ssid)` |

### WiFi password screen

| Region / Key | Action |
|---|---|
| Top-left (y < 38, x < 80) | `showWifiListScreen()` (Back) |
| Show/Hide toggle (x 396–474, y 72–110) | Toggle `kbShowChars`; redraw password field |
| Shift (`\t`) | Toggle `kbShifted`; redraw keyboard |
| Symbols (`0x01`) | Toggle `kbSymbols`; redraw keyboard |
| Backspace (`\b`) | Remove last char; redraw password field |
| Done (`\n`) | Call `wmConnect(ssid, pass)` → update `wifiConnected`; return to menu |
| Any printable char | Append to `passwordBuf`; one-shot shift if active |

---

## FSM ↔ Display Contract

`ChessBoard.ino` owns the screen. `gameloop.cpp` never calls any draw function directly — it only updates internal state and writes to `displayStatusBar()` for brief messages. The table below shows exactly which FSM query drives which draw call:

| FSM query | Display function called |
|---|---|
| `cgm_isGameOver()` becomes true | `drawGameOverScreen(cgm_getGameResultString())` |
| `cgm_getIncomingFEN()` changes | `drawGameScreenWithMove(committed, incoming)` |
| `cgm_getPendingFEN()` changes | `drawGameScreenWithMove(committed, pending)` |
| `cgm_getCommittedFEN()` changes (no move pair) | `drawGameScreen(committed)` |
| `cgm_getTurnStatusString()` changes | `displayStatusBar(turnStatus)` |
| `cgm_getPieceLiftSquare()` returns a square | `drawPiecePickedUp(square)` |
| Piece placed (lift clears) | `drawGameScreen(committed)` |
| `cgm_isInCheck()` becomes true | `drawCheckAlert(whiteToMove)` |
| Check resolves | `drawGameScreen(committed)` |
| `cgm_isConfirming()` becomes true | `drawConfirmOverlay()` |
| Confirm/Cancel resolves | `drawGameScreen(committed)` |

---

## Board Test Loop

`BOARD_TEST` runs independently of the game FSM. Every 100 ms:

```
drawBoardTestLive()
 ├─ for each of 8 chips × 8 channels (64 squares):
 │    raw = readRawChannel(chip, ch)
 │    diff = raw − getBaseline(chip, ch)
 │    compare diff to previous; detect threshold crossings (±300 counts)
 │
 ├─ if any square changed:
 │    bt_redrawGrid()        ← repaint all 64 cells
 │    update "Last event" label in right panel
 │
 └─ Cell colours:
      diff ≥ +300  → green   (N-pole, label 'P')
      diff ≤ −300  → orange  (S-pole, label 'p')
      otherwise    → dark grey (empty)
```

Tap anywhere on this screen to return to the menu.

---

## Background WiFi Reconnect

`cgm_tick()` checks WiFi health every 5 s (regardless of game state):

```cpp
if (!cgm_wifiConnected() && now - cgm.lastWifiRetryMs >= 5000)
    cgm_connectWiFi();   // calls wmConnect(WIFI_SSID, WIFI_PASS)
```

This runs silently — it only updates the status bar if it succeeds or fails. The game FSM continues to run during a WiFi outage; moves will fail to POST but are committed locally, and polling is skipped until reconnected.
