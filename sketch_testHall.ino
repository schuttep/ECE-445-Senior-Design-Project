#include <Wire.h>
#include <FastLED.h>

#define ADC_ADDR 0x10

#define NUM_SENSORS 8
#define NUM_LEDS 8
#define LED_PIN 2

#define THRESHOLD 150   // deviation from baseline

CRGB leds[NUM_LEDS];

uint16_t hall[NUM_SENSORS];
uint16_t baseline[NUM_SENSORS];


// Read all 8 channels from ADS7128
void readADC()
{
  Wire.beginTransmission(ADC_ADDR);
  Wire.write(0x20);      // start of conversion registers
  Wire.endTransmission();

  Wire.requestFrom(ADC_ADDR, 16);

  for(int i=0;i<NUM_SENSORS;i++)
  {
    hall[i] = (Wire.read() << 8) | Wire.read();
  }
}


// Establish baseline with no magnets
void calibrateSensors()
{
  Serial.println("Calibrating sensors... remove magnets");

  delay(2000);

  readADC();

  for(int i=0;i<NUM_SENSORS;i++)
  {
    baseline[i] = hall[i];
  }

  Serial.println("Calibration complete");
}


void setup()
{
  neopixelWrite(48, 0, 0, 0); 

  Serial.begin(115200);

  Wire.begin(8,9);

  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);

  FastLED.clear();
  FastLED.show();

  delay(1000);

  calibrateSensors();
}


void loop()
{
  readADC();

  for(int i=0;i<NUM_SENSORS;i++)
  {
    int diff = abs(hall[i] - baseline[i]);

    if(diff > THRESHOLD)
      leds[i] = CRGB::Green;
    else
      leds[i] = CRGB::Black;

    // Serial Plotter output
    Serial.print(i);
    Serial.print(" ");
    Serial.print(hall[i]);
    Serial.print(" ");
  }

  Serial.println();

  FastLED.show();

  delay(50);
}