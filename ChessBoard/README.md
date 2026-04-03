# ESP32 Chessboard Firmware

## 🔧 Setup (Clone & Upload from GitHub)

Follow these steps to get the firmware running on your ESP32 using the Arduino IDE:

### 1. Clone the Repository

```bash
git clone https://github.com/YOUR_USERNAME/YOUR_REPO_NAME.git
```

Or download the ZIP from GitHub and extract it.

---

### 2. Open in Arduino IDE

* Open **Arduino IDE**
* Click **File → Open**
* Navigate to the folder
* Open:

```text
ChessBoard/ChessBoard.ino
```

⚠️ Important:

* The `.ino` file must be inside a folder with the **same name** (`ChessBoard/ChessBoard.ino`)
* All `.cpp` and `.h` files must be in the same folder

---

### 3. Install Required Libraries

In Arduino IDE:

* Go to **Tools → Manage Libraries**

* Install:

* DFRobot_GDL

* DFRobot_Touch

* DFRobot_UI

* ArduinoJson

* Adafruit NeoPixel

---

### 4. Install ESP32 Board Support

* Go to **File → Preferences**
* Add this to *Additional Board Manager URLs*:

```text
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```

Then:

* Go to **Tools → Board → Boards Manager**
* Search **ESP32**
* Install **esp32 by Espressif Systems**

---

### 5. Select Board & Port

* Tools → Board → **ESP32S3 Dev Module** (or your specific board)
* Tools → Port → select your ESP32

---

### 6. Configure WiFi

Open:

```text
headers.h
```

Update:

```cpp
constexpr const char* WIFI_SSID = "your_wifi";
constexpr const char* WIFI_PASS = "your_password";
```

---

### 7. Upload

* Click **Upload**
* Open Serial Monitor (115200 baud)

---

## 📁 File Structure

```text
ChessBoard/
├── ChessBoard.ino        # Main entry point
├── config.h              # Pin definitions
├── headers.h             # WiFi credentials
│
├── display.cpp / .h      # UI rendering
├── wifi_driver.cpp / .h  # WiFi connection logic
├── api_client.cpp / .h   # API communication (GET/POST FEN)
├── LED_driver.cpp / .h   # LED board control
```

---

## 🧠 Architecture

The sketch is split into independent modules:

### `.ino`

* Initializes hardware (SPI, I2C, screen, touch)
* Manages high-level flow
* Connects modules together

---

### `display.*`

* Handles all screen drawing
* Renders:

  * connection status
  * FEN board
  * UI states
* No networking or logic

---

### `wifi_driver.*`

* Connects to WiFi
* Returns connection status

---

### `api_client.*`

* Fetches latest FEN from backend
* Pushes moves + FEN to backend

---

### `LED_driver.*`

* Controls WS2812 LED strip
* Maps board positions → LED indices
* Displays pieces using colors

---

### `config.h`

* All pin definitions
* Central hardware configuration

---

### `headers.h`

* WiFi credentials (SSID / password)

---

## 🔁 Firmware Flow

### Startup

1. Initialize display + hardware
2. Show `"Connecting to WiFi..."`
3. Connect to WiFi

---

### Main Screen

* Displays:

  * WiFi status
  * Board (FEN)
* Shows **"Fetch FEN"** button

---

### Button Press

1. Show `"Fetching..."`
2. Call API
3. Update:

   * display
   * LED board

---

## 🔌 Hardware Used

* ESP32-S3
* WS2812B LED strip (64 LEDs)
* DFRobot 3.5" touchscreen display
* I2C + SPI interfaces

---

## ⚙️ Key Functions

### Fetch latest board state

```cpp
String fetchLatestFEN();
```

### Push move + FEN

```cpp
String pushLatestFEN(const String& move, const String& fen);
```

### Render board

```cpp
drawMainScreen(wifiConnected, fen);
```

### Update LEDs

```cpp
lightFEN(fen.c_str());
```

---

## ⚠️ Notes

* The firmware does NOT compute chess logic
* Full FEN must be provided when sending moves
* LED mapping assumes 8×8 board (adjust if serpentine wiring)

---

## 🚀 Future Improvements

* Sensor integration for move detection
* Local FEN generation
* Auto-refresh polling
* Game state validation

---

## 🧪 Debugging Tips

* Use Serial Monitor (`115200 baud`)
* Check WiFi connection before API calls
* Verify LED mapping with test patterns

---

## 📌 Summary

This firmware is a modular embedded system that:

* connects to a backend
* displays board state
* drives LEDs for visualization

Designed for scalability and easy debugging as features expand.
