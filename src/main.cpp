// https://docs.espressif.com/projects/esp-at/en/release-v2.1.0.0_esp8266/AT_Command_Set/

#include <stdio.h>
#include "pico/stdlib.h"

#include "usb_cdc.h"

#include "hardware/adc.h"

#include "ws2812.h"
#include "mqtt.h"

static const uint NEOPIXEL = 11;
static const uint PIN_LED = 12;
static const uint PIN_ESP8285_MODE = 13;
static const uint PIN_ESP8285_RST = 19;

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

    const float adc = (float)adc_read() * conversionFactor;
    const float tempC = 27.0f - (adc - 0.706f) / 0.001721f;

    return tempC;
}

int main()
{
    stdio_init_all();

    ws2812_init(NEOPIXEL);
    ws2812_color(WS2812_COLOR::CYAN);

    uart_init(uart1, usb_get_baudrate());
    gpio_set_function(4, GPIO_FUNC_UART);
    gpio_set_function(5, GPIO_FUNC_UART);

    init_onboard_temperature();

    gpio_init(PIN_ESP8285_RST);
    gpio_set_dir(PIN_ESP8285_RST, GPIO_OUT);
    gpio_put(PIN_ESP8285_RST, false);

    gpio_init(PIN_ESP8285_MODE);
    gpio_set_dir(PIN_ESP8285_MODE, GPIO_OUT);
    gpio_put(PIN_ESP8285_MODE, true); // true = run, false = flash

    // Reset ESP8285
    mqtt_reset(PIN_ESP8285_RST);

    const uint32_t reset_timeout = 10 * 60'000; // 10 minutes
    absolute_time_t esp_do_reset = make_timeout_time_ms(reset_timeout); // 10 minutes

    uint32_t old_baudrate = usb_get_baudrate();
    absolute_time_t send_temp = get_absolute_time();
    absolute_time_t send_status = get_absolute_time();
    for (;;)
    {
        const uint32_t new_baudrate = usb_get_baudrate();
        if (new_baudrate != old_baudrate)
        {
            uart_deinit(uart1);
            uart_init(uart1, new_baudrate);
            old_baudrate = new_baudrate;

            mqtt_reset(PIN_ESP8285_RST);
        }

        if (time_reached(send_temp))
        {
            send_temp = make_timeout_time_ms(60'000); // wait 1 minute

            char buffer[32];
            sprintf(buffer, "%.02f", read_onboard_temperature() - 15.0f);
            const bool success = mqtt_pub("home/nodes/sensor/rp2040/temperature", buffer, false);
            if (success)
            {
                esp_do_reset = make_timeout_time_ms(reset_timeout);
            }
        }
        else if (time_reached(send_status))
        {
            send_status = make_timeout_time_ms(15 * 60'000); // wait 15 minutes
            const bool success = mqtt_pub("home/nodes/sensor/rp2040/status", "online", false);
            if (success)
            {
                esp_do_reset = make_timeout_time_ms(reset_timeout);
            }
        }
        else if (time_reached(esp_do_reset))
        {
            // 10 minutes since last successful MQTT PUB, reset ESP
            const bool success = mqtt_reset(PIN_ESP8285_RST);
            if (success)
            {
                esp_do_reset = make_timeout_time_ms(reset_timeout);
            }
        }

        const int char_usb = getchar_timeout_us(0);
        if (char_usb != PICO_ERROR_TIMEOUT)
        {
            uart_putc_raw(uart1, char_usb);
        }
        if (uart_is_readable(uart1))
        {
            putchar_raw(uart_getc(uart1));
        }
    }
    return 0;
}
