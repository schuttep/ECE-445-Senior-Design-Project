# Engineering Notebook — Payton Schutte
**ECE 445 Senior Design Laboratory**
**University of Illinois Urbana-Champaign — Spring 2026**
**Project:** Networked Physical Chess Board for Remote Play (Group 51)


## Entry 1 — 2026-01-21: First Group Meeting

**Objectives:** Meet with group members for the first time, brainstorm viable project ideas, and submit candidate proposals to the course web board.

**Record:**

Met with Quinn and Danny to brainstorm project ideas. Each member proposed one concept:

| Member | Proposal |
|---|---|
| Payton | Two physical chess boards that play each other over the internet |
| Quinn | Universal game board using an E-ink display |
| Danny | Auto-dealer poker machine |

The chess board idea was chosen as the group's preferred direction. The core appeal was that it was a real physical artifact two players would interact with simultaneously, not merely a software interface on a screen. Building a physical sensing layer to detect piece positions, synchronizing state over a network, and rendering the opponent's moves locally were identified as the three main technical challenges.

All three ideas were posted to the course web board to obtain feedback from course staff before committing to a final direction.

**Design Decision:** Prioritize physical interactivity over purely digital game experience. The value proposition of the project depends on players using real pieces on a real board, so the sensing layer and physical form factor are non-negotiable constraints.

---

## Entry 2 — 2026-01-24: Getting Ideas on Paper

**Objectives:** Lock in the project direction and document the agreed feature set in writing.

**Record:**

Officially selected the Networked Physical Chess Board for Remote Play as the group project. Agreed on the core feature set:

- Two boards connected over WiFi, communicating through a shared cloud backend
- LED indicators on each board to show the opponent's most recent move
- Touch display for game UI (menus, move confirmation, status)

These decisions were written into an informal design document. Writing forced clarity about where the primary complexity would lie. Two areas stood out: (1) the sensing layer — how to reliably distinguish piece presence and absence on all 64 squares — and (2) network synchronization — how to ensure both boards always agree on the canonical game state without requiring a dedicated referee process.

**References:** None for this session; ideas based on prior knowledge of ESP32 capabilities and chess board geometry.

---

## Entry 3 — 2026-01-28: Request for Approval (RFA)

**Objectives:** Write and submit the RFA document early for extra credit. Formalize subsystems, success criteria, and initial component selections.

**Record:**

Worked with the team to write the RFA. The document covered:

- **Problem statement:** Remote chess requires players to use software clients; there is no affordable product that lets remote players use physical pieces
- **Proposed solution:** Two ESP32-based boards with Hall effect piece detection, networked via AWS
- **Subsystems identified:**
  1. Piece sensing (Hall effect sensors + ADC)
  2. Main MCU and firmware (ESP32)
  3. Display and UI (touchscreen)
  4. Network communication (WiFi + REST API)
  5. Game logic (rule validation, state machine)
- **Initial component selections:** ESP32 for WiFi integration, Hall effect sensors for non-contact piece detection, I2C ADC for multiplexing 64 sensors, DFRobot touch display for UI

**Design Decision:** Use an I2C ADC rather than direct GPIO reads from Hall sensors. The ESP32 does not have 64 available analog input pins, so multiplexing through I2C ADCs is the only practical approach. This decision drove the rest of the sensing architecture.

---

## Entry 4 — 2026-02-02: Buying Breadboard Parts

**Objectives:** Finalize and place the initial component order for breadboard prototyping.

**Record:**

Finalized the component order. Key change from the RFA: the TLA2528 ADC originally selected was out of stock on DigiKey. Substituted the **ADS7128** [R1], a 12-bit 8-channel I2C ADC with similar performance but lower cost and no unused advanced features.

Additional items ordered:
- QFN-16 breakout board (required to use the ADS7128 package on a breadboard)
- Permanent magnets for piece bases (to test Hall effect sensitivity at different distances)

**Design Decision:** Switched from TLA2528 to ADS7128. Both are 12-bit 8-channel I2C ADCs. The ADS7128 was available, cheaper, and had an existing Arduino community footprint. The key required feature — addressable I2C addressing so multiple chips can share one bus — is present on both.

Without real hardware measurements of magnetic field strength at operating distances, the viability of the entire sensing approach remained an open question at this point.

---

## Entry 5 — 2026-02-10: Preparing for First TA Meeting

**Objectives:** Prepare a block diagram that clearly shows subsystem data flow for the first TA meeting.

**Record:**

Worked with the team to build a block diagram (Fig. 1) before the TA meeting. The diagram broke the system into six subsystems and showed the data paths between them:

```
Hall Sensors (64x) --> ADS7128 ADCs (8x) --> ESP32-S3
                                                |
                                   .------------+------------.
                             Display UI        WiFi      Game Logic
                                                |
                                           AWS API
```

**Fig. 1** — High-level data flow block diagram (preliminary). Sensing data flows left to right; network data flows vertically through the MCU.

Key insight from building this diagram: the game logic layer must be coupled to both the sensing layer (to interpret physical moves) and the network layer (to push and pull state), but it should not contain any hardware-specific code itself. This separation became a guiding principle for the firmware architecture.

---

## Entry 6 — 2026-02-11: First TA Meeting

**Objectives:** Present the current design to the TA and collect feedback.

**Record:**

Presented the block diagram and described the planned approach. TA feedback:

- General direction was sound
- Some confusion about how the physical layout of two boards related to the network topology — clarified with a walkthrough of the data flow
- **Identified risk:** Hall effect sensitivity at the target operating distance (magnet embedded in piece base, sensor recessed below playing surface). Flagged as the highest-risk item requiring early validation

**Action item:** Validate Hall effect sensing performance on the breadboard as early as possible before committing to PCB layout.

---

## Entry 7 — 2026-02-13: Project Proposal Document

**Objectives:** Complete the Project Proposal Document with full subsystem specifications, component datasheets, and a schedule.

**Record:**

Finalized the Project Proposal Document. This was significantly more detailed than the RFA and required research into component datasheets:

- **ADS7128 I2C addressing:** Seven address bits configured via the ADDR pin, giving addresses `0x10`–`0x17` for the eight chips needed. This confirmed that all eight chips can share a single I2C bus without address conflicts [R1].
- **Hall effect sensor output:** The chosen sensor outputs a ratiometric analog voltage centered at V_DD/2 when no field is present. At 3.3V supply, the quiescent output is 1.65V [R2].
- **Display SPI initialization:** The ST7365P controller requires a specific initialization command sequence before accepting display data [R3].

**References used this session:** ADS7128 datasheet [R1], Hall effect sensor datasheet [R2], ST7365P datasheet [R3], DFRobot_GDL library source [R4].

---

## Entry 8 — 2026-02-15: Display Connector Investigation

**Objectives:** Determine whether an existing DSI touch display from a prior project can be reused with the ESP32, and if so, how to wire it.

**Record:**

The display in question uses a GDI ribbon connector, which the ESP32-S3 does not support natively — it would require a separate DSI-to-SPI bridge chip. Upon closer inspection of the display PCB, test pads were found for SPI, I2C, and analog control signals. This meant the display could be driven directly from the ESP32 over hardware SPI without any bridge chip.

Confirmed the display uses an **ST7365P** controller [R3], for which the DFRobot_GDL Arduino library [R4] provides a working high-level driver. The capacitive touch layer uses a **GT911** controller [R5] communicating over I2C.

**Design Decision:** Use the SPI test pads rather than the GDI connector. This eliminated the bridge chip, reduced BOM cost, and allowed a much simpler PCB layout. Trade-off: SPI has lower theoretical bandwidth than DSI, but at the target resolution (480x320) and frame rate, SPI at 40 MHz is more than sufficient.

**References:** ST7365P datasheet [R3], DFRobot_GDL library [R4], GT911 datasheet [R5].

---

## Entry 9 — 2026-02-23: Initial Breadboard Setup

**Objectives:** Assemble the Hall effect + ADS7128 breadboard, validate I2C communication, and measure sensor response to magnets at operating distance.

**Record:**

Assembled the breadboard with 8 Hall effect sensors at 1.5" center-to-center spacing (matching the planned tile dimensions) and one ADS7128 on its QFN breakout board connected to the ESP32 over I2C.

**Debugging log:**

| Problem | Root cause | Resolution |
|---|---|---|
| ADC not responding on I2C bus | Wrong I2C address in firmware (ADDR pin state not accounted for) | Measured ADDR pin voltage, recalculated address to `0x10`, updated firmware |
| ADC intermittently dropping off bus | Cold solder joint on QFN breakout pad | Reflowed with additional flux; confirmed continuity |

After fixes, the ADC responded correctly on the bus and all 8 channels returned valid readings.

**Sensor sensitivity test:** The magnets ordered were weaker than expected. Reliable detection required nearly direct contact between magnet and sensor surface. This was below the target operating distance of approximately 3mm through the tile PCB substrate.

**Action item:** Order stronger magnets (higher grade NdFeB). Risk item — if magnets cannot produce a sufficient field at operating distance, the sensing architecture may need to change.

**Fig. 2** — Breadboard test setup showing 8 Hall effect sensors, ADS7128 breakout, ESP32 dev board. *(See `../Danny/hall_effect_sensors_on_breadboard.png` and `../Danny/connected_breadboards_for_esp32_testing.png`)*

---

## Entry 10 — 2026-03-05: Breadboard Demo Planning

**Objectives:** Plan the March 9 demo deliverables and assign tasks for the pre-Spring Break period.

**Record:**

Agreed to demonstrate three capabilities on March 9:

1. ADC-based piece detection on all 8 sensors simultaneously
2. Per-sensor identification with LED visual feedback
3. Successful ESP32 programming over USB

Assigned tasks for Spring Break:
- **Payton:** Breadboard firmware, LED driver
- **Danny:** PCB schematic review, component placement
- **Quinn:** Display driver initial code, tile PCB layout

Success criteria for Spring Break: PCB files ready to send to fab, display initialization working on dev board.

---

## Entry 11 — 2026-03-09: Breadboard Demo

**Objectives:** Demonstrate Hall effect sensing and LED response to course staff.

**Record:**

Demonstrated piece detection across all 8 sensors with real-time LED feedback. The firmware polled the ADS7128 over I2C in a tight loop, compared each channel reading against a fixed threshold, and drove the LED array accordingly.

**Test setup:**
- I2C bus: GPIO 21 (SDA), GPIO 22 (SCL), 100 kHz
- Threshold: 300 counts above/below 2048 midpoint (12-bit ADC, 3.3V ref)
- Sensors: spaced at 1.5" centers, magnets held at approximately 2mm above sensor

**Result:** All 8 sensors responded correctly and independently. Magnetic crosstalk between adjacent sensors was not observed at 1.5" spacing. Professor Fliflet noted this was a successful validation of the sensing approach.

**Key validation:** ADS7128 + Hall effect combination is viable as the sensing layer for the full 64-square board, provided the magnets are strong enough at operating distance.

---

## Entry 12 — 2026-03-24: Main PCB Soldering

**Objectives:** Assemble the first main PCB using reflow soldering and validate through inspection and continuity checks.

**Record:**

Assembly process:
1. Applied solder paste through stencil
2. Placed SMD components by hand (ESP32 module, ADS7128 chips, passives, connectors)
3. Ran board through reflow oven

Post-reflow inspection checklist:
- Visual inspection for bridges — none found on critical nets
- Continuity check: power rails, ESP32 module connections, ADS7128 footprints, display header, programming header
- No cold joints found on first pass

This marked the transition from prototype to production hardware. From this point forward all firmware development targeted the custom PCB.

**Fig. 3** — Main PCB reflow soldering setup. *(See `../Danny/reflow_soldering_main_pcb.png`)*

---

## Entry 13 — 2026-03-25: WiFi and API Driver Bringup

**Objectives:** Write the WiFi connection and HTTPS API communication layers, targeting the ESP32-S3 on the custom PCB.

**Record:**

Designed a two-layer network architecture to keep transport details out of the game loop:

**Layer 1 — `wifi_driver`**
- Wraps ESP32 `WiFi` library
- Connects to network by SSID/password with retry loop (up to 40 attempts, 500ms between attempts)
- Exposes `bool wifiConnect(ssid, password)` and `bool wifiIsConnected()`

**Layer 2 — `api_connect`**
- Uses `WiFiClientSecure` + `HTTPClient` for HTTPS
- TLS validation: Amazon Root CA 1 PEM certificate embedded directly in source (the ESP32 has no system certificate store) [R6]
- Exposes: `fetchGameState()`, `pushFENState()`, `sendHeartbeat()`, `resetGame()`, `sendMessage()`, `fetchBestMove()`

**Design Decision:** Hardcode the Amazon Root CA 1 certificate rather than disabling TLS verification. Skipping verification would expose the device to man-in-the-middle attacks on any network. The certificate is valid until 2038, which exceeds the expected product lifetime.

**Design Decision:** The `api_connect` layer returns structured result types (`GameStateResult`, `ApiResult`, etc.) rather than raw JSON. Parsing happens once at the boundary; the game loop works with typed values instead of string dictionaries.

**References:** AWS API Gateway HTTPS setup [R6], ESP32 WiFiClientSecure documentation [R7].

---

## Entry 14 — 2026-03-27: First Flashing of Main PCB

**Objectives:** Flash firmware onto the assembled main PCB and validate that the ADC driver works on real hardware.

**Record:**

**Debugging log:**

| Attempt | Problem | Root cause | Resolution |
|---|---|---|---|
| 1 | Board would not enumerate over USB | PCB resting on metallic solder stencil causing shorts on bottom pads | Lifted board off stencil; no damage |
| 2 | ADC chips not responding on I2C | Firmware used default ESP32 I2C pins instead of PCB-routed `SDA_DAQ`/`SCL_DAQ` | Updated pin definitions to GPIO 38 (SDA) / GPIO 39 (SCL) |
| 3 (success) | All 8 ADS7128 chips responded correctly | — | — |

This confirmed the PCB layout, routing, and component placement had translated correctly from the breadboard prototype. The custom I2C pin assignment (`Wire1` on GPIO 38/39) became a permanent fixture of the hardware abstraction.

**Additional discussion:** Agreed to use individual tile PCBs (designed by Quinn) to mount Hall effect sensors under each square. Preferred over flexible ribbon cables or point-to-point wiring for consistent sensor orientation and a clean mechanical interface.

---

## Entry 15 — 2026-04-02: Initial Firmware Structure Commit

**Objectives:** Refactor test code into a modular production firmware structure.

**Record:**

Split firmware into clearly separated modules:

| Module | File(s) | Responsibility |
|---|---|---|
| WiFi | `wifi_manager.cpp/.h` | Connect, scan, credential UI |
| API | `api_connect.cpp/.h` | All HTTPS communication |
| Display | `display_driver.cpp/.h` | All screen rendering |
| ADC sensing | `ADC_driver.cpp/.h` | ADS7128 driver, FEN assembly |
| Game logic | `gamelogic.cpp/.h` | Chess rule validation |
| Game loop | `gameloop.cpp/.h` | FSM game controller |
| Main sketch | `ChessBoard.ino` | Hardware init, touch dispatch, `loop()` |

This was the first commit where the project resembled an integrated embedded application. Each module had a defined public interface; the main sketch was not permitted to reach into subsystem internals.

---

## Entry 16 — 2026-04-06: Structure Refactor

**Objectives:** Clean up headers, simplify shared state exposure, and tighten display and API interfaces.

**Record:**

Cleanup items completed:
- Removed redundant `extern` declarations
- All display functions grouped into `display_driver.h`; callers no longer include internal headers
- API result types moved to a shared header so `gameloop.cpp` does not include `api_connect.h` directly
- Consistent naming convention applied across all function signatures

No functional changes. This session was preparatory for the integration phase, where sensing, UI, and networking would all need to operate concurrently inside the same control loop.

---

## Entry 17 — 2026-04-07: Display Driver and WiFi Manager UI

**Objectives:** Build the board renderer, implement dirty-cell caching for performance, and add the runtime WiFi selector UI.

**Record:**

**Display initialization issue:** The ST7365P initialized in portrait mode by default, rendering all content rotated 90 degrees. Fixed by setting the `MADCTL` register value for landscape orientation [R3].

**Board renderer design:**
- 8x8 grid of 32x32 pixel squares, cream and brown alternating colors
- Pieces rendered as filled circles with centered piece letter; white pieces: white disc, black rim; black pieces: black disc, grey rim

Touch coordinates from GT911 are in portrait space and must be transformed to landscape. With display width W = 480 and height H = 320:

    tx = 479 - raw_y      (Eq. 17.1)
    ty = raw_x            (Eq. 17.2)

**Performance — dirty-cell cache:**

Redrawing all 64 squares every loop at 40 MHz SPI:

    t_full_redraw = 64 * 32 * 32 * 16 bits / 40,000,000 bps ≈ 52 ms   (Eq. 17.3)

This exceeded the acceptable loop latency for touch response. Solution: a 64-bit dirty bitfield (`s_dirtyCells`) where bit i is set only when square i changes. Per-loop redraw cost drops to (number of changed squares) × (per-square cost).

**WiFi Manager UI:**
- Scans nearby networks and displays SSID list on screen
- Player taps a network, enters password via on-screen keyboard
- Credentials passed to `wifi_manager.cpp` for connection attempt

**References:** ST7365P MADCTL register [R3], GT911 coordinate system [R5].

---

## Entry 18 — 2026-04-10: Tile PCB Soldering

**Objectives:** Assemble all tile PCBs and verify sensor orientation consistency.

**Record:**

Each tile PCB carries one Hall effect sensor, two bypass capacitors, and a connector. 64 tiles required for a full board.

Assembly process per tile:
1. Solder Hall effect sensor in correct orientation (flat face toward playing surface)
2. Solder bypass capacitors on supply pins
3. Solder column bus connector

**Critical check:** Sensor orientation must be consistent across all 64 tiles. The firmware encodes polarity as white (N-pole) vs black (S-pole) vs empty. A single reversed sensor would permanently misidentify piece color on that square.

At this scale (64 repetitions of the same task), procedural consistency matters more than individual skill. Used the tile support print as an assembly jig to hold each PCB at the correct angle during soldering.

**Fig. 4** — Tile PCB assembly. *(See `../notebooks/Quinn/TilePCB_soldered.jpeg` and `../notebooks/Quinn/TilePCB_assembly.jpeg`)*

---

## Entry 19 — 2026-04-15: Test Mode and Move Validation Integration

**Objectives:** Add a hardware test mode to the main firmware and formally integrate the move validation engine.

**Record:**

Added a `BOARD_TEST` screen state to `ChessBoard.ino` that renders raw ADC counts for all 64 squares. This allowed hardware validation without swapping firmware images.

Move validation (`gamelogic.cpp`) integrated and connected to the game loop. The validator takes two FEN strings (before and after a move) and returns the validated result FEN or `"Invalid Move"`. Handles: castling, en passant, promotion, check, checkmate, stalemate.

**Design Decision:** Move validation runs entirely on the ESP32-S3, not on the server. This keeps rule enforcement at the device, prevents invalid moves from reaching the backend, and means the server can be stateless with respect to chess rules — it only stores and forwards FEN strings.

---

## Entry 20 — 2026-04-16: ADC Driver and Column Mapping

**Objectives:** Complete the production ADC driver with correct column-to-chip mapping and validated detection thresholds.

**Record:**

Eight ADS7128 chips at I2C addresses `0x10`–`0x17` on `Wire1` at 100 kHz. Each chip handles one column of 8 squares.

**Physical column-to-chip mapping:**

Due to PCB routing constraints, file a–h does not map linearly to chip addresses 0x10–0x17:

| File | a | b | c | d | e | f | g | h |
|---|---|---|---|---|---|---|---|---|
| Chip offset | 7 | 6 | 5 | 4 | 0 | 1 | 2 | 3 |

Implemented as `ADC_COL_TO_CHIP[8] = {7, 6, 5, 4, 0, 1, 2, 3}`.

**Threshold derivation:**

ADS7128 is a 12-bit ADC, 3.3V reference. LSB voltage:

    Delta_V_LSB = 3.3 V / 4096 ≈ 0.806 mV/count                    (Eq. 20.1)

Baseline (no magnetic field): 2048 counts (midpoint at V_DD/2). Threshold set at ±300 counts:

    Delta_V_threshold = 300 * 0.806 mV ≈ 242 mV                     (Eq. 20.2)

Measured noise floor: < 20 counts RMS. Measured signal from NdFeB magnets at rated height: 400–800 counts.

Signal-to-noise ratio at minimum signal:

    SNR = 400 counts / 20 counts = 20                                (Eq. 20.3)

The 300-count threshold provides comfortable margin above noise with headroom below the minimum expected signal.

| ADC reading | Encoding |
|---|---|
| > 2348 | White piece — N-pole (`P`) |
| < 1748 | Black piece — S-pole (`p`) |
| 1748–2348 | Empty square (`.`) |

**I2C scan time estimate:**

Each channel read: ~3 I2C transactions (register select + 2-byte data read):

    t_transaction ≈ (3 * 18 bits) / 100,000 bps = 540 µs             (Eq. 20.4)

Full board scan (64 channels):

    t_scan = 64 * 540 µs ≈ 34.6 ms                                   (Eq. 20.5)

Within the 50ms loop budget, leaving ~15ms for display and network operations.

**Fig. 5** — ADC testing setup on main PCB. *(See `../Danny/adc_testing_setup.png`)*

---

## Entry 21 — 2026-04-21: Full System Integration

**Objectives:** Connect ADC sensing, display renderer, WiFi/API layer, and move validation into a single FSM-driven control loop.

**Record:**

Added `gameloop.cpp` implementing a 12-state FSM. An FSM was necessary because gameplay involves many distinct waiting conditions that cannot be safely managed with simple flags.

**FSM state transitions (Fig. 6):**

```
WAIT_FOR_GAME_START
  |--(create)--> GAME_INITIALIZATION --> BOARD_SYNC
  '--(join)----> JOIN_POLLING       --> BOARD_SYNC
                                           |
                                   LOCAL_TURN_WAIT_FOR_BOARD
                                           |
                                   LOCAL_TURN_VALIDATE
                                     |            |
                                  (valid)      (invalid)
                                     |            |
                                LOCAL_TURN_CONFIRM  <-- back to WAIT
                                  |         |
                              (confirm)  (cancel)
                                  |         |
                             SEND_STATE    WAIT_FOR_BOARD
                                  |
                         WAIT_FOR_REMOTE_MOVE
                                  |
                         APPLY_REMOTE_MOVE
                                  |
                         LOCAL_TURN_WAIT_FOR_BOARD (next turn)
                                  |
                            GAME_END (checkmate / stalemate / timeout)
```

**Fig. 6** — FSM state transition diagram for game controller (`gameloop.cpp`).

**State tracked by game manager:**
- Committed FEN, pending FEN
- Castling rights, en passant square, half-move clock
- Server version number (optimistic concurrency)
- Timer state (mode, remaining time per side)
- AI difficulty setting

**References:** FEN notation standard [R8].

---

## Entry 22 — 2026-04-22: Physical-to-Logical FEN Translation

**Objectives:** Implement the algorithm that converts a raw physical board reading (occupancy + polarity only) into a valid logical chess FEN.

**Problem statement:** Hall effect sensors report occupancy and polarity only — not piece type. A reading can say "white piece on e4" but not whether it is a rook, bishop, or queen. The logical FEN requires full piece type information.

**Record:**

**Algorithm — `cgm_physicalToLogicalFEN()`:**

1. Convert the last committed logical FEN to an expected physical board (all white pieces → `P`, all black → `p`, empty → `.`).
2. Compare expected physical board against new physical reading. Count changed squares.
3. If changed count > 4: reject as ambiguous.
4. If only departure squares changed and no arrival appeared: **piece-in-air** case — wait without committing.
5. For normal moves: clear departure squares. Map arrival squares back to the most likely committed source piece of the correct color at the departure square.

**Special cases:**

| Case | Detection criterion | Handling |
|---|---|---|
| Castling | 4-square change (king + rook both move) | King movement direction determines kingside vs queenside |
| En passant | 3-square change (pawn departs, pawn arrives, captured pawn disappears from adjacent square) | Captured pawn removed from FEN at the en passant square |
| Promotion | Pawn disappears from 7th rank, piece appears on 8th rank | FSM enters `LOCAL_TURN_PROMOTION`; piece type from touchscreen picker |

**Design Decision:** The algorithm is inherently stateful — it is always a diff operation against the last known valid logical state. It cannot work from a single snapshot alone.

---

## Entry 23 — 2026-04-23: Integration Cleanup and Promotion Logic

**Objectives:** Tighten interaction between main sketch, display, and game loop; add promotion handling and edge-case testing menu.

**Record:**

Promotion: when the physical-to-logical translator detects a pawn arriving on the back rank, the FSM transitions to `LOCAL_TURN_PROMOTION`. An overlay appears on the touchscreen showing Q, R, B, N. The player's selection completes the FEN.

Edge-case testing menu (`EDGE_CASE_TEST` screen state) loads preset board positions:
- Standard starting position
- En passant available
- Both castling sides available
- Pawn on 7th rank (promotion about to trigger)
- King in check

This menu was critical for demo rehearsal — many important chess rules are rare in normal play but had to work correctly in front of the examiner.

---

## Entry 24 — 2026-04-23: Home WiFi Bringup

**Objectives:** Validate full firmware stack on a real home WiFi network outside the lab.

**Record:**

Successfully connected board to home WiFi, fetched game state from AWS, and completed a full move round-trip. Proved:
- TLS certificate validation works on a non-university network
- API endpoint is reachable from arbitrary networks
- WiFi manager UI correctly stores and retries credentials

This also isolated the university WiFi issue (Entry 25) as a MAC address registration problem, not a firmware bug.

---

## Entry 25 — 2026-04-23: Mock Demo

**Objectives:** Demonstrate full gameplay to TA; identify remaining issues before the final demo.

**Record:**

**Issue encountered:** Second board could not connect to IllinoisNet. The WiFi manager could see the network and attempt connection, but the university's policy was silently rejecting the unregistered MAC address.

**Resolution:** Switched to personal hotspot for the demo. Demonstrated full gameplay flow:
- Move detection and validation
- Display update on both boards
- Network handoff: local board POSTs move, remote board polls and applies it
- Chat messaging between boards
- AI hint feature

After the demo, registered the second board's MAC address through the university self-registration portal. Confirmed connection to IllinoisNet on next attempt. Played a full game between both boards over the university network — all features worked correctly.

**Fig. 7** — Two-board networked game in progress during mock demo. *(See `../Danny/mock_demo.png`)*

---

## Entry 26 — 2026-04-24: Mock Presentation

**Objectives:** Run through a shortened final presentation and collect TA feedback.

**Record:**

TA feedback:
- Reduce text density on slides; replace text with diagrams
- Simplify block diagram layout with clearer labels
- Each speaker should cover only the subsystems they were primarily responsible for

Updates made to slides:
- Replaced FSM description slide with Fig. 6 (state transition diagram)
- Replaced FEN translation description with annotated step-by-step figure
- Moved detailed API field descriptions to a backup/appendix slide

---

## Entry 27 — 2026-04-26: Timed Modes, Versus AI, Hints, and Messenger

**Objectives:** Add game clocks, versus-AI mode, Stockfish hints, and in-game chat.

**Record:**

**Timer modes:**

| Mode | Budget per side |
|---|---|
| Unlimited | No clock |
| Rapid | 10 minutes (600,000 ms) |
| Bullet | 5 minutes (300,000 ms) |

Clocks tracked on-device using `millis()`. The server stores only the timer mode and initial budget. When a clock expires, the board calls `POST /games/1/timeout` with the losing color; the server sets `gameResult` to `white_timeout` or `black_timeout`.

**Versus AI:** Reuses the remote-move path. The server's Lambda function runs Stockfish at the configured depth:

| Difficulty | Stockfish depth |
|---|---|
| Easy | 2 |
| Medium | 5 |
| Hard | 12 |

**Hints:** `POST /games/1/hint` with current FEN returns the Stockfish best move UCI string. Firmware highlights source and destination squares. Each game grants 3 hints.

**Messenger:** Chat messages stored as a list in DynamoDB. Boards poll `GET /games/1/messages` every 5 seconds and display the last 3 messages in a right-panel chat window.

---

## Entry 28 — 2026-04-26: UI Bug Fixes and Polish

**Objectives:** Fix layout and visual issues in the game screen after the large feature commit.

**Record:**

Issues fixed:

| Bug | Root cause | Fix |
|---|---|---|
| Timer display overflow when < 1 min | Width not recalculated for shorter MM:SS string | Recalculate display width dynamically per draw call |
| Chat panel not clearing on new game | Stale messages from previous game persisted in render cache | Clear chat cache on every `GAME_INITIALIZATION` entry |
| Board sync overlay blended into cream squares | Highlight color too similar to board background | Changed highlight to bright cyan |
| Move confirmation buttons misaligned | Screen state transition did not reinitialize button coordinates | Reinitialize layout coordinates on every `LOCAL_TURN_CONFIRM` entry |

---

## Entry 29 — 2026-04-27: Timer and AI Fixes

**Objectives:** Fix edge cases in timed modes, AI mode behavior, and check indicator display.

**Record:**

Fixes:

| Issue | Root cause | Fix |
|---|---|---|
| Timer did not pause during BOARD_SYNC | No pause on FSM entry to `BOARD_SYNC` | Added timer pause on `BOARD_SYNC` entry, resume on exit |
| AI move display lag | Board transitioned to `LOCAL_TURN_WAIT_FOR_BOARD` before display updated | Added display refresh call on `APPLY_REMOTE_MOVE` entry |
| Check indicator not clearing after check resolved | `drawCheckAlert(false)` not called on FSM transitions | Added `drawCheckAlert(false)` on every transition out of `LOCAL_TURN_VALIDATE` |

---

## Entry 30 — 2026-04-27: Final Demo Preparation

**Objectives:** Script the demo game, assign presenter roles, confirm all edge cases trigger correctly in sequence.

**Record:**

Scripted game chosen to demonstrate the most important chess edge cases in 11 moves:

| Move | Notation | Feature demonstrated |
|---|---|---|
| 3 | exd6 e.p. | En passant (3-square change detection) |
| 4 | dxc7 | Standard capture |
| 5 | cxb8=Q | Pawn promotion with touchscreen picker |
| 10 | O-O-O | Queenside castling (4-square change detection) |
| 11 | Qh5# | Checkmate detection and game-over screen |

Full move sequence:
```
1. e4 f6  2. e5 d5  3. exd6 e.p. Bd7  4. dxc7 g5
5. cxb8=Q Rxb8  6. Nc3 a6  7. d3 a5  8. Bd2 a4
9. Qg4 a3  10. O-O-O b6  11. Qh5#
```

Presenter assignments:
- **Payton:** ADC architecture, sensing thresholds, physical-to-logical FEN translation, FSM game loop, WiFi/API layer
- **Danny:** PCB design, tile PCB, hardware assembly
- **Quinn:** Display driver, UI, enclosure design

---

## Entry 31 — 2026-04-27: Second Board Assembly and Bringup

**Objectives:** Complete assembly of the second board and confirm it is ready to flash and test.

**Record:**

Lessons applied from the first board assembly:
- Pre-checked all QFN pads for continuity before reflow
- Used improved stencil alignment jig to reduce paste smearing
- Verified sensor orientation on all 64 tiles before final installation

Confirmed the second board came up correctly on first flash attempt.

**Fig. 8** — Second board fully assembled with all 64 tile PCBs installed. *(See `quarter_of_board_assembled.png` and `full_board_assembled.png`)*

Both boards now functional and ready for the scripted demo.

---

## Entry 32 — 2026-04-29: Final Demo

**Objectives:** Demonstrate the full system to Professor Fliflet with both boards running on separate networks.

**Record:**

Played the scripted game in full. All planned edge cases triggered and resolved correctly:

| Edge case | Result |
|---|---|
| En passant (move 3) | 3-square change correctly identified; FEN updated with captured pawn removed |
| Promotion (move 5) | Picker overlay appeared automatically; queen placed correctly in FEN |
| Castling (move 10) | 4-square change correctly identified as queenside castle |
| Checkmate (move 11) | Both boards displayed game-over screen simultaneously |

Additional demonstrations completed:
- WiFi reconnection: disconnected one board mid-game; it reconnected and resynced to current game state automatically
- AI hint: highlighted suggested move on board and display
- Versus AI: engine played correct responses without a second physical board present

**Examiner questions and answers:**

| Question | Answer |
|---|---|
| How does the board handle a player picking up a piece and putting it back without moving? | Piece-in-air filter in `cgm_physicalToLogicalFEN` detects departure without arrival and waits; no move is committed until a piece lands on a new square |
| How does FEN orientation work when playing as black? | `localIsWhite` flag in `readBoardFEN()` mirrors the entire board reading; rank 1 always maps to the local player's back rank |
| What if both boards POST a move simultaneously? | Server uses `expectedVersion` optimistic concurrency; the second POST receives HTTP 409 with current state; the losing board re-fetches and retries |

**Fig. 9** — Both boards running the final demo. *(See `../Danny/working_single_board_game.png`)*

---

## Entry 33 — 2026-04-30: Final Presentation Preparation

**Objectives:** Polish slides, rehearse timing, practice the physical-to-logical FEN translation explanation.

**Record:**

Slide edits:
- Cut FSM state diagram to show only the 7 most common transitions (full diagram moved to backup slides)
- Reformatted to UIUC ECE presentation template
- Added annotated figure for the en passant 3-square change detection

Practiced the physical-to-logical FEN translation explanation with a physical board, as this was the most technically dense section and most likely to generate follow-up questions.

---

## Entry 34 — 2026-05-01: Final Presentation

**Objectives:** Present and defend the completed project.

**Record:**

Sections presented:
1. **ADC driver and threshold scheme** — Eq. 20.1 through 20.5; column mapping table
2. **Physical-to-logical FEN translation** — algorithm walkthrough with en passant and castling examples
3. **FSM game loop** — Fig. 6; discussion of why an FSM is necessary vs simple flags
4. **WiFi/API layer** — TLS certificate pinning, heartbeat mechanism, optimistic concurrency versioning

Additional questions received:

| Question | Answer |
|---|---|
| Why AWS Lambda + DynamoDB rather than a persistent server? | Lambda scales to zero when no games are active, eliminating idle cost. DynamoDB provides durability without managing a database server. |
| How is move history used? | Stored server-side for auditing; boards only need the latest FEN and version number to operate. |

---

## Entry 35 — 2026-05-04: Awards Ceremony

**Objectives:** Attend ECE 445 awards ceremony.

**Record:**

The project received an **Honorable Mention** at the ECE 445 awards ceremony held in ECEB. Attended with Quinn and Danny; certificates collected.

---

## References

| # | Source |
|---|---|
| R1 | Texas Instruments, *ADS7128 8-Channel, 12-Bit, I2C Analog-to-Digital Converter with GPIO*, SBAS869C, 2021. |
| R2 | Allegro MicroSystems, Hall effect sensor datasheet (part number per BOM). |
| R3 | Sitronix, *ST7365P Color LCD Controller Datasheet*, Rev. 1.0. |
| R4 | DFRobot, *DFRobot_GDL Arduino Library*, GitHub, 2023. |
| R5 | Goodix, *GT911 5-Point Capacitive Touch Controller Datasheet*, Rev. 1.1. |
| R6 | Amazon Web Services, *Amazon Root CA 1*, valid through 2038. |
| R7 | Espressif Systems, *ESP-IDF WiFiClientSecure Documentation*, v5.x. |
| R8 | FIDE Laws of Chess, Appendix C — Algebraic Notation and FEN, 2023. |
