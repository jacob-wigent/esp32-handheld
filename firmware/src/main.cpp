#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "gamer_pins.h"

#define NUM_LEDS      4     // We're using 4 LEDs in our prototype
#define LED_PIN       DISP1 // Using DISP1 from gamer_pins.h (pin 48)

// Create the NeoPixel object with DMA enabled for ESP32
// NEO_KHZ800 + NEO_GRB is the most common configuration
Adafruit_NeoPixel leds(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// Predefined colors as 32-bit values for easier use
static const uint32_t colors[] = {
  0xFF0000,  // Red
  0x00FF00,  // Green
  0x0000FF,  // Blue
  0xFFFF00,  // Yellow
  0xFF00FF,  // Magenta
  0x00FFFF,  // Cyan
  0xFF8000,  // Orange
  0x8000FF   // Purple
};

uint8_t currentPattern = 0;
uint32_t lastButtonCheck = 0;
bool lastStickBtn = false;
bool lastSelectBtn = false;

void setup() {
  Serial.begin(115200);
  
  // Set up button pins
  pinMode(StckBtn, INPUT_PULLUP);  // GPIO15
  pinMode(SELECT, INPUT_PULLUP);   // GPIO0
  
  // Initialize NeoPixels
  leds.begin();           // Initialize pins for output
  leds.setBrightness(32); // Tone it down - they're bright!

  leds.clear();            // Set all pixels to 'off'
  
  // Test pattern - cycle through R,G,B
  uint32_t testColors[] = {0xFF0000, 0x00FF00, 0x0000FF};
  for (int i = 0; i < 3; i++) {
    leds.fill(testColors[i]);
    leds.show();
    delay(500);
  }
}

// Helper function for color wheel
uint32_t wheel(uint8_t pos) {
  pos = 255 - pos;
  if (pos < 85) {
    return leds.Color(255 - pos * 3, 0, pos * 3);
  } else if (pos < 170) {
    pos -= 85;
    return leds.Color(0, pos * 3, 255 - pos * 3);
  } else {
    pos -= 170;
    return leds.Color(pos * 3, 255 - pos * 3, 0);
  }
}

void loop() {
  uint32_t now = millis();
  
  // Button handling (every 20ms)
  if ((now - lastButtonCheck) >= 20) {
    bool stickBtn = !digitalRead(StckBtn);  // Active LOW
    bool selectBtn = !digitalRead(SELECT);   // Active LOW
    
    // StickBtn changes patterns
    if (stickBtn && !lastStickBtn) {
      currentPattern = (currentPattern + 1) % 3;
      Serial.printf("Pattern: %d\n", currentPattern);
    }
    
    lastStickBtn = stickBtn;
    lastSelectBtn = selectBtn;
    lastButtonCheck = now;
  }

  // Update LED patterns based on current mode
  switch (currentPattern) {
    case 0:  // Rainbow cycle
      for (int i = 0; i < NUM_LEDS; i++) {
        uint8_t pos = ((now / 20) + (i * 256 / NUM_LEDS)) & 0xFF;
        leds.setPixelColor(i, wheel(pos));
      }
      break;
      
    case 1:  // Chase pattern
      leds.clear();
      leds.setPixelColor((now / 200) % NUM_LEDS, colors[0]);
      break;
      
    case 2: {  // Color fade
      uint8_t bright = (sin(now * 0.002) + 1) * 127;
      uint32_t color = colors[2];
      uint8_t r = ((color >> 16) & 0xFF) * bright / 255;
      uint8_t g = ((color >> 8) & 0xFF) * bright / 255;
      uint8_t b = (color & 0xFF) * bright / 255;
      uint32_t dimmedColor = leds.Color(r, g, b);
      leds.fill(dimmedColor);
      break;
    }
  }
  
  leds.show();
}
