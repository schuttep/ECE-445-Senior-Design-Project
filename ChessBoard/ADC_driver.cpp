#include "ADC_driver.h"
#include "headers.h"
#include <Adafruit_NeoPixel.h>

#define NUM_ADCS 8
#define ADC_BASE_ADDR 0x10 // ADS7128 chips addressed 0x10 .. 0x17

#define CMD_REG_READ 0x10
#define CMD_REG_WRITE 0x08

#define THRESHOLD 300

// strip is defined in LED_driver.cpp
extern Adafruit_NeoPixel strip;

static uint16_t baseline[NUM_ADCS][8];

// ---- low-level helpers ------------------------------------------------

static bool writeReg(uint8_t addr, uint8_t reg, uint8_t val)
{
    Wire.beginTransmission(addr);
    Wire.write(CMD_REG_WRITE);
    Wire.write(reg);
    Wire.write(val);
    return (Wire.endTransmission() == 0);
}

static uint16_t readADC(uint8_t addr)
{
    if (Wire.requestFrom(addr, (uint8_t)2) != 2)
        return 0xFFFF;

    uint8_t msb = Wire.read();
    uint8_t lsb = Wire.read();
    return ((uint16_t)msb << 4) | (lsb >> 4);
}

static uint16_t readChannel(uint8_t addr, uint8_t ch)
{
    writeReg(addr, 0x10, 0x00);      // manual mode
    writeReg(addr, 0x11, ch & 0x0F); // select channel
    delayMicroseconds(50);
    return readADC(addr);
}

// Mirror the snake-pattern used in LED_driver.cpp so LEDs match squares.
// row 0 = rank 8 (ADC 0), col 0 = file a.
static int adcToLedIndex(int row, int col)
{
    if (row % 2 == 0)
        return row * 8 + col;
    else
        return row * 8 + (7 - col);
}

// ---- public API -------------------------------------------------------

void initADCs()
{
    Wire.begin(SDA_DAQ, SCL_DAQ);
    Wire.setClock(100000);

    for (int adc = 0; adc < NUM_ADCS; adc++)
    {
        uint8_t addr = ADC_BASE_ADDR + adc;
        writeReg(addr, 0x05, 0x00); // all pins as analog inputs
        writeReg(addr, 0x10, 0x00); // manual channel-select mode
    }
}

void calibrateBaselines()
{
    Serial.println("Calibrating baselines... remove all pieces");
    delay(1000);

    for (int adc = 0; adc < NUM_ADCS; adc++)
    {
        uint8_t addr = ADC_BASE_ADDR + adc;
        for (int ch = 0; ch < 8; ch++)
        {
            uint32_t sum = 0;
            for (int i = 0; i < 20; i++)
            {
                sum += readChannel(addr, ch);
                delay(5);
            }
            baseline[adc][ch] = (uint16_t)(sum / 20);

            Serial.print("ADC");
            Serial.print(adc);
            Serial.print(" CH");
            Serial.print(ch);
            Serial.print(": ");
            Serial.println(baseline[adc][ch]);
        }
    }

    Serial.println("Calibration done");
}

void readBoardFEN(char *fenOut)
{
    strip.clear();

    int fenPos = 0;

    for (int adc = 0; adc < NUM_ADCS; adc++) // adc = board row (rank 8 down to 1)
    {
        uint8_t addr = ADC_BASE_ADDR + adc;
        int empty = 0;

        for (int ch = 0; ch < 8; ch++) // ch = board column (file a..h)
        {
            uint16_t raw = readChannel(addr, ch);
            int diff = (int)raw - (int)baseline[adc][ch];
            int led = adcToLedIndex(adc, ch);

            if (diff >= THRESHOLD)
            {
                // Positive polarity (N-pole) → RED
                strip.setPixelColor(led, strip.Color(255, 0, 0));
                if (empty > 0)
                {
                    fenOut[fenPos++] = '0' + empty;
                    empty = 0;
                }
                fenOut[fenPos++] = 'P';
            }
            else if (diff <= -THRESHOLD)
            {
                // Negative polarity (S-pole) → WHITE
                strip.setPixelColor(led, strip.Color(255, 255, 255));
                if (empty > 0)
                {
                    fenOut[fenPos++] = '0' + empty;
                    empty = 0;
                }
                fenOut[fenPos++] = 'p';
            }
            else
            {
                strip.setPixelColor(led, 0); // no magnet → off
                empty++;
            }
        }

        if (empty > 0)
        {
            fenOut[fenPos++] = '0' + empty;
        }
        if (adc < NUM_ADCS - 1)
        {
            fenOut[fenPos++] = '/';
        }
    }

    fenOut[fenPos] = '\0';
    strip.show();
}

ADCTestResult testADCs()
{
    ADCTestResult result = {0, 0, 0};

    for (int adc = 0; adc < NUM_ADCS; adc++)
    {
        uint8_t addr = ADC_BASE_ADDR + adc;

        // --- chip presence probe ---
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() != 0)
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