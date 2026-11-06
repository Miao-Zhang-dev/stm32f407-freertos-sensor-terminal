#include "terminal.h"

void terminal_default_thresholds(terminal_thresholds_t *thresholds)
{
    if (thresholds == 0) {
        return;
    }

    thresholds->air_quality_high = 2000u;
    thresholds->temperature_low_centi_c = 0;
    thresholds->temperature_high_centi_c = 4000;
    thresholds->humidity_high_centi_percent = 8000u;
}

bool terminal_thresholds_valid(const terminal_thresholds_t *thresholds)
{
    return thresholds != 0 &&
           thresholds->air_quality_high <= 4095u &&
           thresholds->temperature_low_centi_c < thresholds->temperature_high_centi_c &&
           thresholds->temperature_low_centi_c >= -4000 &&
           thresholds->temperature_high_centi_c <= 12500 &&
           thresholds->humidity_high_centi_percent <= 10000u;
}

terminal_alarm_t terminal_evaluate_alarms(const terminal_sample_t *sample,
                                           const terminal_thresholds_t *thresholds)
{
    terminal_alarm_t alarm = { false, false, false, false };

    if (sample == 0 || thresholds == 0) {
        return alarm;
    }

    alarm.air_quality_high = (sample->valid_flags & TERMINAL_VALID_AIR_QUALITY) != 0u &&
                             sample->air_quality_raw >= thresholds->air_quality_high;
    alarm.temperature_low = (sample->valid_flags & TERMINAL_VALID_TEMPERATURE) != 0u &&
                            sample->temperature_centi_c <= thresholds->temperature_low_centi_c;
    alarm.temperature_high = (sample->valid_flags & TERMINAL_VALID_TEMPERATURE) != 0u &&
                             sample->temperature_centi_c >= thresholds->temperature_high_centi_c;
    alarm.humidity_high = (sample->valid_flags & TERMINAL_VALID_HUMIDITY) != 0u &&
                          sample->humidity_centi_percent >= thresholds->humidity_high_centi_percent;
    return alarm;
}
