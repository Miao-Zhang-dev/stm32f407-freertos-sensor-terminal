#ifndef TERMINAL_H
#define TERMINAL_H

#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint32_t timestamp_ms;
    uint16_t air_quality_raw;
    int16_t temperature_centi_c;
    uint16_t humidity_centi_percent;
    uint16_t valid_flags;
} terminal_sample_t;

enum {
    TERMINAL_VALID_AIR_QUALITY = 1u << 0,
    TERMINAL_VALID_TEMPERATURE = 1u << 1,
    TERMINAL_VALID_HUMIDITY = 1u << 2
};

typedef struct {
    uint16_t air_quality_high;
    int16_t temperature_low_centi_c;
    int16_t temperature_high_centi_c;
    uint16_t humidity_high_centi_percent;
} terminal_thresholds_t;

typedef struct {
    bool air_quality_high;
    bool temperature_low;
    bool temperature_high;
    bool humidity_high;
} terminal_alarm_t;

void terminal_default_thresholds(terminal_thresholds_t *thresholds);
bool terminal_thresholds_valid(const terminal_thresholds_t *thresholds);
terminal_alarm_t terminal_evaluate_alarms(const terminal_sample_t *sample,
                                           const terminal_thresholds_t *thresholds);

#endif
