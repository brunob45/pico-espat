// https://docs.espressif.com/projects/esp-at/en/release-v2.1.0.0_esp8266/AT_Command_Set/Wi-Fi_AT_Commands.html#cmd-JAP

#include <stdio.h>
#include "pico/stdlib.h"

int main()
{
    stdio_init_all();
    uart_init(uart1, 115200);

    gpio_set_function(4, GPIO_FUNC_UART);
    gpio_set_function(5, GPIO_FUNC_UART);

    sleep_ms(5000);

    uart_puts(uart1, "AT+MQTTUSERCFG=0,1,\"rp2040\",\"\",\"\",0,0,\"\"\r\n");
    sleep_ms(100);
    uart_puts(uart1, "AT+MQTTCONNCFG=0,0,0,\"home/nodes/sensor/rp2040/status\",\"offline\",0,0\r\n");
    sleep_ms(100);
    uart_puts(uart1, "AT+MQTTCONN=0,\"10.0.0.167\",1883,0\r\n");
    sleep_ms(100);
    uart_puts(uart1, "AT+MQTTPUB=0,\"home/nodes/sensor/rp2040/status\",\"online\",0,0\r\n");
    sleep_ms(100);

    while (true)
    {
        int char_usb = getchar_timeout_us(1);
        if (char_usb != PICO_ERROR_TIMEOUT)
        {
            uart_putc_raw(uart1, char_usb);
            if (char_usb == '\r')
            {
                uart_putc_raw(uart1, '\n');
            }
        }
        if (uart_is_readable(uart1))
        {
            putchar_raw(uart_getc(uart1));
        }
    }
    return 0;
}
