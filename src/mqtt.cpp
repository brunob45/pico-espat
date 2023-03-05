#include "mqtt.h"

#include <stdio.h>

#include "ws2812.h"

static const uint32_t ESP_AT_TIMEOUT = 10'000; // 10 s

// Util
static size_t string_size(const char *s, const size_t max_len = 512)
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

static int find_pattern(const char *pattern, const char *buffer)
{
    const size_t pattern_size = string_size(pattern);
    const size_t buffer_size = string_size(buffer);

    if (pattern_size > buffer_size)
    {
        return -1; // pattern is bigger than buffer, cannot find it
    }

    for (size_t i = 0; i <= (buffer_size - pattern_size); i++)
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

static bool wait_for_uart(const char *pattern, uint32_t timeout_ms = ESP_AT_TIMEOUT)
{
    char buffer[16];
    char pattern_swap[16];

    const uint pattern_size = string_size(pattern);

    if (pattern_size >= sizeof(pattern_swap))
    {
        // pattern is larger than buffer, exit early
        return -1;
    }

    // swap buffer order: "abcde" becomes "edcba" for faster pattern match
    for (uint i = 0; i < pattern_size; i++)
    {
        pattern_swap[i] = pattern[pattern_size - 1 - i];
    }
    pattern_swap[pattern_size] = 0; // insert null character at the end

    buffer[0] = 0; // make sure first char is null

    const absolute_time_t end = make_timeout_time_ms(timeout_ms);
    while (!time_reached(end))
    {
        if (uart_is_readable(uart1))
        {
            const char new_char = uart_getc(uart1);
            putchar_raw(new_char);

            for (int i = pattern_size; i > 0; i--)
            {
                buffer[i] = buffer[i - 1];
            }

            buffer[0] = new_char;
            buffer[pattern_size] = 0;

            if (find_pattern(pattern_swap, buffer) >= 0)
            {
                return true;
            }
        }
    }
    return false;
}

// MQTT
bool mqtt_usercfg(const char *scheme, const char *client_id)
{
    uart_puts(uart1, "AT+MQTTUSERCFG=0,");
    uart_puts(uart1, scheme);
    uart_puts(uart1, ",\"");
    uart_puts(uart1, client_id);
    uart_puts(uart1, "\",\"\",\"\",0,0,\"\"\r\n");

    return wait_for_uart("OK");
}

bool mqtt_conncfg(const char *keep_alive, const char *lwt_topic, const char *lwt_data, const bool retain)
{
    uart_puts(uart1, "AT+MQTTCONNCFG=0,");
    uart_puts(uart1, keep_alive);
    uart_puts(uart1, ",0,\"");
    uart_puts(uart1, lwt_topic);
    uart_puts(uart1, "\",\"");
    uart_puts(uart1, lwt_data);
    uart_puts(uart1, "\",0,");
    uart_puts(uart1, retain ? "1" : "0");
    uart_puts(uart1, "\r\n");

    return wait_for_uart("OK");
}

bool mqtt_conn(const char *host, const char *port)
{
    uart_puts(uart1, "AT+MQTTCONN=0,\"");
    uart_puts(uart1, host);
    uart_puts(uart1, "\",");
    uart_puts(uart1, port);
    uart_puts(uart1, ",0\r\n");

    return wait_for_uart("OK");
}

bool mqtt_clean()
{
    uart_puts(uart1, "AT+MQTTCLEAN=0\r\n");
    return wait_for_uart("OK");
}

bool mqtt_pub(const char *topic, const char *data, const bool retain)
{
    char data_len[32];
    sprintf(data_len, "%d", string_size(data));
    uart_puts(uart1, "AT+MQTTPUBRAW=0,\"");
    uart_puts(uart1, topic);
    uart_puts(uart1, "\",");
    uart_puts(uart1, data_len);
    uart_puts(uart1, ",0,");
    uart_puts(uart1, retain ? "1" : "0");
    uart_puts(uart1, "\r\n");

    wait_for_uart(">");

    uart_puts(uart1, data);
    puts(data); // echo in usb

    return wait_for_uart("OK");
}

void mqtt_reset(uint pin)
{
    // toggle reset line
    gpio_put(pin, false);
    ws2812_color(WS2812_COLOR::RED);

    sleep_ms(1);
    gpio_put(pin, true);

    wait_for_uart("WIFI GOT IP");

    ws2812_color(WS2812_COLOR::GREEN);

    mqtt_usercfg("1", "rp2040");
    mqtt_conncfg("0", "home/nodes/sensor/rp2040/status", "offline", true);
    mqtt_conn("10.0.0.167", "1883");

    mqtt_pub("homeassistant/sensor/rp2040/temperature/config",
             "{"
             "\"name\":\"Challenger RP2040 Temperature\","
             "\"uniq_id\":\"rp2040-10af4047_temp\","
             "\"dev_cla\":\"temperature\","
             "\"~\":\"home/nodes/sensor/rp2040\","
             "\"state_topic\":\"~/temperature\","
             "\"unit_of_meas\":\"\u00b0C\","
             "\"avty_t\": \"~/status\","
             "\"pl_avail\": \"online\","
             "\"pl_not_avail\": \"offline\","
             "\"dev\":{"
             "\"identifiers\":[\"rp2040-10af4047\"],"
             "\"name\":\"RP2040\""
             "}"
             "}",
             true);
}
