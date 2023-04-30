#include <stdio.h>
#include "pico/binary_info.h"
#include "pico/multicore.h"
#include "pico/stdlib.h"
#include "seesaw.h"
#include "usb_cdc.h"

static semaphore_t sem_decoder;
static uint32_t tooth_time;
const uint decode_pin_base = 14;
static uint32_t core1_cycle_max = 0;
static uint32_t rev_count = 0;

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

uint16_t tooth_angle[] = {0, 700, 1800, 2500, 3600, 4300, 5400, 6100, 7200};

uint16_t make_lumi(uint16_t value)
{
    const uint32_t squared = (uint32_t)value * value;
    return (uint16_t)(squared >> 16);
}

struct filter_t
{
    uint32_t width;
    int32_t hyst_upper, hyst_lower;
    int32_t output;
};

void filter_init(filter_t *filter, uint32_t width)
{
    if (filter)
    {
        filter->width = width;
        filter->hyst_upper = 0;
        filter->hyst_lower = 0;
        filter->output = 0;
    }
}

int32_t filter_update_tolband(filter_t *filter, int32_t value)
{
    if (filter)
    {
        if (value > (filter->output + filter->width))
        {
            filter->output = value - filter->width;
        }
        else if (value < (filter->output - filter->width))
        {
            filter->output = value + filter->width;
        }
        return filter->output;
    }
    return 0;
}

int32_t filter_update_hysteresis(filter_t *filter, int32_t value)
{
    if (filter)
    {
        if (value > filter->hyst_upper)
        {
            filter->output = value;
            filter->hyst_upper = value;
            filter->hyst_lower = value - filter->width;
        }
        else if (value < filter->hyst_lower)
        {
            filter->output = value;
            filter->hyst_lower = value;
            filter->hyst_upper = value + filter->width;
        }
        return filter->output;
    }
    return 0;
}

void core1_entry()
{
    bool crank_last = false;
    uint32_t time_low, time_high;
    uint32_t last_micros;
    absolute_time_t debounce = get_absolute_time();
    absolute_time_t next_tooth = at_the_end_of_time;
    absolute_time_t prev_tooth;
    uint8_t current_tooth = 0;
    uint32_t last_cycle = time_us_32();
    bool tooth_completed;
    uint32_t current_angle;
    bool speed_up;

    gpio_init_mask(0b111 << 16);
    gpio_set_dir_out_masked(0b111 << 16);

    for (;;)
    {
        const absolute_time_t now = get_absolute_time();
        const uint32_t now_micros = to_us_since_boot(now);

        // cycle time statistics
        const uint32_t cycle_time = now_micros - last_cycle;
        if (cycle_time > core1_cycle_max)
        {
            core1_cycle_max = cycle_time;
        }
        last_cycle = now_micros;

        const bool cam_state = gpio_get(decode_pin_base);
        const bool crank_state = gpio_get(decode_pin_base + 1);
        if (crank_last != crank_state)
        {
            if (absolute_time_diff_us(debounce, now) > 0)
            {
                debounce = delayed_by_us(now, 500);
                crank_last = crank_state;
                prev_tooth = now;

                speed_up = !tooth_completed;
                tooth_completed = false;

                if (++current_tooth >= 8)
                {
                    current_tooth = 0;
                    ++rev_count;
                }

                if (crank_state)
                {
                    // rising edge
                    time_low = now_micros - last_micros;
                    next_tooth = delayed_by_us(now, time_high);
                }
                else
                {
                    // falling edge
                    time_high = now_micros - last_micros;
                    next_tooth = delayed_by_us(now, time_low);
                    if (cam_state)
                    {
                        current_tooth = 5;
                    }
                }
                tooth_time = time_low + time_high;
                last_micros = now_micros;
                sem_release(&sem_decoder);
            }
        }

        if (!tooth_completed)
        {
            if (absolute_time_diff_us(next_tooth, now) >= 0)
            {
                tooth_completed = true;
            }
            else
            {
                const int32_t time_since_last_tooth = absolute_time_diff_us(prev_tooth, now);
                const uint32_t angle_since_last_tooth = 1800 * time_since_last_tooth / tooth_time;
                const uint32_t new_angle = tooth_angle[current_tooth] + angle_since_last_tooth;
                if (speed_up)
                {
                    ++current_angle;
                    speed_up = (current_angle < new_angle);
                }
                else
                {
                    current_angle = new_angle;
                }

                if (current_angle == 650)
                    gpio_put(18, true);
                else if (current_angle == 650+1800)
                    gpio_put(18, false);
            }
        }
        gpio_put(16, tooth_completed);
        gpio_put(17, speed_up);
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
    filter_t filter_tol, filter_hyst;

    filter_init(&filter_tol, 5);
    filter_init(&filter_hyst, 10);

    absolute_time_t next_update = make_timeout_time_ms(rate_ms);
    absolute_time_t uart_timeout = get_absolute_time();
    absolute_time_t next_toggle = make_timeout_time_ms(rate_ms);

    while (!ss_ping())
    {
        busy_wait_ms(1);
        tight_loop_contents();
    }

    rev_count = (ss_eepromRead(0) << 24) + (ss_eepromRead(1) << 16) + (ss_eepromRead(2) << 8) + (ss_eepromRead(3) << 0);
    absolute_time_t eeprom_save = make_timeout_time_ms(10'000);

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
            const int32_t new_rpm = 60'000'000 / (tooth_time * 2);
            const int32_t rpm_tol = filter_update_tolband(&filter_tol, new_rpm);
            const int32_t rpm_hyst = filter_update_hysteresis(&filter_hyst, new_rpm);
            if ((usb_get_bitrate() <= 115200) && time_reached(uart_timeout))
            {
                printf("%d %d %d %d\n", rpm_tol, tooth_time/180, core1_cycle_max, rev_count);
            }
        }

        if (time_reached(next_toggle))
        {
            gpio_put_masked(0b11 << decode_pin_base, decode_sim[tooth].pins << decode_pin_base);
            const uint next_delay = decode_sim[tooth].delay * (235 - (level / 320));

            if (++tooth >= 12)
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

        if (time_reached(eeprom_save))
        {
            ss_eepromWrite(0, rev_count);
            eeprom_save = delayed_by_ms(eeprom_save, 10'000);
        }
    }
    return 0;
}
