#include <Arduino.h>

#define PRODUCT_CODE 1234
#define CONFIG_I2C_PERIPH_ADDR 0x49

#define CONFIG_ADC 1
// #define CONFIG_PWM 1
// #define CONFIG_UART 1
// #define CONFIG_UART_BUF_MAX 120
// #define CONFIG_UART_SERCOM Serial

#include "Adafruit_seesawPeripheral.h"

// Avoid using TCA0 as the millis timer
#ifndef MILLIS_USE_TIMERD0
#error "This sketch is written for use with TCD0 as the millis timing source"
#endif

void setup()
{
    Adafruit_seesawPeripheral_begin();
}

void loop()
{
    Adafruit_seesawPeripheral_run();
}