#include "display_driver.h"
#include "DFRobot_GDL.h"

extern DFRobot_ST7365P_320x480_HW_SPI screen;

// ── Layout constants ──────────────────────────────────────────────────────────
static constexpr int SCR_W = 480;
static constexpr int SCR_H = 320;
static constexpr int STATUS_Y = 298; // y-start of bottom status bar (SCR_H - STATUS_H)
static constexpr int STATUS_H = 22;  // height of status bar

// Chess-board geometry (landscape game screen)
static constexpr int BOARD_X = 8;            // left edge of the board
static constexpr int BOARD_Y = 40;           // top edge of the board
static constexpr int CELL_SIZE = 32;         // 8 × 32 = 256 px per side
static constexpr uint16_t SQ_LIGHT = 0xFFDF; // cream
static constexpr uint16_t SQ_DARK = 0x8C51;  // medium brown

// Right info panel next to the board
static constexpr int RPANEL_X = BOARD_X + 8 * CELL_SIZE + 8; // = 272
static constexpr int RPANEL_W = SCR_W - RPANEL_X - 8;        // = 200

// ── Internal helpers ──────────────────────────────────────────────────────────

// Parse a board-only FEN into an 8×8 char array ('.' = empty).
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

// Render a board-only FEN as a proper 8×8 chessboard with coloured squares.
// Piece characters are drawn size-2 (12×16 px) centred in each cell.
// flipped=true rotates the board 180° (black pieces at the bottom).
static void drawFENBoard(const String &fen, int boardX, int boardY, int cellSize, bool flipped)
{
  char board[8][8];
  for (int r = 0; r < 8; r++)
    for (int c = 0; c < 8; c++)
      board[r][c] = '.';
  fenToBoard(fen, board);

  for (int r = 0; r < 8; r++)
  {
    // Rank label in the left margin (size-1: 6×8 px)
    char rankLabel = flipped ? ('1' + r) : ('8' - r);
    screen.setTextSize(1);
    screen.setTextColor(COLOR_RGB565_BLACK);
    screen.setCursor(1, boardY + r * cellSize + (cellSize - 8) / 2);
    screen.print(rankLabel);

    for (int c = 0; c < 8; c++)
    {
      int fenR = flipped ? (7 - r) : r;
      int fenC = flipped ? (7 - c) : c;
      int px = boardX + c * cellSize;
      int py = boardY + r * cellSize;
      bool light = ((r + c) % 2 == 0);
      screen.fillRect(px, py, cellSize, cellSize, light ? SQ_LIGHT : SQ_DARK);
      if (board[fenR][fenC] != '.')
      {
        screen.setTextSize(2);
        screen.setTextColor(light ? COLOR_RGB565_BLACK : COLOR_RGB565_WHITE);
        // size-2 glyph: 12 × 16 px → centre in the cell
        screen.setCursor(px + (cellSize - 12) / 2, py + (cellSize - 16) / 2);
        screen.print(board[fenR][fenC]);
      }
    }
  }
  screen.drawRect(boardX - 1, boardY - 1, 8 * cellSize + 2, 8 * cellSize + 2,
                  COLOR_RGB565_BLACK);
}

// ── Primitives ────────────────────────────────────────────────────────────────
void displayClear()
{
  screen.fillScreen(COLOR_RGB565_WHITE);
}

void displayHeader(bool wifiConnected)
{
  screen.setTextSize(1);
  screen.setTextColor(COLOR_RGB565_BLACK);

  screen.setCursor(20, 20);
  screen.print("ChessBoard");

  screen.setCursor(360, 20);
  screen.print(wifiConnected ? "WiFi: OK" : "WiFi: --");

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
  // Use black text on light buttons, white text on dark buttons.
  // Rough luminance check: R≥24, G≥48, B≥24 (in 5-6-5) = "light"
  uint8_t r5 = (color >> 11) & 0x1F;
  uint8_t g6 = (color >> 5) & 0x3F;
  uint8_t b5 = color & 0x1F;
  bool isLight = (r5 >= 24 && g6 >= 48 && b5 >= 24);
  screen.setTextColor(isLight ? COLOR_RGB565_BLACK : COLOR_RGB565_WHITE);
  int textX = x + (w - (int)strlen(label) * 12) / 2;
  int textY = y + (h - 16) / 2;
  screen.setCursor(textX, textY);
  screen.print(label);
}

void displayStatusBar(const char *msg, uint16_t bgColor)
{
  screen.fillRect(0, STATUS_Y, SCR_W, STATUS_H, bgColor);
  screen.setTextSize(1);
  screen.setTextColor(COLOR_RGB565_WHITE);
  screen.setCursor(6, STATUS_Y + 7);
  screen.print(msg);
}

void displayDivider(int y, uint16_t color)
{
  screen.drawFastHLine(0, y, SCR_W, color);
}

// ── Full screens ──────────────────────────────────────────────────────────────
void initDisplay()
{
  screen.begin();
  screen.setRotation(3); // landscape: left side of PCB is now the bottom
  screen.setTextWrap(false);
}

void drawConnectingScreen(const char *ssid)
{
  displayClear();
  displayCenteredText("ChessBoard", 96, 3, COLOR_RGB565_BLACK);
  displayCenteredText("Connecting to WiFi...", 156, 1, COLOR_RGB565_BLACK);
  displayCenteredText(ssid, 176, 1, COLOR_RGB565_BLUE);
  displayStatusBar("Please wait...", COLOR_RGB565_BLUE);
}

void drawMenuScreen(bool wifiConnected)
{
  displayClear();
  displayHeader(wifiConnected);
  displayCenteredText("Chess Board", 58, 2, COLOR_RGB565_BLACK);
}

void drawGameScreen(bool wifiConnected, bool fenOk, const String &data, bool localIsWhite)
{
  displayClear();
  displayHeader(wifiConnected);

  if (fenOk)
  {
    // Flip the board for black so the local player's pieces are always at the bottom.
    drawFENBoard(data, BOARD_X, BOARD_Y, CELL_SIZE, !localIsWhite);
  }
  else
  {
    // Placeholder board outline
    screen.drawRect(BOARD_X - 1, BOARD_Y - 1, 8 * CELL_SIZE + 2, 8 * CELL_SIZE + 2,
                    COLOR_RGB565_BLACK);
    screen.setTextSize(1);
    screen.setTextColor(COLOR_RGB565_BLACK);
    screen.setCursor(BOARD_X + 8, BOARD_Y + (8 * CELL_SIZE) / 2 - 4);
    screen.print(data);
  }

  // Right info panel outline
  screen.drawRect(RPANEL_X - 2, BOARD_Y, RPANEL_W + 2, 8 * CELL_SIZE,
                  (uint16_t)0xC618);

  // ── Color indicators: top = opponent, bottom = local player ──────────────
  const char *topLabel = localIsWhite ? "BLACK" : "WHITE";
  const char *botLabel = localIsWhite ? "WHITE" : "BLACK";
  uint16_t topBg = localIsWhite ? COLOR_RGB565_BLACK : COLOR_RGB565_WHITE;
  uint16_t botBg = localIsWhite ? COLOR_RGB565_WHITE : COLOR_RGB565_BLACK;
  uint16_t topFg = localIsWhite ? COLOR_RGB565_WHITE : COLOR_RGB565_BLACK;
  uint16_t botFg = localIsWhite ? COLOR_RGB565_BLACK : COLOR_RGB565_WHITE;

  // Top badge  (opponent)
  screen.fillRect(RPANEL_X, BOARD_Y, RPANEL_W, 22, topBg);
  screen.drawRect(RPANEL_X, BOARD_Y, RPANEL_W, 22, (uint16_t)0xC618);
  screen.setTextSize(1);
  screen.setTextColor(topFg);
  screen.setCursor(RPANEL_X + 4, BOARD_Y + 7);
  screen.print(topLabel);
  screen.print(" (opp)");

  // Bottom badge (local player)
  int botY = BOARD_Y + 8 * CELL_SIZE - 22;
  screen.fillRect(RPANEL_X, botY, RPANEL_W, 22, botBg);
  screen.drawRect(RPANEL_X, botY, RPANEL_W, 22, (uint16_t)0xC618);
  screen.setTextColor(botFg);
  screen.setCursor(RPANEL_X + 4, botY + 7);
  screen.print(botLabel);
  screen.print(" (you)");

  displayStatusBar(
      fenOk ? "Game active" : "Waiting...",
      fenOk ? COLOR_RGB565_GREEN : COLOR_RGB565_BLUE);
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
                            bool localIsWhite)
{
  drawGameScreen(wifiConnected, afterFEN.length() > 0, afterFEN, localIsWhite);

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

      if (before[r][c] != '.' && after[r][c] == '.')
      {
        // Source square (piece left): yellow
        screen.fillRect(px, py, CELL_SIZE, CELL_SIZE, COLOR_RGB565_YELLOW);
      }
      else if (after[r][c] != '.')
      {
        // Destination square: green with piece
        screen.fillRect(px, py, CELL_SIZE, CELL_SIZE, COLOR_RGB565_GREEN);
        screen.setTextSize(2);
        screen.setTextColor(COLOR_RGB565_BLACK);
        screen.setCursor(px + (CELL_SIZE - 12) / 2, py + (CELL_SIZE - 16) / 2);
        screen.print(after[r][c]);
      }
    }
  }
}

// ---------------------------------------------------------------------------
// drawBoardSyncOverlay
// Overlays mismatch highlights on an already-drawn board without full redraw.
//   Extra piece  (physical!='.', logical=='.')  → red tint over the square
//   Missing piece (logical!='.', physical=='.') → piece letter in dim red
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

  static constexpr uint16_t EXTRA_COL = 0xF000;   // deep red tint
  static constexpr uint16_t MISSING_COL = 0xF810; // dim red-orange for text

  for (int r = 0; r < 8; r++)
  {
    for (int c = 0; c < 8; c++)
    {
      char lp = toPhys(logical[r][c]);
      char pp = physical[r][c];

      // Map logical row/col to display pixel
      int dr = flipped ? (7 - r) : r;
      int dc = flipped ? (7 - c) : c;
      int px = BOARD_X + dc * CELL_SIZE;
      int py = BOARD_Y + dr * CELL_SIZE;
      bool light = ((dr + dc) % 2 == 0);

      if (lp == pp)
      {
        // Square is now correct — erase any previous mismatch highlight by
        // redrawing it in its normal colour with the normal piece (if any).
        screen.fillRect(px, py, CELL_SIZE, CELL_SIZE, light ? SQ_LIGHT : SQ_DARK);
        if (logical[r][c] != '.')
        {
          screen.setTextSize(2);
          screen.setTextColor(light ? COLOR_RGB565_BLACK : COLOR_RGB565_WHITE);
          screen.setCursor(px + (CELL_SIZE - 12) / 2, py + (CELL_SIZE - 16) / 2);
          screen.print(logical[r][c]);
        }
        continue;
      }

      if (pp != '.' && lp == '.')
      {
        // Extra piece: fill square with a red tint
        screen.fillRect(px, py, CELL_SIZE, CELL_SIZE, EXTRA_COL);
      }
      else if (pp == '.' && logical[r][c] != '.')
      {
        // Missing piece: redraw square background then print piece in dim red
        screen.fillRect(px, py, CELL_SIZE, CELL_SIZE, light ? SQ_LIGHT : SQ_DARK);
        screen.setTextSize(2);
        screen.setTextColor(MISSING_COL);
        screen.setCursor(px + (CELL_SIZE - 12) / 2, py + (CELL_SIZE - 16) / 2);
        screen.print(logical[r][c]);
      }
    }
  }
}

// ---------------------------------------------------------------------------
// drawCheckAlert  — yellow banner overlaid on the game screen
// ---------------------------------------------------------------------------
void drawCheckAlert(bool whiteInCheck)
{
  // Yellow banner in the right info panel, at the top
  screen.fillRect(RPANEL_X - 2, BOARD_Y, RPANEL_W + 2, 22, COLOR_RGB565_YELLOW);
  screen.setTextSize(1);
  screen.setTextColor(COLOR_RGB565_BLACK);
  const char *msg = whiteInCheck ? "!! WHITE IN CHECK !!" : "!! BLACK IN CHECK !!";
  screen.setCursor(RPANEL_X + 2, BOARD_Y + 7);
  screen.print(msg);
}

// ---------------------------------------------------------------------------
// drawGameOverScreen  — full-panel shown when the game ends
// ---------------------------------------------------------------------------
void drawGameOverScreen(const char *resultLine)
{
  screen.fillScreen(COLOR_RGB565_BLACK);

  uint16_t bannerCol = COLOR_RGB565_RED;
  if (strstr(resultLine, "stalemate") || strstr(resultLine, "Draw") ||
      strstr(resultLine, "draw"))
    bannerCol = 0x7BEF;

  screen.fillRect(0, 0, SCR_W, 58, bannerCol);
  screen.setTextSize(3);
  screen.setTextColor(COLOR_RGB565_WHITE);
  // "GAME OVER" — size-3 glyph: 18 px wide × 9 chars = 162 px
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
// drawPiecePickedUp  — small overlay when a piece leaves its square
// ---------------------------------------------------------------------------
void drawPiecePickedUp(const char *squareName)
{
  // Orange banner in the right info panel, below any check alert
  screen.fillRect(RPANEL_X - 2, BOARD_Y + 26, RPANEL_W + 2, 22, 0xFD20);
  screen.setTextSize(1);
  screen.setTextColor(COLOR_RGB565_WHITE);
  char buf[32];
  snprintf(buf, sizeof(buf), "  Lifted: %s", squareName ? squareName : "?");
  screen.setCursor(RPANEL_X + 2, BOARD_Y + 33);
  screen.print(buf);
}

void drawPromotionPicker(bool isWhite)
{
  // Clear the right info panel area with a dark navy background
  screen.fillRect(RPANEL_X - 2, BOARD_Y, RPANEL_W + 2, 8 * CELL_SIZE, (uint16_t)0x18C3);

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
      {"Queen", 0xFEA0, isWhite ? 'Q' : 'q'},
      {"Rook", 0x7BEF, isWhite ? 'R' : 'r'},
      {"Bishop", 0x07E0, isWhite ? 'B' : 'b'},
      {"Knight", 0x001F, isWhite ? 'N' : 'n'},
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

void drawEdgeCaseMenuScreen(const char *const labels[], uint8_t count, int8_t selectedIdx)
{
  screen.fillScreen(COLOR_RGB565_WHITE);

  // Header bar
  screen.fillRect(0, 0, SCR_W, 36, (uint16_t)0x2945);
  screen.setTextSize(1);
  screen.setTextColor(COLOR_RGB565_WHITE);
  screen.setCursor(6, 14);
  screen.print("Edge Case Test  ");
  screen.setTextColor((uint16_t)0x07FF);
  screen.print("Tap a scenario  |  Back: top-left");

  // Draw one button per scenario
  const int BTN_X = 20;
  const int BTN_W = SCR_W - 40;
  const int BTN_H = 44;
  const int BTN_GAP = 6;
  const int START_Y = 46;

  for (int i = 0; i < (int)count; i++)
  {
    int btnY = START_Y + i * (BTN_H + BTN_GAP);
    bool sel = (i == selectedIdx);
    uint16_t bg = sel ? (uint16_t)0x0460 : (uint16_t)0x2945;
    screen.fillRect(BTN_X, btnY, BTN_W, BTN_H, bg);
    screen.drawRect(BTN_X, btnY, BTN_W, BTN_H, sel ? COLOR_RGB565_WHITE : (uint16_t)0x7BEF);
    screen.setTextSize(1);
    screen.setTextColor(COLOR_RGB565_WHITE);
    screen.setCursor(BTN_X + 10, btnY + (BTN_H - 8) / 2);
    screen.print(labels[i]);
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
                                 : (uint16_t)0x2945;
  screen.fillRect(0, BAR_Y, SCR_W, BAR_H, bg);
  screen.drawFastHLine(0, BAR_Y, SCR_W, (uint16_t)0x7BEF);

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
  screen.fillScreen(COLOR_RGB565_BLACK);

  // Green header bar with black title text
  screen.fillRect(0, 0, SCR_W, 18, COLOR_RGB565_GREEN);
  screen.setTextSize(1);
  screen.setTextColor(COLOR_RGB565_BLACK);
  screen.setCursor(4, 5);
  screen.print("-- DEBUG --");

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

// ── WiFi screens ──────────────────────────────────────────────────────────────

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
    screen.fillRect(bx, by, 4, bh, (i < bars) ? COLOR_RGB565_BLACK : (uint16_t)0xC618);
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

  // Row 1 – 10 keys
  for (int i = 0; i < 10; i++)
  {
    int kx = KB_ROW1_X + i * KB_STRIDE;
    screen.fillRect(kx, KB_ROW1_Y, KB_STD_W, KB_KEY_H, (uint16_t)0x8410);
    screen.setTextSize(1);
    screen.setTextColor(COLOR_RGB565_WHITE);
    screen.setCursor(kx + (KB_STD_W - 6) / 2, KB_ROW1_Y + (KB_KEY_H - 8) / 2);
    screen.print(row1[i]);
  }
  // Row 2 – 9 keys
  for (int i = 0; row2[i]; i++)
  {
    int kx = KB_ROW2_X + i * KB_STRIDE;
    screen.fillRect(kx, KB_ROW2_Y, KB_STD_W, KB_KEY_H, (uint16_t)0x8410);
    screen.setTextSize(1);
    screen.setTextColor(COLOR_RGB565_WHITE);
    screen.setCursor(kx + (KB_STD_W - 6) / 2, KB_ROW2_Y + (KB_KEY_H - 8) / 2);
    screen.print(row2[i]);
  }
  // Row 3 – Shift/ABC + 7 keys + DEL
  screen.fillRect(0, KB_ROW3_Y, KB_SHIFT_W, KB_KEY_H,
                  symbols ? (uint16_t)0x4208
                          : (shifted ? COLOR_RGB565_BLUE : (uint16_t)0x4208));
  screen.setTextSize(1);
  screen.setTextColor(COLOR_RGB565_WHITE);
  screen.setCursor(KB_SHIFT_W / 2 - 9, KB_ROW3_Y + (KB_KEY_H - 8) / 2);
  screen.print(symbols ? "ABC" : (shifted ? "SFT" : "sft"));

  for (int i = 0; row3[i]; i++)
  {
    int kx = KB_ROW3_LX + i * KB_STRIDE;
    screen.fillRect(kx, KB_ROW3_Y, KB_STD_W, KB_KEY_H, (uint16_t)0x8410);
    screen.setTextSize(1);
    screen.setTextColor(COLOR_RGB565_WHITE);
    screen.setCursor(kx + (KB_STD_W - 6) / 2, KB_ROW3_Y + (KB_KEY_H - 8) / 2);
    screen.print(row3[i]);
  }
  screen.fillRect(KB_DEL_X, KB_ROW3_Y, KB_DEL_W, KB_KEY_H, (uint16_t)0x4208);
  screen.setTextSize(1);
  screen.setTextColor(COLOR_RGB565_WHITE);
  screen.setCursor(KB_DEL_X + (KB_DEL_W - 18) / 2, KB_ROW3_Y + (KB_KEY_H - 8) / 2);
  screen.print("DEL");

  // Row 4 – Sym toggle + Space + Done
  screen.fillRect(0, KB_ROW4_Y, KB_SYM_W, KB_KEY_H, (uint16_t)0x4208);
  screen.setTextSize(1);
  screen.setTextColor(COLOR_RGB565_WHITE);
  screen.setCursor(KB_SYM_W / 2 - 12, KB_ROW4_Y + (KB_KEY_H - 8) / 2);
  screen.print(symbols ? "ABC" : "?123");

  screen.fillRect(KB_SPACE_X, KB_ROW4_Y, KB_SPACE_W, KB_KEY_H, (uint16_t)0x8410);
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
  screen.fillRect(8, 72, 384, 38, (uint16_t)0xEF7D);
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
  displayClear();
  screen.fillRect(0, 0, SCR_W, 38, COLOR_RGB565_BLACK);
  screen.setTextSize(1);
  screen.setTextColor(COLOR_RGB565_WHITE);
  screen.setCursor(6, 15);
  screen.print("< Back");
  displayCenteredText("Enter Password", 15, 1, COLOR_RGB565_WHITE);
  displayDivider(39, (uint16_t)0xC618);

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
  displayClear();
  screen.fillRect(0, 0, SCR_W, 38, COLOR_RGB565_BLACK);
  screen.setTextSize(1);
  screen.setTextColor(COLOR_RGB565_WHITE);
  screen.setCursor(6, 15);
  screen.print("< Back");
  displayCenteredText("WiFi Settings", 15, 1, COLOR_RGB565_WHITE);
  screen.fillRect(386, 4, 88, 30, (uint16_t)0x2945);
  screen.setCursor(396, 15);
  screen.print("Rescan");
  displayDivider(39, (uint16_t)0xC618);

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
    displayStatusBar("No networks in range", (uint16_t)0x8410);
    return;
  }

  for (uint8_t i = 0; i < count && i < 5; i++)
  {
    int ry = WIFLIST_ROW_Y_START + i * WIFLIST_ROW_H;
    screen.fillRect(0, ry, SCR_W, WIFLIST_ROW_H - 1,
                    (i % 2 == 0) ? COLOR_RGB565_WHITE : (uint16_t)0xF7BE);
    drawSignalBars(8, ry + 6, nets[i].rssi);

    screen.setTextSize(1);
    screen.setTextColor(COLOR_RGB565_BLACK);
    screen.setCursor(36, ry + 10);
    char truncated[36];
    strncpy(truncated, nets[i].ssid, 35);
    truncated[35] = '\0';
    screen.print(truncated);

    screen.setTextColor((uint16_t)0x8410);
    screen.setCursor(36, ry + 28);
    screen.print(nets[i].rssi);
    screen.print(" dBm");
  }

  char statusMsg[50];
  snprintf(statusMsg, sizeof(statusMsg), "%d network%s found  -  tap to connect",
           count, count == 1 ? "" : "s");
  displayStatusBar(statusMsg, (uint16_t)0x4208);
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
