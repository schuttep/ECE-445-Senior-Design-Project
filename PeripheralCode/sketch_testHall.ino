#include <Wire.h>
#include <Adafruit_NeoPixel.h>

#define ADC_ADDR 0x10
#define SDA_PIN 38
#define SCL_PIN 32

#define LED_PIN 6
#define NUM_LEDS 64

#define CMD_REG_READ 0x10
#define CMD_REG_WRITE 0x08

#define THRESHOLD 300

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

uint16_t baseline[8];

bool writeReg(uint8_t reg, uint8_t val)
{
  Wire.beginTransmission(ADC_ADDR);
  Wire.write(CMD_REG_WRITE);
  Wire.write(reg);
  Wire.write(val);
  return (Wire.endTransmission() == 0);
}

uint8_t readReg(uint8_t reg)
{
  Wire.beginTransmission(ADC_ADDR);
  Wire.write(CMD_REG_READ);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0)
    return 0xFF;

  if (Wire.requestFrom(ADC_ADDR, (uint8_t)1) != 1)
    return 0xFF;
  return Wire.read();
}

uint16_t readADC()
{
  if (Wire.requestFrom(ADC_ADDR, (uint8_t)2) != 2)
    return 0xFFFF;

  uint8_t msb = Wire.read();
  uint8_t lsb = Wire.read();

  return ((uint16_t)msb << 4) | (lsb >> 4);
}

uint16_t readChannel(uint8_t ch)
{
  writeReg(0x10, 0x00);      // manual mode
  writeReg(0x11, ch & 0x0F); // select channel
  delayMicroseconds(50);
  return readADC();
}

void calibrateBaselines()
{
  Serial.println("Calibrating baselines... remove magnets");
  delay(1000);

  for (int ch = 0; ch < 8; ch++)
  {
    uint32_t sum = 0;
    const int samples = 20;

    for (int i = 0; i < samples; i++)
    {
      sum += readChannel(ch);
      delay(5);
    }

    baseline[ch] = sum / samples;

    Serial.print("Baseline ");
    Serial.print(ch);
    Serial.print(": ");
    Serial.println(baseline[ch]);
  }

  Serial.println("Calibration done");
}

void setup()
{
  Serial.begin(115200);
  delay(1000);

  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);

  strip.begin();
  strip.setBrightness(40);
  strip.clear();
  strip.show();

  // ADS7128 setup
  writeReg(0x05, 0x00); // all pins analog
  writeReg(0x10, 0x00); // manual mode

  calibrateBaselines();
}

void loop()
{
  strip.clear();

  for (int ch = 0; ch < 8; ch++)
  {
    uint16_t raw = readChannel(ch);
    int diff = (int)raw - (int)baseline[ch];

    Serial.print("CH");
    Serial.print(ch);
    Serial.print(": ");
    Serial.print(raw);
    Serial.print("  diff: ");
    Serial.print(diff);
    Serial.print("   ");

    if (diff >= THRESHOLD)
    {
      strip.setPixelColor(ch, strip.Color(255, 255, 255)); // white
    }
    else if (diff <= -THRESHOLD)
    {
      strip.setPixelColor(ch, strip.Color(255, 0, 0)); // red
    }
    else
    {
      strip.setPixelColor(ch, 0); // off
    }
  }

  Serial.println();
  strip.show();
  delay(50);
}
