/**
 * @file example.c
 * @brief Simple neopixel driver example
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "neopixel.h"

static const char *TAG = "example";

void app_main(void)
{
    // Create two independent strips
    neopixel_handle_t strip1 = neopixel_new(18, 40);  // GPIO 18, 40 LEDs
    neopixel_handle_t strip2 = neopixel_new(19, 60);  // GPIO 19, 60 LEDs
    
    // Set some pixels on strip 1
    neopixel_set_pixel(strip1, 0, 255, 0, 0);   // Red
    neopixel_set_pixel(strip1, 1, 0, 255, 0);   // Green
    neopixel_set_pixel(strip1, 2, 0, 0, 255);   // Blue
    neopixel_show(strip1);
    
    // Direct buffer access for strip 2 (advanced)
    uint8_t *buf = neopixel_get_buffer(strip2);
    uint16_t len = neopixel_get_length(strip2);
    
    for (int i = 0; i < len; i++) {
        // GRB format: buf[i*3+0]=G, buf[i*3+1]=R, buf[i*3+2]=B
        buf[i * 3 + 0] = 0;    // Green
        buf[i * 3 + 1] = 50;   // Red
        buf[i * 3 + 2] = 100;  // Blue
    }
    neopixel_show(strip2);
    
    // Wait for both to finish
    neopixel_wait(strip1);
    neopixel_wait(strip2);
    
    ESP_LOGI(TAG, "Done");
    
    // Rainbow chase on strip 1
    while (1) {
        static uint8_t offset = 0;
        
        for (int i = 0; i < 40; i++) {
            uint8_t hue = (i * 255 / 40 + offset) % 255;
            
            // Simple HSV to RGB (hue only, full saturation/value)
            uint8_t r, g, b;
            if (hue < 85) {
                r = 255 - hue * 3;
                g = hue * 3;
                b = 0;
            } else if (hue < 170) {
                hue -= 85;
                r = 0;
                g = 255 - hue * 3;
                b = hue * 3;
            } else {
                hue -= 170;
                r = hue * 3;
                g = 0;
                b = 255 - hue * 3;
            }
            
            neopixel_set_pixel(strip1, i, r, g, b);
        }
        
        neopixel_show(strip1);
        offset += 5;
        
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    
    // Cleanup (never reached in this example)
    neopixel_del(strip1);
    neopixel_del(strip2);
}