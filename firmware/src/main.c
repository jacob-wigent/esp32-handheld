#include <stdio.h>

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"

#include "pins.h"

static const uint32_t BLINK_PERIOD_MS = 1000;

void app_main() {

    uint8_t led_state = 0;

    // Configure the GPIO pin for the status LED
    gpio_reset_pin(STATUS_LED);
    gpio_set_direction(STATUS_LED, GPIO_MODE_OUTPUT);

    // Superloop
    while(1) {

        // Toggle the status LED
        led_state = !led_state;
        gpio_set_level(STATUS_LED, led_state);

        // Print the current state of the LED to the console
        printf("Status LED is %s\n", led_state ? "ON" : "OFF");
        
        // Wait for the next blink period
        // Note: vTaskDelay() takes a delay time in "ticks". The portTICK_PERIOD_MS macro converts milliseconds to ticks.
        vTaskDelay(BLINK_PERIOD_MS / portTICK_PERIOD_MS);
        
    }
}