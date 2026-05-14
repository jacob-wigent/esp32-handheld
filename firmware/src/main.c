/**
 * @file example.c
 * @brief Simple neopixel driver example
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "neopixel.h"
#include "pins.h"

static const char *TAG = "example";

void app_main(void)
{
    // Create two independent strips
    neopixel_handle_t strip1 = neopixel_create(DISP1, 40);
    neopixel_handle_t strip2 = neopixel_create(DISP8, 40);

    neopixel_clear(strip2);
    neopixel_show(strip2);

    // Set some pixels on strip 1
    neopixel_set_pixel(strip1, 0, 25, 0, 0);   // Red
    neopixel_set_pixel(strip1, 1, 0, 25, 0);   // Green
    neopixel_set_pixel(strip1, 2, 0, 0, 25);   // Blue
    neopixel_set_pixel(strip1, 3, 0, 0, 25);   // Blue
    neopixel_show(strip1);
}