#ifndef W25Q_FATFS_DISKIO_H
#define W25Q_FATFS_DISKIO_H

#include <stdbool.h>

#include "diskio.h"
#include "w25qxx.h"

/* Bind one flash before registering these callbacks with FatFs. */
bool w25q_fatfs_bind(w25qxx_t *flash);
DSTATUS w25q_fatfs_initialize(BYTE drive);
DSTATUS w25q_fatfs_status(BYTE drive);
DRESULT w25q_fatfs_read(BYTE drive, BYTE *buffer, DWORD sector, UINT count);
DRESULT w25q_fatfs_write(BYTE drive, const BYTE *buffer, DWORD sector, UINT count);
DRESULT w25q_fatfs_ioctl(BYTE drive, BYTE command, void *buffer);

#endif
