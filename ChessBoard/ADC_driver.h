#ifndef ADC_DRIVER_H
#define ADC_DRIVER_H

#include <Wire.h>

// Initialise I2C bus and configure all ADS7128 ADC chips.
void initADCs();

// Sample all 64 channels with no pieces on the board and store as baselines.
void calibrateBaselines();

// Read all 64 squares, drive the LED strip (RED = positive/N-pole,
// WHITE = negative/S-pole, off = empty), and write a Modified FEN into
// fenOut.  fenOut must be at least 72 bytes.
// Modified FEN format: 'P' = N-pole present, 'p' = S-pole present,
// digit = run of empty squares, '/' = rank separator.
void readBoardFEN(char *fenOut);

// Full ADC self-test result.
struct ADCTestResult
{
    uint8_t chipMask;   // bit N set = chip N responded on I2C
    uint8_t chanMask;   // bit N set = all 8 channels on chip N returned valid data
    uint8_t totalValid; // total channels (0-64) that returned non-0xFFFF
};

// Probe all 8 chips and read all 64 channels. Returns detailed test result.
ADCTestResult testADCs();

// Read a single channel from a single chip. chip=0-7, ch=0-7.
// Returns raw 12-bit value, or 0xFFFF on error.
uint16_t readRawChannel(uint8_t chip, uint8_t ch);

// Returns the calibrated baseline for a given chip/channel.
uint16_t getBaseline(uint8_t chip, uint8_t ch);

#endif