#include "modbus_rtu.h"

#include <stdbool.h>

enum {
    FUNCTION_READ_HOLDING = 0x03,
    FUNCTION_READ_INPUT = 0x04,
    FUNCTION_WRITE_SINGLE = 0x06,
    FUNCTION_WRITE_MULTIPLE = 0x10,
    EXCEPTION_ILLEGAL_FUNCTION = 0x01,
    EXCEPTION_ILLEGAL_ADDRESS = 0x02,
    EXCEPTION_ILLEGAL_VALUE = 0x03,
    HOLDING_REGISTER_COUNT = 4,
    INPUT_REGISTER_COUNT = 7
};

static uint16_t read_u16(const uint8_t *p)
{
    return (uint16_t)(((uint16_t)p[0] << 8) | p[1]);
}

static void write_u16(uint8_t *p, uint16_t value)
{
    p[0] = (uint8_t)(value >> 8);
    p[1] = (uint8_t)value;
}

uint16_t modbus_rtu_crc16(const uint8_t *data, size_t length)
{
    uint16_t crc = 0xFFFFu;
    size_t i;
    uint8_t bit;

    for (i = 0; i < length; ++i) {
        crc ^= data[i];
        for (bit = 0; bit < 8u; ++bit) {
            crc = (crc & 1u) != 0u ? (uint16_t)((crc >> 1) ^ 0xA001u) : (uint16_t)(crc >> 1);
        }
    }
    return crc;
}

static uint16_t holding_get(const terminal_thresholds_t *thresholds, uint16_t address)
{
    switch (address) {
    case 0: return thresholds->air_quality_high;
    case 1: return (uint16_t)thresholds->temperature_low_centi_c;
    case 2: return (uint16_t)thresholds->temperature_high_centi_c;
    default: return thresholds->humidity_high_centi_percent;
    }
}

static void holding_set(terminal_thresholds_t *thresholds, uint16_t address, uint16_t value)
{
    switch (address) {
    case 0: thresholds->air_quality_high = value; break;
    case 1: thresholds->temperature_low_centi_c = (int16_t)value; break;
    case 2: thresholds->temperature_high_centi_c = (int16_t)value; break;
    case 3: thresholds->humidity_high_centi_percent = value; break;
    }
}

static uint16_t input_get(const modbus_rtu_state_t *state, uint16_t address)
{
    terminal_alarm_t alarm;
    uint16_t flags = 0u;

    switch (address) {
    case 0: return state->sample.air_quality_raw;
    case 1: return (uint16_t)state->sample.temperature_centi_c;
    case 2: return state->sample.humidity_centi_percent;
    case 3: return state->sample.valid_flags;
    case 4: return (uint16_t)(state->sample.timestamp_ms >> 16);
    case 5: return (uint16_t)state->sample.timestamp_ms;
    default:
        alarm = terminal_evaluate_alarms(&state->sample, &state->thresholds);
        flags |= alarm.air_quality_high ? (1u << 0) : 0u;
        flags |= alarm.temperature_low ? (1u << 1) : 0u;
        flags |= alarm.temperature_high ? (1u << 2) : 0u;
        flags |= alarm.humidity_high ? (1u << 3) : 0u;
        return flags;
    }
}

static size_t exception_response(uint8_t address, uint8_t function, uint8_t exception,
                                 uint8_t *response, size_t capacity)
{
    uint16_t crc;
    if (capacity < 5u) {
        return 0u;
    }
    response[0] = address;
    response[1] = (uint8_t)(function | 0x80u);
    response[2] = exception;
    crc = modbus_rtu_crc16(response, 3u);
    response[3] = (uint8_t)crc;
    response[4] = (uint8_t)(crc >> 8);
    return 5u;
}

static size_t finish_response(uint8_t *response, size_t length, size_t capacity)
{
    uint16_t crc;
    if (capacity < length + 2u) {
        return 0u;
    }
    crc = modbus_rtu_crc16(response, length);
    response[length] = (uint8_t)crc;
    response[length + 1u] = (uint8_t)(crc >> 8);
    return length + 2u;
}

size_t modbus_rtu_process(modbus_rtu_state_t *state,
                          const uint8_t *request,
                          size_t request_length,
                          uint8_t *response,
                          size_t response_capacity)
{
    uint16_t received_crc;
    uint16_t crc;
    uint16_t start;
    uint16_t count;
    uint8_t function;
    size_t i;

    if (state == 0 || request == 0 || response == 0 || request_length < 4u ||
        request[0] != MODBUS_RTU_ADDRESS || request_length > MODBUS_RTU_MAX_ADU) {
        return 0u;
    }

    received_crc = (uint16_t)(request[request_length - 2u] |
                              ((uint16_t)request[request_length - 1u] << 8));
    crc = modbus_rtu_crc16(request, request_length - 2u);
    if (crc != received_crc) {
        return 0u;
    }

    function = request[1];
    if (function == FUNCTION_READ_HOLDING || function == FUNCTION_READ_INPUT) {
        modbus_rtu_state_t staged;
        if (request_length != 8u) {
            return 0u;
        }
        start = read_u16(&request[2]);
        count = read_u16(&request[4]);
        if (count == 0u || count > 125u) {
            return exception_response(request[0], function, EXCEPTION_ILLEGAL_VALUE,
                                      response, response_capacity);
        }
        if ((function == FUNCTION_READ_HOLDING &&
             (start >= HOLDING_REGISTER_COUNT || count > HOLDING_REGISTER_COUNT - start)) ||
            (function == FUNCTION_READ_INPUT &&
             (start >= INPUT_REGISTER_COUNT || count > INPUT_REGISTER_COUNT - start))) {
            return exception_response(request[0], function, EXCEPTION_ILLEGAL_ADDRESS,
                                      response, response_capacity);
        }
        if (response_capacity < 5u + (size_t)count * 2u) {
            return 0u;
        }
        response[0] = request[0];
        response[1] = function;
        response[2] = (uint8_t)(count * 2u);
        staged = *state;
        for (i = 0; i < count; ++i) {
            uint16_t value = function == FUNCTION_READ_HOLDING
                           ? holding_get(&staged.thresholds, (uint16_t)(start + i))
                           : input_get(&staged, (uint16_t)(start + i));
            write_u16(&response[3u + i * 2u], value);
        }
        return finish_response(response, 3u + (size_t)count * 2u, response_capacity);
    }

    if (function == FUNCTION_WRITE_SINGLE) {
        terminal_thresholds_t staged;
        if (request_length != 8u) {
            return 0u;
        }
        start = read_u16(&request[2]);
        if (start >= HOLDING_REGISTER_COUNT) {
            return exception_response(request[0], function, EXCEPTION_ILLEGAL_ADDRESS,
                                      response, response_capacity);
        }
        staged = state->thresholds;
        holding_set(&staged, start, read_u16(&request[4]));
        if (!terminal_thresholds_valid(&staged)) {
            return exception_response(request[0], function, EXCEPTION_ILLEGAL_VALUE,
                                      response, response_capacity);
        }
        if (response_capacity < 8u) {
            return 0u;
        }
        state->thresholds = staged;
        for (i = 0; i < 6u; ++i) {
            response[i] = request[i];
        }
        return finish_response(response, 6u, response_capacity);
    }

    if (function == FUNCTION_WRITE_MULTIPLE) {
        terminal_thresholds_t staged;
        uint8_t byte_count;
        if (request_length < 9u) {
            return 0u;
        }
        start = read_u16(&request[2]);
        count = read_u16(&request[4]);
        byte_count = request[6];
        if (count == 0u || count > HOLDING_REGISTER_COUNT) {
            return exception_response(request[0], function, EXCEPTION_ILLEGAL_VALUE,
                                      response, response_capacity);
        }
        if (start >= HOLDING_REGISTER_COUNT || count > HOLDING_REGISTER_COUNT - start) {
            return exception_response(request[0], function, EXCEPTION_ILLEGAL_ADDRESS,
                                      response, response_capacity);
        }
        if (byte_count != count * 2u || request_length != 9u + byte_count) {
            return 0u;
        }
        staged = state->thresholds;
        for (i = 0; i < count; ++i) {
            holding_set(&staged, (uint16_t)(start + i), read_u16(&request[7u + i * 2u]));
        }
        if (!terminal_thresholds_valid(&staged)) {
            return exception_response(request[0], function, EXCEPTION_ILLEGAL_VALUE,
                                      response, response_capacity);
        }
        if (response_capacity < 8u) {
            return 0u;
        }
        state->thresholds = staged;
        for (i = 0; i < 6u; ++i) {
            response[i] = request[i];
        }
        return finish_response(response, 6u, response_capacity);
    }

    return exception_response(request[0], function, EXCEPTION_ILLEGAL_FUNCTION,
                              response, response_capacity);
}
