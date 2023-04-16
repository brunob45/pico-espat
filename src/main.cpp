// https://docs.espressif.com/projects/esp-at/en/release-v2.1.0.0_esp8266/AT_Command_Set/Wi-Fi_AT_Commands.html#cmd-JAP

#include <stdio.h>
#include "pico/stdlib.h"

#include "usb_cdc.h"

int main()
{
    stdio_init_all();

    uint32_t old_baudrate = usb_get_baudrate();

    gpio_init(25);
    gpio_set_dir(25, GPIO_OUT);
    gpio_put(25, true);

    uart_init(uart0, old_baudrate);
    gpio_set_function(0, GPIO_FUNC_UART);
    gpio_set_function(1, GPIO_FUNC_UART);

    for (;;)
    {
        const uint32_t new_baudrate = usb_get_baudrate();
        if (new_baudrate != old_baudrate)
        {
            uart_deinit(uart0);
            uart_init(uart0, new_baudrate);
            old_baudrate = new_baudrate;
        }

        int char_usb = getchar_timeout_us(1);
        if (char_usb != PICO_ERROR_TIMEOUT)
        {
            gpio_put(25, !gpio_get(25));
            uart_putc_raw(uart0, char_usb);
        }
        if (uart_is_readable(uart0))
        {
            putchar_raw(uart_getc(uart0));
        }
    }
    return 0;
}
