#ifndef PICO_ESPAT_USBCDC
#define PICO_ESPAT_USBCDC

#include "pico/stdlib.h"

#ifdef __cplusplus
extern "C" {
#endif

uint32_t usb_get_baudrate();
bool usb_get_rts();
bool usb_get_dtr();

#ifdef __cplusplus
} // extern "C"
#endif

#endif // PICO_ESPAT_USBCDC