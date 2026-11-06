#include "modbus_rtu.h"
#include "terminal.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static size_t make_request(uint8_t *request, const uint8_t *body, size_t body_length)
{
    uint16_t crc;
    memcpy(request, body, body_length);
    crc = modbus_rtu_crc16(request, body_length);
    request[body_length] = (uint8_t)crc;
    request[body_length + 1u] = (uint8_t)(crc >> 8);
    return body_length + 2u;
}

static void test_crc(void)
{
    static const uint8_t text[] = "123456789";
    assert(modbus_rtu_crc16(text, 9u) == 0x4B37u);
}

static void test_read_input_registers(void)
{
    modbus_rtu_state_t state = { 0 };
    uint8_t request[8];
    uint8_t response[32];
    const uint8_t body[] = { 1u, 4u, 0u, 0u, 0u, 4u };
    size_t length;
    state.sample.air_quality_raw = 1234u;
    state.sample.temperature_centi_c = -125;
    state.sample.humidity_centi_percent = 4567u;
    state.sample.valid_flags = 7u;
    length = modbus_rtu_process(&state, request, make_request(request, body, sizeof(body)),
                                response, sizeof(response));
    assert(length == 13u);
    assert(response[0] == 1u && response[1] == 4u && response[2] == 8u);
    assert(response[3] == 0x04u && response[4] == 0xD2u);
    assert(response[5] == 0xFFu && response[6] == 0x83u);
    assert(response[7] == 0x11u && response[8] == 0xD7u);
}

static void test_threshold_write_is_validated_atomically(void)
{
    modbus_rtu_state_t state = { 0 };
    uint8_t request[16];
    uint8_t response[16];
    const uint8_t valid_body[] = { 1u, 6u, 0u, 0u, 0x08u, 0x34u };
    const uint8_t invalid_body[] = { 1u, 6u, 0u, 2u, 0xFFu, 0xFFu };
    size_t length;
    terminal_default_thresholds(&state.thresholds);
    length = modbus_rtu_process(&state, request, make_request(request, valid_body, sizeof(valid_body)),
                                response, sizeof(response));
    assert(length == 8u);
    assert(state.thresholds.air_quality_high == 0x0834u);
    length = modbus_rtu_process(&state, request, make_request(request, invalid_body, sizeof(invalid_body)),
                                response, sizeof(response));
    assert(length == 5u && response[1] == (uint8_t)(6u | 0x80u));
    assert(response[2] == 3u);
    assert(state.thresholds.temperature_high_centi_c == 4000);
}

static void test_bad_crc_is_ignored(void)
{
    modbus_rtu_state_t state = { 0 };
    uint8_t request[8] = { 1u, 4u, 0u, 0u, 0u, 1u, 0u, 0u };
    uint8_t response[16];
    assert(modbus_rtu_process(&state, request, sizeof(request), response, sizeof(response)) == 0u);
}

static void test_alarm_validity_and_thresholds(void)
{
    terminal_sample_t sample = { 0 };
    terminal_thresholds_t thresholds;
    terminal_alarm_t alarm;
    terminal_default_thresholds(&thresholds);
    sample.air_quality_raw = 2000u;
    sample.temperature_centi_c = 4000;
    sample.humidity_centi_percent = 8000u;
    alarm = terminal_evaluate_alarms(&sample, &thresholds);
    assert(!alarm.air_quality_high && !alarm.temperature_high && !alarm.humidity_high);
    sample.valid_flags = TERMINAL_VALID_AIR_QUALITY | TERMINAL_VALID_TEMPERATURE |
                         TERMINAL_VALID_HUMIDITY;
    alarm = terminal_evaluate_alarms(&sample, &thresholds);
    assert(alarm.air_quality_high && alarm.temperature_high && alarm.humidity_high);
}

int main(void)
{
    test_crc();
    test_read_input_registers();
    test_threshold_write_is_validated_atomically();
    test_bad_crc_is_ignored();
    test_alarm_validity_and_thresholds();
    puts("terminal core tests passed");
    return 0;
}
