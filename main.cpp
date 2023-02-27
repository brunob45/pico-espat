// https://docs.espressif.com/projects/esp-at/en/release-v2.1.0.0_esp8266/AT_Command_Set/

#include <stdio.h>
#include "pico/stdlib.h"

static const uint32_t ESP_AT_TIMEOUT = 100;

// Util
size_t string_size(const char *s, const size_t max_len = 256)
{
    for (size_t i = 0; i < max_len; i++)
    {
        if (s[i] == 0)
        {
            return i;
        }
    }
    return 0;
}

int find_pattern(const char *pattern, const char *buffer)
{
    size_t i, j;

    size_t pattern_size = string_size(pattern);
    size_t buffer_size = string_size(buffer);

    if (pattern_size > buffer_size)
    {
        return -1; // pattern is bigger than buffer, cannot find it
    }

    for (size_t i = 0; i < (buffer_size - pattern_size); i++)
    {
        bool match = true;
        for (size_t j = 0; j < pattern_size; j++)
        {
            if (buffer[i + j] != pattern[j])
            {
                match = false;
                break;
            }
        }
        if (match)
        {
            return i;
        }
    }
    return -1;
}

bool at_ack()
{
    const size_t max_len = 255;
    char buffer[max_len] = {
        0,
    }; // init with null character
    size_t buffer_size = 0;

    uart_puts(uart1, "\r\n");
    absolute_time_t time_end = make_timeout_time_ms(ESP_AT_TIMEOUT);
    while (!time_reached(time_end))
    {
        if (uart_is_readable(uart1))
        {
            buffer[buffer_size] = uart_getc(uart1);
            ++buffer_size;
            buffer[buffer_size] = 0; // put null character;
            if (find_pattern("OK", buffer) >= 0)
            {
                puts("OK");
                return true;
            }
            else if (buffer_size >= max_len)
            {
                puts("BUFFER");
                return false;
            }
        }
    }
    puts("TIMEOUT");
    return false;
}

// WIFI
bool is_connected()
{
    char buffer[256] = {
        0,
    }; // init with null character
    size_t buffer_size = 0;

    uart_puts(uart1, "AT+CWJAP?\r\n");

    absolute_time_t time_end = make_timeout_time_ms(ESP_AT_TIMEOUT);
    while (!time_reached(time_end))
    {
        if (uart_is_readable(uart1))
        {
            buffer[buffer_size] = uart_getc(uart1);
            ++buffer_size;
            buffer[buffer_size] = 0; // put null character;
        }
    }

    return find_pattern("+CWJAP:", buffer) >= 0;
}

// MQTT
bool mqtt_usercfg(const char *scheme, const char *client_id)
{
    uart_puts(uart1, "AT+MQTTUSERCFG=0,");
    uart_puts(uart1, scheme);
    uart_puts(uart1, ",\"");
    uart_puts(uart1, client_id);
    uart_puts(uart1, "\",\"\",\"\",0,0,\"\"");

    return at_ack();
}

bool mqtt_conncfg(const char *lwt_topic, const char *lwt_data)
{
    uart_puts(uart1, "AT+MQTTCONNCFG=0,0,0,\"");
    uart_puts(uart1, lwt_topic);
    uart_puts(uart1, "\",\"");
    uart_puts(uart1, lwt_data);
    uart_puts(uart1, "\",0,0\r\n");

    return at_ack();
}

bool mqtt_conn(const char *host, const char *port)
{
    uart_puts(uart1, "AT+MQTTCONN=0,\"");
    uart_puts(uart1, host);
    uart_puts(uart1, "\",");
    uart_puts(uart1, port);
    uart_puts(uart1, ",0\r\n");

    return at_ack();
}

bool mqtt_clean()
{
    uart_puts(uart1, "AT+MQTTCLEAN=0");
    return at_ack();
}

bool mqtt_pub(const char *topic, const char *data)
{
    char data_len[10];
    sprintf(data_len, "%d", string_size(data));
    uart_puts(uart1, "AT+MQTTPUBRAW=0,\"");
    uart_puts(uart1, topic);
    uart_puts(uart1, "\",");
    uart_puts(uart1, data_len);
    uart_puts(uart1, ",0,0\r\n");

    bool ready = false;
    while (!ready)
    {
        if (uart_is_readable(uart1))
        {
            ready = (uart_getc(uart1) == '>');
        }
    }

    uart_puts(uart1, data);

    return at_ack();
}

int main()
{
    stdio_init_all();
    uart_init(uart1, 115200);

    gpio_set_function(4, GPIO_FUNC_UART);
    gpio_set_function(5, GPIO_FUNC_UART);

    while (!is_connected())
    {
        // wait for wifi...
    }

    mqtt_usercfg("1", "rp2040");
    mqtt_conncfg("home/nodes/sensor/rp2040/status", "offline");
    mqtt_conn("10.0.0.167", "1883");
    mqtt_pub("home/nodes/sensor/rp2040/status", "online");

    mqtt_pub("homeassistant/sensor/rp2040/monitor/config",
             "{\"name\":\"rp2040\", \"~\":\"home/nodes/sensor/rp2040\", \"state_topic\":\"~/status\"}");

    for (;;)
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
