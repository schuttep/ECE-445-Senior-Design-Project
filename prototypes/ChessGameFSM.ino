#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ============================================================
// ChessGameFSM.ino
// This file does three main jobs:
// 1. Manages gameplay with a real finite state machine
// 2. Tracks turns and detects when the game ends
// 3. Sends / receives board FEN state through your API
// ============================================================

// -------------------------
// External functions from move_api(1).ino
// -------------------------
String validateMoveAndReturnFEN(const String& beforeFEN,
                                const String& afterFEN,
                                bool whiteToMove,
                                const bool castling[4],
                                char promotionPiece);

bool parseFENBoard(const String& fen, char board[8][8]);
bool isKingInCheck(char board[8][8], bool whiteKing);
bool hasAnyLegalMove(char board[8][8], bool whiteToMove, const bool castling[4]);

// ============================================================
// User configuration
// ============================================================
namespace CGMConfig {
  const char* WIFI_SSID = "YOUR_WIFI_NAME";
  const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";

  // Use your real endpoint here.
  const char* API_URL = "https://j3zvk9adv0.execute-api.us-east-2.amazonaws.com/api/v1/games/1/moves";

  const uint32_t POLL_INTERVAL_MS = 2000;
  const uint32_t LOCAL_STABLE_TIME_MS = 600;
  const uint32_t WIFI_RETRY_INTERVAL_MS = 5000;
  const uint32_t STATUS_PRINT_INTERVAL_MS = 1500;

  const uint16_t GAME_ID = 1;
  const uint8_t BOARD_NUMBER = 1;

  // false = local board plays black and waits for white first
  // true  = local board plays white and moves first
  const bool LOCAL_IS_WHITE = true;

  // Default promotion if you do not yet have a UI prompt
  const char DEFAULT_PROMOTION = 'Q';
}

// ============================================================
// Hooks that other files can call into
// ============================================================
// Your sensor code should call cgm_setPhysicalBoardFEN(...) every
// time it reconstructs the full physical board state.
//
// Touchscreen code can call:
//   cgm_confirmPendingMove();
//   cgm_cancelPendingMove();
//   cgm_requestNewGame();
// ============================================================

static String cgm_physicalBoardFEN = "";
static volatile bool cgm_moveConfirmed = false;
static volatile bool cgm_moveCancelled = false;
static volatile bool cgm_newGameRequested = false;
static volatile char cgm_requestedPromotion = CGMConfig::DEFAULT_PROMOTION;

void cgm_setPhysicalBoardFEN(const String& fen) {
  cgm_physicalBoardFEN = fen;
}

void cgm_confirmPendingMove() {
  cgm_moveConfirmed = true;
}

void cgm_cancelPendingMove() {
  cgm_moveCancelled = true;
}

void cgm_requestNewGame() {
  cgm_newGameRequested = true;
}

void cgm_setPromotionPiece(char piece) {
  cgm_requestedPromotion = piece;
}

// ============================================================
// Small helpers
// ============================================================

String cgm_boardOnlyFen(const String& fen) {
  int space = fen.indexOf(' ');
  if (space == -1) {
    return fen;
  }
  return fen.substring(0, space);
}

String cgm_startFen() {
  return "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR";
}

bool cgm_sameBoardFen(const String& a, const String& b) {
  return cgm_boardOnlyFen(a) == cgm_boardOnlyFen(b);
}

bool cgm_wifiConnected() {
  return WiFi.status() == WL_CONNECTED;
}

void cgm_copyCastle(const bool src[4], bool dst[4]) {
  for (int i = 0; i < 4; i++) {
    dst[i] = src[i];
  }
}

void cgm_resetCastle(bool rights[4]) {
  rights[0] = true;
  rights[1] = true;
  rights[2] = true;
  rights[3] = true;
}

bool cgm_sameBoard(char a[8][8], char b[8][8]) {
  for (int r = 0; r < 8; r++) {
    for (int c = 0; c < 8; c++) {
      if (a[r][c] != b[r][c]) {
        return false;
      }
    }
  }
  return true;
}

// Recomputes castling rights by looking only at board state.
// This is intentionally conservative.
// Once a king or rook is missing from its original square, the right is gone.
void cgm_rebuildCastlingRightsFromBoard(const String& fen, bool rights[4]) {
  char board[8][8];
  if (!parseFENBoard(fen, board)) {
    rights[0] = false;
    rights[1] = false;
    rights[2] = false;
    rights[3] = false;
    return;
  }

  rights[0] = (board[7][4] == 'K' && board[7][7] == 'R');
  rights[1] = (board[7][4] == 'K' && board[7][0] == 'R');
  rights[2] = (board[0][4] == 'k' && board[0][7] == 'r');
  rights[3] = (board[0][4] == 'k' && board[0][0] == 'r');
}

String cgm_squareName(int row, int col) {
  char file = 'a' + col;
  char rank = '8' - row;
  String out = "";
  out += file;
  out += rank;
  return out;
}

// For LEDs or debug messages. Works for ordinary moves and castles.
bool cgm_findMoveSquares(const String& beforeFEN,
                         const String& afterFEN,
                         int& fromRow, int& fromCol,
                         int& toRow, int& toCol) {
  char before[8][8];
  char after[8][8];

  if (!parseFENBoard(beforeFEN, before) || !parseFENBoard(afterFEN, after)) {
    return false;
  }

  int sourceCount = 0;
  int destCount = 0;
  fromRow = -1;
  fromCol = -1;
  toRow = -1;
  toCol = -1;

  for (int r = 0; r < 8; r++) {
    for (int c = 0; c < 8; c++) {
      if (before[r][c] == after[r][c]) {
        continue;
      }

      if (before[r][c] != '.' && after[r][c] == '.') {
        sourceCount++;
        if (fromRow == -1) {
          fromRow = r;
          fromCol = c;
        }
      }

      if (after[r][c] != '.' && before[r][c] != after[r][c]) {
        destCount++;
        toRow = r;
        toCol = c;
      }
    }
  }

  if (sourceCount >= 1 && destCount >= 1) {
    return true;
  }

  return false;
}

// ============================================================
// UI and LED hooks
// Replace these bodies with your real display and LED calls.
// ============================================================

void cgm_uiMessage(const String& line1, const String& line2 = "") {
  Serial.print("[UI] ");
  Serial.print(line1);
  if (line2.length() > 0) {
    Serial.print(" | ");
    Serial.print(line2);
  }
  Serial.println();
}

void cgm_ledClear() {
  // Replace with your LED strip clear / show logic.
}

void cgm_ledShowMoveSquares(const String& beforeFEN, const String& afterFEN) {
  int fromRow, fromCol, toRow, toCol;
  if (cgm_findMoveSquares(beforeFEN, afterFEN, fromRow, fromCol, toRow, toCol)) {
    Serial.print("[LED] Highlight ");
    Serial.print(cgm_squareName(fromRow, fromCol));
    Serial.print(" -> ");
    Serial.println(cgm_squareName(toRow, toCol));
  } else {
    Serial.println("[LED] Could not infer move squares");
  }
}

// ============================================================
// Networking helpers
// ============================================================

void cgm_connectWiFi() {
  if (cgm_wifiConnected()) {
    return;
  }

  cgm_uiMessage("Connecting to WiFi");
  WiFi.mode(WIFI_STA);
  WiFi.begin(CGMConfig::WIFI_SSID, CGMConfig::WIFI_PASS);

  uint32_t start = millis();
  while (!cgm_wifiConnected() && millis() - start < 10000) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();

  if (cgm_wifiConnected()) {
    cgm_uiMessage("WiFi connected", WiFi.localIP().toString());
  } else {
    cgm_uiMessage("WiFi not connected");
  }
}

bool cgm_sendFenToServer(const String& fen, bool localIsWhite) {
  if (!cgm_wifiConnected()) {
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  if (!http.begin(client, CGMConfig::API_URL)) {
    return false;
  }

  http.addHeader("Content-Type", "application/json");

  DynamicJsonDocument doc(1024);
  doc["game_id"] = CGMConfig::GAME_ID;
  doc["board_number"] = CGMConfig::BOARD_NUMBER;
  doc["fen"] = fen;
  doc["player_color"] = localIsWhite ? "white" : "black";
  doc["mac_address"] = WiFi.macAddress();

  String payload;
  serializeJson(doc, payload);

  int code = http.POST(payload);
  String response = http.getString();

  Serial.print("[NET] POST code: ");
  Serial.println(code);
  if (response.length() > 0) {
    Serial.print("[NET] POST body: ");
    Serial.println(response);
  }

  http.end();
  return code >= 200 && code < 300;
}

bool cgm_fetchLatestFenFromServer(String& latestFen) {
  latestFen = "";

  if (!cgm_wifiConnected()) {
    return false;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  if (!http.begin(client, CGMConfig::API_URL)) {
    return false;
  }

  int code = http.GET();
  if (code <= 0) {
    http.end();
    return false;
  }

  String payload = http.getString();
  http.end();

  DynamicJsonDocument doc(8192);
  DeserializationError error = deserializeJson(doc, payload);
  if (error) {
    Serial.print("[NET] JSON parse failed: ");
    Serial.println(error.c_str());
    return false;
  }

  JsonArray moves = doc["moves"].as<JsonArray>();
  if (moves.isNull() || moves.size() == 0) {
    return false;
  }

  const char* fen = moves[moves.size() - 1]["fen"];
  if (fen == nullptr) {
    return false;
  }

  latestFen = String(fen);
  return true;
}

// ============================================================
// Game result helpers
// ============================================================

enum CGMGameResult {
  CGM_RESULT_NONE,
  CGM_RESULT_WHITE_WIN,
  CGM_RESULT_BLACK_WIN,
  CGM_RESULT_STALEMATE
};

CGMGameResult cgm_getGameResult(const String& fen, bool whiteToMove, const bool castling[4]) {
  char board[8][8];
  if (!parseFENBoard(fen, board)) {
    return CGM_RESULT_NONE;
  }

  bool inCheck = isKingInCheck(board, whiteToMove);
  bool hasMove = hasAnyLegalMove(board, whiteToMove, castling);

  if (hasMove) {
    return CGM_RESULT_NONE;
  }

  if (inCheck) {
    return whiteToMove ? CGM_RESULT_BLACK_WIN : CGM_RESULT_WHITE_WIN;
  }

  return CGM_RESULT_STALEMATE;
}

String cgm_resultToString(CGMGameResult result) {
  if (result == CGM_RESULT_WHITE_WIN) return "White wins by checkmate";
  if (result == CGM_RESULT_BLACK_WIN) return "Black wins by checkmate";
  if (result == CGM_RESULT_STALEMATE) return "Draw by stalemate";
  return "Game active";
}

// ============================================================
// Finite State Machine
// ============================================================

enum CGMState {
  CGM_WAIT_FOR_GAME_START,
  CGM_GAME_INITIALIZATION,
  CGM_LOCAL_TURN_WAIT_FOR_BOARD,
  CGM_LOCAL_TURN_VALIDATE,
  CGM_LOCAL_TURN_CONFIRM,
  CGM_SEND_STATE,
  CGM_WAIT_FOR_REMOTE_MOVE,
  CGM_APPLY_REMOTE_MOVE,
  CGM_GAME_END,
  CGM_ERROR_STATE
};

struct ChessGameManager {
  CGMState state;

  bool localIsWhite;
  bool whiteToMove;
  bool gameActive;

  bool castling[4];

  String committedFEN;
  String pendingFEN;
  String remoteIncomingFEN;

  String stableCandidateFEN;
  uint32_t stableSinceMs;

  uint32_t lastPollMs;
  uint32_t lastWifiRetryMs;
  uint32_t lastStatusPrintMs;

  CGMGameResult result;
};

ChessGameManager cgm;

void cgm_resetManager() {
  cgm.state = CGM_WAIT_FOR_GAME_START;
  cgm.localIsWhite = CGMConfig::LOCAL_IS_WHITE;
  cgm.whiteToMove = true;
  cgm.gameActive = false;
  cgm_resetCastle(cgm.castling);
  cgm.committedFEN = cgm_startFen();
  cgm.pendingFEN = "";
  cgm.remoteIncomingFEN = "";
  cgm.stableCandidateFEN = "";
  cgm.stableSinceMs = 0;
  cgm.lastPollMs = 0;
  cgm.lastWifiRetryMs = 0;
  cgm.lastStatusPrintMs = 0;
  cgm.result = CGM_RESULT_NONE;
}

void cgm_startGameNow() {
  cgm.state = CGM_GAME_INITIALIZATION;
}

void cgm_finishGame(CGMGameResult result) {
  cgm.result = result;
  cgm.gameActive = false;
  cgm.state = CGM_GAME_END;
  cgm_ledClear();
  cgm_uiMessage("Game over", cgm_resultToString(result));
}

bool cgm_isLocalTurn() {
  return cgm.whiteToMove == cgm.localIsWhite;
}

void cgm_setState(CGMState newState) {
  cgm.state = newState;
}

void cgm_beginWaitingForStableBoard() {
  cgm.stableCandidateFEN = "";
  cgm.stableSinceMs = 0;
}

bool cgm_candidateBoardReady(String& candidateFen) {
  candidateFen = cgm_boardOnlyFen(cgm_physicalBoardFEN);

  if (candidateFen.length() == 0) {
    return false;
  }

  if (cgm_sameBoardFen(candidateFen, cgm.committedFEN)) {
    cgm.stableCandidateFEN = "";
    cgm.stableSinceMs = 0;
    return false;
  }

  if (candidateFen != cgm.stableCandidateFEN) {
    cgm.stableCandidateFEN = candidateFen;
    cgm.stableSinceMs = millis();
    return false;
  }

  if (millis() - cgm.stableSinceMs < CGMConfig::LOCAL_STABLE_TIME_MS) {
    return false;
  }

  return true;
}

void cgm_handleWaitForGameStart() {
  static bool bannerShown = false;

  if (!bannerShown) {
    cgm_uiMessage("Ready", "Call cgm_startGameNow() or map it to a button");
    bannerShown = true;
  }

  if (cgm_newGameRequested) {
    cgm_newGameRequested = false;
    cgm_startGameNow();
    bannerShown = false;
  }
}

void cgm_handleGameInitialization() {
  cgm.gameActive = true;
  cgm.committedFEN = cgm_startFen();
  cgm.pendingFEN = "";
  cgm.remoteIncomingFEN = "";
  cgm.whiteToMove = true;
  cgm_resetCastle(cgm.castling);
  cgm_beginWaitingForStableBoard();
  cgm_moveConfirmed = false;
  cgm_moveCancelled = false;
  cgm_requestedPromotion = CGMConfig::DEFAULT_PROMOTION;

  cgm_uiMessage("Game initialized", cgm.localIsWhite ? "You are white" : "You are black");

  if (cgm_isLocalTurn()) {
    cgm_uiMessage("Your turn", "Make a move on the board");
    cgm_setState(CGM_LOCAL_TURN_WAIT_FOR_BOARD);
  } else {
    cgm_uiMessage("Waiting for opponent", "Black waits for white" );
    cgm_setState(CGM_WAIT_FOR_REMOTE_MOVE);
  }
}

void cgm_handleLocalTurnWaitForBoard() {
  String candidateFen;
  if (!cgm_candidateBoardReady(candidateFen)) {
    return;
  }

  cgm.pendingFEN = candidateFen;
  cgm_setState(CGM_LOCAL_TURN_VALIDATE);
}

void cgm_handleLocalTurnValidate() {
  String validated = validateMoveAndReturnFEN(
    cgm.committedFEN,
    cgm.pendingFEN,
    cgm.whiteToMove,
    cgm.castling,
    cgm_requestedPromotion
  );

  if (validated == "Invalid Move") {
    cgm_uiMessage("Illegal move", "Reset pieces and try again");
    cgm_ledShowMoveSquares(cgm.committedFEN, cgm.pendingFEN);
    cgm_beginWaitingForStableBoard();
    cgm.pendingFEN = "";
    cgm_setState(CGM_LOCAL_TURN_WAIT_FOR_BOARD);
    return;
  }

  cgm_uiMessage("Move valid", "Confirm on touchscreen");
  cgm_ledShowMoveSquares(cgm.committedFEN, validated);
  cgm_setState(CGM_LOCAL_TURN_CONFIRM);
}

void cgm_handleLocalTurnConfirm() {
  if (cgm_moveCancelled) {
    cgm_moveCancelled = false;
    cgm_uiMessage("Move cancelled", "Restore board and try again");
    cgm_beginWaitingForStableBoard();
    cgm.pendingFEN = "";
    cgm_ledClear();
    cgm_setState(CGM_LOCAL_TURN_WAIT_FOR_BOARD);
    return;
  }

  if (!cgm_moveConfirmed) {
    return;
  }

  cgm_moveConfirmed = false;
  cgm_setState(CGM_SEND_STATE);
}

void cgm_handleSendState() {
  bool newRights[4];
  cgm_rebuildCastlingRightsFromBoard(cgm.pendingFEN, newRights);
  cgm_copyCastle(newRights, cgm.castling);

  if (!cgm_sendFenToServer(cgm.pendingFEN, cgm.localIsWhite)) {
    cgm_uiMessage("Send failed", "Will retry automatically");
    return;
  }

  cgm.committedFEN = cgm.pendingFEN;
  cgm.pendingFEN = "";
  cgm_ledClear();

  cgm.whiteToMove = !cgm.whiteToMove;

  CGMGameResult result = cgm_getGameResult(cgm.committedFEN, cgm.whiteToMove, cgm.castling);
  if (result != CGM_RESULT_NONE) {
    cgm_finishGame(result);
    return;
  }

  cgm_uiMessage("Move sent", "Waiting for opponent");
  cgm_setState(CGM_WAIT_FOR_REMOTE_MOVE);
}

void cgm_handleWaitForRemoteMove() {
  uint32_t now = millis();

  if (now - cgm.lastPollMs < CGMConfig::POLL_INTERVAL_MS) {
    return;
  }
  cgm.lastPollMs = now;

  String latestFen;
  if (!cgm_fetchLatestFenFromServer(latestFen)) {
    cgm_uiMessage("Polling failed");
    return;
  }

  latestFen = cgm_boardOnlyFen(latestFen);

  if (latestFen.length() == 0) {
    return;
  }

  if (cgm_sameBoardFen(latestFen, cgm.committedFEN)) {
    return;
  }

  cgm.remoteIncomingFEN = latestFen;
  cgm_setState(CGM_APPLY_REMOTE_MOVE);
}

void cgm_handleApplyRemoteMove() {
  cgm_ledShowMoveSquares(cgm.committedFEN, cgm.remoteIncomingFEN);
  cgm_uiMessage("Opponent move received", "Replicate move on physical board");

  String physicalNow = cgm_boardOnlyFen(cgm_physicalBoardFEN);
  if (!cgm_sameBoardFen(physicalNow, cgm.remoteIncomingFEN)) {
    return;
  }

  cgm.committedFEN = cgm.remoteIncomingFEN;
  cgm.remoteIncomingFEN = "";
  cgm_ledClear();

  cgm_rebuildCastlingRightsFromBoard(cgm.committedFEN, cgm.castling);
  cgm.whiteToMove = !cgm.whiteToMove;

  CGMGameResult result = cgm_getGameResult(cgm.committedFEN, cgm.whiteToMove, cgm.castling);
  if (result != CGM_RESULT_NONE) {
    cgm_finishGame(result);
    return;
  }

  cgm_beginWaitingForStableBoard();

  if (cgm_isLocalTurn()) {
    cgm_uiMessage("Your turn", "Make your move");
    cgm_setState(CGM_LOCAL_TURN_WAIT_FOR_BOARD);
  } else {
    cgm_uiMessage("Waiting for opponent");
    cgm_setState(CGM_WAIT_FOR_REMOTE_MOVE);
  }
}

void cgm_handleGameEnd() {
  if (!cgm_newGameRequested) {
    return;
  }

  cgm_newGameRequested = false;
  cgm_setState(CGM_GAME_INITIALIZATION);
}

void cgm_handleErrorState() {
  cgm_uiMessage("FSM error state");
}

void cgm_tick() {
  uint32_t now = millis();

  if (!cgm_wifiConnected() && now - cgm.lastWifiRetryMs >= CGMConfig::WIFI_RETRY_INTERVAL_MS) {
    cgm.lastWifiRetryMs = now;
    cgm_connectWiFi();
  }

  if (now - cgm.lastStatusPrintMs >= CGMConfig::STATUS_PRINT_INTERVAL_MS) {
    cgm.lastStatusPrintMs = now;
    Serial.print("[FSM] state=");
    Serial.print((int)cgm.state);
    Serial.print(" whiteToMove=");
    Serial.print(cgm.whiteToMove ? "W" : "B");
    Serial.print(" local=");
    Serial.print(cgm.localIsWhite ? "W" : "B");
    Serial.print(" fen=");
    Serial.println(cgm.committedFEN);
  }

  switch (cgm.state) {
    case CGM_WAIT_FOR_GAME_START:
      cgm_handleWaitForGameStart();
      break;

    case CGM_GAME_INITIALIZATION:
      cgm_handleGameInitialization();
      break;

    case CGM_LOCAL_TURN_WAIT_FOR_BOARD:
      cgm_handleLocalTurnWaitForBoard();
      break;

    case CGM_LOCAL_TURN_VALIDATE:
      cgm_handleLocalTurnValidate();
      break;

    case CGM_LOCAL_TURN_CONFIRM:
      cgm_handleLocalTurnConfirm();
      break;

    case CGM_SEND_STATE:
      cgm_handleSendState();
      break;

    case CGM_WAIT_FOR_REMOTE_MOVE:
      cgm_handleWaitForRemoteMove();
      break;

    case CGM_APPLY_REMOTE_MOVE:
      cgm_handleApplyRemoteMove();
      break;

    case CGM_GAME_END:
      cgm_handleGameEnd();
      break;

    case CGM_ERROR_STATE:
      cgm_handleErrorState();
      break;
  }
}

// ============================================================
// Arduino entry points
// ============================================================
// If you already have another setup() / loop() in your main sketch,
// move the body of these functions into that file and just call:
//
//   cgm_setup();
//   cgm_tick();
//
// from your existing setup() and loop().
// ============================================================

void cgm_setup() {
  Serial.begin(115200);
  delay(500);

  cgm_resetManager();
  cgm_connectWiFi();
  cgm_uiMessage("Chess FSM loaded");

  // Auto start by default so it is easy to test.
  cgm_startGameNow();
}

#ifndef CGM_USE_EXTERNAL_MAIN_LOOP
void setup() {
  cgm_setup();
}

void loop() {
  cgm_tick();
  delay(10);
}
#endif
