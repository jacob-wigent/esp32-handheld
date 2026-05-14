#include <stdlib.h>
#include <string.h>

#include "driver/rmt_tx.h"
#include "freertos/FreeRTOS.h"

#include "encoder.h"
#include "neopixel.h"

#define RMT_RESOLUTION_HZ 10000000

struct sNeopixelContext {
    rmt_channel_handle_t chan;
    rmt_encoder_handle_t encoder;
    uint32_t pixels;
    uint8_t *buf;
};

static inline void rgb_to_grb(uint32_t rgb, uint8_t *out)
{
    out[0] = (rgb >> 8) & 0xFF;
    out[1] = (rgb >> 16) & 0xFF;
    out[2] = rgb & 0xFF;
}

tNeopixelContext *neopixel_Init(uint32_t pixels, int dout_pin)
{
    tNeopixelContext *ctx;
    rmt_tx_channel_config_t tx_cfg;
    led_strip_encoder_config_t enc_cfg;

    if (pixels == 0) {
        return NULL;
    }

    ctx = calloc(1, sizeof(*ctx));
    if (!ctx) {
        return NULL;
    }

    ctx->pixels = pixels;
    ctx->buf = calloc(pixels, 3);
    if (!ctx->buf) {
        free(ctx);
        return NULL;
    }

    tx_cfg.clk_src = RMT_CLK_SRC_DEFAULT;
    tx_cfg.gpio_num = (gpio_num_t)dout_pin;
    tx_cfg.mem_block_symbols = 64;
    tx_cfg.resolution_hz = RMT_RESOLUTION_HZ;
    tx_cfg.trans_queue_depth = 1;
    tx_cfg.flags.invert_out = 0;
    tx_cfg.flags.with_dma = 0;
    tx_cfg.flags.io_loop_back = 0;
    tx_cfg.flags.io_od_mode = 0;

    if (rmt_new_tx_channel(&tx_cfg, &ctx->chan) != ESP_OK) {
        free(ctx->buf);
        free(ctx);
        return NULL;
    }

    enc_cfg.resolution = RMT_RESOLUTION_HZ;
    if (rmt_new_led_strip_encoder(&enc_cfg, &ctx->encoder) != ESP_OK) {
        rmt_del_channel(ctx->chan);
        free(ctx->buf);
        free(ctx);
        return NULL;
    }

    if (rmt_enable(ctx->chan) != ESP_OK) {
        rmt_del_encoder(ctx->encoder);
        rmt_del_channel(ctx->chan);
        free(ctx->buf);
        free(ctx);
        return NULL;
    }

    return ctx;
}

void neopixel_Deinit(tNeopixelContext *ctx)
{
    if (!ctx) {
        return;
    }

    rmt_disable(ctx->chan);
    rmt_del_encoder(ctx->encoder);
    rmt_del_channel(ctx->chan);
    free(ctx->buf);
    free(ctx);
}

bool neopixel_SetPixel(tNeopixelContext *ctx, tNeopixel *pixel, uint32_t pixelCount)
{
    uint32_t i;
    rmt_transmit_config_t tx_cfg = {
        .loop_count = 0,
    };

    if (!ctx || !pixel) {
        return false;
    }

    for (i = 0; i < pixelCount; ++i) {
        uint32_t idx = pixel[i].index;
        if (idx >= ctx->pixels) {
            continue;
        }
        rgb_to_grb(pixel[i].rgb, &ctx->buf[idx * 3]);
    }

    if (rmt_transmit(ctx->chan, ctx->encoder, ctx->buf, ctx->pixels * 3, &tx_cfg) != ESP_OK) {
        return false;
    }

    return rmt_tx_wait_all_done(ctx->chan, portMAX_DELAY) == ESP_OK;
}

uint32_t neopixel_GetRefreshRate(tNeopixelContext *ctx)
{
    if (!ctx || ctx->pixels == 0) {
        return 0;
    }

    return 800000UL / (24UL * ctx->pixels);
}
