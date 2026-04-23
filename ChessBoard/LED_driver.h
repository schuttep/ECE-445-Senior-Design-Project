#ifndef LED_DRIVER_H
#define LED_DRIVER_H

#include <Arduino.h>

void initLEDs();
void lightFEN(const char *fen);
void clearLEDs();
void showLEDs();
void demoSequence();
// Flashes all LEDs green for ~800 ms. Returns the pixel count.
int testLEDs();
void setLEDForSquare(int row, int col, uint8_t r, uint8_t g, uint8_t b);
void lightLossAlert();
void lightMoveAlert(const char *fen, const char *fenBefore);
void lightWinAlert();

// Compare committed logical FEN against physical sensor FEN and light
// mismatched squares: red = missing piece, yellow = extra piece, orange = wrong polarity.
void lightBoardSync(const char *logicalFEN, const char *physicalFEN);

#endif