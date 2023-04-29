#include "seesaw.h"
#include "seesaw_intern.h"

#include "i2c_dma.h"

#define USE_I2C_DMA 1

#if USE_I2C_DMA
#include "i2c_dma.h"
static i2c_dma_t *i2c0_dma;
#else
#include "hardware/i2c.h"
#endif

void ss_init(uint32_t bitrate)
{
#if USE_I2C_DMA
    i2c_dma_init(&i2c0_dma, i2c_default, bitrate, PICO_DEFAULT_I2C_SDA_PIN, PICO_DEFAULT_I2C_SCL_PIN);
#else
    i2c_init(i2c_default, bitrate);
    gpio_set_function(PICO_DEFAULT_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(PICO_DEFAULT_I2C_SDA_PIN);
    gpio_pull_up(PICO_DEFAULT_I2C_SCL_PIN);
#endif

}

int ss_pinMode(int pin_mask, bool output)
{
    uint8_t cmd[] = {SEESAW_GPIO_BASE,
                     output ? SEESAW_GPIO_DIRSET_BULK : SEESAW_GPIO_DIRCLR_BULK,
                     (uint8_t)(pin_mask >> 24), (uint8_t)(pin_mask >> 16),
                     (uint8_t)(pin_mask >> 8), (uint8_t)pin_mask};

#if USE_I2C_DMA
    return i2c_dma_write(i2c0_dma, SEESAW_ADDRESS, cmd, sizeof(cmd));
#else
    return i2c_write_blocking(i2c_default, SEESAW_ADDRESS, cmd, sizeof(cmd), false);
#endif
}

int ss_digitalWrite(uint32_t pin_mask, uint8_t value)
{
    uint8_t cmd[] = {
        SEESAW_GPIO_BASE,
        value ? SEESAW_GPIO_BULK_SET : SEESAW_GPIO_BULK_CLR,
        (uint8_t)(pin_mask >> 24),
        (uint8_t)(pin_mask >> 16),
        (uint8_t)(pin_mask >> 8),
        (uint8_t)(pin_mask >> 0),
    };

#if USE_I2C_DMA
    return i2c_dma_write(i2c0_dma, SEESAW_ADDRESS, cmd, sizeof(cmd));
#else
    return i2c_write_blocking(i2c_default, SEESAW_ADDRESS, cmd, sizeof(cmd), false);
#endif
}

int ss_analogWrite(uint8_t pin, uint16_t value)
{
    uint8_t cmd[] = {SEESAW_TIMER_BASE,
                     SEESAW_TIMER_PWM,
                     (uint8_t)pin,
                     (uint8_t)(value >> 8), (uint8_t)value};

#if USE_I2C_DMA
    return i2c_dma_write(i2c0_dma, SEESAW_ADDRESS, cmd, sizeof(cmd));
#else
    return i2c_write_blocking(i2c_default, SEESAW_ADDRESS, cmd, sizeof(cmd), false);
#endif
}

uint16_t ss_analogRead(uint8_t pin)
{
    uint8_t cmd[] = {SEESAW_ADC_BASE,
                     (uint8_t)(SEESAW_ADC_CHANNEL_OFFSET + pin)};
    uint8_t res[2];

#if USE_I2C_DMA
    i2c_dma_write_read(i2c0_dma, SEESAW_ADDRESS, cmd, sizeof(cmd), res, sizeof(res));
#else
    i2c_write_blocking(i2c_default, SEESAW_ADDRESS, cmd, sizeof(cmd), false);
    sleep_ms(1);
    i2c_read_blocking(i2c_default, SEESAW_ADDRESS, res, sizeof(res), false);
#endif
    return (res[0] << 8) + res[1];
}

int ss_uartBitRate(uint32_t bitrate)
{
    uint8_t cmd[] = {
        SEESAW_SERCOM0_BASE,
        SEESAW_SERCOM_BAUD,
        (uint8_t)(bitrate >> 24),
        (uint8_t)(bitrate >> 16),
        (uint8_t)(bitrate >> 8),
        (uint8_t)(bitrate >> 0),
    };
    return i2c_dma_write(i2c0_dma, SEESAW_ADDRESS, cmd, sizeof(cmd));
}

int ss_uartWrite(uint8_t data)
{
    uint8_t cmd[] = {
        SEESAW_SERCOM0_BASE,
        SEESAW_SERCOM_DATA,
        data,
    };
    return i2c_dma_write(i2c0_dma, SEESAW_ADDRESS, cmd, sizeof(cmd));
}

int16_t ss_uartRead()
{
    uint8_t cmd[] = {
        SEESAW_SERCOM0_BASE,
        SEESAW_SERCOM_DATA
    };
    uint8_t res[2] = {0xff, 0xff};
    i2c_dma_write_read(i2c0_dma, SEESAW_ADDRESS, cmd, sizeof(cmd), res, sizeof(res));
    return (res[0] << 8) + res[1];
}

uint8_t ss_eepromRead(uint8_t address)
{
    uint8_t cmd[] = {
        SEESAW_EEPROM_BASE,
        address
    };
    uint8_t res[1] = {0xff};
    i2c_dma_write_read(i2c0_dma, SEESAW_ADDRESS, cmd, sizeof(cmd), res, sizeof(res));
    return res[0];
}

int ss_eepromWrite(uint8_t address, uint32_t data)
{
    uint8_t cmd[] = {
        SEESAW_EEPROM_BASE,
        address,
        (uint8_t)(data >> 24),
        (uint8_t)(data >> 16),
        (uint8_t)(data >> 8),
        (uint8_t)(data >> 0),
    };
    return i2c_dma_write(i2c0_dma, SEESAW_ADDRESS, cmd, sizeof(cmd));
}

bool ss_ping()
{
    uint8_t cmd[] = {
        SEESAW_STATUS_BASE
    };
    return i2c_dma_write(i2c0_dma, SEESAW_ADDRESS, cmd, sizeof(cmd)) == PICO_OK;
}