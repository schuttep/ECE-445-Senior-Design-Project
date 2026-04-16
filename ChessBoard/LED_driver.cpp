#include "LED_driver.h"
#include <Adafruit_NeoPixel.h>
#include "headers.h"

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

static uint32_t pieceColor(char piece)
{
  switch (piece)
  {
  // White pieces
  case 'P':
    return strip.Color(255, 255, 255); // white pawn
  case 'R':
    return strip.Color(255, 0, 0); // white rook
  case 'N':
    return strip.Color(0, 0, 255); // white knight
  case 'B':
    return strip.Color(0, 255, 0); // white bishop
  case 'Q':
    return strip.Color(255, 0, 255); // white queen
  case 'K':
    return strip.Color(255, 255, 0); // white king

  // Black pieces
  case 'p':
    return strip.Color(120, 120, 120); // gray pawn
  case 'r':
    return strip.Color(120, 0, 0); // dark red rook
  case 'n':
    return strip.Color(0, 120, 120); // cyan knight
  case 'b':
    return strip.Color(0, 120, 0); // dark green bishop
  case 'q':
    return strip.Color(120, 0, 120); // purple queen
  case 'k':
    return strip.Color(255, 140, 0); // orange king

  default:
    return 0; // off
  }
}

static int boardToLedIndex(int row, int col)
{
  // If your strip snakes, use this instead:
  if (row % 2 == 0)
  {
    return row * 8 + col;
  }
  else
  {
    return row * 8 + (7 - col);
  }
}

void initLEDs()
{
  strip.begin();
  strip.setBrightness(40);
  strip.clear();
  strip.show();
}

void clearLEDs()
{
  strip.clear();
}

void showLEDs()
{
  strip.show();
}

void lightFEN(const char *fen)
{
  if (fen == nullptr)
    return;

  strip.clear();

  int row = 0;
  int col = 0;

  for (int i = 0; fen[i] != '\0' && row < 8; i++)
  {
    char c = fen[i];

    // stop after board part of FEN
    if (c == ' ')
    {
      break;
    }

    if (c == '/')
    {
      row++;
      col = 0;
      continue;
    }

    if (c >= '1' && c <= '8')
    {
      int emptyCount = c - '0';
      for (int j = 0; j < emptyCount && col < 8; j++)
      {
        int ledIndex = boardToLedIndex(row, col);
        strip.setPixelColor(ledIndex, 0);
        col++;
      }
      continue;
    }

    if (col < 8)
    {
      int ledIndex = boardToLedIndex(row, col);
      strip.setPixelColor(ledIndex, pieceColor(c));
      col++;
    }
  }

  strip.show();
}

int testLEDs()
{
  strip.fill(strip.Color(0, 60, 0)); // dim green across all pixels
  strip.show();
  delay(800);
  strip.clear();
  strip.show();
  return (int)strip.numPixels();
}

void demoSequence()
{

  strip.clear();

  // EVEN LEDs ON one by one
  for (int i = 0; i < NUM_LEDS; i += 2)
  {
    strip.setPixelColor(i, strip.Color(0, 255, 0)); // green
    strip.show();
    delay(100);
  }

  delay(300);

  // EVEN LEDs OFF one by one
  for (int i = 0; i < NUM_LEDS; i += 2)
  {
    strip.setPixelColor(i, 0);
    strip.show();
    delay(100);
  }

  delay(300);

  // ODD LEDs ON one by one
  for (int i = 1; i < NUM_LEDS; i += 2)
  {
    strip.setPixelColor(i, strip.Color(0, 255, 0)); // green
    strip.show();
    delay(100);
  }

  delay(300);

  // ODD LEDs OFF one by one
  for (int i = 1; i < NUM_LEDS; i += 2)
  {
    strip.setPixelColor(i, 0);
    strip.show();
    delay(100);
  }

  delay(300);
}

void setLEDForSquare(int row, int col, uint8_t r, uint8_t g, uint8_t b)
{
  int ledIndex = boardToLedIndex(row, col);
  strip.setPixelColor(ledIndex, strip.Color(r, g, b));
  strip.show();
}

void lightLossAlert()
{
  strip.fill(strip.Color(255, 0, 0)); // red
  strip.show();
}

void lightCheckAlert(const char *fen)
{
}

void lightMoveAlert(const char *fen, const char *fenBefore)
{
  if (!fen || !fenBefore)
    return;

  // Parse both FEN board strings into 8x8 char arrays
  char before[8][8];
  char after[8][8];

  for (int r = 0; r < 8; r++)
    for (int c = 0; c < 8; c++)
      before[r][c] = after[r][c] = '.';

  auto parseFEN = [](const char *f, char board[8][8])
  {
    int r = 0, c = 0;
    for (int i = 0; f[i] != '\0' && r < 8; i++)
    {
      char ch = f[i];
      if (ch == ' ')
        break;
      if (ch == '/')
      {
        r++;
        c = 0;
      }
      else if (ch >= '1' && ch <= '8')
      {
        int n = ch - '0';
        while (n-- > 0 && c < 8)
          c++;
      }
      else if (c < 8)
      {
        board[r][c++] = ch;
      }
    }
  };

  parseFEN(fenBefore, before);
  parseFEN(fen, after);

  strip.clear();

  // Light only the changed squares: amber = from, green = to
  for (int r = 0; r < 8; r++)
  {
    for (int c = 0; c < 8; c++)
    {
      if (before[r][c] == after[r][c])
        continue;

      int idx = boardToLedIndex(r, c);

      if (before[r][c] != '.' && after[r][c] == '.')
      {
        // Square the piece left — amber
        strip.setPixelColor(idx, strip.Color(255, 100, 0));
      }
      else if (after[r][c] != '.')
      {
        // Square the piece moved to — bright green
        strip.setPixelColor(idx, strip.Color(0, 255, 80));
      }
    }
  }

  strip.show();
}

void lightWinAlert()
{
  strip.fill(strip.Color(0, 255, 0)); // green
  strip.show();
}