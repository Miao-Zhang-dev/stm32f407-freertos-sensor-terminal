#include "app_port.h"

#include <string.h>

static app_port_ops_t bound_ops;
static bool is_bound;

bool app_port_bind(const app_port_ops_t *ops)
{
    if (ops == 0 || ops->read_sensors == 0 || ops->append_log == 0 ||
        ops->set_alarm_outputs == 0 || ops->receive_modbus == 0 ||
        ops->send_modbus == 0) {
        return false;
    }
    bound_ops = *ops;
    is_bound = true;
    return true;
}

bool app_port_read_sensors(terminal_sample_t *sample)
{
    if (!is_bound || sample == 0) {
        return false;
    }
    memset(sample, 0, sizeof(*sample));
    return bound_ops.read_sensors(bound_ops.context, sample);
}

bool app_port_append_log(const terminal_sample_t *sample, const terminal_alarm_t *alarm)
{
    return is_bound && sample != 0 && alarm != 0 &&
           bound_ops.append_log(bound_ops.context, sample, alarm);
}

void app_port_set_alarm_outputs(const terminal_alarm_t *alarm)
{
    if (is_bound && alarm != 0) {
        bound_ops.set_alarm_outputs(bound_ops.context, alarm);
    }
}

bool app_port_receive_modbus(uint8_t *frame, size_t capacity, size_t *length,
                             uint32_t timeout_ms)
{
    return is_bound && frame != 0 && length != 0 &&
           bound_ops.receive_modbus(bound_ops.context, frame, capacity, length, timeout_ms);
}

bool app_port_send_modbus(const uint8_t *frame, size_t length)
{
    return is_bound && frame != 0 && length != 0u &&
           bound_ops.send_modbus(bound_ops.context, frame, length);
}
