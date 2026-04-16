#ifndef LED_DRIVER_H
#define LED_DRIVER_H

void initLEDs();
void lightFEN(const char *fen);
void clearLEDs();
void showLEDs();
void demoSequence();
// Flashes all LEDs green for ~800 ms. Returns the pixel count.
int testLEDs();

#endif