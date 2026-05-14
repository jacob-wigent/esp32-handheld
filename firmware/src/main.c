#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "pins.h"
#include "display.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_LOGI(TAG, "Starting up...");

    if(!display_init((gpio_num_t[]){DISP1, DISP2, DISP3, DISP4})) {
        ESP_LOGE(TAG, "Failed to initialize display");
        return;
    }

    display_clear();
    display_set_pixel(0, 0, 25, 0, 0); // Top-left pixel red
    display_set_pixel(19, 0, 0, 25, 0); // Top-right pixel green
    display_set_pixel(0, 7, 0, 0, 25); // Bottom-left pixel blue 
    display_set_pixel(19, 7, 12, 12, 0); // Bottom-right pixel yellow
    display_show();
}