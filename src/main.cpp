#include <stdio.h>
#include "pico/binary_info.h"
#include "pico/stdlib.h"
#include "seesaw.h"
#include "usb_cdc.h"

#define USE_I2C_DMA 1

#if USE_I2C_DMA
#include "i2c_dma.h"
static i2c_dma_t *i2c0_dma;
#else
#include "hardware/i2c.h"
#endif

void ss_read(uint8_t regHigh, uint8_t regLow, uint8_t *buf, uint8_t num)
{
    uint8_t pos = 0;
    uint8_t prefix[2];
    prefix[0] = (uint8_t)regHigh;
    prefix[1] = (uint8_t)regLow;
    while (pos < num)
    {
        uint8_t read_now = [num, pos]
        {
            const uint8_t a = num - pos;
            return 32 < a ? 32 : a;
        }();
        i2c_write_blocking(i2c_default, SEESAW_ADDRESS, prefix, 2, true);
        i2c_read_blocking(i2c_default, SEESAW_ADDRESS, buf + pos, read_now, false);
        pos += read_now;
    }
}

void ss_getID()
{
    uint8_t c = 0;
    ss_read(SEESAW_STATUS_BASE, SEESAW_STATUS_HW_ID, &c, 1);
}

void ss_pinMode(int pins)
{
    uint8_t cmd[] = {SEESAW_GPIO_BASE,
                     SEESAW_GPIO_DIRSET_BULK,
                     (uint8_t)(pins >> 24), (uint8_t)(pins >> 16),
                     (uint8_t)(pins >> 8), (uint8_t)pins};

#if USE_I2C_DMA
    i2c_dma_write(i2c0_dma, SEESAW_ADDRESS, cmd, sizeof(cmd));
#else
    i2c_write_blocking(i2c_default, SEESAW_ADDRESS, cmd, sizeof(cmd), false);
#endif
}

void ss_digitalWrite(uint32_t pins, uint8_t value)
{
    uint8_t cmd[] = {SEESAW_GPIO_BASE,
                     value ? SEESAW_GPIO_BULK_SET : SEESAW_GPIO_BULK_CLR,
                     (uint8_t)(pins >> 24), (uint8_t)(pins >> 16),
                     (uint8_t)(pins >> 8), (uint8_t)pins};

#if USE_I2C_DMA
    i2c_dma_write(i2c0_dma, SEESAW_ADDRESS, cmd, sizeof(cmd));
#else
    i2c_write_blocking(i2c_default, SEESAW_ADDRESS, cmd, sizeof(cmd), false);
#endif
}

void ss_analogWrite(uint32_t pin, uint16_t value)
{
    uint8_t cmd[] = {SEESAW_TIMER_BASE,
                     SEESAW_TIMER_PWM,
                     (uint8_t)pin,
                     (uint8_t)(value >> 8), (uint8_t)value};

#if USE_I2C_DMA
    i2c_dma_write(i2c0_dma, SEESAW_ADDRESS, cmd, sizeof(cmd));
#else
    i2c_write_blocking(i2c_default, SEESAW_ADDRESS, cmd, sizeof(cmd), false);
#endif
}

uint16_t make_lumi(uint16_t value)
{
    const float normalized = value / 65535.0f;
    return (uint16_t)(65535 * normalized * normalized);
}

int main()
{
    stdio_init_all();

    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    uart_init(uart0, 115200);
    gpio_set_function(PICO_DEFAULT_UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(PICO_DEFAULT_UART_RX_PIN, GPIO_FUNC_UART);
    // Make the UART pins available to picotool
    bi_decl(bi_2pins_with_func(PICO_DEFAULT_UART_TX_PIN, PICO_DEFAULT_UART_RX_PIN, GPIO_FUNC_UART));
    usb_link_uart(uart0);

    const uint32_t i2c_speed = 400'000;

#if USE_I2C_DMA
    i2c_dma_init(&i2c0_dma, i2c_default, i2c_speed, PICO_DEFAULT_I2C_SDA_PIN, PICO_DEFAULT_I2C_SCL_PIN);
#else
    i2c_init(i2c_default, i2c_speed);
    gpio_set_function(PICO_DEFAULT_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(PICO_DEFAULT_I2C_SDA_PIN);
    gpio_pull_up(PICO_DEFAULT_I2C_SCL_PIN);
#endif

    // Make the I2C pins available to picotool
    bi_decl(bi_2pins_with_func(PICO_DEFAULT_I2C_SDA_PIN, PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C));

    ss_pinMode((1 << 4) | (1 << 5));

    int32_t rate = 100;
    int32_t fadeAmount = 655;
    int32_t brightness = 0;

    absolute_time_t next_toggle = make_timeout_time_ms(rate);
    absolute_time_t uart_timeout = make_timeout_time_ms(0);

    for (;;)
    {
        if (time_reached(next_toggle) && time_reached(uart_timeout))
        {
            next_toggle = make_timeout_time_ms(rate);
            ss_analogWrite(4, 65535 - make_lumi(brightness));
            ss_analogWrite(5, 65535 - make_lumi(65535 - brightness));

            brightness += fadeAmount;
            if (brightness <= 0 || brightness >= 65535)
            {
                fadeAmount *= -1;
                brightness += fadeAmount;
            }
        }

        if (usb_get_bitrate() > 115200)
        {
            // UPDI speed is 230400
            const int char_usb = getchar_timeout_us(1);
            if (char_usb != PICO_ERROR_TIMEOUT)
            {
                uart_timeout = make_timeout_time_ms(1000);
                gpio_xor_mask(1 << PICO_DEFAULT_LED_PIN);
                uart_putc_raw(uart0, char_usb);
            }
            if (uart_is_readable(uart0))
            {
                uart_timeout = make_timeout_time_ms(1000);
                putchar_raw(uart_getc(uart0));
            }
        }
    }
    return 0;
}
