// https://docs.espressif.com/projects/esp-at/en/release-v2.1.0.0_esp8266/AT_Command_Set/

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "pico/bootrom.h"
#include "tusb.h"

static const uint32_t ESP_AT_TIMEOUT = 100;

static const uint NEOPIXEL = 11;
static const uint PIN_LED = 12;
static const uint PIN_ESP8285_MODE = 13;
static const uint PIN_ESP8285_RST = 19;

static volatile uint32_t baudrate = 115200;

void init_onboard_temperature()
{
    adc_init();
    adc_set_temp_sensor_enabled(true);
    adc_select_input(4);
}

float read_onboard_temperature()
{
    /* 12-bit conversion, assume max value == ADC_VREF == 3.3 V */
    const float conversionFactor = 3.3f / (1 << 12);

    float adc = (float)adc_read() * conversionFactor;
    float tempC = 27.0f - (adc - 0.706f) / 0.001721f;

    return tempC;
}

// Util
size_t string_size(const char *s, const size_t max_len = 512)
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

size_t uart_gets(uart_inst_t *uart, char *buffer, size_t buffer_size)
{
    size_t len = 0;
    absolute_time_t time_end = make_timeout_time_ms(ESP_AT_TIMEOUT);

    buffer[0] = 0; // put null character

    while (!time_reached(time_end) && (len < buffer_size))
    {
        if (uart_is_readable(uart))
        {
            buffer[len] = uart_getc(uart);
            ++len;
            buffer[len] = 0; // put null character
        }
    }

    return len;
}

bool wait_for_uart(const char *pattern)
{
    char buffer[256];
    uart_gets(uart1, buffer, sizeof(buffer));
    return find_pattern(pattern, buffer) >= 0;
}

// WIFI
bool is_connected()
{
    uart_puts(uart1, "AT+CWJAP?\r\n");
    return wait_for_uart("+CWJAP:");
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

bool mqtt_conncfg(const char *lwt_topic, const char *lwt_data)
{
    uart_puts(uart1, "AT+MQTTCONNCFG=0,0,0,\"");
    uart_puts(uart1, lwt_topic);
    uart_puts(uart1, "\",\"");
    uart_puts(uart1, lwt_data);
    uart_puts(uart1, "\",0,0\r\n");

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

    return wait_for_uart("OK");
}

int main()
{
    stdio_init_all();

    uart_init(uart1, baudrate);
    gpio_set_function(4, GPIO_FUNC_UART);
    gpio_set_function(5, GPIO_FUNC_UART);

    gpio_init(PIN_LED);
    gpio_set_dir(PIN_LED, GPIO_OUT);
    gpio_put(PIN_LED, true);

    gpio_init(PIN_ESP8285_RST);
    gpio_set_dir(PIN_ESP8285_RST, GPIO_OUT);
    gpio_put(PIN_ESP8285_RST, false);

    gpio_init(PIN_ESP8285_MODE);
    gpio_set_dir(PIN_ESP8285_MODE, GPIO_OUT);
    gpio_put(PIN_ESP8285_MODE, false); // true = run, false = flash

    gpio_put(PIN_LED, false);

    // reset ESP8285
    // sleep_ms(5000);
    // gpio_put(PIN_ESP8285_RST, true);
    // gpio_put(PIN_LED, false);

    // absolute_time_t reset_timeout = at_the_end_of_time;
    uint old_baudrate = baudrate;
    for (;;)
    {
        const uint32_t new_baudrate = baudrate;
        if (new_baudrate != old_baudrate)
        {
            // gpio_put(PIN_ESP8285_RST, false);
            // gpio_put(PIN_LED, true);

            uart_deinit(uart1);
            uart_init(uart1, new_baudrate);
            old_baudrate = new_baudrate;

            // gpio_put(PIN_ESP8285_RST, true);
            // gpio_put(PIN_LED, false);
        }

        if (uart_is_writable(uart1))
        {
            int char_usb = getchar_timeout_us(0);
            if (char_usb != PICO_ERROR_TIMEOUT)
            {
                uart_putc_raw(uart1, char_usb);
            }
        }
        while (uart_is_readable(uart1))
        {
            putchar_raw(uart_getc(uart1));
        }
    }
    return 0;
}

extern "C"
void tud_cdc_line_coding_cb(__unused uint8_t itf, cdc_line_coding_t const *p_line_coding)
{
    if (p_line_coding->bit_rate == PICO_STDIO_USB_RESET_MAGIC_BAUD_RATE)
    {
#ifdef PICO_STDIO_USB_RESET_BOOTSEL_ACTIVITY_LED
        const uint gpio_mask = 1u << PICO_STDIO_USB_RESET_BOOTSEL_ACTIVITY_LED;
#else
        const uint gpio_mask = 0u;
#endif
        reset_usb_boot(gpio_mask, PICO_STDIO_USB_RESET_BOOTSEL_INTERFACE_DISABLE_MASK);
    }
    baudrate = p_line_coding->bit_rate;
}

extern "C"
void tud_cdc_line_state_cb(uint8_t itf, bool dtr, bool rts)
{
    gpio_put(PIN_ESP8285_MODE, dtr);
    gpio_put(PIN_ESP8285_RST, rts);
    gpio_put(PIN_LED, rts);
}