/**
 * @file neopixel.h
 * @brief Simple RMT-based Neopixel driver
 */

#ifndef NEOPIXEL_H
#define NEOPIXEL_H

#include <stdint.h>
#include "esp_err.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Neopixel strip handle
 */
typedef void* neopixel_handle_t;

/**
 * @brief Create a neopixel strip
 * 
 * @param gpio GPIO pin number
 * @param num_leds Number of LEDs in the strip
 * @return Handle to the strip, or NULL on failure
 */
neopixel_handle_t neopixel_create(gpio_num_t gpio, uint16_t num_leds);

/**
 * @brief Delete a neopixel strip
 * 
 * @param handle Strip handle
 */
void neopixel_delete(neopixel_handle_t handle);

/**
 * @brief Set pixel color (RGB order)
 * 
 * @param handle Strip handle
 * @param index LED index (0-based)
 * @param r Red (0-255)
 * @param g Green (0-255)
 * @param b Blue (0-255)
 */
void neopixel_set_pixel(neopixel_handle_t handle, uint16_t index, 
                       uint8_t r, uint8_t g, uint8_t b);

/**
 * @brief Get pointer to raw pixel buffer (GRB format)
 * 
 * Direct access for advanced users. Format: [G, R, B, G, R, B, ...]
 * 
 * @param handle Strip handle
 * @return Pointer to pixel buffer, or NULL on error
 */
uint8_t* neopixel_get_buffer(neopixel_handle_t handle);

/**
 * @brief Get number of LEDs in strip
 * 
 * @param handle Strip handle
 * @return Number of LEDs
 */
uint16_t neopixel_get_length(neopixel_handle_t handle);

/**
 * @brief Update the strip (send data to LEDs)
 * 
 * Non-blocking. Returns immediately, data is sent via DMA.
 * 
 * @param handle Strip handle
 * @return ESP_OK on success
 */
esp_err_t neopixel_show(neopixel_handle_t handle);

/**
 * @brief Wait for transmission to complete
 * 
 * @param handle Strip handle
 * @return ESP_OK on success
 */
esp_err_t neopixel_wait(neopixel_handle_t handle);

/**
 * @brief Clear all pixels (set to black)
 * 
 * Note: Must call neopixel_show() to update the strip
 * 
 * @param handle Strip handle
 */
void neopixel_clear(neopixel_handle_t handle);

#ifdef __cplusplus
}
#endif

#endif // NEOPIXEL_H