#ifndef APP_PORT_H
#define APP_PORT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "modbus_rtu.h"
#include "terminal.h"

typedef struct {
    void *context;
    bool (*read_sensors)(void *context, terminal_sample_t *sample);
    bool (*append_log)(void *context, const terminal_sample_t *sample,
                       const terminal_alarm_t *alarm);
    void (*set_alarm_outputs)(void *context, const terminal_alarm_t *alarm);
    bool (*receive_modbus)(void *context, uint8_t *frame, size_t capacity,
                           size_t *length, uint32_t timeout_ms);
    bool (*send_modbus)(void *context, const uint8_t *frame, size_t length);
} app_port_ops_t;

/* Bind the board-specific callbacks before starting the scheduler. */
bool app_port_bind(const app_port_ops_t *ops);
bool app_port_read_sensors(terminal_sample_t *sample);
bool app_port_append_log(const terminal_sample_t *sample, const terminal_alarm_t *alarm);
void app_port_set_alarm_outputs(const terminal_alarm_t *alarm);
bool app_port_receive_modbus(uint8_t *frame, size_t capacity, size_t *length,
                             uint32_t timeout_ms);
bool app_port_send_modbus(const uint8_t *frame, size_t length);

#endif
