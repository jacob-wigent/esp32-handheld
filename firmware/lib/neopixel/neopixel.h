/**
 * @file neopixel.h
 * @brief Simple RMT-based Neopixel driver with double buffering
 * 
 * Uses double buffering for smooth animations:
 * - Working buffer: Where you write pixels
 * - Display buffer: What RMT transmits (copied from working on show())
 * 
 * This allows you to prepare the next frame while current frame transmits.
 */

#pragma once

#include <stdint.h>
#include "esp_err.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Neopixel chain handle
 */
typedef void* neopixel_handle_t;

/**
 * @brief Initialize a new neopixel chain
 * 
 * Allocates buffers and configures RMT hardware.
 * 
 * @param gpio GPIO pin number
 * @param num_leds Number of LEDs in the chain
 * @return Handle to the chain, or NULL on failure
 */
neopixel_handle_t neopixel_new(gpio_num_t gpio, uint16_t num_leds);

/**
 * @brief Delete a neopixel chain and free resources
 * 
 * Waits for any pending transmission to complete before cleanup.
 * 
 * @param handle chain handle
 */
void neopixel_del(neopixel_handle_t handle);

/**
 * @brief Set pixel color (RGB order)
 * 
 * @param handle chain handle
 * @param index LED index (0-based)
 * @param r Red (0-255)
 * @param g Green (0-255)
 * @param b Blue (0-255)
 */
void neopixel_set_pixel(neopixel_handle_t handle, uint16_t index, 
                       uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief Get pointer to working pixel buffer (GRB format)
 * 
 * Direct access for advanced users. This is the WORKING buffer where
 * you prepare frames. When you call neopixel_show(), this gets copied
 * to the display buffer and transmitted.
 * 
 * Format: [G, R, B, G, R, B, ...]
 * 
 * Safe to modify at any time, even during transmission.
 * 
 * @param handle chain handle
 * @return Pointer to working buffer, or NULL on error
 */
uint8_t* neopixel_get_buffer(neopixel_handle_t handle);

/**
 * @brief Get number of LEDs in chain
 * 
 * @param handle chain handle
 * @return Number of LEDs
 */
uint16_t neopixel_get_length(neopixel_handle_t handle);

/**
 * @brief Update the chain (send data to LEDs)
 * 
 * Copies working buffer -> display buffer, then transmits display buffer.
 * Non-blocking: returns immediately, data is sent via RMT DMA.
 * 
 * You can start modifying the working buffer immediately after calling this,
 * even while transmission is in progress.
 * 
 * @param handle chain handle
 * @return ESP_OK on success
 */
esp_err_t neopixel_show(neopixel_handle_t handle);

/**
 * @brief Wait for transmission to complete
 * 
 * Blocks until RMT finishes sending the current frame (~7.7ms for 256 LEDs).
 * 
 * Typically not needed due to double buffering. Only call before:
 * - neopixel_del() (REQUIRED)
 * - Task/function exit
 * - Hardware reconfiguration
 * 
 * Don't call in animation loops.
 * 
 * @param handle chain handle
 * @return ESP_OK on success
 */
esp_err_t neopixel_wait(neopixel_handle_t handle);

/**
 * @brief Clear all pixels (set to black)
 * 
 * Clears the working buffer. Call neopixel_show() to update the chain.
 * 
 * @param handle chain handle
 */
void neopixel_clear(neopixel_handle_t handle);

#ifdef __cplusplus
}
#endif