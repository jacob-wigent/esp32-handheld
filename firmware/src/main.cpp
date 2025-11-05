#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "gamer_pins.h"
// For safe, low-level GPIO writes from an ISR
#include <driver/gpio.h>

#define NUM_LEDS      4     // We're using 4 LEDs in our prototype
#define LED_PIN       DISP1 // Using DISP1 from gamer_pins.h (pin 48)

#define BRIGHTNESS 40 // Set BRIGHTNESS to about 1/5 (max = 255)

// prototypes

void rainbowFade2White(int wait, int rainbowLoops, int whiteLoops);
void pulseWhite(uint8_t wait);
void whiteOverRainbow(int whiteSpeed, int whiteLength);
void colorWipe(uint32_t color, int wait);

// Create the NeoPixel object with DMA enabled for ESP32
// NEO_KHZ800 + NEO_GRB is the most common configuration
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRBW + NEO_KHZ800);
// Argument 1 = Number of pixels in NeoPixel strip
// Argument 2 = Arduino pin number (most are valid)
// Argument 3 = Pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
//   NEO_RGBW    Pixels are wired for RGBW bitstream (NeoPixel RGBW products)

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
// Indicator LEDs for developer feedback (GPIO42 and GPIO40)
#define IND_LED_A     (Y)  // GPIO42
#define IND_LED_B     (B)  // GPIO40

// ISR-safe button state/events
volatile bool stickState = false;
volatile bool selectState = false;
volatile bool stickPressedEvent = false;   // set in ISR when pressed
volatile bool selectPressedEvent = false;  // set in ISR when pressed

// Pattern indicator: light indicator LEDs for a short time on pattern change
volatile uint32_t patternIndicatorUntil = 0;
volatile uint8_t patternIndicatorMask = 0; // bit0 -> IND_LED_A, bit1 -> IND_LED_B

void setup() {
  Serial.begin(115200);
  
  // Set up button pins
  pinMode(StckBtn, INPUT_PULLUP);  // GPIO15
  pinMode(SELECT, INPUT_PULLUP);   // GPIO0

  // Attach interrupts to buttons so presses are captured even while
  // animations are running. ISR will update volatile state variables
  // and set a small event flag for the main loop to handle (printing,
  // pattern changes, etc.). Use CHANGE so we get press and release.
  attachInterrupt(digitalPinToInterrupt(StckBtn), [](){
    bool p = !digitalRead(StckBtn); // active LOW
    stickState = p;
    if (p) stickPressedEvent = true;
    // immediate visual feedback from ISR (safe low-level write):
    gpio_set_level((gpio_num_t)IND_LED_A, p ? 1 : 0);
  }, CHANGE);

  attachInterrupt(digitalPinToInterrupt(SELECT), [](){
    bool p = !digitalRead(SELECT); // active LOW
    selectState = p;
    if (p) selectPressedEvent = true;
    gpio_set_level((gpio_num_t)IND_LED_B, p ? 1 : 0);
  }, CHANGE);

  // Initialize NeoPixel strip
  strip.begin();           // INITIALIZE NeoPixel strip object (REQUIRED)
  strip.show();            // Turn OFF all pixels ASAP
  strip.setBrightness(BRIGHTNESS);

  strip.clear();            // Set all pixels to 'off'
  
  // Test pattern - cycle through R,G,B
  uint32_t testColors[] = {0xFF0000, 0x00FF00, 0x0000FF};
  for (int i = 0; i < 3; i++) {
    strip.fill(testColors[i]);
    strip.show();
    delay(500);
  }

  // Initialize indicator LEDs to OFF
  pinMode(IND_LED_A, OUTPUT);
  pinMode(IND_LED_B, OUTPUT);
  gpio_set_level((gpio_num_t)IND_LED_A, 0);
  gpio_set_level((gpio_num_t)IND_LED_B, 0);
}

void loop() {
  // Handle any button events captured by ISR: print and change pattern.
  // We handle printing and pattern logic here (non-ISR) to keep ISRs small.
  static uint32_t lastStatusPrint = 0;
  uint32_t now = millis();

  if (stickPressedEvent) {
    // Clear flag
    stickPressedEvent = false;
    // Cycle pattern
    currentPattern = (currentPattern + 1) % 3;
    // Set a brief indicator so programmer can see which pattern was selected
    patternIndicatorMask = (currentPattern == 0) ? 0x01 : (currentPattern == 1) ? 0x02 : 0x03;
    patternIndicatorUntil = now + 300; // ms
    Serial.printf("StickBtn pressed -> pattern %d\n", currentPattern);
  }

  if (selectPressedEvent) {
    selectPressedEvent = false;
    // For now, just log the select press and flash the B indicator
    patternIndicatorMask = 0x02;
    patternIndicatorUntil = now + 300;
    Serial.println("SELECT pressed");
  }

  // Update indicator LEDs: during patternIndicatorUntil show mask, else mirror
  // the live button state (press = LED on)
  if (now < patternIndicatorUntil) {
    gpio_set_level((gpio_num_t)IND_LED_A, (patternIndicatorMask & 0x01) ? 1 : 0);
    gpio_set_level((gpio_num_t)IND_LED_B, (patternIndicatorMask & 0x02) ? 1 : 0);
  } else {
    gpio_set_level((gpio_num_t)IND_LED_A, stickState ? 1 : 0);
    gpio_set_level((gpio_num_t)IND_LED_B, selectState ? 1 : 0);
  }

  // Periodic status print (every 2s)
  if ((now - lastStatusPrint) >= 2000) {
    lastStatusPrint = now;
    Serial.printf("status: pattern=%d stick=%d select=%d\n", currentPattern, stickState ? 1 : 0, selectState ? 1 : 0);
  }

  // Fill along the length of the strip in various colors...
  colorWipe(strip.Color(255,   0,   0)     , 50); // Red
  colorWipe(strip.Color(  0, 255,   0)     , 50); // Green
  colorWipe(strip.Color(  0,   0, 255)     , 50); // Blue
  colorWipe(strip.Color(  0,   0,   0, 255), 50); // True white (not RGB white)

  whiteOverRainbow(75, 5);

  pulseWhite(5);

  rainbowFade2White(3, 3, 1);
}

// Fill strip pixels one after another with a color. Strip is NOT cleared
// first; anything there will be covered pixel by pixel. Pass in color
// (as a single 'packed' 32-bit value, which you can get by calling
// strip.Color(red, green, blue) as shown in the loop() function above),
// and a delay time (in milliseconds) between pixels.
void colorWipe(uint32_t color, int wait) {
  for(int i=0; i<strip.numPixels(); i++) { // For each pixel in strip...
    strip.setPixelColor(i, color);         //  Set pixel's color (in RAM)
    strip.show();                          //  Update strip to match
    delay(wait);                           //  Pause for a moment
  }
}

void whiteOverRainbow(int whiteSpeed, int whiteLength) {

  if(whiteLength >= strip.numPixels()) whiteLength = strip.numPixels() - 1;

  int      head          = whiteLength - 1;
  int      tail          = 0;
  int      loops         = 3;
  int      loopNum       = 0;
  uint32_t lastTime      = millis();
  uint32_t firstPixelHue = 0;

  for(;;) { // Repeat forever (or until a 'break' or 'return')
    for(int i=0; i<strip.numPixels(); i++) {  // For each pixel in strip...
      if(((i >= tail) && (i <= head)) ||      //  If between head & tail...
         ((tail > head) && ((i >= tail) || (i <= head)))) {
        strip.setPixelColor(i, strip.Color(0, 0, 0, 255)); // Set white
      } else {                                             // else set rainbow
        int pixelHue = firstPixelHue + (i * 65536L / strip.numPixels());
        strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue)));
      }
    }

    strip.show(); // Update strip with new contents
    // There's no delay here, it just runs full-tilt until the timer and
    // counter combination below runs out.

    firstPixelHue += 40; // Advance just a little along the color wheel

    if((millis() - lastTime) > whiteSpeed) { // Time to update head/tail?
      if(++head >= strip.numPixels()) {      // Advance head, wrap around
        head = 0;
        if(++loopNum >= loops) return;
      }
      if(++tail >= strip.numPixels()) {      // Advance tail, wrap around
        tail = 0;
      }
      lastTime = millis();                   // Save time of last movement
    }
  }
}

void pulseWhite(uint8_t wait) {
  for(int j=0; j<256; j++) { // Ramp up from 0 to 255
    // Fill entire strip with white at gamma-corrected brightness level 'j':
    strip.fill(strip.Color(0, 0, 0, strip.gamma8(j)));
    strip.show();
    delay(wait);
  }

  for(int j=255; j>=0; j--) { // Ramp down from 255 to 0
    strip.fill(strip.Color(0, 0, 0, strip.gamma8(j)));
    strip.show();
    delay(wait);
  }
}

void rainbowFade2White(int wait, int rainbowLoops, int whiteLoops) {
  int fadeVal=0, fadeMax=100;

  // Hue of first pixel runs 'rainbowLoops' complete loops through the color
  // wheel. Color wheel has a range of 65536 but it's OK if we roll over, so
  // just count from 0 to rainbowLoops*65536, using steps of 256 so we
  // advance around the wheel at a decent clip.
  for(uint32_t firstPixelHue = 0; firstPixelHue < rainbowLoops*65536;
    firstPixelHue += 256) {

    for(int i=0; i<strip.numPixels(); i++) { // For each pixel in strip...

      // Offset pixel hue by an amount to make one full revolution of the
      // color wheel (range of 65536) along the length of the strip
      // (strip.numPixels() steps):
      uint32_t pixelHue = firstPixelHue + (i * 65536L / strip.numPixels());

      // strip.ColorHSV() can take 1 or 3 arguments: a hue (0 to 65535) or
      // optionally add saturation and value (brightness) (each 0 to 255).
      // Here we're using just the three-argument variant, though the
      // second value (saturation) is a constant 255.
      strip.setPixelColor(i, strip.gamma32(strip.ColorHSV(pixelHue, 255,
        255 * fadeVal / fadeMax)));
    }

    strip.show();
    delay(100);

    if(firstPixelHue < 65536) {                              // First loop,
      if(fadeVal < fadeMax) fadeVal++;                       // fade in
    } else if(firstPixelHue >= ((rainbowLoops-1) * 65536)) { // Last loop,
      if(fadeVal > 0) fadeVal--;                             // fade out
    } else {
      fadeVal = fadeMax; // Interim loop, make sure fade is at max
    }
  }

  for(int k=0; k<whiteLoops; k++) {
    for(int j=0; j<256; j++) { // Ramp up 0 to 255
      // Fill entire strip with white at gamma-corrected brightness level 'j':
      strip.fill(strip.Color(0, 0, 0, strip.gamma8(j)));
      strip.show();
    }
    delay(1000); // Pause 1 second
    for(int j=255; j>=0; j--) { // Ramp down 255 to 0
      strip.fill(strip.Color(0, 0, 0, strip.gamma8(j)));
      strip.show();
    }
  }

  delay(500); // Pause 1/2 second
}