#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef struct sNeopixelContext tNeopixelContext;

typedef struct sNeopixel {
    uint32_t index;
    uint32_t rgb;
} tNeopixel;

tNeopixelContext *neopixel_Init(uint32_t pixels, int dout_pin);
void neopixel_Deinit(tNeopixelContext *ctx);
bool neopixel_SetPixel(tNeopixelContext *ctx, tNeopixel *pixel, uint32_t pixelCount);
uint32_t neopixel_GetRefreshRate(tNeopixelContext *ctx);
