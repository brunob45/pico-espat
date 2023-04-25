#ifndef SEESAW_H
#define SEESAW_H

#include "pico/stdlib.h"
#include "hardware/i2c.h"

void ss_init(uint32_t speed);
int ss_pinMode(int pin_mask, bool output);
int ss_digitalWrite(uint32_t pin_mask, uint8_t value);
int ss_analogWrite(uint8_t pin, uint16_t value);
uint16_t ss_analogRead(uint8_t pin);
int ss_uartBitRate(uint32_t bitrate);
int ss_uartWrite(uint8_t data);
int16_t ss_uartRead();

#endif // SEESAW_H