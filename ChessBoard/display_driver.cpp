#include "display_driver.h"
#include "DFRobot_GDL.h"

extern DFRobot_ST7365P_320x480_HW_SPI screen;

// -- Layout constants ----------------------------------------------------------
static constexpr int SCR_W = 480;
static constexpr int SCR_H = 320;
static constexpr int HEADER_H = 38;
static constexpr int STATUS_Y = 298; // y-start of bottom status bar (SCR_H - STATUS_H)
static constexpr int STATUS_H = 22;  // height of status bar
static constexpr int BACK_HIT_W = 80;
static constexpr int BACK_BTN_X = 6;
static constexpr int BACK_BTN_Y = 6;
static constexpr int BACK_BTN_W = 68;
static constexpr int BACK_BTN_H = 24;

// Chess-board geometry (landscape game screen)
static constexpr int BOARD_X = 8;            // left edge of the board
static constexpr int BOARD_Y = 40;           // top edge of the board
static constexpr int CELL_SIZE = 32;         // 8 � 32 = 256 px per side
static constexpr uint16_t SQ_LIGHT = 0xFFDF; // cream
static constexpr uint16_t SQ_DARK = 0x8C51;  // medium brown

// Right info panel next to the board
static constexpr int RPANEL_X = BOARD_X + 8 * CELL_SIZE + 8; // = 272
static constexpr int RPANEL_W = SCR_W - RPANEL_X - 8;        // = 200

// -- Internal helpers ----------------------------------------------------------

// Per-cell board cache � avoids repainting unchanged squares every frame.
// s_cachedBoard holds the logical content of every square as last drawn by drawFENBoard.
// s_dirtyCells is a 64-bit bitfield (bit r*8+c) marking squares whose visual state
// was overwritten by a highlight overlay and must be forcibly repainted next frame.
static char s_cachedBoard[8][8] = {};
static bool s_cacheFlipped = false;
static bool s_cacheValid = false;
static uint64_t s_dirtyCells = 0; // bit r*8+c ? square needs forced repaint

// Per-square overlay state, indexed by logical [rank][file].
// 0 = clean, 1 = extra piece (red), 2 = missing piece (dim).
// Avoids repainting every square every loop in drawBoardSyncOverlay.
static uint8_t s_overlayCache[8][8] = {};

// Chrome (header strip + left margin + right panel) cache.
// Only repaint these areas when WiFi status, player colour, or fenOk changes.
static bool s_chromeValid = false;
static bool s_chromeWifi = false;
static bool s_chromeIsWhite = false;
static bool s_chromeFenOk = false;
static bool s_chromeAiGame = false;

// Draw a chess-piece token: filled circle (radius 13) with the piece letter
// centred inside.  White pieces get a white disc with a black rim; black pieces
// get a black disc with a grey rim.  lightSquare affects the rim tint slightly.
static void drawPieceToken(int px, int py, char piece, bool lightSquare)
{
  bool whitePiece = (piece >= 'A' && piece <= 'Z');
  uint16_t tokenBg = whitePiece ? (uint16_t)COLOR_RGB565_WHITE : (uint16_t)COLOR_RGB565_BLACK;
  uint16_t tokenFg = whitePiece ? (uint16_t)COLOR_RGB565_BLACK : (uint16_t)COLOR_RGB565_WHITE;
  uint16_t tokenRim = lightSquare ? (uint16_t)COLOR_RGB565_BLACK : (uint16_t)COLOR_RGB565_MID_GREY;
  int cx = px + CELL_SIZE / 2;
  int cy = py + CELL_SIZE / 2;
  screen.fillCircle(cx, cy, 13, tokenBg);
  screen.drawCircle(cx, cy, 13, tokenRim);
  screen.setTextSize(2);
  screen.setTextColor(tokenFg);
  screen.setCursor(px + (CELL_SIZE - 12) / 2, py + (CELL_SIZE - 16) / 2);
  screen.print(piece);
}

static void drawBackButton(uint16_t bgColor = (uint16_t)COLOR_RGB565_DARK_GREY,
                           uint16_t fgColor = COLOR_RGB565_WHITE,
                           uint16_t borderColor = (uint16_t)COLOR_RGB565_MID_GREY)
{
  screen.fillRect(BACK_BTN_X, BACK_BTN_Y, BACK_BTN_W, BACK_BTN_H, bgColor);
  screen.drawRect(BACK_BTN_X, BACK_BTN_Y, BACK_BTN_W, BACK_BTN_H, borderColor);
  screen.setTextSize(1);
  screen.setTextColor(fgColor);
  screen.setCursor(BACK_BTN_X + 8, BACK_BTN_Y + 8);
  screen.print("< Back");
}

void invalidateBoardCache()
{
  s_cacheValid = false;
  s_dirtyCells = 0;
  s_chromeValid = false;                             // force chrome repaint on next drawGameScreen call
  memset(s_overlayCache, 0, sizeof(s_overlayCache)); // overlays are gone after a full repaint
}

// Parse a board-only FEN into an 8�8 char array ('.' = empty).
// Returns false if the FEN is malformed.
static bool fenToBoard(const String &fen, char board[8][8])
{
  int r = 0, c = 0;
  for (int i = 0; i < (int)fen.length(); i++)
  {
    char ch = fen[i];
    if (ch == ' ' || ch == '\0')
      break;
    if (ch == '/')
    {
      r++;
      c = 0;
      if (r >= 8)
        break;
      continue;
    }
    if (ch >= '1' && ch <= '8')
    {
      int skip = ch - '0';
      for (int k = 0; k < skip && c < 8; k++, c++)
        board[r][c] = '.';
      continue;
    }
    if (c < 8)
      board[r][c++] = ch;
  }
  return (r == 7);
}

// Render a board-only FEN as a proper 8�8 chessboard with coloured squares.
// Piece characters are drawn size-2 (12�16 px) centred in each cell.
// flipped=true rotates the board 180� (black pieces at the bottom).
static void drawFENBoard(const String &fen, int boardX, int boardY, int cellSize, bool flipped)
{
  char board[8][8];
  for (int r = 0; r < 8; r++)
    for (int c = 0; c < 8; c++)
      board[r][c] = '.';
  fenToBoard(fen, board);

  // Force a full repaint when the cache is cold or the orientation changed.
  bool forceAll = !s_cacheValid || (flipped != s_cacheFlipped);

  for (int r = 0; r < 8; r++)
  {
    // Rank labels only need redrawing when the cache is cold (forceAll),
    // because the left margin is only cleared during a chrome repaint, which
    // also invalidates the board cache.  Skip them on incremental cell updates.
    if (forceAll)
    {
      char rankLabel = flipped ? ('1' + r) : ('8' - r);
      screen.setTextSize(1);
      screen.setTextColor(COLOR_RGB565_BLACK);
      screen.setCursor(1, boardY + r * cellSize + (cellSize - 8) / 2);
      screen.print(rankLabel);
    }

    for (int c = 0; c < 8; c++)
    {
      int fenR = flipped ? (7 - r) : r;
      int fenC = flipped ? (7 - c) : c;

      // Skip squares whose content hasn't changed and have no overlay to clear.
      bool dirty = (s_dirtyCells >> (fenR * 8 + fenC)) & 1;
      bool changed = forceAll || dirty || (board[fenR][fenC] != s_cachedBoard[fenR][fenC]);
      if (!changed)
        continue;

      int px = boardX + c * cellSize;
      int py = boardY + r * cellSize;
      bool light = ((r + c) % 2 == 0);
      // Painting this square overwrites any overlay � clear the overlay cache entry.
      s_overlayCache[fenR][fenC] = 0;
      screen.fillRect(px, py, cellSize, cellSize, light ? SQ_LIGHT : SQ_DARK);
      if (board[fenR][fenC] != '.')
        drawPieceToken(px, py, board[fenR][fenC], light);
    }
  }
  screen.drawRect(boardX - 1, boardY - 1, 8 * cellSize + 2, 8 * cellSize + 2,
                  COLOR_RGB565_BLACK);

  // Commit cache.
  memcpy(s_cachedBoard, board, sizeof(board));
  s_cacheFlipped = flipped;
  s_cacheValid = true;
  s_dirtyCells = 0;
}

// -- Primitives ----------------------------------------------------------------
void displayClear()
{
  screen.fillScreen(COLOR_RGB565_WHITE);
}

void displayHeader(bool wifiConnected, bool showBack)
{
  screen.setTextSize(1);
  screen.setTextColor(COLOR_RGB565_BLACK);

  if (showBack)
    drawBackButton();

  screen.setCursor(showBack ? 92 : 20, 20);
  screen.print("ChessBoard");

  // WiFi settings button � top-right corner
  uint16_t wifiBtnBg = wifiConnected ? (uint16_t)COLOR_RGB565_GREEN : COLOR_RGB565_DARK_GREY;
  screen.fillRect(WIFI_SETTINGS_BTN_X, WIFI_SETTINGS_BTN_Y,
                  WIFI_SETTINGS_BTN_W, WIFI_SETTINGS_BTN_H, wifiBtnBg);
  screen.drawRect(WIFI_SETTINGS_BTN_X, WIFI_SETTINGS_BTN_Y,
                  WIFI_SETTINGS_BTN_W, WIFI_SETTINGS_BTN_H, COLOR_RGB565_MID_GREY);
  screen.setTextSize(1);
  screen.setTextColor(COLOR_RGB565_WHITE);
  const char *wifiLabel = wifiConnected ? "WiFi: OK" : "WiFi: --";
  // Centre the 8-char label (48 px wide at size-1) inside the 80 px button
  screen.setCursor(WIFI_SETTINGS_BTN_X + (WIFI_SETTINGS_BTN_W - 48) / 2,
                   WIFI_SETTINGS_BTN_Y + (WIFI_SETTINGS_BTN_H - 8) / 2);
  screen.print(wifiLabel);

  screen.drawFastHLine(0, 38, SCR_W, COLOR_RGB565_BLACK);
}

void displayCenteredText(const char *text, int y, uint8_t size, uint16_t color)
{
  int textW = (int)strlen(text) * 6 * size;
  int x = (SCR_W - textW) / 2;
  if (x < 0)
    x = 0;
  screen.setTextSize(size);
  screen.setTextColor(color);
  screen.setCursor(x, y);
  screen.print(text);
}

void displayButton(int x, int y, int w, int h, uint16_t color, const char *label)
{
  screen.fillRect(x, y, w, h, color);
  // Draw a border so white buttons are visible against a white background
  screen.drawRect(x, y, w, h, COLOR_RGB565_BLACK);
  screen.setTextSize(2);
  // Weighted luminance (0.299R + 0.587G + 0.114B) to pick legible text colour.
  uint8_t r5 = (color >> 11) & 0x1F;
  uint8_t g6 = (color >> 5) & 0x3F;
  uint8_t b5 = color & 0x1F;
  uint8_t r8b = (uint8_t)(r5 * 255 / 31);
  uint8_t g8b = (uint8_t)(g6 * 255 / 63);
  uint8_t b8b = (uint8_t)(b5 * 255 / 31);
  uint16_t lum = (uint16_t)((299u * r8b + 587u * g8b + 114u * b8b) / 1000u);
  screen.setTextColor(lum > 128 ? (uint16_t)COLOR_RGB565_BLACK : (uint16_t)COLOR_RGB565_WHITE);
  int textX = x + (w - (int)strlen(label) * 12) / 2;
  int textY = y + (h - 16) / 2;
  screen.setCursor(textX, textY);
  screen.print(label);
}

void displayStatusBar(const char *msg, uint16_t bgColor)
{
  screen.fillRect(0, STATUS_Y, SCR_W, STATUS_H, bgColor);
  screen.setTextSize(1);
  // Weighted luminance (0.299R + 0.587G + 0.114B) chooses legible text colour.
  uint8_t r8 = (uint8_t)(((bgColor >> 11) & 0x1F) * 255 / 31);
  uint8_t g8 = (uint8_t)(((bgColor >> 5) & 0x3F) * 255 / 63);
  uint8_t b8 = (uint8_t)((bgColor & 0x1F) * 255 / 31);
  uint16_t lum = (uint16_t)((299u * r8 + 587u * g8 + 114u * b8) / 1000u);
  screen.setTextColor(lum > 128 ? (uint16_t)COLOR_RGB565_BLACK : (uint16_t)COLOR_RGB565_WHITE);
  screen.setCursor(6, STATUS_Y + 7);
  screen.print(msg);
}

void displayDivider(int y, uint16_t color)
{
  screen.drawFastHLine(0, y, SCR_W, color);
}

// -- Full screens --------------------------------------------------------------
void initDisplay()
{
  screen.begin();
  screen.setRotation(3); // landscape: left side of PCB is now the bottom
  screen.setTextWrap(false);
}

void drawConnectingScreen(const char *ssid)
{
  // Cover the full splash region so previous screens cannot bleed through.
  screen.fillRect(0, 0, SCR_W, 185, COLOR_RGB565_WHITE);
  displayCenteredText("ChessBoard", 96, 3, COLOR_RGB565_BLACK);
  displayCenteredText("Connecting to WiFi...", 156, 1, COLOR_RGB565_BLACK);
  displayCenteredText(ssid, 176, 1, COLOR_RGB565_BLUE);
  screen.fillRect(0, 185, SCR_W, STATUS_Y - 185, COLOR_RGB565_WHITE);
  displayStatusBar("Please wait...", COLOR_RGB565_BLUE);
  invalidateBoardCache();
}

void drawMenuScreen(bool wifiConnected)
{
  // Header band: fill + draw together so header appears immediately.
  screen.fillRect(0, 0, SCR_W, BOARD_Y, COLOR_RGB565_WHITE);
  displayHeader(wifiConnected);
  // Body band: fill + draw title together.
  screen.fillRect(0, BOARD_Y, SCR_W, STATUS_Y - BOARD_Y, COLOR_RGB565_WHITE);
  displayCenteredText("Chess Board", 58, 2, COLOR_RGB565_BLACK);
  invalidateBoardCache();
}

void drawTimerModeScreen(bool wifiConnected, const char *title)
{
  screen.fillRect(0, 0, SCR_W, SCR_H, COLOR_RGB565_WHITE);
  displayHeader(wifiConnected, true);
  displayCenteredText(title, 48, 2, COLOR_RGB565_BLACK);

  displayButton(TIMER_BTN_X, TIMER_BTN_UNLIM_Y, TIMER_BTN_W, TIMER_BTN_H,
                COLOR_RGB565_WHITE, "Unlimited");
  displayButton(TIMER_BTN_X, TIMER_BTN_RAPID_Y, TIMER_BTN_W, TIMER_BTN_H,
                COLOR_RGB565_BLUE, "Rapid  (10:00 / side)");
  displayButton(TIMER_BTN_X, TIMER_BTN_BULLET_Y, TIMER_BTN_W, TIMER_BTN_H,
                COLOR_RGB565_RED, "Bullet  (5:00 / side)");

  displayStatusBar("Choose a time control for this game", COLOR_RGB565_BLUE);
  invalidateBoardCache();
}

void drawGameScreen(bool wifiConnected, bool fenOk, const String &data, bool localIsWhite, bool aiGame)
{
  // -- Chrome cache --------------------------------------------------------
  // The header strip, left margin, and right panel only need repainting when
  // WiFi status, player colour, or board validity changes.  For normal move
  // updates none of these change, so zero chrome pixels are written.
  bool needChrome = !s_chromeValid || s_chromeWifi != wifiConnected || s_chromeIsWhite != localIsWhite || s_chromeFenOk != fenOk || s_chromeAiGame != aiGame;

  if (needChrome)
  {
    // Clearing the left margin invalidates the rank labels that live there,
    // so force a full board repaint so they are redrawn.
    s_cacheValid = false;
    s_dirtyCells = 0;

    screen.fillRect(0, 0, SCR_W, BOARD_Y, COLOR_RGB565_WHITE);
    screen.fillRect(0, BOARD_Y, BOARD_X - 1, 8 * CELL_SIZE + 2, COLOR_RGB565_WHITE);
    screen.fillRect(BOARD_X + 8 * CELL_SIZE + 1, BOARD_Y - 1,
                    SCR_W - (BOARD_X + 8 * CELL_SIZE + 1),
                    STATUS_Y - (BOARD_Y - 1),
                    COLOR_RGB565_WHITE);
    displayHeader(wifiConnected, true);

    screen.drawRect(RPANEL_X - 2, BOARD_Y, RPANEL_W + 2, 8 * CELL_SIZE, COLOR_RGB565_MID_GREY);

    const char *topLabel = aiGame ? "STOCKFISH" : (localIsWhite ? "BLACK" : "WHITE");
    const char *botLabel = localIsWhite ? "WHITE" : "BLACK";
    uint16_t topBg = localIsWhite ? COLOR_RGB565_BLACK : COLOR_RGB565_WHITE;
    uint16_t botBg = localIsWhite ? COLOR_RGB565_WHITE : COLOR_RGB565_BLACK;
    uint16_t topFg = localIsWhite ? COLOR_RGB565_WHITE : COLOR_RGB565_BLACK;
    uint16_t botFg = localIsWhite ? COLOR_RGB565_BLACK : COLOR_RGB565_WHITE;

    screen.fillRect(RPANEL_X, BOARD_Y, RPANEL_W, 22, topBg);
    screen.drawRect(RPANEL_X, BOARD_Y, RPANEL_W, 22, COLOR_RGB565_MID_GREY);
    screen.setTextSize(1);
    screen.setTextColor(topFg);
    screen.setCursor(RPANEL_X + 4, BOARD_Y + 7);
    screen.print(topLabel);
    screen.print(" (opp)");

    int botY = BOARD_Y + 8 * CELL_SIZE - 22;
    screen.fillRect(RPANEL_X, botY, RPANEL_W, 22, botBg);
    screen.drawRect(RPANEL_X, botY, RPANEL_W, 22, COLOR_RGB565_MID_GREY);
    screen.setTextColor(botFg);
    screen.setCursor(RPANEL_X + 4, botY + 7);
    screen.print(botLabel);
    screen.print(" (you)");

    displayStatusBar(
        fenOk ? "Game active" : "Waiting...",
        fenOk ? COLOR_RGB565_GREEN : COLOR_RGB565_BLUE);

    s_chromeValid = true;
    s_chromeWifi = wifiConnected;
    s_chromeIsWhite = localIsWhite;
    s_chromeFenOk = fenOk;
    s_chromeAiGame = aiGame;
  }

  if (fenOk)
  {
    drawFENBoard(data, BOARD_X, BOARD_Y, CELL_SIZE, !localIsWhite);
  }
  else
  {
    screen.fillRect(BOARD_X - 1, BOARD_Y - 1, 8 * CELL_SIZE + 2, 8 * CELL_SIZE + 2,
                    COLOR_RGB565_WHITE);
    screen.drawRect(BOARD_X - 1, BOARD_Y - 1, 8 * CELL_SIZE + 2, 8 * CELL_SIZE + 2,
                    COLOR_RGB565_BLACK);
    screen.setTextSize(1);
    screen.setTextColor(COLOR_RGB565_BLACK);
    screen.setCursor(BOARD_X + 8, BOARD_Y + (8 * CELL_SIZE) / 2 - 4);
    screen.print(data);
    s_cacheValid = false;
  }
}

// ---------------------------------------------------------------------------
// drawGameScreenWithMove
// Redraws the full board from afterFEN, then highlights:
//   - source square (where piece was) in yellow
//   - destination square (where piece went) in green
// ---------------------------------------------------------------------------
void drawGameScreenWithMove(bool wifiConnected,
                            const String &beforeFEN,
                            const String &afterFEN,
                            bool localIsWhite,
                            bool aiGame)
{
  drawGameScreen(wifiConnected, afterFEN.length() > 0, afterFEN, localIsWhite, aiGame);

  if (beforeFEN.length() == 0 || afterFEN.length() == 0)
    return;

  char before[8][8], after[8][8];
  for (int r = 0; r < 8; r++)
    for (int c = 0; c < 8; c++)
      before[r][c] = after[r][c] = '.';

  fenToBoard(beforeFEN, before);
  fenToBoard(afterFEN, after);

  // Flip board for black player so their pieces are at the bottom.
  bool flipped = !localIsWhite;
  for (int r = 0; r < 8; r++)
  {
    for (int c = 0; c < 8; c++)
    {
      if (before[r][c] == after[r][c])
        continue;

      int dr = flipped ? (7 - r) : r;
      int dc = flipped ? (7 - c) : c;
      int px = BOARD_X + dc * CELL_SIZE;
      int py = BOARD_Y + dr * CELL_SIZE;

      // Mark the square dirty so the next drawFENBoard call repaints it,
      // clearing the highlight colour regardless of FEN content.
      s_dirtyCells |= (1ULL << (r * 8 + c));

      if (before[r][c] != '.' && after[r][c] == '.')
      {
        // Source square (piece left): yellow
        screen.fillRect(px, py, CELL_SIZE, CELL_SIZE, COLOR_RGB565_YELLOW);
      }
      else if (after[r][c] != '.')
      {
        // Destination square: green highlight with piece token
        screen.fillRect(px, py, CELL_SIZE, CELL_SIZE, COLOR_RGB565_GREEN);
        drawPieceToken(px, py, after[r][c], false);
      }
    }
  }
}

// ---------------------------------------------------------------------------
// drawBoardSyncOverlay
// Overlays mismatch highlights on an already-drawn board without full redraw.
//   Extra piece  (physical!='.', logical=='.')  ? red tint over the square
//   Missing piece (logical!='.', physical=='.') ? piece letter in dim red
//
// Uses s_overlayCache to skip squares whose mismatch state hasn't changed
// since the last call � so only newly-changed squares write any SPI pixels.
// ---------------------------------------------------------------------------
void drawBoardSyncOverlay(const String &logicalFEN, const String &physicalFEN, bool flipped)
{
  if (logicalFEN.length() == 0 || physicalFEN.length() == 0)
    return;

  char logical[8][8], physical[8][8];
  for (int r = 0; r < 8; r++)
    for (int c = 0; c < 8; c++)
      logical[r][c] = physical[r][c] = '.';

  fenToBoard(logicalFEN, logical);
  fenToBoard(physicalFEN, physical);

  // Convert logical piece to expected physical polarity
  auto toPhys = [](char lc) -> char
  {
    if (lc == '.')
      return '.';
    return (lc >= 'A' && lc <= 'Z') ? 'P' : 'p';
  };

  static constexpr uint16_t EXTRA_COL = COLOR_RGB565_DEEP_RED;  // extra piece tint
  static constexpr uint16_t MISSING_COL = COLOR_RGB565_DIM_RED; // missing piece text

  for (int r = 0; r < 8; r++)
  {
    for (int c = 0; c < 8; c++)
    {
      char lp = toPhys(logical[r][c]);
      char pp = physical[r][c];

      // Determine new overlay state for this square.
      uint8_t newState;
      if (lp == pp)
        newState = 0; // correct � no overlay
      else if (pp != '.' && lp == '.')
        newState = 1; // extra piece present
      else if (pp == '.' && logical[r][c] != '.')
        newState = 2; // piece missing
      else
        newState = 0;

      // Skip squares whose visual state hasn't changed � zero SPI writes.
      uint8_t oldState = s_overlayCache[r][c];
      if (newState == oldState)
        continue;

      s_overlayCache[r][c] = newState;

      // Map logical row/col to display pixel.
      int dr = flipped ? (7 - r) : r;
      int dc = flipped ? (7 - c) : c;
      int px = BOARD_X + dc * CELL_SIZE;
      int py = BOARD_Y + dr * CELL_SIZE;
      bool light = ((dr + dc) % 2 == 0);

      if (newState == 0)
      {
        // Mismatch resolved � restore the square to its normal appearance.
        screen.fillRect(px, py, CELL_SIZE, CELL_SIZE, light ? SQ_LIGHT : SQ_DARK);
        if (logical[r][c] != '.')
          drawPieceToken(px, py, logical[r][c], light);
      }
      else if (newState == 1)
      {
        // Extra piece: fill square with a red tint.
        screen.fillRect(px, py, CELL_SIZE, CELL_SIZE, EXTRA_COL);
        // Mark dirty so drawFENBoard clears this overlay if it runs first.
        s_dirtyCells |= (1ULL << (r * 8 + c));
      }
      else // newState == 2
      {
        // Missing piece: normal background + piece letter and rim in dim red.
        screen.fillRect(px, py, CELL_SIZE, CELL_SIZE, light ? SQ_LIGHT : SQ_DARK);
        screen.drawCircle(px + CELL_SIZE / 2, py + CELL_SIZE / 2, 13, MISSING_COL);
        screen.setTextSize(2);
        screen.setTextColor(MISSING_COL);
        screen.setCursor(px + (CELL_SIZE - 12) / 2, py + (CELL_SIZE - 16) / 2);
        screen.print(logical[r][c]);
        s_dirtyCells |= (1ULL << (r * 8 + c));
      }
    }
  }
}

// ---------------------------------------------------------------------------
// drawCheckAlert  � yellow banner overlaid on the game screen
// ---------------------------------------------------------------------------
void drawCheckAlert(bool whiteInCheck)
{
  // Yellow banner inside the right info panel top strip (within the border).
  screen.fillRect(RPANEL_X, BOARD_Y, RPANEL_W, 22, COLOR_RGB565_YELLOW);
  screen.setTextSize(1);
  screen.setTextColor(COLOR_RGB565_BLACK);
  const char *msg = whiteInCheck ? "!! WHITE IN CHECK !!" : "!! BLACK IN CHECK !!";
  screen.setCursor(RPANEL_X + 2, BOARD_Y + 7);
  screen.print(msg);
}

// Restore the top player-label strip after the check-alert banner is dismissed.
// Uses the cached chrome state � zero board pixels are repainted.
void clearCheckAlert()
{
  const char *topLabel = s_chromeAiGame    ? "STOCKFISH"
                         : s_chromeIsWhite ? "BLACK"
                                           : "WHITE";
  uint16_t topBg = s_chromeIsWhite ? (uint16_t)COLOR_RGB565_BLACK : (uint16_t)COLOR_RGB565_WHITE;
  uint16_t topFg = s_chromeIsWhite ? (uint16_t)COLOR_RGB565_WHITE : (uint16_t)COLOR_RGB565_BLACK;
  screen.fillRect(RPANEL_X, BOARD_Y, RPANEL_W, 22, topBg);
  screen.drawRect(RPANEL_X, BOARD_Y, RPANEL_W, 22, COLOR_RGB565_MID_GREY);
  screen.setTextSize(1);
  screen.setTextColor(topFg);
  screen.setCursor(RPANEL_X + 4, BOARD_Y + 7);
  screen.print(topLabel);
  screen.print(" (opp)");
}

// ---------------------------------------------------------------------------
// drawGameOverScreen  � full-panel shown when the game ends
// ---------------------------------------------------------------------------
void drawGameOverScreen(const char *resultLine)
{
  // fillScreen is not needed � the two fillRects below cover the entire screen.
  // Invalidate caches so the game screen fully repaints if the player starts a new game.
  invalidateBoardCache();

  uint16_t bannerCol = COLOR_RGB565_RED;
  if (strstr(resultLine, "stalemate") || strstr(resultLine, "Draw") ||
      strstr(resultLine, "draw"))
    bannerCol = COLOR_RGB565_PALE_GREY; // neutral colour for draw/stalemate

  screen.fillRect(0, 0, SCR_W, 58, bannerCol);
  screen.setTextSize(3);
  screen.setTextColor(COLOR_RGB565_WHITE);
  // "GAME OVER" � size-3 glyph: 18 px wide � 9 chars = 162 px
  screen.setCursor((SCR_W - 162) / 2, 16);
  screen.print("GAME OVER");

  screen.fillRect(0, 58, SCR_W, SCR_H - 58 - STATUS_H, COLOR_RGB565_BLACK);
  screen.setTextWrap(true);
  screen.setTextSize(2);
  screen.setTextColor(COLOR_RGB565_YELLOW);
  int textW = (int)strlen(resultLine) * 12;
  int x = (SCR_W - textW) / 2;
  if (x < 6)
    x = 6;
  screen.setCursor(x, 118);
  screen.print(resultLine);
  screen.setTextWrap(false);

  displayStatusBar("Tap to play again", bannerCol);
}

// ---------------------------------------------------------------------------
// drawPiecePickedUp  � small overlay when a piece leaves its square
// ---------------------------------------------------------------------------
void drawPiecePickedUp(const char *squareName)
{
  // Orange banner inside the right info panel, below any check alert (within border).
  screen.fillRect(RPANEL_X, BOARD_Y + 26, RPANEL_W, 22, COLOR_RGB565_ORANGE);
  screen.setTextSize(1);
  screen.setTextColor(COLOR_RGB565_WHITE);
  char buf[32];
  snprintf(buf, sizeof(buf), "  Lifted: %s", squareName ? squareName : "?");
  screen.setCursor(RPANEL_X + 2, BOARD_Y + 33);
  screen.print(buf);
}

// Clear the piece-lift banner by restoring white panel background for that strip.
void clearPieceLiftOverlay()
{
  screen.fillRect(RPANEL_X, BOARD_Y + 26, RPANEL_W, 22, COLOR_RGB565_WHITE);
}

// Clear the confirm/cancel buttons by filling their area with the panel background.
// Also restores the "you" player-label strip at the bottom of the right panel
// (which was overwritten by the cancel button) so the caller only needs to
// redraw the timer value on top.
void clearConfirmOverlay()
{
  // Fill the button area above the bottom label strip.
  screen.fillRect(RPANEL_X, 200, RPANEL_W, 74, COLOR_RGB565_WHITE); // y=200..274

  // Restore the bottom player-label strip (same logic as in drawGameScreen chrome).
  int botY = BOARD_Y + 8 * CELL_SIZE - 22; // = 274
  uint16_t botBg = s_chromeIsWhite
                       ? (uint16_t)COLOR_RGB565_WHITE
                       : (uint16_t)COLOR_RGB565_BLACK;
  uint16_t botFg = s_chromeIsWhite
                       ? (uint16_t)COLOR_RGB565_BLACK
                       : (uint16_t)COLOR_RGB565_WHITE;
  screen.fillRect(RPANEL_X, botY, RPANEL_W, 22, botBg);
  screen.drawRect(RPANEL_X, botY, RPANEL_W, 22, COLOR_RGB565_MID_GREY);
  screen.setTextSize(1);
  screen.setTextColor(botFg);
  screen.setCursor(RPANEL_X + 4, botY + 7);
  screen.print(s_chromeIsWhite ? "WHITE" : "BLACK");
  screen.print(" (you)");
}

// ---------------------------------------------------------------------------
// drawTimerDisplay � overlay clock values on both player-label strips
// ---------------------------------------------------------------------------
// Paints only the rightmost 64 px of each strip so the label text is preserved.
// Opponent clock ? top strip (y = BOARD_Y..BOARD_Y+22).
// My clock       ? bottom strip (y = BOARD_Y+234..BOARD_Y+256, i.e. y=274..296).
// ---------------------------------------------------------------------------
void drawTimerDisplay(int32_t whiteMs, int32_t blackMs,
                      bool clockRunning, bool clockForWhite,
                      bool localIsWhite)
{
  // Format mm:ss; if time < 0 clamp to 0.
  auto fmtTime = [](char *buf, int32_t ms)
  {
    if (ms < 0)
      ms = 0;
    int secs = (int)(ms / 1000);
    int mins = secs / 60;
    secs %= 60;
    snprintf(buf, 8, "%02d:%02d", mins, secs);
  };

  char oppBuf[8], myBuf[8];
  int32_t oppMs = localIsWhite ? blackMs : whiteMs;
  int32_t myMs = localIsWhite ? whiteMs : blackMs;
  bool oppRunning = clockRunning && (clockForWhite != localIsWhite);
  bool myRunning = clockRunning && (clockForWhite == localIsWhite);

  fmtTime(oppBuf, oppMs);
  fmtTime(myBuf, myMs);

  // -- Opponent clock (top label strip) -------------------------------------
  int topY = BOARD_Y; // 40
  // Background: bright green when running, dark when paused
  uint16_t oppBg = oppRunning ? (uint16_t)COLOR_RGB565_GREEN
                              : COLOR_RGB565_DARK_NAVY; // dark navy
  uint16_t oppFg = oppRunning ? (uint16_t)COLOR_RGB565_BLACK
                              : (uint16_t)COLOR_RGB565_WHITE;
  int timerX = RPANEL_X + RPANEL_W - 64;
  screen.fillRect(timerX, topY + 1, 62, 20, oppBg);
  screen.setTextSize(1);
  screen.setTextColor(oppFg);
  screen.setCursor(timerX + 8, topY + 7);
  screen.print(oppBuf);

  // -- My clock (bottom label strip) ----------------------------------------
  int botY = BOARD_Y + 8 * CELL_SIZE - 22; // 274
  uint16_t myBg = myRunning ? (uint16_t)COLOR_RGB565_GREEN
                            : COLOR_RGB565_DARK_NAVY;
  uint16_t myFg = myRunning ? (uint16_t)COLOR_RGB565_BLACK
                            : (uint16_t)COLOR_RGB565_WHITE;
  screen.fillRect(timerX, botY + 1, 62, 20, myBg);
  screen.setTextColor(myFg);
  screen.setCursor(timerX + 8, botY + 7);
  screen.print(myBuf);
}

void drawPromotionPicker(bool isWhite)
{
  // Clear the right info panel area with a dark navy background
  screen.fillRect(RPANEL_X - 2, BOARD_Y, RPANEL_W + 2, 8 * CELL_SIZE, COLOR_RGB565_DARK_PURPLE);

  // Title
  screen.setTextSize(1);
  screen.setTextColor(COLOR_RGB565_WHITE);
  screen.setCursor(RPANEL_X + 6, BOARD_Y + 10);
  screen.print("Promote pawn to:");

  // Four buttons: Queen, Rook, Bishop, Knight
  struct
  {
    const char *label;
    uint16_t bg;
    char piece; // for future use
  } choices[4] = {
      {"Queen", COLOR_RGB565_GOLD, isWhite ? 'Q' : 'q'},
      {"Rook", COLOR_RGB565_DARK_GREY, isWhite ? 'R' : 'r'},   // was 0x7BEF (low contrast)
      {"Bishop", COLOR_RGB565_DARK_NAVY, isWhite ? 'B' : 'b'}, // was 0x07E0 (unreadable green)
      {"Knight", COLOR_RGB565_BLUE, isWhite ? 'N' : 'n'},
  };

  const int btnX = PROMO_BTN_X;
  const int btnW = PROMO_BTN_W;
  const int btnH = PROMO_BTN_H;

  for (int i = 0; i < 4; i++)
  {
    int btnY = PROMO_BTN_Y0 + i * (btnH + PROMO_BTN_GAP);
    screen.fillRect(btnX, btnY, btnW, btnH, choices[i].bg);
    screen.drawRect(btnX, btnY, btnW, btnH, COLOR_RGB565_WHITE);

    // Piece letter (size-3) on the left
    screen.setTextSize(3);
    screen.setTextColor(COLOR_RGB565_WHITE);
    screen.setCursor(btnX + 8, btnY + (btnH - 24) / 2);
    screen.print(choices[i].piece);

    // Piece name (size-2) to the right of the letter
    screen.setTextSize(2);
    screen.setCursor(btnX + 34, btnY + (btnH - 16) / 2);
    screen.print(choices[i].label);
  }
}

// ---------------------------------------------------------------------------
// drawHintButton � overlaid in the 27px gap between the opponent label and
// the chat card in the right info panel.
// ---------------------------------------------------------------------------
void drawHintButton(int hintsLeft)
{
  uint16_t bg = (hintsLeft > 0) ? (uint16_t)COLOR_RGB565_BLUE : COLOR_RGB565_DARK_GREY;
  screen.fillRect(HINT_BTN_X, HINT_BTN_Y, HINT_BTN_W, HINT_BTN_H, bg);
  screen.drawRect(HINT_BTN_X, HINT_BTN_Y, HINT_BTN_W, HINT_BTN_H, COLOR_RGB565_MID_GREY);
  screen.setTextSize(1);
  screen.setTextColor(COLOR_RGB565_WHITE);
  char label[22];
  snprintf(label, sizeof(label), "Hint (%d left)", hintsLeft);
  int textW = (int)strlen(label) * 6;
  screen.setCursor(HINT_BTN_X + (HINT_BTN_W - textW) / 2,
                   HINT_BTN_Y + (HINT_BTN_H - 8) / 2);
  screen.print(label);
}

void drawGameMessagePanel(const ChatDisplayMsg *msgs, int count, const char *draft)
{
  // Card background
  screen.fillRect(GAME_MSG_CARD_X, GAME_MSG_CARD_Y, GAME_MSG_CARD_W, GAME_MSG_CARD_H,
                  (uint16_t)0xD6FA);
  screen.drawRect(GAME_MSG_CARD_X, GAME_MSG_CARD_Y, GAME_MSG_CARD_W, GAME_MSG_CARD_H,
                  COLOR_RGB565_PALE_GREY);

  // Header row
  screen.setTextSize(1);
  screen.setTextColor(COLOR_RGB565_BLACK);
  screen.setCursor(GAME_MSG_CARD_X + 6, GAME_MSG_CARD_Y + 4);
  screen.print("Chat");
  screen.setTextColor(COLOR_RGB565_OLIVE);
  screen.setCursor(GAME_MSG_CARD_X + 44, GAME_MSG_CARD_Y + 4);
  screen.print("tap to reply");

  // Divider
  screen.drawLine(GAME_MSG_CARD_X + 2, GAME_MSG_CARD_Y + 14,
                  GAME_MSG_CARD_X + GAME_MSG_CARD_W - 2, GAME_MSG_CARD_Y + 14,
                  COLOR_RGB565_MID_GREY);

  // Message bubbles � show last CHAT_MAX_DISPLAY messages
  const int BUBBLE_H = 14;
  const int BUBBLE_GAP = 2;
  const int BUBBLE_X = GAME_MSG_CARD_X + 4;
  const int BUBBLE_W = GAME_MSG_CARD_W - 8;
  const int BUBBLES_TOP = GAME_MSG_CARD_Y + 17;

  int startIdx = count > CHAT_MAX_DISPLAY ? count - CHAT_MAX_DISPLAY : 0;

  for (int slot = 0; slot < CHAT_MAX_DISPLAY; slot++)
  {
    int msgIdx = startIdx + slot;
    int bubY = BUBBLES_TOP + slot * (BUBBLE_H + BUBBLE_GAP);

    if (msgIdx < count)
    {
      // Mine = light blue, theirs = light grey
      uint16_t bg = msgs[msgIdx].isMine ? (uint16_t)0xB5F7 : (uint16_t)0xE73C;
      screen.fillRect(BUBBLE_X, bubY, BUBBLE_W, BUBBLE_H, bg);

      char preview[32];
      strncpy(preview, msgs[msgIdx].text, sizeof(preview) - 1);
      preview[sizeof(preview) - 1] = '\0';
      int tlen = strlen(msgs[msgIdx].text);
      if (tlen >= (int)(sizeof(preview) - 1))
      {
        preview[sizeof(preview) - 4] = '.';
        preview[sizeof(preview) - 3] = '.';
        preview[sizeof(preview) - 2] = '.';
        preview[sizeof(preview) - 1] = '\0';
      }
      screen.setTextColor(COLOR_RGB565_BLACK);
      screen.setCursor(BUBBLE_X + 3, bubY + 3);
      screen.print(msgs[msgIdx].isMine ? "Me: " : "  > ");
      screen.print(preview);
    }
    else
    {
      // Empty slot � faint placeholder
      screen.fillRect(BUBBLE_X, bubY, BUBBLE_W, BUBBLE_H, COLOR_RGB565_LIGHT_GREY);
    }
  }

  // Draft row
  int draftY = BUBBLES_TOP + CHAT_MAX_DISPLAY * (BUBBLE_H + BUBBLE_GAP) + 2;
  screen.setTextColor(COLOR_RGB565_DARK_NAVY);
  screen.setCursor(BUBBLE_X, draftY);
  screen.print("Draft: ");
  if (draft == nullptr || draft[0] == '\0')
  {
    screen.setTextColor(COLOR_RGB565_OLIVE);
    screen.print("tap to type");
  }
  else
  {
    char dPrev[30];
    strncpy(dPrev, draft, sizeof(dPrev) - 1);
    dPrev[sizeof(dPrev) - 1] = '\0';
    if (strlen(draft) >= (size_t)(sizeof(dPrev) - 1))
    {
      dPrev[sizeof(dPrev) - 4] = '.';
      dPrev[sizeof(dPrev) - 3] = '.';
      dPrev[sizeof(dPrev) - 2] = '.';
      dPrev[sizeof(dPrev) - 1] = '\0';
    }
    screen.setTextColor(COLOR_RGB565_BLACK);
    screen.print(dPrev);
  }
}

void drawGameMessageComposer(const char *message, bool shifted, bool symbols)
{
  screen.fillRect(0, 0, SCR_W, HEADER_H, COLOR_RGB565_BLACK);
  screen.fillRect(0, HEADER_H, SCR_W, KB_ROW1_Y - 4 - HEADER_H, COLOR_RGB565_WHITE);
  drawBackButton(COLOR_RGB565_DARK_GREY, COLOR_RGB565_WHITE, COLOR_RGB565_PALE_GREY);

  screen.setTextSize(1);
  screen.setTextColor(COLOR_RGB565_WHITE);
  displayCenteredText("Draft Message", 15, 1, COLOR_RGB565_WHITE);
  displayDivider(39, COLOR_RGB565_MID_GREY);

  screen.fillRect(8, 52, SCR_W - 16, 42, COLOR_RGB565_LIGHT_GREY);
  screen.drawRect(8, 52, SCR_W - 16, 42, COLOR_RGB565_BLACK);
  screen.setTextSize(1);
  screen.setTextWrap(true);
  screen.setCursor(12, 60);
  if (message == nullptr || message[0] == '\0')
  {
    screen.setTextColor(COLOR_RGB565_OLIVE);
    screen.print("Type a note. DONE saves the draft.");
  }
  else
  {
    screen.setTextColor(COLOR_RGB565_BLACK);
    screen.print(message);
  }
  screen.setTextWrap(false);

  drawKeyboard(shifted, symbols);
  displayStatusBar("DONE sends message to other board", COLOR_RGB565_DARK_GREY);
}

// Redraws only the draft text field inside the composer (y=52, h=42).
// Call this instead of the full drawGameMessageComposer when only the text changed.
void drawGameMessageComposerField(const char *message)
{
  screen.fillRect(8, 52, SCR_W - 16, 42, COLOR_RGB565_LIGHT_GREY);
  screen.drawRect(8, 52, SCR_W - 16, 42, COLOR_RGB565_BLACK);
  screen.setTextSize(1);
  screen.setTextWrap(true);
  screen.setCursor(12, 60);
  if (message == nullptr || message[0] == '\0')
  {
    screen.setTextColor(COLOR_RGB565_OLIVE);
    screen.print("Type a note. DONE saves the draft.");
  }
  else
  {
    screen.setTextColor(COLOR_RGB565_BLACK);
    screen.print(message);
  }
  screen.setTextWrap(false);
}

void drawEdgeCaseMenuScreen(const char *const labels[], uint8_t count, int8_t selectedIdx,
                            int8_t scrollOffset)
{
  // Fill only the body area (buttons overwrite their rows; header is drawn immediately after).
  screen.fillRect(0, HEADER_H, SCR_W, STATUS_Y - HEADER_H, COLOR_RGB565_WHITE);

  const int PAGE = 5; // max rows visible at once
  const int BTN_X = 20;
  const int BTN_W = SCR_W - 40;
  const int BTN_H = 44;
  const int BTN_GAP = 6;
  const int START_Y = 46;

  // ---------- Header bar ----------
  screen.fillRect(0, 0, SCR_W, HEADER_H, COLOR_RGB565_DARK_NAVY);
  drawBackButton(COLOR_RGB565_DARK_GREY, COLOR_RGB565_WHITE, COLOR_RGB565_PALE_GREY);
  screen.setTextSize(1);
  screen.setTextColor(COLOR_RGB565_WHITE);
  screen.setCursor(90, 14);
  screen.print("Edge Case Tests");
  screen.setTextColor(COLOR_RGB565_CYAN);
  screen.setCursor(220, 14);
  screen.print("Tap scenario");

  // Scroll arrows (top-right)  ? at x=400-439  ? at x=440-479
  int maxScroll = (int)count - PAGE;
  if (maxScroll < 0)
    maxScroll = 0;

  screen.setTextSize(2);
  // ? up arrow
  screen.setTextColor(scrollOffset > 0 ? COLOR_RGB565_WHITE : COLOR_RGB565_DARK_GREY);
  screen.setCursor(402, 8);
  screen.print("^");
  // ? down arrow  ("v" renders well at size 2)
  screen.setTextColor(scrollOffset < maxScroll ? COLOR_RGB565_WHITE : COLOR_RGB565_DARK_GREY);
  screen.setCursor(446, 8);
  screen.print("v");

  // ---------- Buttons ----------
  int visibleCount = min((int)count - scrollOffset, PAGE);
  for (int i = 0; i < visibleCount; i++)
  {
    int realIdx = scrollOffset + i;
    int btnY = START_Y + i * (BTN_H + BTN_GAP);
    bool sel = (realIdx == selectedIdx);
    uint16_t bg = sel ? COLOR_RGB565_DARK_GREEN : COLOR_RGB565_DARK_NAVY;
    screen.fillRect(BTN_X, btnY, BTN_W, BTN_H, bg);
    screen.drawRect(BTN_X, btnY, BTN_W, BTN_H, sel ? COLOR_RGB565_WHITE : COLOR_RGB565_PALE_GREY);
    screen.setTextSize(1);
    screen.setTextColor(COLOR_RGB565_WHITE);
    screen.setCursor(BTN_X + 10, btnY + (BTN_H - 8) / 2);
    screen.print(labels[realIdx]);
  }
}

void drawEdgeCaseStatus(const char *scenarioName, const char *instruction, int8_t result)
{
  // Only redraws the status panel area below the board (status bar region +
  // a narrow overlay bar just above it), keeping the board intact.
  const int BAR_H = 46;
  const int BAR_Y = STATUS_Y - BAR_H;

  // Background strip
  uint16_t bg = (result == 1)    ? COLOR_RGB565_GREEN
                : (result == -1) ? COLOR_RGB565_RED
                                 : COLOR_RGB565_DARK_NAVY;
  screen.fillRect(0, BAR_Y, SCR_W, BAR_H, bg);
  screen.drawFastHLine(0, BAR_Y, SCR_W, COLOR_RGB565_PALE_GREY);

  screen.setTextSize(1);
  screen.setTextColor(COLOR_RGB565_WHITE);

  // Line 1: scenario name
  screen.setCursor(6, BAR_Y + 6);
  screen.print(scenarioName);

  // Line 2: instruction or result badge
  screen.setCursor(6, BAR_Y + 22);
  if (result == 1)
    screen.print("PASS - move accepted!");
  else if (result == -1)
    screen.print("FAIL - move rejected");
  else
    screen.print(instruction);
}

void drawErrorScreen(const char *title, const char *detail)
{
  // Red banner at top
  screen.fillRect(0, 0, SCR_W, 52, COLOR_RGB565_RED);
  screen.setTextSize(2);
  screen.setTextColor(COLOR_RGB565_WHITE);
  screen.setCursor(12, 18);
  screen.print("! ERROR");

  // White body
  screen.fillRect(0, 52, SCR_W, SCR_H - 52 - STATUS_H, COLOR_RGB565_WHITE);

  // Bold title + divider
  displayCenteredText(title, 68, 2, COLOR_RGB565_RED);
  displayDivider(100, COLOR_RGB565_RED);

  // Multi-line detail text (wrap enabled)
  screen.setTextWrap(true);
  screen.setTextSize(1);
  screen.setTextColor(COLOR_RGB565_BLACK);
  screen.setCursor(10, 112);
  screen.print(detail);
  screen.setTextWrap(false);

  displayStatusBar("Tap anywhere to dismiss", COLOR_RGB565_RED);
}

void drawDebugScreen(const char *const lines[], uint8_t count)
{
  // Draw header band and log area directly � no pre-clear sweep.
  screen.fillRect(0, 0, SCR_W, 18, COLOR_RGB565_GREEN);
  screen.setTextSize(1);
  screen.setTextColor(COLOR_RGB565_BLACK);
  screen.setCursor(4, 5);
  screen.print("-- DEBUG --");
  screen.fillRect(0, 18, SCR_W, SCR_H - 18, COLOR_RGB565_BLACK);

  // Log lines in green
  screen.setTextColor(COLOR_RGB565_GREEN);
  static constexpr uint8_t MAX_VISIBLE = 30;
  uint8_t n = (count < MAX_VISIBLE) ? count : MAX_VISIBLE;
  for (uint8_t i = 0; i < n; i++)
  {
    if (lines[i] == nullptr)
      break;
    screen.setCursor(4, 22 + i * 14);
    screen.print(lines[i]);
  }
}

// -- WiFi screens --------------------------------------------------------------

// 4-bar signal strength indicator, 22px wide.
static void drawSignalBars(int x, int y, int8_t rssi)
{
  int bars = (rssi > -60) ? 4 : (rssi > -70) ? 3
                            : (rssi > -80)   ? 2
                                             : 1;
  for (int i = 0; i < 4; i++)
  {
    int bx = x + i * 6;
    int bh = 4 + i * 3;
    int by = y + 12 - bh;
    screen.fillRect(bx, by, 4, bh, (i < bars) ? COLOR_RGB565_BLACK : COLOR_RGB565_MID_GREY);
  }
}

void drawKeyboard(bool shifted, bool symbols)
{
  // Character lookup: [row][col=0:lower, 1:upper, 2:symbols]
  static const char *LUT[3][3] = {
      {"qwertyuiop", "QWERTYUIOP", "1234567890"},
      {"asdfghjkl", "ASDFGHJKL", "@#$%&-_+="},
      {"zxcvbnm", "ZXCVBNM", "!'\"();:"}};
  int col = symbols ? 2 : (shifted ? 1 : 0);
  const char *row1 = LUT[0][col];
  const char *row2 = LUT[1][col];
  const char *row3 = LUT[2][col];

  // Keyboard background
  screen.fillRect(0, KB_ROW1_Y - 4, SCR_W,
                  KB_ROW4_Y + KB_KEY_H + 8 - (KB_ROW1_Y - 4), 0xDEDB);

  // Row 1 � 10 keys
  for (int i = 0; i < 10; i++)
  {
    int kx = KB_ROW1_X + i * KB_STRIDE;
    screen.fillRect(kx, KB_ROW1_Y, KB_STD_W, KB_KEY_H, COLOR_RGB565_KB_KEY);
    screen.setTextSize(1);
    screen.setTextColor(COLOR_RGB565_WHITE);
    screen.setCursor(kx + (KB_STD_W - 6) / 2, KB_ROW1_Y + (KB_KEY_H - 8) / 2);
    screen.print(row1[i]);
  }
  // Row 2 � 9 keys
  for (int i = 0; row2[i]; i++)
  {
    int kx = KB_ROW2_X + i * KB_STRIDE;
    screen.fillRect(kx, KB_ROW2_Y, KB_STD_W, KB_KEY_H, COLOR_RGB565_KB_KEY);
    screen.setTextSize(1);
    screen.setTextColor(COLOR_RGB565_WHITE);
    screen.setCursor(kx + (KB_STD_W - 6) / 2, KB_ROW2_Y + (KB_KEY_H - 8) / 2);
    screen.print(row2[i]);
  }
  // Row 3 � Shift/ABC + 7 keys + DEL
  screen.fillRect(0, KB_ROW3_Y, KB_SHIFT_W, KB_KEY_H,
                  symbols ? COLOR_RGB565_DARK_GREY
                          : (shifted ? COLOR_RGB565_BLUE : COLOR_RGB565_DARK_GREY));
  screen.setTextSize(1);
  screen.setTextColor(COLOR_RGB565_WHITE);
  screen.setCursor(KB_SHIFT_W / 2 - 9, KB_ROW3_Y + (KB_KEY_H - 8) / 2);
  screen.print(symbols ? "ABC" : (shifted ? "SFT" : "sft"));

  for (int i = 0; row3[i]; i++)
  {
    int kx = KB_ROW3_LX + i * KB_STRIDE;
    screen.fillRect(kx, KB_ROW3_Y, KB_STD_W, KB_KEY_H, COLOR_RGB565_KB_KEY);
    screen.setTextSize(1);
    screen.setTextColor(COLOR_RGB565_WHITE);
    screen.setCursor(kx + (KB_STD_W - 6) / 2, KB_ROW3_Y + (KB_KEY_H - 8) / 2);
    screen.print(row3[i]);
  }
  screen.fillRect(KB_DEL_X, KB_ROW3_Y, KB_DEL_W, KB_KEY_H, COLOR_RGB565_DARK_GREY);
  screen.setTextSize(1);
  screen.setTextColor(COLOR_RGB565_WHITE);
  screen.setCursor(KB_DEL_X + (KB_DEL_W - 18) / 2, KB_ROW3_Y + (KB_KEY_H - 8) / 2);
  screen.print("DEL");

  // Row 4 � Sym toggle + Space + Done
  screen.fillRect(0, KB_ROW4_Y, KB_SYM_W, KB_KEY_H, COLOR_RGB565_DARK_GREY);
  screen.setTextSize(1);
  screen.setTextColor(COLOR_RGB565_WHITE);
  screen.setCursor(KB_SYM_W / 2 - 12, KB_ROW4_Y + (KB_KEY_H - 8) / 2);
  screen.print(symbols ? "ABC" : "?123");

  screen.fillRect(KB_SPACE_X, KB_ROW4_Y, KB_SPACE_W, KB_KEY_H, COLOR_RGB565_KB_KEY);
  screen.setTextColor(COLOR_RGB565_WHITE);
  screen.setCursor(KB_SPACE_X + KB_SPACE_W / 2 - 18, KB_ROW4_Y + (KB_KEY_H - 8) / 2);
  screen.print("SPACE");

  screen.fillRect(KB_DONE_X, KB_ROW4_Y, KB_DONE_W, KB_KEY_H, COLOR_RGB565_GREEN);
  screen.setTextColor(COLOR_RGB565_WHITE);
  screen.setCursor(KB_DONE_X + (KB_DONE_W - 24) / 2, KB_ROW4_Y + (KB_KEY_H - 8) / 2);
  screen.print("DONE");
}

void drawPasswordField(const char *password, bool showChars)
{
  screen.fillRect(8, 72, 384, 38, COLOR_RGB565_LIGHT_GREY);
  screen.drawRect(8, 72, 384, 38, COLOR_RGB565_BLACK);
  screen.setTextSize(1);
  screen.setTextColor(COLOR_RGB565_BLACK);
  screen.setCursor(12, 84);
  if (showChars)
  {
    screen.print(password);
  }
  else
  {
    for (int i = 0; password[i]; i++)
      screen.print('*');
  }
  // Show / Hide toggle button (right side)
  screen.fillRect(396, 72, 78, 38, (uint16_t)0x6B4D);
  screen.setTextSize(1);
  screen.setTextColor(COLOR_RGB565_WHITE);
  screen.setCursor(400, 84);
  screen.print(showChars ? "HIDE" : "SHOW");
}

void drawPasswordScreen(const char *ssid, const char *password,
                        bool showChars, bool shifted, bool symbols)
{
  // Header: fill black directly (no pre-clear needed).
  screen.fillRect(0, 0, SCR_W, HEADER_H, COLOR_RGB565_BLACK);
  // Label area between header and keyboard/password field � fill white.
  screen.fillRect(0, HEADER_H, SCR_W, KB_ROW1_Y - 4 - HEADER_H, COLOR_RGB565_WHITE);
  drawBackButton(COLOR_RGB565_DARK_GREY, COLOR_RGB565_WHITE, COLOR_RGB565_PALE_GREY);
  screen.setTextSize(1);
  screen.setTextColor(COLOR_RGB565_WHITE);
  displayCenteredText("Enter Password", 15, 1, COLOR_RGB565_WHITE);
  displayDivider(39, COLOR_RGB565_MID_GREY);

  screen.setTextSize(1);
  screen.setTextColor(COLOR_RGB565_BLACK);
  screen.setCursor(10, 48);
  screen.print("Network: ");
  screen.setTextColor(COLOR_RGB565_BLUE);
  screen.print(ssid);

  screen.setTextColor(COLOR_RGB565_BLACK);
  screen.setCursor(10, 66);
  screen.print("Password:");

  drawPasswordField(password, showChars);
  drawKeyboard(shifted, symbols);
}

void drawWifiListScreen(const ScannedNetwork *nets, uint8_t count, bool scanning)
{
  // Header: fill black directly (no pre-clear needed).
  screen.fillRect(0, 0, SCR_W, HEADER_H, COLOR_RGB565_BLACK);
  // Body: fill white so text labels and network-row fills have a clean background.
  screen.fillRect(0, HEADER_H, SCR_W, STATUS_Y - HEADER_H, COLOR_RGB565_WHITE);
  drawBackButton(COLOR_RGB565_DARK_GREY, COLOR_RGB565_WHITE, COLOR_RGB565_PALE_GREY);
  screen.setTextSize(1);
  screen.setTextColor(COLOR_RGB565_WHITE);
  displayCenteredText("WiFi Settings", 15, 1, COLOR_RGB565_WHITE);
  screen.fillRect(386, 4, 88, 30, COLOR_RGB565_DARK_NAVY);
  screen.setCursor(396, 15);
  screen.print("Rescan");
  displayDivider(39, COLOR_RGB565_MID_GREY);

  if (scanning)
  {
    displayCenteredText("Scanning...", 168, 1, COLOR_RGB565_BLACK);
    displayStatusBar("Please wait...", COLOR_RGB565_BLUE);
    return;
  }
  if (count == 0)
  {
    displayCenteredText("No networks found", 158, 1, COLOR_RGB565_BLACK);
    displayCenteredText("Tap Rescan to retry", 178, 1, COLOR_RGB565_BLACK);
    displayStatusBar("No networks in range", COLOR_RGB565_KB_KEY);
    return;
  }

  for (uint8_t i = 0; i < count && i < 5; i++)
  {
    int ry = WIFLIST_ROW_Y_START + i * WIFLIST_ROW_H;
    screen.fillRect(0, ry, SCR_W, WIFLIST_ROW_H - 1,
                    (i % 2 == 0) ? COLOR_RGB565_WHITE : (uint16_t)COLOR_RGB565_PALE_PINK);
    drawSignalBars(8, ry + 6, nets[i].rssi);

    screen.setTextSize(1);
    screen.setTextColor(COLOR_RGB565_BLACK);
    screen.setCursor(36, ry + 10);
    char truncated[36];
    strncpy(truncated, nets[i].ssid, 35);
    truncated[35] = '\0';
    screen.print(truncated);

    screen.setTextColor(COLOR_RGB565_KB_KEY);
    screen.setCursor(36, ry + 28);
    screen.print(nets[i].rssi);
    screen.print(" dBm");
  }

  char statusMsg[50];
  snprintf(statusMsg, sizeof(statusMsg), "%d network%s found  -  tap to connect",
           count, count == 1 ? "" : "s");
  displayStatusBar(statusMsg, COLOR_RGB565_DARK_GREY);
}

char keyboardHitTest(int tx, int ty, bool symbols)
{
  static const char *ROW_LOWER[3] = {"qwertyuiop", "asdfghjkl", "zxcvbnm"};
  static const char *ROW_SYM[3] = {"1234567890", "@#$%&-_+=", "!'\"();:"};

  int row = -1;
  if (ty >= KB_ROW1_Y && ty < KB_ROW1_Y + KB_KEY_H)
    row = 0;
  else if (ty >= KB_ROW2_Y && ty < KB_ROW2_Y + KB_KEY_H)
    row = 1;
  else if (ty >= KB_ROW3_Y && ty < KB_ROW3_Y + KB_KEY_H)
    row = 2;
  else if (ty >= KB_ROW4_Y && ty < KB_ROW4_Y + KB_KEY_H)
    row = 3;
  else
    return 0;

  if (row == 0)
  {
    int idx = (tx - KB_ROW1_X) / KB_STRIDE;
    if (tx < KB_ROW1_X || idx < 0 || idx > 9)
      return 0;
    return (symbols ? ROW_SYM : ROW_LOWER)[0][idx];
  }
  if (row == 1)
  {
    const char *r = (symbols ? ROW_SYM : ROW_LOWER)[1];
    int idx = (tx - KB_ROW2_X) / KB_STRIDE;
    if (tx < KB_ROW2_X || idx < 0 || idx >= (int)strlen(r))
      return 0;
    return r[idx];
  }
  if (row == 2)
  {
    if (tx < KB_SHIFT_W)
      return '\t'; // shift / ABC toggle
    if (tx >= KB_DEL_X)
      return '\b'; // backspace
    const char *r = (symbols ? ROW_SYM : ROW_LOWER)[2];
    int idx = (tx - KB_ROW3_LX) / KB_STRIDE;
    if (tx < KB_ROW3_LX || idx < 0 || idx >= (int)strlen(r))
      return 0;
    return r[idx];
  }
  // row == 3
  if (tx < KB_SYM_W)
    return 0x01; // symbols page toggle
  if (tx >= KB_DONE_X)
    return '\n'; // Done
  return ' ';    // Space
}
