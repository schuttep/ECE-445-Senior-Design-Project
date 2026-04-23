#ifndef ADC_DRIVER_H
#define ADC_DRIVER_H

#include <Wire.h>

// Initialise I2C bus and configure all ADS7128 ADC chips.
void initADCs();

// Read all 64 squares and write a Modified FEN into fenOut (at least 72 bytes).
// Always produces rank 8 first, file a..h per row, matching standard FEN.
// localIsWhite: when false the physical board is read mirrored so that the
// row of channels closest to the user (ch7) maps to rank 8 and file columns
// are reversed, matching the black-side display orientation.
void readBoardFEN(char *fenOut, bool localIsWhite = true);

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

// Returns the fixed baseline (2048 = mid-scale of the 12-bit ADC).
uint16_t getBaseline(uint8_t chip, uint8_t ch);

// Physical column (0=file a .. 7=file h) to ADC chip index mapping.
// Accounts for the non-sequential wiring of 0x14/0x15 and 0x16/0x17.
extern const uint8_t ADC_COL_TO_CHIP[8];

#endif