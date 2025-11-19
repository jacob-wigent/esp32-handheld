#include <Arduino.h>
#include <FastLED.h>

// Test configuration: 8 sections, 40 pixels each
#define NUM_SECTIONS 8
#define PIXELS_PER_SECTION 40

// Pins for each section (use the P# macros you mentioned)
const uint8_t SECTION_PINS[NUM_SECTIONS] = { P20, P21, P22, P23, P24, P25, P31, P32 };

// Per-section LED buffers
CRGB leds[NUM_SECTIONS][PIXELS_PER_SECTION];

void setup() {
  Serial.begin(115200);
  delay(50);
  Serial.println("Starting LED matrix test");

  // Register each section with FastLED. Each call uses the pin macro for that section.
  FastLED.addLeds<WS2812, P20, GRB>(leds[0], PIXELS_PER_SECTION);
  FastLED.addLeds<WS2812, P21, GRB>(leds[1], PIXELS_PER_SECTION);
  FastLED.addLeds<WS2812, P22, GRB>(leds[2], PIXELS_PER_SECTION);
  FastLED.addLeds<WS2812, P23, GRB>(leds[3], PIXELS_PER_SECTION);
  FastLED.addLeds<WS2812, P24, GRB>(leds[4], PIXELS_PER_SECTION);
  FastLED.addLeds<WS2812, P25, GRB>(leds[5], PIXELS_PER_SECTION);
  FastLED.addLeds<WS2812, P31, GRB>(leds[6], PIXELS_PER_SECTION);
  FastLED.addLeds<WS2812, P32, GRB>(leds[7], PIXELS_PER_SECTION);

  // Set global brightness to ~50%
  FastLED.setBrightness(128);

  // Set all pixels in every section to white
  for (int s = 0; s < NUM_SECTIONS; ++s) {
    for (int i = 0; i < PIXELS_PER_SECTION; ++i) leds[s][i] = CRGB::White;
  }
  FastLED.show();
  Serial.println("All pixels set to white at ~50% brightness.");
}

void loop() {
  // Keep the test static. If you want blinking or cycling, add code here.
  delay(1000);
}
