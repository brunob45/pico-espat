#ifndef PICO_ESPAT_USBCDC
#define PICO_ESPAT_USBCDC

#include "pico/stdlib.h"

#ifdef __cplusplus
extern "C" {
#endif

void usb_link_uart(uart_inst_t *uart);
uint32_t usb_get_bitrate();
bool usb_get_rts();
bool usb_get_dtr();

#ifdef __cplusplus
} // extern "C"
#endif

#endif // PICO_ESPAT_USBCDC