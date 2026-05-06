#include <Adafruit_NeoPixel.h>

#define LED_PIN   2
#define NUM_LEDS  150

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

void setup() {
  strip.begin();
  strip.setBrightness(40);
}

void loop() {

  strip.clear();

  // First 10 on one by one
  for (int i = 0; i < 10; i++) {
    strip.setPixelColor(i, strip.Color(0, 0, 255));
    strip.show();
    delay(120);
  }

  delay(300);

  // First 10 off
  for (int i = 0; i < 10; i++) {
    strip.setPixelColor(i, 0);
    strip.show();
    delay(120);
  }

  delay(300);

  // Every second LED red
  for (int i = 0; i < NUM_LEDS; i++) {
    strip.setPixelColor(i, (i % 2 == 0) ? strip.Color(255, 0, 0) : 0);
  }

  strip.show();
  delay(700);
}
