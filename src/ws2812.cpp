
#include "ws2812.h"

#include "hardware/pio.h"
#include "hardware/clocks.h"

#include "ws2812.pio.h"

const bool IS_RGBW = true;

static uint ws2812_pin;

static PIO ws2812_pio = pio0;
static int ws2812_sm = 0;

static inline void put_pixel(uint32_t pixel_grb) {
    pio_sm_put_blocking(ws2812_pio, ws2812_sm, pixel_grb << 8u);
}

void ws2812_init(uint pin)
{
    // todo get free sm
    uint offset = pio_add_program(ws2812_pio, &ws2812_program);
    ws2812_program_init(ws2812_pio, ws2812_sm, offset, pin, 800000, IS_RGBW);
}

void ws2812_color(WS2812_COLOR color)
{
    put_pixel((uint32_t)color);
}

void ws2812_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    put_pixel(urgb_u32(r, g, b));
}

