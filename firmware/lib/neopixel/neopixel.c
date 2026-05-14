/**
 * @file neopixel.c
 * @brief Simple RMT-based Neopixel driver with double buffering
 */

#include "neopixel.h"
#include "led_encoder.h"
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
    uint8_t *working_buffer;    // Buffer you write to
    uint8_t *display_buffer;    // Buffer being transmitted
    uint16_t num_leds;
} neopixel_chain_t;

neopixel_handle_t neopixel_new(gpio_num_t gpio, uint16_t num_leds) {
    if (num_leds == 0) {
        ESP_LOGE(TAG, "num_leds must be > 0");
        return NULL;
    }
    
    neopixel_chain_t *chain = calloc(1, sizeof(neopixel_chain_t));
    if (!chain) {
        ESP_LOGE(TAG, "Failed to allocate chain");
        return NULL;
    }
    
    chain->num_leds = num_leds;
    size_t buffer_size = num_leds * 3;
    
    // Allocate working buffer (where you write pixels)
    chain->working_buffer = calloc(buffer_size, 1);
    if (!chain->working_buffer) {
        ESP_LOGE(TAG, "Failed to allocate working buffer");
        free(chain);
        return NULL;
    }
    
    // Allocate display buffer (what gets transmitted)
    chain->display_buffer = calloc(buffer_size, 1);
    if (!chain->display_buffer) {
        ESP_LOGE(TAG, "Failed to allocate display buffer");
        free(chain->working_buffer);
        free(chain);
        return NULL;
    }
    
    // Create RMT TX channel
    rmt_tx_channel_config_t tx_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .gpio_num = gpio,
        .mem_block_symbols = 48,
        .resolution_hz = RMT_RESOLUTION_HZ,
        .trans_queue_depth = 4,
    };
    
    if (rmt_new_tx_channel(&tx_config, &chain->rmt_chan) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create RMT channel");
        free(chain->display_buffer);
        free(chain->working_buffer);
        free(chain);
        return NULL;
    }
    
    // Create LED encoder
    led_strip_encoder_config_t encoder_config = {
        .resolution = RMT_RESOLUTION_HZ,
    };
    
    if (rmt_new_led_strip_encoder(&encoder_config, &chain->encoder) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create encoder");
        rmt_del_channel(chain->rmt_chan);
        free(chain->display_buffer);
        free(chain->working_buffer);
        free(chain);
        return NULL;
    }
    
    // Enable channel
    if (rmt_enable(chain->rmt_chan) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to enable RMT channel");
        rmt_del_encoder(chain->encoder);
        rmt_del_channel(chain->rmt_chan);
        free(chain->display_buffer);
        free(chain->working_buffer);
        free(chain);
        return NULL;
    }
    
    ESP_LOGI(TAG, "Created chain: %d LEDs on GPIO %d (double buffered)", num_leds, gpio);
    return (neopixel_handle_t)chain;
}

void neopixel_del(neopixel_handle_t handle) {
    if (!handle) return;
    
    neopixel_chain_t *chain = (neopixel_chain_t*)handle;
    
    rmt_disable(chain->rmt_chan);
    rmt_del_encoder(chain->encoder);
    rmt_del_channel(chain->rmt_chan);
    free(chain->working_buffer);
    free(chain->display_buffer);
    free(chain);
}

void neopixel_set_pixel(neopixel_handle_t handle, uint16_t index, 
                       uint8_t r, uint8_t g, uint8_t b) {
    if (!handle) return;
    
    neopixel_chain_t *chain = (neopixel_chain_t*)handle;
    
    if (index >= chain->num_leds) return;
    
    // Write to working buffer (WS2812B uses GRB format)
    uint16_t offset = index * 3;
    chain->working_buffer[offset + 0] = g;
    chain->working_buffer[offset + 1] = r;
    chain->working_buffer[offset + 2] = b;
}

uint8_t* neopixel_get_buffer(neopixel_handle_t handle) {
    if (!handle) return NULL;
    
    neopixel_chain_t *chain = (neopixel_chain_t*)handle;
    return chain->working_buffer;
}

uint16_t neopixel_get_length(neopixel_handle_t handle) {
    if (!handle) return 0;
    
    neopixel_chain_t *chain = (neopixel_chain_t*)handle;
    return chain->num_leds;
}

esp_err_t neopixel_show(neopixel_handle_t handle) {
    if (!handle) return ESP_ERR_INVALID_ARG;
    
    neopixel_chain_t *chain = (neopixel_chain_t*)handle;
    
    // Copy working buffer to display buffer
    size_t buffer_size = chain->num_leds * 3;
    memcpy(chain->display_buffer, chain->working_buffer, buffer_size);
    
    // Transmit display buffer
    rmt_transmit_config_t tx_config = {
        .loop_count = 0,
    };
    
    return rmt_transmit(chain->rmt_chan, chain->encoder, 
                       chain->display_buffer, buffer_size, &tx_config);
}

esp_err_t neopixel_wait(neopixel_handle_t handle) {
    if (!handle) return ESP_ERR_INVALID_ARG;
    
    neopixel_chain_t *chain = (neopixel_chain_t*)handle;
    
    return rmt_tx_wait_all_done(chain->rmt_chan, portMAX_DELAY);
}

void neopixel_clear(neopixel_handle_t handle) {
    if (!handle) return;
    
    neopixel_chain_t *chain = (neopixel_chain_t*)handle;
    memset(chain->working_buffer, 0, chain->num_leds * 3);
}