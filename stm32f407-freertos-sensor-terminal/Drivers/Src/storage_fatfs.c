#include "storage_fatfs.h"

#include <stdio.h>
#include <string.h>

#define STORAGE_LINE_MAX 128u

FRESULT storage_fatfs_mount(storage_fatfs_t *storage, const char *drive,
                            const char *filename)
{
    size_t drive_length;
    size_t filename_length;
    FRESULT result;

    if (storage == 0 || drive == 0 || filename == 0) {
        return FR_INVALID_PARAMETER;
    }
    drive_length = strlen(drive);
    filename_length = strlen(filename);
    if (drive_length == 0u || drive_length >= sizeof(storage->drive) ||
        filename_length == 0u || filename_length >= sizeof(storage->log_path)) {
        return FR_INVALID_NAME;
    }

    memset(storage, 0, sizeof(*storage));
    memcpy(storage->drive, drive, drive_length + 1u);
    memcpy(storage->log_path, filename, filename_length + 1u);
    result = f_mount(&storage->filesystem, storage->drive, 1u);
    storage->mounted = result == FR_OK;
    return result;
}

FRESULT storage_fatfs_append(storage_fatfs_t *storage,
                             const terminal_sample_t *sample,
                             const terminal_alarm_t *alarm)
{
    static const char header[] =
        "tick_ms,air_quality_raw,temperature_centi_c,humidity_centi_percent,valid_flags,alarm_flags\r\n";
    FIL file;
    FRESULT result;
    UINT written;
    char line[STORAGE_LINE_MAX];
    int length;
    uint16_t alarm_flags;

    if (storage == 0 || !storage->mounted || sample == 0 || alarm == 0) {
        return FR_INVALID_OBJECT;
    }

    result = f_open(&file, storage->log_path, FA_OPEN_ALWAYS | FA_WRITE);
    if (result != FR_OK) {
        return result;
    }
    if (f_size(&file) == 0u) {
        result = f_write(&file, header, sizeof(header) - 1u, &written);
        if (result != FR_OK || written != sizeof(header) - 1u) {
            (void)f_close(&file);
            return result == FR_OK ? FR_DISK_ERR : result;
        }
    }
    result = f_lseek(&file, f_size(&file));
    alarm_flags = (uint16_t)((alarm->air_quality_high ? 1u : 0u) |
                            (alarm->temperature_low ? 2u : 0u) |
                            (alarm->temperature_high ? 4u : 0u) |
                            (alarm->humidity_high ? 8u : 0u));
    length = snprintf(line, sizeof(line), "%lu,%u,%d,%u,%u,%u\r\n",
                      (unsigned long)sample->timestamp_ms,
                      (unsigned int)sample->air_quality_raw,
                      (int)sample->temperature_centi_c,
                      (unsigned int)sample->humidity_centi_percent,
                      (unsigned int)sample->valid_flags,
                      (unsigned int)alarm_flags);
    if (result == FR_OK && (length < 0 || (size_t)length >= sizeof(line))) {
        result = FR_INVALID_PARAMETER;
    }
    if (result == FR_OK) {
        result = f_write(&file, line, (UINT)length, &written);
        if (result == FR_OK && written != (UINT)length) {
            result = FR_DISK_ERR;
        }
    }
    if (result == FR_OK) {
        result = f_sync(&file);
    }
    {
        FRESULT close_result = f_close(&file);
        if (result == FR_OK) {
            result = close_result;
        }
    }
    return result;
}
