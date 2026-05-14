/**
 * @file neopixel.c
 * @brief Simple RMT-based Neopixel driver implementation
 */

#include "neopixel.h"
#include "encoder.h"
#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "driver/rmt_tx.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "neopixel";

#define RMT_RESOLUTION_HZ 10000000  // 10MHz

typedef struct {
    rmt_channel_handle_t rmt_chan;
    rmt_encoder_handle_t encoder;
    uint8_t *pixels;
    uint16_t num_leds;
} neopixel_strip_t;

neopixel_handle_t neopixel_create(gpio_num_t gpio, uint16_t num_leds) {
    if (num_leds == 0) {
        ESP_LOGE(TAG, "num_leds must be > 0");
        return NULL;
    }
    
    neopixel_strip_t *strip = calloc(1, sizeof(neopixel_strip_t));
    if (!strip) {
        ESP_LOGE(TAG, "Failed to allocate strip");
        return NULL;
    }
    
    strip->num_leds = num_leds;
    strip->pixels = calloc(num_leds * 3, 1);
    if (!strip->pixels) {
        ESP_LOGE(TAG, "Failed to allocate pixel buffer");
        free(strip);
        return NULL;
    }
    
    // Create RMT TX channel
    rmt_tx_channel_config_t tx_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = gpio,
        .mem_block_symbols = 64,
        .resolution_hz = RMT_RESOLUTION_HZ,
        .trans_queue_depth = 4,
    };
    
    if (rmt_new_tx_channel(&tx_config, &strip->rmt_chan) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create RMT channel");
        free(strip->pixels);
        free(strip);
        return NULL;
    }
    
    // Create LED encoder
    led_strip_encoder_config_t encoder_config = {
        .resolution = RMT_RESOLUTION_HZ,
    };
    
    if (rmt_new_led_strip_encoder(&encoder_config, &strip->encoder) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create encoder");
        rmt_del_channel(strip->rmt_chan);
        free(strip->pixels);
        free(strip);
        return NULL;
    }
    
    // Enable channel
    if (rmt_enable(strip->rmt_chan) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable RMT channel");
        rmt_del_encoder(strip->encoder);
        rmt_del_channel(strip->rmt_chan);
        free(strip->pixels);
        free(strip);
        return NULL;
    }
    
    ESP_LOGI(TAG, "Created strip: %d LEDs on GPIO %d", num_leds, gpio);
    return (neopixel_handle_t)strip;
}

void neopixel_delete(neopixel_handle_t handle) {
    if (!handle) return;
    
    neopixel_strip_t *strip = (neopixel_strip_t*)handle;
    
    rmt_disable(strip->rmt_chan);
    rmt_del_encoder(strip->encoder);
    rmt_del_channel(strip->rmt_chan);
    free(strip->pixels);
    free(strip);
}

void neopixel_set_pixel(neopixel_handle_t handle, uint16_t index, 
                       uint8_t r, uint8_t g, uint8_t b) {
    if (!handle) return;
    
    neopixel_strip_t *strip = (neopixel_strip_t*)handle;
    
    if (index >= strip->num_leds) return;
    
    // WS2812B uses GRB format
    uint16_t offset = index * 3;
    strip->pixels[offset + 0] = g;
    strip->pixels[offset + 1] = r;
    strip->pixels[offset + 2] = b;
}

uint8_t* neopixel_get_buffer(neopixel_handle_t handle) {
    if (!handle) return NULL;
    
    neopixel_strip_t *strip = (neopixel_strip_t*)handle;
    return strip->pixels;
}

uint16_t neopixel_get_length(neopixel_handle_t handle) {
    if (!handle) return 0;
    
    neopixel_strip_t *strip = (neopixel_strip_t*)handle;
    return strip->num_leds;
}

esp_err_t neopixel_show(neopixel_handle_t handle) {
    if (!handle) return ESP_ERR_INVALID_ARG;
    
    neopixel_strip_t *strip = (neopixel_strip_t*)handle;
    
    rmt_transmit_config_t tx_config = {
        .loop_count = 0,
    };
    
    return rmt_transmit(strip->rmt_chan, strip->encoder, 
                       strip->pixels, strip->num_leds * 3, &tx_config);
}

esp_err_t neopixel_wait(neopixel_handle_t handle) {
    if (!handle) return ESP_ERR_INVALID_ARG;
    
    neopixel_strip_t *strip = (neopixel_strip_t*)handle;
    
    return rmt_tx_wait_all_done(strip->rmt_chan, portMAX_DELAY);
}

void neopixel_clear(neopixel_handle_t handle) {
    if (!handle) return;
    
    neopixel_strip_t *strip = (neopixel_strip_t*)handle;
    memset(strip->pixels, 0, strip->num_leds * 3);
}