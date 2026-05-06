# ECE 445 Senior Design Project — Networked Physical Chessboard (Group 51)

## Project Overview

![Both Game Boards](notebooks/Quinn/Both_Game_Boards.jpg)

This project proposes a networked physical chessboard hardware platform designed to support customizable software experiences built around real-world chess interaction. The system enables remote players to play chess using physical pieces while maintaining board synchronization through wireless communication and magnetic piece detection. This document explores the problem being solved and the approach Group 51 is taking to develop a flexible hardware foundation for future software expansion. Furthermore, the requirements and designs used to meet those requirements for each subsystem are presented. The compatibility and accuracy of the sensors and analog-to-digital converters are validated through basic calculations and engineering analysis. Finally, the ethical considerations and societal impact of the project are discussed.

---

## Table of Contents

1. [Problem Statement](#problem-statement)
2. [Proposed Solution](#proposed-solution)
3. [System Architecture](#system-architecture)
4. [Subsystem Descriptions](#subsystem-descriptions)
   - [Microcontroller & Firmware](#microcontroller--firmware)
   - [Piece Detection (Hall-Effect Sensors & ADCs)](#piece-detection-hall-effect-sensors--adcs)
   - [Display & Touchscreen UI](#display--touchscreen-ui)
   - [Wireless Communication & Backend](#wireless-communication--backend)
   - [Game Logic & FSM](#game-logic--fsm)
   - [Power & PCB Design](#power--pcb-design)
5. [Requirements & Verification](#requirements--verification)
6. [Engineering Analysis](#engineering-analysis)
7. [Ethical Considerations & Societal Impact](#ethical-considerations--societal-impact)
8. [Team Members](#team-members)
9. [Repository Structure](#repository-structure)

---

## Problem Statement

Traditional online chess is played entirely in software, disconnecting players from the tactile experience of moving physical pieces on a board. Existing products that bridge the physical and digital chess experience are either prohibitively expensive, proprietary, or locked to a single software platform. There is a need for an open, extensible hardware platform that can detect real piece positions, synchronize state between two remote boards over a network, and serve as a foundation for a rich set of software-defined game experiences.

---

## Proposed Solution

Group 51 designed and built a pair of networked physical chessboards, each built around an **ESP32-S3** microcontroller. Each board detects the position of every piece using an array of 64 Hall-effect sensors (one per square) read through eight **ADS7128** 12-bit I2C ADCs. A **480×320 IPS touchscreen** provides the primary user interface for game control, move confirmation, chat, timer display, and AI hints. The two boards communicate through an **AWS API Gateway + DynamoDB** backend, which acts as a state store to synchronize game state between remote players in real time.

| | |
|---|---|
| ![Assembled Board and Pieces](notebooks/Danny/assembled_board_and_pieces.png) | ![Full Board Assembled](notebooks/Danny/full_board_assembled.png) |

Key software features include:
- Full chess rule validation (castling, en passant, promotion, check, checkmate, stalemate) running entirely on the microcontroller.
- A finite-state machine (FSM) game controller that decouples hardware input from network and display logic.
- An on-board chat panel with a touchscreen keyboard, polled every 5 seconds.
- Optional game clocks (Rapid: 10 min/side; Bullet: 5 min/side) tracked on the Arduino.
- Stockfish AI hints (3 per game) fetched from the cloud backend.
- A WiFi network selector and password entry screen rendered at runtime.

---

## System Architecture

```
┌──────────────────────────────────────────────────────────┐
│                     ESP32-S3 (per board)                 │
│                                                          │
│  ┌────────────┐   ┌──────────────┐   ┌────────────────┐ │
│  │  ADC Layer │   │  Game Logic  │   │  Display Layer │ │
│  │ 8× ADS7128 │──▶│  (FSM + rule │──▶│ ST7365P 480×320│ │
│  │ 64 Hall Sx │   │  validation) │   │ GT911 touch    │ │
│  └────────────┘   └──────┬───────┘   └────────────────┘ │
│                          │                               │
│                   ┌──────▼───────┐                       │
│                   │  WiFi / HTTPS│                       │
│                   │  api_connect │                       │
│                   └──────┬───────┘                       │
└──────────────────────────┼───────────────────────────────┘
                           │ HTTPS / AWS API Gateway
              ┌────────────▼────────────────┐
              │  AWS Lambda + DynamoDB       │
              │  (game state, move history,  │
              │   chat, timer, AI hints)     │
              └─────────────────────────────┘
                           │
┌──────────────────────────┼───────────────────────────────┐
│                     ESP32-S3 (remote board)               │
│                        (mirror architecture)              │
└──────────────────────────────────────────────────────────┘
```

![System Block Diagram](notebooks/Quinn/Updated_Block_Diagram.png)

---

## Subsystem Descriptions

### Microcontroller & Firmware

**Part:** Espressif ESP32-S3  
**Interfaces:** SPI (display), I2C × 2 (touch + ADCs), WiFi 802.11 b/g/n

The ESP32-S3 was selected for its dual-core Xtensa LX7 processor, ample GPIO, hardware SPI/I2C peripherals, and built-in WiFi. Two independent I2C buses are used: `Wire` for the touchscreen controller and `Wire1` (GPIO 38/39, 100 kHz) for the eight ADC chips. The main loop runs a tight read → FSM-tick → redraw cycle; only dirty screen regions are repainted each iteration to maintain UI responsiveness.

**Firmware modules:**

| File | Responsibility |
|---|---|
| `ChessBoard.ino` | Hardware init, UI state machine, touch dispatch, main loop |
| `gameloop.cpp/.h` | FSM game controller (12 states) |
| `gamelogic.cpp/.h` | Pure chess rule validation |
| `ADC_driver.cpp/.h` | ADS7128 driver, board FEN assembly |
| `display_driver.cpp/.h` | All screen rendering with dirty-cell cache |
| `api_connect.cpp/.h` | HTTPS communication with AWS backend |
| `wifi_manager.cpp/.h` | WiFi scan and credential management |

---

### Piece Detection (Hall-Effect Sensors & ADCs)

**Sensors:** 64 Hall-effect sensors, one per square (magnetic piece bases)  
**ADCs:** 8× Texas Instruments ADS7128 (12-bit, 8-channel, I2C)  
**I2C Addresses:** `0x10` – `0x17`

Each chess piece has a small permanent magnet embedded in its base. The Hall-effect sensor beneath each square measures the magnetic field strength and polarity. The ADS7128 converts the analog sensor voltage to a 12-bit digital value. The firmware interprets deviations from the 2048 mid-scale baseline:

| Condition | ADC Value | Interpretation |
|---|---|---|
| No piece | ~2048 | Empty square (`.`) |
| N-pole detected (positive deviation ≥ 300 counts) | > 2348 | White piece (`P`) |
| S-pole detected (negative deviation ≥ 300 counts) | < 1748 | Black piece (`p`) |

The physical column-to-chip mapping compensates for PCB routing constraints:

```
File a → h  maps to chips:  {7, 6, 5, 4, 0, 1, 2, 3}
```

The `localIsWhite` flag mirrors the entire board reading when the local player is Black, ensuring rank 8 always corresponds to the opponent's back rank regardless of board orientation.

**ADC Self-Test:** `testADCs()` probes all eight chips and all 64 channels at startup, reporting any unresponsive sensors.

| Game Piece with Magnet | Magnet Installation | Hall-Effect Sensor Prototype | ADC Testing Setup |
|---|---|---|---|
| ![Game Piece](notebooks/Quinn/Game_Piece.jpeg) | ![Magnets in Pieces](notebooks/Danny/magnets_fit_into_pieces.png) | ![Hall Effect Sensors on Breadboard](notebooks/Danny/hall_effect_sensors_on_breadboard.png) | ![ADC Testing Setup](notebooks/Danny/adc_testing_setup.png) |

---

### Display & Touchscreen UI

**Display:** DFRobot ST7365P 480×320 IPS, Hardware SPI  
**Touchscreen:** Goodix GT911 capacitive, I2C (`Wire`, address `0x5D`)

The display operates in landscape mode (480×320). Raw portrait touch coordinates from the GT911 are transformed to landscape at runtime:

$$t_x = 479 - \text{raw}_y, \quad t_y = \text{raw}_x$$

A per-square dirty-cache system (`s_dirtyCells`, 64-bit bitfield) ensures only changed squares are redrawn each loop iteration, keeping frame latency low even on a 480×320 panel.

**UI Screens:**

| Screen State | Description |
|---|---|
| `MENU` | Main menu: Create Game, Join Game, Board Test |
| `GAME` | Live board + right panel (clocks, chat, hint button) |
| `BOARD_SYNC` | Overlay highlighting missing or extra pieces during setup |
| `WIFI_LIST` | Scanned network list with signal strength |
| `WIFI_PASS_SCREEN` | On-screen keyboard for WiFi password entry |
| `TIMER_MODE_SELECT` | Select Unlimited / Rapid / Bullet time control |
| `AI_MODE_SELECT` | Select Easy / Medium / Hard Stockfish difficulty |
| `BOARD_TEST` | Raw sensor value display for hardware validation |

![Touch Display UI](notebooks/Quinn/TouchDisplay.png)

---

### Wireless Communication & Backend

**Protocol:** HTTPS (TLS) over WiFi 802.11 b/g/n  
**Backend:** AWS API Gateway → AWS Lambda → DynamoDB  
**Base URL:** `https://j3zvk9adv0.execute-api.us-east-2.amazonaws.com/api/v1`

The backend is a **stateless REST API**; chess rule enforcement and move validation run entirely on the ESP32-S3. The server stores game state, move history, timer state, and chat messages. Optimistic concurrency control (`expectedVersion`) prevents conflicting simultaneous updates.

**API Endpoints:**

| Method | Endpoint | Function |
|---|---|---|
| `GET` | `/games/1` | Fetch current game state (FEN, turn, version) |
| `GET` | `/games/1/moves` | Fetch full move history |
| `POST` | `/games/1/moves` | Submit a validated move + updated FEN |
| `POST` | `/games/1/reset` | Reset game to starting position |
| `POST` | `/games/1/heartbeat` | Connectivity keepalive (every ~3 s) |
| `POST` | `/games/1/timeout` | Notify server of clock expiry |
| `GET` | `/games/1/messages` | Fetch chat messages |
| `POST` | `/games/1/messages` | Send a chat message |
| `POST` | `/games/1/hint` | Request Stockfish best-move hint |

The Amazon Root CA 1 TLS certificate is embedded directly in firmware (valid until 2038). All API calls verify WiFi connectivity before attempting a request.

---

### Game Logic & FSM

**FSM Controller (`gameloop.cpp`)** drives the full game lifecycle through 12 states:

| State | Description |
|---|---|
| `CGM_WAIT_FOR_GAME_START` | Idle on the menu |
| `CGM_JOIN_POLLING` | Polling server until opponent's first move appears |
| `CGM_GAME_INITIALIZATION` | Fetching initial game state from server |
| `CGM_BOARD_SYNC` | Waiting for physical board to match the expected position |
| `CGM_LOCAL_TURN_WAIT_FOR_BOARD` | Waiting for player to lift and place a piece |
| `CGM_LOCAL_TURN_PROMOTION` | Waiting for promotion piece selection via touchscreen |
| `CGM_LOCAL_TURN_VALIDATE` | Validating the detected move against chess rules |
| `CGM_LOCAL_TURN_CONFIRM` | Waiting for player to confirm or cancel the move |
| `CGM_SEND_STATE` | Sending confirmed move and FEN to server |
| `CGM_WAIT_FOR_REMOTE_MOVE` | Polling server for opponent's move |
| `CGM_APPLY_REMOTE_MOVE` | Waiting for player to replicate opponent's move on the board |
| `CGM_GAME_END` | Game over (checkmate, stalemate, timeout) |

**Rule Validator (`gamelogic.cpp`)** is a self-contained chess engine with no hardware or network dependencies. It validates any before/after FEN pair against the complete ruleset including castling rights, en passant captures, pawn promotion, and king safety (check, checkmate, stalemate detection).

| Working Single Board Game | Mock Demo |
|---|---|
| ![Working Single Board Game](notebooks/Danny/working_single_board_game.png) | ![Mock Demo](notebooks/Danny/mock_demo.png) |

---

### Power & PCB Design

The project uses two custom PCBs:

**Main Board PCB** (`ECE-445-PCBs/Main_Board/`)  
Hosts the ESP32-S3, display/touch connectors, power regulation, and I2C bus breakout for the ADC array. Gerber files are provided for fabrication.

**Tile PCB** (`ECE-445-PCBs/Tile_PCB/`)  
A small per-square PCB carrying one Hall-effect sensor. 64 tiles tile together to form the 8×8 sensing matrix. Each tile connects to the column ADC via a shared I2C bus segment.

**Main Board PCB:**

| 3D View | PCB Routing | Assembled & Soldered |
|---|---|---|
| ![Main PCB 3D View](notebooks/Quinn/MainPCB_3d_View.png) | ![Main PCB Routing](notebooks/Quinn/MainPCB_Routing.png) | ![Main PCB Soldered](notebooks/Quinn/MainPCB_soldered.jpeg) |

**Tile PCB:**

| 3D View (Front) | 3D View (Back) | PCB Routing | Soldered | Wiring |
|---|---|---|---|---|
| ![Tile PCB 3D Front](notebooks/Quinn/TilePCB_3d_View_Front.png) | ![Tile PCB 3D Back](notebooks/Quinn/TilPCB_3d_View_Back.png) | ![Tile PCB Routing](notebooks/Quinn/TilePCB_Routing.png) | ![Tile PCB Soldered](notebooks/Quinn/TilePCB_soldered.jpeg) | ![Tile PCB Wiring](notebooks/Quinn/TilePCB_Wiring.jpeg) |

**3D-Printed Enclosure** (Fusion 360 source files and STLs in `Enclosure/`):

| Baseplate Corner | Baseplate Edge | Display Support | Tile PCB Support | Main Board Support |
|---|---|---|---|---|
| ![Baseplate Corner](notebooks/Quinn/Baseplate_Corner.png) | ![Baseplate Edge](notebooks/Quinn/Baseplate_Edge.png) | ![Display Support](notebooks/Quinn/Display_Support.png) | ![Tile PCB Support](notebooks/Quinn/TilePCB_Support.png) | ![Main Board Support](notebooks/Quinn/MainPCB_support.png) |

---

## Build Process

| Reflow Soldering Main PCB | Soldering Tile PCBs | Quarter Board Assembled | Full Board Assembled |
|---|---|---|---|
| ![Reflow Soldering Main PCB](notebooks/Danny/reflow_soldering_main_pcb.png) | ![Soldering Tile PCBs](notebooks/Danny/soldering_tile_pcbs.png) | ![Quarter of Board Assembled](notebooks/Danny/quarter_of_board_assembled.png) | ![Full Board Assembled](notebooks/Payton/full_board_assembled.png) |

| First Completed Board | Second Completed Board |
|---|---|
| ![First Game Board](notebooks/Quinn/First_Game_Board.JPEG) | ![Second Game Board](notebooks/Quinn/Second_Game_Board.jpeg) |

---

## Requirements & Verification

| Requirement | Verification Method |
|---|---|
| All 64 squares reliably detect piece presence/absence | ADC self-test (`testADCs()`); manual placement of pieces across all squares |
| Board FEN matches physical position within 1 loop cycle (~50 ms) | Serial monitor comparison of sensor FEN vs. expected FEN during test positions |
| WiFi connection established within 10 seconds of credential entry | Timed connection attempts in `wifi_manager.cpp` |
| Move validated and sent to server within 3 seconds of confirmation | Timestamped serial log during live game |
| Remote move received and displayed within 5 seconds of opponent submission | End-to-end latency test between two boards |
| Chat messages delivered within 10 seconds | Measured poll cycle (`MSG_POLL_INTERVAL_MS = 5000 ms`) |
| Timer accuracy within ±1 second per 10 minutes of play | Comparison of Arduino millis() clock against reference timer |
| Touchscreen registers all button presses with < 100 ms response | Manual interaction testing across all UI screens |

---

## Engineering Analysis

### Hall-Effect Sensor Threshold Calculation

The ADS7128 is a 12-bit ADC with a 3.3 V reference, giving a resolution of:

$$\Delta V = \frac{3.3\text{ V}}{2^{12}} \approx 0.806 \text{ mV/count}$$

The firmware uses a ±300 count threshold around the 2048 midpoint:

$$\Delta V_{\text{threshold}} = 300 \times 0.806 \text{ mV} \approx 242 \text{ mV}$$

This threshold was chosen to be large enough to reject sensor noise (empirically measured at < 20 counts RMS) while reliably detecting the magnetic field of the embedded piece magnets (measured at 400–800 counts deviation at rated piece height).

### I2C Bus Timing

Eight ADS7128 chips share the `Wire1` bus at 100 kHz. A full board scan reads 64 channels sequentially. Each channel read requires approximately 3 I2C transactions (register select + 2-byte read), each taking:

$$t_{\text{transaction}} \approx \frac{3 \times 18 \text{ bits}}{100{,}000 \text{ bps}} \approx 540 \text{ µs}$$

Total scan time estimate:

$$t_{\text{scan}} \approx 64 \times 540 \text{ µs} \approx 34.6 \text{ ms}$$

This is well within the target loop period of ~50 ms, leaving sufficient headroom for display updates, FSM logic, and WiFi polling.

### Optimistic Concurrency

The server uses a version counter to prevent conflicting move submissions. If both boards somehow attempt to push a move simultaneously, the server rejects the request with a lower `expectedVersion`, and the losing board re-fetches the current state before retrying. This eliminates the need for server-side locking in a two-player scenario.

---

## Ethical Considerations & Societal Impact

**Accessibility:** The physical-digital bridge this project creates enables players who prefer or require tactile interaction — including visually impaired users with adapted piece markings — to participate in networked chess. The touchscreen UI is designed with large tap targets and high-contrast color themes.

**Privacy & Data:** The system transmits only game state (FEN strings), optional chat messages, and anonymous heartbeat signals. No personally identifiable information is collected or stored by the backend. Credentials (WiFi SSID/password) are stored locally in `secrets.h` and are never transmitted to the cloud.

**Security:** All client–server communication is over TLS (HTTPS) with a pinned Amazon Root CA certificate. The backend uses optimistic concurrency control to prevent unauthorized state manipulation. WiFi credentials are excluded from version control via `.gitignore`.

**Open Hardware Philosophy:** All PCB design files (KiCad), 3D enclosure models (Fusion 360), and firmware are maintained in this repository, enabling the community to reproduce, modify, and extend the platform without proprietary lock-in.

**Societal Impact:** Chess is a globally played game with deep cultural significance. By lowering the cost and technical barrier to networked physical chess, this platform has the potential to reconnect distributed families and communities through shared physical play experiences, and to serve as an educational platform for embedded systems and game AI coursework.

---

*ECE 445 — Senior Design Laboratory, University of Illinois Urbana-Champaign*  
*Group 51 — Spring 2026*

---

## Repository Structure

```
ChessBoard/          ← Production firmware (ESP32-S3 Arduino sketch)
ECE-445-PCBs/        ← KiCad PCB design files + Gerbers
Enclosure/           ← Fusion 360 source files + STLs
prototypes/          ← Early-stage / exploratory sketches (non-production)
backend/
  api.py               ← AWS Lambda handler (backend)
  api.md               ← Full REST API endpoint reference
docs/
  diagrams/
    ChessBoard.drawio  ← System block diagram (editable)
Danny/               ← Build photos and design notes
Quinn/               ← Build photos, PCB renders, enclosure renders
Payton/              ← Design notes
README.md            ← This file
```

**Prototype sketches** (`prototypes/`) are independent `.ino` files used during development to validate individual subsystems (ADC driver, display, hall sensor, WiFi). They are not part of the production build in `ChessBoard/`.

**API documentation** is maintained in [`backend/api.md`](backend/api.md). The backend source is [`backend/api.py`](backend/api.py) (AWS Lambda + DynamoDB).
