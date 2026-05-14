# Neopixel Driver for ESP32

Simple RMT-based driver for controlling WS2812B/Neopixel LED chains on ESP32 using ESP-IDF.

## Features

- **Simple API**: 8 functions to control LED chains
- **Multiple chains**: Independent control of multiple LED chains
- **Hardware Accelerated**: Uses ESP32 RMT peripheral with DMA
- **Non-blocking**: Transmission happens in background, no CPU waiting
- **Double Buffered**: Prepare next frame while current frame transmits
- **Direct Buffer Access**: Full control for performance-critical code

## Quick Start

```c
#include "neopixel.h"

void app_main(void) {
    // Initialize chain: 40 LEDs on GPIO 18
    neopixel_handle_t chain = neopixel_new(18, 40);
    
    // Set some pixels (RGB format)
    neopixel_set_pixel(chain, 0, 255, 0, 0);   // Red
    neopixel_set_pixel(chain, 1, 0, 255, 0);   // Green
    neopixel_set_pixel(chain, 2, 0, 0, 255);   // Blue
    
    // Update the chain
    neopixel_show(chain);
    
    // Wait for transmission to complete (before cleanup)
    neopixel_wait(chain);
    
    // Cleanup
    neopixel_del(chain);
}
```

## API Reference

```c
// Initialize and cleanup
neopixel_handle_t neopixel_new(gpio_num_t gpio, uint16_t num_leds);
void neopixel_del(neopixel_handle_t handle);

// Set pixels
void neopixel_set_pixel(neopixel_handle_t handle, uint16_t index, 
                       uint8_t r, uint8_t g, uint8_t b);
void neopixel_clear(neopixel_handle_t handle);

// Update chain
esp_err_t neopixel_show(neopixel_handle_t handle);  // Non-blocking
esp_err_t neopixel_wait(neopixel_handle_t handle);  // Wait for completion

// Direct buffer access (GRB format)
uint8_t* neopixel_get_buffer(neopixel_handle_t handle);
uint16_t neopixel_get_length(neopixel_handle_t handle);
```

## Usage Examples

### Basic Usage

```c
void app_main(void) {
    neopixel_handle_t chain = neopixel_new(18, 40);
    
    // Set individual pixels
    neopixel_set_pixel(chain, 0, 255, 0, 0);   // Red
    neopixel_set_pixel(chain, 1, 0, 255, 0);   // Green
    neopixel_set_pixel(chain, 2, 0, 0, 255);   // Blue
    
    neopixel_show(chain);
    neopixel_wait(chain);
    neopixel_del(chain);
}
```

### Animation Loop

```c
void rainbow_animation(void) {
    neopixel_handle_t chain = neopixel_new(18, 60);
    uint8_t *buf = neopixel_get_buffer(chain);
    uint8_t offset = 0;
    
    while (1) {
        // Calculate frame
        for (int i = 0; i < 60; i++) {
            uint8_t hue = (i * 255 / 60 + offset) % 255;
            buf[i * 3 + 0] = calculate_green(hue);
            buf[i * 3 + 1] = calculate_red(hue);
            buf[i * 3 + 2] = calculate_blue(hue);
        }
        
        // Update chain (non-blocking)
        neopixel_show(chain);
        
        offset++;
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
```

### Multiple chains

```c
void multi_chain_demo(void) {
    neopixel_handle_t chain1 = neopixel_new(18, 40);
    neopixel_handle_t chain2 = neopixel_new(19, 60);
    
    // Control independently
    neopixel_set_pixel(chain1, 0, 255, 0, 0);
    neopixel_set_pixel(chain2, 0, 0, 0, 255);
    
    neopixel_show(chain1);
    neopixel_show(chain2);  // Both transmit in parallel
}
```

## Direct Buffer Access

For performance-critical applications, access the pixel buffer directly:

```c
uint8_t *buf = neopixel_get_buffer(chain);
uint16_t len = neopixel_get_length(chain);

// Buffer format: GRB (WS2812B native)
for (int i = 0; i < len; i++) {
    buf[i * 3 + 0] = green_value;
    buf[i * 3 + 1] = red_value;
    buf[i * 3 + 2] = blue_value;
}

neopixel_show(chain);
```

This is faster than calling `neopixel_set_pixel()` for every LED.

## How It Works

### Double Buffering
The driver uses two pixel buffers:
- **Working buffer**: Where you write/prepare frames
- **Display buffer**: What the RMT hardware transmits

When you call `neopixel_show()`, the working buffer is copied to the display buffer, then transmission begins. This means you can immediately start preparing the next frame without waiting for transmission to complete.

### RMT Hardware
The ESP32 RMT (Remote Control) peripheral generates precise timing signals via DMA, requiring no CPU intervention during transmission. For 256 LEDs, transmission takes ~7.7ms at the WS2812B data rate of 800kHz.

## Important Notes

- **Color Format**: Buffer uses GRB format (WS2812B native order)
- **Non-blocking**: `neopixel_show()` returns immediately, transmission happens in background
- **Wait Required**: Always call `neopixel_wait()` before `neopixel_del()`
- **Thread Safety**: Handle your own locking if accessing from multiple tasks
- **Memory**: Uses ~2× LED count in bytes (e.g., 256 LEDs = 1.5KB)

## Performance

Typical performance for 256 LEDs:
- Buffer copy: ~0.1ms
- Transmission: ~7.7ms (hardware limit)
- Frame rate: Up to 130 FPS with proper pipelining

## Project Files

- `neopixel.h` - Driver header
- `neopixel.c` - Driver implementation
- `example.c` - Basic usage examples
- `led_encoder.h/c` - RMT encoder (from ESP-IDF examples)

## Installation

Copy files to your PlatformIO or ESP-IDF project:
```
lib/neopixel/
├── neopixel.h
├── neopixel.c
├── led_encoder.h
└── led_encoder.c
```

Include in your code:
```c
#include "neopixel.h"
```