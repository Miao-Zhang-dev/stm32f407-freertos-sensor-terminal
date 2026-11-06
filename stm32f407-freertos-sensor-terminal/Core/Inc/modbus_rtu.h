#ifndef MODBUS_RTU_H
#define MODBUS_RTU_H

#include <stddef.h>
#include <stdint.h>

#include "terminal.h"

#define MODBUS_RTU_MAX_ADU 256u
#define MODBUS_RTU_ADDRESS 1u

typedef struct {
    terminal_sample_t sample;
    terminal_thresholds_t thresholds;
} modbus_rtu_state_t;

uint16_t modbus_rtu_crc16(const uint8_t *data, size_t length);

/* Returns response length, or zero for a frame to ignore. Request and response may alias. */
size_t modbus_rtu_process(modbus_rtu_state_t *state,
                          const uint8_t *request,
                          size_t request_length,
                          uint8_t *response,
                          size_t response_capacity);

#endif
