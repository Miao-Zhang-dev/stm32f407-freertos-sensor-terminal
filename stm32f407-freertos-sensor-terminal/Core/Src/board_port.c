#include "board_port.h"

#include <string.h>

static bool board_read_sensors(void *context, terminal_sample_t *sample)
{
    app_board_port_t *board = (app_board_port_t *)context;
    bool any_valid = false;
    uint16_t air_quality;
    int16_t temperature;
    uint16_t humidity;

    memset(sample, 0, sizeof(*sample));
    sample->timestamp_ms = board->get_tick_ms(board->context);
    if (board->read_air_quality_adc(board->context, &air_quality)) {
        sample->air_quality_raw = air_quality;
        sample->valid_flags |= TERMINAL_VALID_AIR_QUALITY;
        any_valid = true;
    }
    if (sht3x_read_measurement(board->temperature_humidity_sensor, &temperature, &humidity)) {
        sample->temperature_centi_c = temperature;
        sample->humidity_centi_percent = humidity;
        sample->valid_flags |= TERMINAL_VALID_TEMPERATURE | TERMINAL_VALID_HUMIDITY;
        any_valid = true;
    }
    return any_valid;
}

static bool board_append_log(void *context, const terminal_sample_t *sample,
                             const terminal_alarm_t *alarm)
{
    app_board_port_t *board = (app_board_port_t *)context;
    return storage_fatfs_append(board->storage, sample, alarm) == FR_OK;
}

static void board_set_alarm_outputs(void *context, const terminal_alarm_t *alarm)
{
    app_board_port_t *board = (app_board_port_t *)context;
    board->set_alarm_outputs(board->context, alarm);
}

static bool board_receive_modbus(void *context, uint8_t *frame, size_t capacity,
                                 size_t *length, uint32_t timeout_ms)
{
    app_board_port_t *board = (app_board_port_t *)context;
    return board->receive_modbus_frame(board->context, frame, capacity, length, timeout_ms);
}

static bool board_send_modbus(void *context, const uint8_t *frame, size_t length)
{
    app_board_port_t *board = (app_board_port_t *)context;
    return board->send_modbus_frame(board->context, frame, length);
}

bool app_board_port_bind(app_board_port_t *board)
{
    app_port_ops_t ops;
    if (board == 0 || board->read_air_quality_adc == 0 || board->get_tick_ms == 0 ||
        board->set_alarm_outputs == 0 || board->receive_modbus_frame == 0 ||
        board->send_modbus_frame == 0 || board->temperature_humidity_sensor == 0 ||
        board->storage == 0 || !board->storage->mounted) {
        return false;
    }
    memset(&ops, 0, sizeof(ops));
    ops.context = board;
    ops.read_sensors = board_read_sensors;
    ops.append_log = board_append_log;
    ops.set_alarm_outputs = board_set_alarm_outputs;
    ops.receive_modbus = board_receive_modbus;
    ops.send_modbus = board_send_modbus;
    return app_port_bind(&ops);
}
