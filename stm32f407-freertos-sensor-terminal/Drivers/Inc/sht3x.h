#ifndef SHT3X_H
#define SHT3X_H

#include <stdbool.h>
#include <stdint.h>

typedef bool (*sht3x_write_fn)(void *context, uint8_t address,
                               const uint8_t *data, uint16_t length,
                               uint32_t timeout_ms);
typedef bool (*sht3x_read_fn)(void *context, uint8_t address,
                              uint8_t *data, uint16_t length,
                              uint32_t timeout_ms);
typedef void (*sht3x_delay_fn)(void *context, uint32_t delay_ms);

typedef struct {
    void *context;
    /* address is the unshifted 7-bit I2C address (0x44 or 0x45). */
    uint8_t address;
    sht3x_write_fn write;
    sht3x_read_fn read;
    sht3x_delay_fn delay;
} sht3x_t;

bool sht3x_read_measurement(sht3x_t *sensor, int16_t *temperature_centi_c,
                            uint16_t *humidity_centi_percent);

#endif
