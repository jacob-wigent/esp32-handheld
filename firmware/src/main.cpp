#include <Arduino.h>
#include <FastLED.h>
#include <gamer_pins.h>
#include <driver/adc.h>
#include <esp_adc_cal.h>



// Test configuration: 8 sections, 40 pixels each
#define NUM_SECTIONS 8
#define PIXELS_PER_SECTION 40

// Pins for each section (use the P# macros you mentioned)
const uint8_t SECTION_PINS[NUM_SECTIONS] = { P20, P21, P22, P23, P24, P25, P31, P32 };

// Per-section LED buffers
CRGB leds[NUM_SECTIONS][PIXELS_PER_SECTION];

// Time and state variables for battery/LED handling
unsigned long t_battRead = 0; // Time of last battery read
unsigned long t_blinkTick = 0; // Time of last blink tick
unsigned long t_blinkSpeed = 500; // Blink speed in ms for charging
bool blinkState = false; // Current blink state for StatusLED

// ADC calibration variables
esp_adc_cal_characteristics_t *adc_chars;
#define DEFAULT_VREF    1100        // Use adc2_vref_to_gpio() to obtain a better estimate
#define NO_OF_SAMPLES   64          // Multisampling for stability
#define ADC_ATTEN       ADC_ATTEN_DB_12  // 11dB attenuation (~0-3.6V)

float readBatteryVoltage();

void setup() {
  Serial.begin(115200);
  delay(50);
  Serial.println("Starting LED matrix test");

  pinMode(StatusLED, OUTPUT);
  digitalWrite(StatusLED, HIGH);

  pinMode(ChgStat, INPUT);

  // Configure ADC for battery voltage reading
  analogReadResolution(12);  // 12-bit resolution (0-4095)
  analogSetAttenuation(ADC_11db);  // 11dB attenuation for ~0-3.6V range

  // Configure ADC calibration
  adc_chars = (esp_adc_cal_characteristics_t *)calloc(1, sizeof(esp_adc_cal_characteristics_t));
  esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN, ADC_WIDTH_BIT_12, DEFAULT_VREF, adc_chars);

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
  // Keep the LED matrix static for this test
  FastLED.show();

  // Periodically check battery every 2s
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
        // Blink StatusLED while charging
        if (millis() - t_blinkTick >= t_blinkSpeed) {
          t_blinkTick = millis();
          blinkState = !blinkState;
          digitalWrite(StatusLED, blinkState);
        }
      } else {
        Serial.println("Battery is full / not charging");
        // Solid on to indicate ready/full
        digitalWrite(StatusLED, HIGH);
      }
    } else {
      Serial.println("No battery detected!");
      digitalWrite(StatusLED, LOW);
    }
    Serial.println();
  }

  // keep some idle time — FastLED updates above
  delay(50);
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
