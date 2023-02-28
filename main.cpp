// https://docs.espressif.com/projects/esp-at/en/release-v2.1.0.0_esp8266/AT_Command_Set/

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/adc.h"
#include "pico/bootrom.h"
#include "tusb.h"

static const uint32_t ESP_AT_TIMEOUT = 10'000; // 10 s

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

bool wait_for_uart(const char *pattern, absolute_time_t timeout_ms = ESP_AT_TIMEOUT)
{
    char buffer[16];
    size_t buffer_index = 0;
    absolute_time_t end = make_timeout_time_ms(timeout_ms);

    buffer[0] = 0; // make sure first char is null

    for(;;)
    {
        while (!uart_is_readable(uart1))
        {
            if (time_reached(end))
            {
                return false;
            }
        }
        if (buffer_index < 15)
        {
            buffer[buffer_index] = uart_getc(uart1);
            buffer_index++;
            buffer[buffer_index] = 0; // put null
        }
        else
        {
            for(int i = 0; i < 14; i++)
            {
                buffer[i] = buffer[i+1];
            }
            buffer[14] = uart_getc(uart1);
            buffer[15] = 0;
        }
        if (find_pattern(pattern, buffer) >= 0)
        {
            return true;
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

bool mqtt_conncfg(const char* keep_alive, const char *lwt_topic, const char *lwt_data)
{
    uart_puts(uart1, "AT+MQTTCONNCFG=0,");
    uart_puts(uart1, keep_alive);
    uart_puts(uart1, ",0,\"");
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

    init_onboard_temperature();

    gpio_init(PIN_LED);
    gpio_set_dir(PIN_LED, GPIO_OUT);
    gpio_put(PIN_LED, true);

    gpio_init(PIN_ESP8285_RST);
    gpio_set_dir(PIN_ESP8285_RST, GPIO_OUT);
    gpio_put(PIN_ESP8285_RST, false);

    gpio_init(PIN_ESP8285_MODE);
    gpio_set_dir(PIN_ESP8285_MODE, GPIO_OUT);
    gpio_put(PIN_ESP8285_MODE, true); // true = run, false = flash

    // Reset ESP8285
    sleep_ms(1);
    gpio_put(PIN_ESP8285_RST, true);

    wait_for_uart("WIFI GOT IP");

    gpio_put(PIN_LED, false);

    mqtt_usercfg("1", "rp2040");
    mqtt_conncfg("60", "home/nodes/sensor/rp2040/status", "offline");
    mqtt_conn("10.0.0.167", "1883");
    mqtt_pub("home/nodes/sensor/rp2040/status", "online");

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
             "\"name\":\"rp2040\""
             "}"
             "}");

    uint32_t old_baudrate = baudrate;
    absolute_time_t send_temp = get_absolute_time();
    for (;;)
    {
        const uint32_t new_baudrate = baudrate;
        if (new_baudrate != old_baudrate)
        {
            gpio_put(PIN_LED, true);
            gpio_put(PIN_ESP8285_RST, false);

            uart_deinit(uart1);
            uart_init(uart1, new_baudrate);
            old_baudrate = new_baudrate;

            sleep_ms(10);

            gpio_put(PIN_ESP8285_RST, true);
            gpio_put(PIN_LED, false);
        }

        if (time_reached(send_temp))
        {
            char buffer[32];
            sprintf(buffer, "%.02f", read_onboard_temperature());
            mqtt_pub("home/nodes/sensor/rp2040/temperature", buffer);
            send_temp = make_timeout_time_ms(60'000); // wait 1 minute
        }

        int char_usb = getchar_timeout_us(0);
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

extern "C" void tud_cdc_line_coding_cb(__unused uint8_t itf, cdc_line_coding_t const *p_line_coding)
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
