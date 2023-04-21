#include "usb_cdc.h"

#include "pico/bootrom.h"
#include "tusb.h"

static uart_inst_t *my_uart;
static volatile cdc_line_control_state_t line_control;
static volatile cdc_line_coding_t line_coding;

void usb_link_uart(uart_inst_t *uart)
{
    my_uart = uart;
}

uint32_t usb_get_bitrate()
{
    return line_coding.bit_rate;
}

bool usb_get_dtr()
{
    return line_control.dtr;
}

bool usb_get_rts()
{
    return line_control.rts;
}

static uart_parity_t usb_translate_parity(uint8_t parity)
{
    switch (parity)
    {
    case 0:
        return UART_PARITY_NONE;
    case 1:
        return UART_PARITY_ODD;
    case 2:
        return UART_PARITY_EVEN;
        // case 3: mark;
        // case 4: space;
    }
    return UART_PARITY_NONE;
}

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
    line_coding = *p_line_coding;
    if (my_uart != 0)
    {
        uart_set_baudrate(my_uart, p_line_coding->bit_rate);
        uart_set_format(
            my_uart,
            p_line_coding->data_bits,
            p_line_coding->stop_bits,
            usb_translate_parity(p_line_coding->parity));
    }
}

void tud_cdc_line_state_cb(__unused uint8_t itf, bool dtr, bool rts)
{
    line_control.dtr = dtr;
    line_control.rts = rts;
}