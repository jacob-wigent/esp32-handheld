#pragma once
#include <stdint.h>
#include <stdbool.h>

bool display_init();

void display_set_pixel(uint8_t x, uint8_t y, uint32_t rgb);
void display_clear(uint32_t rgb);
 
void display_show();