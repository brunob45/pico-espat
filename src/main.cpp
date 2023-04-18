#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"

#include "hardware/i2c.h"
#include "seesaw.h"

#include "usb_cdc.h"

void ss_read(uint8_t regHigh, uint8_t regLow, uint8_t *buf, uint8_t num)
{
    uint8_t pos = 0;
    uint8_t prefix[2];
    prefix[0] = (uint8_t)regHigh;
    prefix[1] = (uint8_t)regLow;
    while (pos < num)
    {
        uint8_t read_now = [num, pos]
        {
            const uint8_t a = num - pos;
            return 32 < a ? 32 : a;
        }();
        i2c_write_blocking(i2c_default, SEESAW_ADDRESS, prefix, 2, false);
        i2c_read_blocking(i2c_default, SEESAW_ADDRESS, buf + pos, read_now, false);
        pos += read_now;
    }
}

void ss_write(uint8_t regHigh, uint8_t regLow, uint8_t *buf = NULL, uint8_t num = 0)
{
    uint8_t prefix[] = {regHigh, regLow};
    i2c_write_blocking(i2c_default, SEESAW_ADDRESS, prefix, 2, true);
    i2c_write_blocking(i2c_default, SEESAW_ADDRESS, buf, num, false);
}

void ss_getID()
{
    uint8_t c = 0;
    ss_read(SEESAW_STATUS_BASE, SEESAW_STATUS_HW_ID, &c, 1);
}

void ss_pinMode(int pins)
{
    uint8_t cmd[] = {(uint8_t)(pins >> 24), (uint8_t)(pins >> 16),
                     (uint8_t)(pins >> 8), (uint8_t)pins};
    ss_write(SEESAW_GPIO_BASE, SEESAW_GPIO_DIRSET_BULK, cmd, 4);
}

void ss_digitalWrite(int pin, int level)
{
}

bool reserved_addr(uint8_t addr)
{
    return (addr & 0x78) == 0 || (addr & 0x78) == 0x78;
}

int main()
{
    stdio_init_all();

    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    gpio_put(PICO_DEFAULT_LED_PIN, true);

    uart_init(uart0, 115200);
    gpio_set_function(PICO_DEFAULT_UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(PICO_DEFAULT_UART_RX_PIN, GPIO_FUNC_UART);
    // Make the UART pins available to picotool
    bi_decl(bi_2pins_with_func(PICO_DEFAULT_UART_TX_PIN, PICO_DEFAULT_UART_RX_PIN, GPIO_FUNC_UART));
    usb_link_uart(uart0);

    i2c_init(i2c_default, 100 * 1000);
    gpio_set_function(PICO_DEFAULT_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(PICO_DEFAULT_I2C_SDA_PIN);
    gpio_pull_up(PICO_DEFAULT_I2C_SCL_PIN);
    // Make the I2C pins available to picotool
    bi_decl(bi_2pins_with_func(PICO_DEFAULT_I2C_SDA_PIN, PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C));

    sleep_ms(5000);

    printf("\nI2C Bus Scan\n");
    printf("   0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\n");

    for (int addr = 0; addr < (1 << 7); ++addr)
    {
        if (addr % 16 == 0)
        {
            printf("%02x ", addr);
        }

        // Perform a 1-byte dummy read from the probe address. If a slave
        // acknowledges this address, the function returns the number of bytes
        // transferred. If the address byte is ignored, the function returns
        // -1.

        // Skip over any reserved addresses.
        int ret;
        uint8_t rxdata;
        if (reserved_addr(addr))
            ret = PICO_ERROR_GENERIC;
        else
            ret = i2c_read_blocking(i2c_default, addr, &rxdata, 1, false);

        printf(ret < 0 ? "." : "@");
        printf(addr % 16 == 15 ? "\n" : "  ");
    }
    printf("Done.\n");

    for (;;)
    {
        tight_loop_contents();
    }

    for (;;)
    {
        const int char_usb = getchar_timeout_us(1);
        if (char_usb != PICO_ERROR_TIMEOUT)
        {
            gpio_put(PICO_DEFAULT_LED_PIN, !gpio_get(PICO_DEFAULT_LED_PIN));
            uart_putc_raw(uart0, char_usb);
        }
        if (uart_is_readable(uart0))
        {
            putchar_raw(uart_getc(uart0));
        }
    }
    return 0;
}
