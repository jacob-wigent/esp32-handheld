#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "driver/gpio.h"

#define DISPLAY_WIDTH   20
#define DISPLAY_HEIGHT  8 //16
#define NUM_SEGMENTS    4 //8
#define SEG_HEIGHT      2
#define SEG_PIXELS      40

bool display_init(gpio_num_t gpios[NUM_SEGMENTS]);

void display_set_pixel(uint8_t x, uint8_t y, uint8_t r, uint8_t g, uint8_t b);
void display_clear();
 
void display_show();