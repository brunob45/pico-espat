// https://docs.espressif.com/projects/esp-at/en/release-v2.1.0.0_esp8266/AT_Command_Set/

#include <stdio.h>
#include "pico/stdlib.h"

int find_pattern(const char *pattern, const uint8_t *buffer)
{
    size_t i, j;

    size_t pattern_size;
    for (pattern_size = 0; pattern[pattern_size] != 0; pattern_size++)
        ;

    size_t buffer_size;
    for (buffer_size = 0; buffer[buffer_size] != 0; buffer_size++)
        ;

    if (pattern_size > buffer_size)
    {
        return -1; // pattern is bigger than buffer, cannot find it
    }

    for (size_t i = 0; i < (buffer_size - pattern_size); i++)
    {
        bool match = true;
        for (size_t j = 0; j < pattern_size; j++)
        {
            match &= (buffer[i] == pattern[j]);
        }
        if (match)
        {
            return i;
        }
    }
    return -1;
}

bool mqtt_usercfg(uint8_t scheme, const char *client_id, uint32_t timeout)
{
    uart_puts(uart1, "AT+MQTTUSERCFG=0,");
    if (scheme < 10)
    {
        uart_putc(uart1, '0' + scheme);
        uart_putc(uart1, ',');
    }
    else
    {
        uart_puts(uart1, "10,");
    }
    uart_puts(uart1, client_id);
    uart_puts(uart1, ",\"\",\"\",0,0,\"\"\r\n");

    uint8_t buffer[255] = {
        0,
    }; // init with null character
    uint8_t buffer_size = 0;
    bool match = false;

    absolute_time_t time_end = make_timeout_time_ms(timeout);
    while (!match && get_absolute_time() <= time_end)
    {
        if (uart_is_readable(uart1))
        {
            buffer[buffer_size] = uart_getc(uart1);
            ++buffer_size;
            buffer[buffer_size] = 0; // put null character;
            if (find_pattern("OK", buffer) >= 0)
            {
                return true;
            }
        }
    }
    return false;
}

int main()
{
    stdio_init_all();
    uart_init(uart1, 115200);

    gpio_set_function(4, GPIO_FUNC_UART);
    gpio_set_function(5, GPIO_FUNC_UART);

    sleep_ms(10000);

    uart_puts(uart1, "AT+MQTTUSERCFG=0,1,\"rp2040\",\"\",\"\",0,0,\"\"\r\n");
    sleep_ms(100);
    uart_puts(uart1, "AT+MQTTCONNCFG=0,0,0,\"home/nodes/sensor/rp2040/status\",\"offline\",0,0\r\n");
    sleep_ms(100);
    uart_puts(uart1, "AT+MQTTCONN=0,\"10.0.0.167\",1883,0\r\n");
    sleep_ms(100);
    uart_puts(uart1, "AT+MQTTPUB=0,\"home/nodes/sensor/rp2040/status\",\"online\",0,0\r\n");
    sleep_ms(100);
    uart_puts(uart1, "AT+MQTTPUBRAW=0,\"homeassistant/sensor/rp2040/monitor/config\",75,0,0\r\n");
    sleep_ms(100);
    uart_puts(uart1, "{\"name\":\"rp2040\", \"~\":\"home/nodes/sensor/rp2040\", \"state_topic\":\"~/status\"}\r\n");

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
