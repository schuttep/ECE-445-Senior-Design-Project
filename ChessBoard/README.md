# display_driver — Reference

`display_driver.h` / `display_driver.cpp` provides two layers of API for driving the 320×480 IPS screen: low-level **primitives** you can combine freely, and **pre-built full screens** for the four main states of the app.

---

## Setup

Call once in `setup()` before drawing anything:

```cpp
initDisplay();
```

This calls `screen.begin()` and disables text wrap.

---

## Primitives

Building blocks for composing custom layouts directly from `ChessBoard.ino`.

### `displayClear()`
Fills the entire screen white.

```cpp
displayClear();
```

---

### `displayHeader(bool wifiConnected)`
Draws the standard top bar: "ChessBoard" title on the left, "WiFi: OK" / "WiFi: --" on the right, and a full-width divider line at y=38.

```cpp
displayHeader(true);
```

---

### `displayCenteredText(const char* text, int y, uint8_t size, uint16_t color)`
Prints text horizontally centered on the screen at the given y coordinate.

| Parameter | Description |
|---|---|
| `text` | Null-terminated string to display |
| `y` | Vertical pixel position (top of text) |
| `size` | Text scale factor (1 = 6×8px per char, 2 = 12×16px, etc.) |
| `color` | RGB565 color constant, e.g. `COLOR_RGB565_BLACK` |

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
