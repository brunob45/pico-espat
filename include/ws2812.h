#ifndef PICO_ESPAT_WS2812
#define PICO_ESPAT_WS2812

#include "pico/stdlib.h"

constexpr uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
    const uint32_t BRIGHTNESS = 16;
    return
            ((uint32_t) (r/BRIGHTNESS) << 8) |
            ((uint32_t) (g/BRIGHTNESS) << 16) |
            (uint32_t) (b/BRIGHTNESS);
}

enum class WS2812_COLOR : uint32_t {
    RED = urgb_u32(0xff, 0, 0),
    GREEN = urgb_u32(0, 0xff, 0),
    BLUE = urgb_u32(0, 0, 0xff),
    MAGENTA = urgb_u32(0xff, 0, 0xff),
    YELLOW = urgb_u32(0xff, 0xff, 0),
    CYAN = urgb_u32(0, 0xff, 0xff),
    WHITE = urgb_u32(0xff, 0xff, 0xff),
};

void ws2812_init(uint pin);
void ws2812_color(WS2812_COLOR color);
void ws2812_rgb(uint8_t r, uint8_t g, uint8_t b);

#endif // PICO_ESPAT_WS2812