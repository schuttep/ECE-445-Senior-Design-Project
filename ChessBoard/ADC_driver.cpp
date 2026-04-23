#include "ADC_driver.h"
#include "headers.h"

#define NUM_ADCS 8
#define ADC_BASE_ADDR 0x10 // ADS7128 chips addressed 0x10 .. 0x17

#define CMD_REG_READ 0x10
#define CMD_REG_WRITE 0x08

#define THRESHOLD 300

static const uint16_t baseline = 2048;

// Physical column → ADC chip index. Left-to-right (file a..h) the chips are
// wired in order: 7, 6, 5, 4, 0, 1, 2, 3.
const uint8_t ADC_COL_TO_CHIP[8] = {7, 6, 5, 4, 0, 1, 2, 3};

// ---- low-level helpers ------------------------------------------------

static bool writeReg(uint8_t addr, uint8_t reg, uint8_t val)
{
    Wire1.beginTransmission(addr);
    Wire1.write(CMD_REG_WRITE);
    Wire1.write(reg);
    Wire1.write(val);
    return (Wire1.endTransmission() == 0);
}

static uint16_t readADC(uint8_t addr)
{
    if (Wire1.requestFrom(addr, (uint8_t)2) != 2)
        return 0xFFFF;

    uint8_t msb = Wire1.read();
    uint8_t lsb = Wire1.read();
    return ((uint16_t)msb << 4) | (lsb >> 4);
}

static uint16_t readChannel(uint8_t addr, uint8_t ch)
{
    writeReg(addr, 0x10, 0x00);      // manual mode
    writeReg(addr, 0x11, ch & 0x0F); // select channel
    delayMicroseconds(50);
    return readADC(addr);
}

// Public single-channel read (chip 0-7, channel 0-7). Returns raw 12-bit value.
uint16_t readRawChannel(uint8_t chip, uint8_t ch)
{
    if (chip >= NUM_ADCS)
        return 0xFFFF;
    return readChannel(ADC_BASE_ADDR + chip, ch & 0x0F);
}

// Returns the fixed baseline (2048 = mid-scale of the 12-bit ADC).
uint16_t getBaseline(uint8_t chip, uint8_t ch)
{
    return baseline;
}

// ---- public API -------------------------------------------------------

void initADCs()
{
    Wire1.begin(SDA_DAQ, SCL_DAQ);
    Wire1.setClock(100000);

    for (int adc = 0; adc < NUM_ADCS; adc++)
    {
        uint8_t addr = ADC_BASE_ADDR + adc;
        writeReg(addr, 0x05, 0x00); // all pins as analog inputs
        writeReg(addr, 0x10, 0x00); // manual channel-select mode
    }
}

void readBoardFEN(char *fenOut, bool localIsWhite)
{
    int fenPos = 0;

    // White orientation: ch0 = rank 8 (far side), ch7 = rank 1 (user side).
    // Black orientation: ch7 = rank 8 (user side), ch0 = rank 1 (far side).
    // File columns are also mirrored for black so that col 0 in FEN (file a)
    // always maps to the correct physical column from the user's perspective.
    for (int rank = 0; rank < 8; rank++) // rank 0 in loop = rank 8 in FEN
    {
        int ch = localIsWhite ? rank : (7 - rank);
        int empty = 0;

        for (int file = 0; file < NUM_ADCS; file++) // file 0 = FEN file a
        {
            int col = localIsWhite ? file : (7 - file);
            uint8_t addr = ADC_BASE_ADDR + ADC_COL_TO_CHIP[col];
            uint16_t raw = readChannel(addr, (uint8_t)ch);
            int diff = (int)raw - (int)baseline;

            if (diff >= THRESHOLD)
            {
                if (empty > 0)
                {
                    fenOut[fenPos++] = '0' + empty;
                    empty = 0;
                }
                fenOut[fenPos++] = 'P';
            }
            else if (diff <= -THRESHOLD)
            {
                if (empty > 0)
                {
                    fenOut[fenPos++] = '0' + empty;
                    empty = 0;
                }
                fenOut[fenPos++] = 'p';
            }
            else
            {
                empty++;
            }
        }

        if (empty > 0)
            fenOut[fenPos++] = '0' + empty;
        if (rank < 7)
            fenOut[fenPos++] = '/';
    }

    fenOut[fenPos] = '\0';
}

ADCTestResult testADCs()
{
    ADCTestResult result = {0, 0, 0};

    for (int adc = 0; adc < NUM_ADCS; adc++)
    {
        uint8_t addr = ADC_BASE_ADDR + adc;

        // --- chip presence probe ---
        Wire1.beginTransmission(addr);
        if (Wire1.endTransmission() != 0)
            continue; // chip not found; leave both bits clear

        result.chipMask |= (1 << adc);

        // --- channel read validation ---
        uint8_t validCh = 0;
        for (int ch = 0; ch < 8; ch++)
        {
            uint16_t raw = readChannel(addr, ch);
            if (raw != 0xFFFF)
            {
                validCh++;
                result.totalValid++;
            }
        }

        if (validCh == 8)
            result.chanMask |= (1 << adc);
    }

    return result;
}