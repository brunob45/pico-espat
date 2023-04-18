#include <Arduino.h>

#define PRODUCT_CODE 1234
#define CONFIG_I2C_PERIPH_ADDR 0x49

#define CONFIG_ADC 1
#define CONFIG_PWM 1

#include "Adafruit_seesawPeripheral.h"

void setup()
{
    Adafruit_seesawPeripheral_begin();
}

void loop()
{
    Adafruit_seesawPeripheral_run();
}