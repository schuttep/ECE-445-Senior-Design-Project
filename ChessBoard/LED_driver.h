#ifndef LED_DRIVER_H
#define LED_DRIVER_H

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

#endif