#include "usb_cdc.h"

#include "pico/bootrom.h"
#include "tusb.h"

static volatile uint32_t usb_baudrate = 115200;
static volatile bool usb_dtr, usb_rts;

uint32_t usb_get_baudrate()
{
    return usb_baudrate;
}

bool usb_get_dtr()
{
    return usb_dtr;
}

bool usb_get_rts()
{
    return usb_rts;
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
    usb_baudrate = p_line_coding->bit_rate;
}

void tud_cdc_line_state_cb(__unused uint8_t itf, bool dtr, bool rts)
{
    usb_dtr = dtr;
    usb_rts = rts;
}