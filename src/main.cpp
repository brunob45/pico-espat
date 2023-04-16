// https://docs.espressif.com/projects/esp-at/en/release-v2.1.0.0_esp8266/AT_Command_Set/Wi-Fi_AT_Commands.html#cmd-JAP

#include <stdio.h>
#include "pico/stdlib.h"

int main()
{
    stdio_init_all();

    while (true)
    {
        int char_usb = getchar_timeout_us(1);
        if (char_usb != PICO_ERROR_TIMEOUT)
        {
            uart_putc_raw(uart0, char_usb);
        }
        if (uart_is_readable(uart0))
        {
            putchar_raw(uart_getc(uart0));
        }
    }
    return 0;
}
