#include <stdio.h>
#include "pico/binary_info.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "seesaw.h"
#include "usb_cdc.h"

static semaphore_t sem_decoder;
static uint32_t tooth_avg;
static int32_t tooth_accel;
const uint decode_pin_base = 14;

struct
{
    uint8_t pins;
    uint8_t delay;
} decode_sim[12] = {
    {.pins = 0b11, .delay = 65},
    {.pins = 0b10, .delay = 5},
    {.pins = 0b00, .delay = 110},
    {.pins = 0b10, .delay = 70},
    {.pins = 0b00, .delay = 105},
    {.pins = 0b01, .delay = 5},
    {.pins = 0b11, .delay = 70},
    {.pins = 0b01, .delay = 55},
    {.pins = 0b00, .delay = 55},
    {.pins = 0b10, .delay = 70},
    {.pins = 0b00, .delay = 105},
    {.pins = 0b01, .delay = 5},
};

uint16_t make_lumi(uint16_t value)
{
    const uint32_t squared = (uint32_t)value * value;
    return (uint16_t)(squared >> 16);
}

void core1_entry()
{
    bool last_state = false;
    uint32_t tooth_len[2];
    uint32_t last_micros;
    absolute_time_t debounce = get_absolute_time();

    for (;;)
    {
        const bool current_state = gpio_get(decode_pin_base);
        if (last_state != current_state)
        {
            const uint32_t now_micros = time_us_32();
            if (time_reached(debounce))
            {
                debounce = make_timeout_time_ms(1);

                tooth_len[current_state ? 0 : 1] = now_micros - last_micros;
                const uint32_t last_avg = tooth_avg;
                tooth_avg = tooth_len[0] + tooth_len[1];
                tooth_accel = tooth_avg - last_avg;
                last_micros = now_micros;
                sem_release(&sem_decoder);
            }
            last_state = current_state;
        }
    }
}

int main()
{
    stdio_init_all();

    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    gpio_init_mask(0b11 << decode_pin_base);
    gpio_set_dir_out_masked(0b11 << decode_pin_base);

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

    const uint32_t rate_ms = 100;

    uint32_t level = 0;
    uint8_t tooth = 0;

    absolute_time_t next_update = make_timeout_time_ms(rate_ms);
    absolute_time_t uart_timeout = get_absolute_time();
    absolute_time_t next_toggle = make_timeout_time_ms(rate_ms);

    sem_init(&sem_decoder, 0, 1);

    multicore_launch_core1(core1_entry);

    for (;;)
    {
        if (time_reached(next_update))
        {
            gpio_clr_mask(1 << PICO_DEFAULT_LED_PIN);

            level = ss_analogRead(0) * 64; // 1024 --> 65536
            ss_analogWrite(4, 0xffff - make_lumi(level));
            ss_analogWrite(5, 0xffff - make_lumi(level));

            next_update = delayed_by_ms(next_update, rate_ms);
        }

        if (sem_try_acquire(&sem_decoder))
        {
            if ((usb_get_bitrate() <= 115200) && time_reached(uart_timeout))
            {
                printf("%d %d\n", tooth_avg, tooth_accel);
            }
        }

        if (time_reached(next_toggle))
        {
            gpio_put_masked(0b11 << decode_pin_base, decode_sim[tooth].pins << decode_pin_base);
            const uint next_delay = decode_sim[tooth].delay * (level + 30000) / 1000;

            ++tooth;
            if (tooth >= 12)
            {
                tooth = 0;
            }

            next_toggle = delayed_by_us(next_toggle, next_delay);
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
