#ifndef STORAGE_FATFS_H
#define STORAGE_FATFS_H

#include "ff.h"
#include "terminal.h"

typedef struct {
    FATFS filesystem;
    char drive[4];
    char log_path[32];
    bool mounted;
} storage_fatfs_t;

FRESULT storage_fatfs_mount(storage_fatfs_t *storage, const char *drive,
                            const char *filename);
FRESULT storage_fatfs_append(storage_fatfs_t *storage,
                             const terminal_sample_t *sample,
                             const terminal_alarm_t *alarm);

#endif
