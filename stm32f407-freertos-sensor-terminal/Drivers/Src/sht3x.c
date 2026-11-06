#include "sht3x.h"

static uint8_t crc8(const uint8_t *data, uint8_t length)
{
    uint8_t crc = 0xFFu;
    uint8_t i;
    uint8_t bit;

    for (i = 0; i < length; ++i) {
        crc ^= data[i];
        for (bit = 0; bit < 8u; ++bit) {
            crc = (crc & 0x80u) != 0u ? (uint8_t)((crc << 1) ^ 0x31u)
                                      : (uint8_t)(crc << 1);
        }
    }
    return crc;
}

bool sht3x_read_measurement(sht3x_t *sensor, int16_t *temperature_centi_c,
                            uint16_t *humidity_centi_percent)
{
    const uint8_t command[] = { 0x24u, 0x00u };
    uint8_t data[6];
    uint16_t raw_temperature;
    uint16_t raw_humidity;
    int32_t temperature;
    uint32_t humidity;

    if (sensor == 0 || temperature_centi_c == 0 || humidity_centi_percent == 0 ||
        sensor->write == 0 || sensor->read == 0 || sensor->delay == 0) {
        return false;
    }
    if (!sensor->write(sensor->context, sensor->address, command, sizeof(command), 100u)) {
        return false;
    }
    sensor->delay(sensor->context, 15u);
    if (!sensor->read(sensor->context, sensor->address, data, sizeof(data), 100u) ||
        crc8(&data[0], 2u) != data[2] || crc8(&data[3], 2u) != data[5]) {
        return false;
    }

    raw_temperature = (uint16_t)(((uint16_t)data[0] << 8) | data[1]);
    raw_humidity = (uint16_t)(((uint16_t)data[3] << 8) | data[4]);
    temperature = -4500 + ((int32_t)17500 * raw_temperature + 32767) / 65535;
    humidity = ((uint32_t)10000u * raw_humidity + 32767u) / 65535u;
    if (humidity > 10000u) {
        humidity = 10000u;
    }
    *temperature_centi_c = (int16_t)temperature;
    *humidity_centi_percent = (uint16_t)humidity;
    return true;
}
