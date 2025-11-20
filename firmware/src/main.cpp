#include <Arduino.h>
#include <FastLED.h>
#include <gamer_pins.h>
#include <driver/adc.h>
#include <esp_adc_cal.h>



// Test configuration: 8 sections, 40 pixels each
#define NUM_SECTIONS 8
#define PIXELS_PER_SECTION 40

// Pins for each section 
const uint8_t SECTION_PINS[NUM_SECTIONS] = { DISP1, DISP2, DISP3, DISP4, DISP5, DISP6, DISP7, DISP8 };
// Per-section LED buffers
CRGB leds[NUM_SECTIONS][PIXELS_PER_SECTION];

// Time variable for battery handling
unsigned long t_battRead = 0; // Time of last battery read

// Animation state — simplified: cycle solid colors
unsigned long t_animTick = 0;
unsigned long animInterval = 100; // ms between animation updates
// color-cycling
const CRGB SOLID_COLORS[] = { CRGB::Red, CRGB::Green, CRGB::Blue, CRGB::Yellow, CRGB::Purple, CRGB::Cyan, CRGB::White };
const int NUM_SOLID_COLORS = sizeof(SOLID_COLORS) / sizeof(SOLID_COLORS[0]);
int colorIndex = 0;
unsigned long t_colorChange = 0;
const unsigned long colorChangeInterval = 2000; // ms per solid color

const uint8_t BASE_BRIGHTNESS = 26;

// helper prototypes
void animStepOnce();
void fillAll(const CRGB &c);

// ADC calibration variables
esp_adc_cal_characteristics_t *adc_chars;
#define DEFAULT_VREF    1100        // Use adc2_vref_to_gpio() to obtain a better estimate
#define NO_OF_SAMPLES   64          // Multisampling for stability
#define ADC_ATTEN       ADC_ATTEN_DB_12  // 11dB attenuation (~0-3.6V)

float readBatteryVoltage();

// --- Animation implementations ---
void fillAll(const CRGB &c) {
  for (int s = 0; s < NUM_SECTIONS; ++s)
    for (int i = 0; i < PIXELS_PER_SECTION; ++i)
      leds[s][i] = c;
}

void animStepOnce() {
  // show the current solid color at base brightness
  FastLED.setBrightness(BASE_BRIGHTNESS);
  fillAll(SOLID_COLORS[colorIndex]);
  FastLED.show();
}

// Old animated modes removed — now using simple solid-color cycle


void setup() {
  Serial.begin(115200);
  delay(50);
  Serial.println("Starting LED matrix test");

  pinMode(StatusLED, OUTPUT);
  digitalWrite(StatusLED, HIGH);

  // Configure ADC for battery voltage reading
  analogReadResolution(12);  // 12-bit resolution (0-4095)
  analogSetAttenuation(ADC_11db);  // 11dB attenuation for ~0-3.6V range

  // Configure ADC calibration
  adc_chars = (esp_adc_cal_characteristics_t *)calloc(1, sizeof(esp_adc_cal_characteristics_t));
  esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN, ADC_WIDTH_BIT_12, DEFAULT_VREF, adc_chars);

  // Register each section with FastLED. Each call uses the pin macro for that section.
  FastLED.addLeds<WS2812, DISP1, GRB>(leds[0], PIXELS_PER_SECTION);
  FastLED.addLeds<WS2812, DISP2, GRB>(leds[1], PIXELS_PER_SECTION);
  FastLED.addLeds<WS2812, DISP3, GRB>(leds[2], PIXELS_PER_SECTION);
  FastLED.addLeds<WS2812, DISP4, GRB>(leds[3], PIXELS_PER_SECTION);
  FastLED.addLeds<WS2812, DISP5, GRB>(leds[4], PIXELS_PER_SECTION);
  FastLED.addLeds<WS2812, DISP6, GRB>(leds[5], PIXELS_PER_SECTION);
  FastLED.addLeds<WS2812, DISP7, GRB>(leds[6], PIXELS_PER_SECTION);
  FastLED.addLeds<WS2812, DISP8, GRB>(leds[7], PIXELS_PER_SECTION);

  // Set global brightness to ~10%
  FastLED.setBrightness(BASE_BRIGHTNESS);

  // Set all pixels in every section to white
  for (int s = 0; s < NUM_SECTIONS; ++s) {
    for (int i = 0; i < PIXELS_PER_SECTION; ++i) leds[s][i] = CRGB::White;
  }
  FastLED.show();
  Serial.println("All pixels set to white at ~10% brightness.");
  // initialize animation timers
  t_animTick = millis();
  t_colorChange = millis();
  colorIndex = 0;

  
}

void loop() {
  // Animation frame timing (non-blocking)
  if (millis() - t_animTick >= animInterval) {
    t_animTick = millis();
    animStepOnce();
  }

  // Cycle solid color every colorChangeInterval ms
  if (millis() - t_colorChange >= colorChangeInterval) {
    t_colorChange = millis();
    colorIndex = (colorIndex + 1) % NUM_SOLID_COLORS;
  }

  // Periodically check battery every 2s (unchanged)
  if (millis() - t_battRead > 2000) {
    t_battRead = millis();

    // Raw ADC for diagnostics
    int raw_adc = analogRead(ABAT);
    float voltage = readBatteryVoltage();

    Serial.println("-------------------------------");
    Serial.print("Raw ADC Value: ");
    Serial.println(raw_adc);
    Serial.print("Calibrated Battery Voltage: ");
    Serial.print(voltage, 2);
    Serial.println(" V");

    // Check if battery likely present (adjust threshold as needed)
    if (raw_adc < 3000) {
      // Battery connected - check charge status
      if (digitalRead(ChgStat) == LOW) {
        Serial.println("Battery is charging");
      } else {
        Serial.println("Battery is full / not charging");
      }
    } else {
      Serial.println("No battery detected!");
    }
    Serial.println();
  }


  delay(5);
}

float readBatteryVoltage() {
  uint32_t adc_reading = 0;
  for (int i = 0; i < NO_OF_SAMPLES; i++) {
    adc_reading += analogRead(ABAT);
  }
  adc_reading /= NO_OF_SAMPLES;

  uint32_t voltage_mv = esp_adc_cal_raw_to_voltage(adc_reading, adc_chars);
  float battery_voltage = (voltage_mv / 1000.0) * 2.0; // account for divider
  battery_voltage += 0.01; // small offset for FET drop
  return battery_voltage;
}
