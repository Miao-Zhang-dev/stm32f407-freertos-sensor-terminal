#ifndef BOARD_PORT_H
#define BOARD_PORT_H

#include "app_port.h"
#include "sht3x.h"
#include "storage_fatfs.h"

typedef struct {
    void *context;
    bool (*read_air_quality_adc)(void *context, uint16_t *raw_value);
    uint32_t (*get_tick_ms)(void *context);
    void (*set_alarm_outputs)(void *context, const terminal_alarm_t *alarm);
    bool (*receive_modbus_frame)(void *context, uint8_t *frame, size_t capacity,
                                 size_t *length, uint32_t timeout_ms);
    bool (*send_modbus_frame)(void *context, const uint8_t *frame, size_t length);
    sht3x_t *temperature_humidity_sensor;
    storage_fatfs_t *storage;
} app_board_port_t;

bool app_board_port_bind(app_board_port_t *board);

#endif
