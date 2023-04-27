#include <stdio.h>
#include "pico/binary_info.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "seesaw.h"
#include "usb_cdc.h"

uint16_t make_lumi(uint16_t value)
{
    const uint32_t squared = (uint32_t)value * value;
    return (uint16_t)(squared >> 16);
}

static semaphore_t sem_micros;
static uint32_t delta_micros;
static uint32_t last_micros;

void core1_entry()
{
    bool last_state = false;

    for (;;)
    {
        const bool current_state = gpio_get(15);
        if (!last_state && current_state)
        {
            const uint32_t now_micros = time_us_32();
            delta_micros = now_micros - last_micros;
            last_micros = now_micros;
            sem_release(&sem_micros);
        }
        last_state = current_state;
    }
}

int main()
{
    stdio_init_all();

    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);

    gpio_init(15);
    gpio_set_dir(15, GPIO_OUT);

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

    absolute_time_t next_update = make_timeout_time_ms(rate_ms);
    absolute_time_t uart_timeout = make_timeout_time_ms(0);
    absolute_time_t next_toggle = make_timeout_time_ms(rate_ms);

    sem_init(&sem_micros, 0, 1);
    multicore_launch_core1(core1_entry);

    for (;;)
    {
        if (time_reached(next_update))
        {
            next_update = delayed_by_ms(next_update, rate_ms);
            gpio_clr_mask(1 << PICO_DEFAULT_LED_PIN);

            level = ss_analogRead(0) * 64; // 1024 --> 65536
            ss_analogWrite(4, 0xffff - make_lumi(level));
            ss_analogWrite(5, 0xffff - make_lumi(level));
        }

        if (sem_try_acquire(&sem_micros))
        {
            if ((usb_get_bitrate() <= 115200) && time_reached(uart_timeout))
            {
                printf("%d\n", delta_micros);
            }
        }

        if (time_reached(next_toggle))
        {
            next_toggle = delayed_by_ms(next_toggle, level / 1000 + 100);
            gpio_xor_mask(1 << 15);
        }

        if (usb_get_bitrate() > 115200)
        {
            // UPDI speed is 230400
            const int char_usb = getchar_timeout_us(1);
            if (char_usb != PICO_ERROR_TIMEOUT)
            {
                uart_timeout = delayed_by_ms(uart_timeout, 1000);
                gpio_xor_mask(1 << PICO_DEFAULT_LED_PIN);
                uart_putc_raw(uart0, char_usb);
            }
            if (uart_is_readable(uart0))
            {
                uart_timeout = delayed_by_ms(uart_timeout, 1000);
                putchar_raw(uart_getc(uart0));
            }
        }
    }
    return 0;
}
