#include <stdio.h>
#include "pico/binary_info.h"
#include "pico/stdlib.h"
#include "seesaw.h"
#include "usb_cdc.h"

uint16_t make_lumi(uint16_t value)
{
    const uint32_t squared = (uint32_t)value * value;
    return (uint16_t)(squared >> 16);
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

    ss_init(400'000);

    // Make the I2C pins available to picotool
    bi_decl(bi_2pins_with_func(PICO_DEFAULT_I2C_SDA_PIN, PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C));

    // ss_pinMode((1 << 4) | (1 << 5), true);

    int32_t rate = 100;
    int32_t fadeAmount = 0xffff / 100;
    int32_t brightness = 0;

    absolute_time_t next_toggle = make_timeout_time_ms(rate);
    absolute_time_t uart_timeout = make_timeout_time_ms(0);

    for (;;)
    {
        if (time_reached(next_toggle) && time_reached(uart_timeout))
        {
            next_toggle = make_timeout_time_ms(rate);
            uint16_t res = ss_analogRead(0) * 64; // 1024 --> 65536
            ss_analogWrite(4, 0xffff - make_lumi(res));
            ss_analogWrite(5, 0xffff - make_lumi(brightness));

            brightness += fadeAmount;
            if (brightness <= 0 || brightness >= 0xffff)
            {
                fadeAmount *= -1;
                brightness += fadeAmount;
            }

            if (usb_get_bitrate() <= 115200)
            {
                printf("%d\n", res);
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
