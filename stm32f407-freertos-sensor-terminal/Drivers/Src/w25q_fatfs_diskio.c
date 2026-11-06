#include "w25q_fatfs_diskio.h"

#include <string.h>

#define FATFS_SECTOR_SIZE 512u
#define FATFS_BLOCK_SECTORS (W25QXX_SECTOR_SIZE / FATFS_SECTOR_SIZE)

static w25qxx_t *bound_flash;
static BYTE physical_sector[W25QXX_SECTOR_SIZE];
static bool disk_ready;

bool w25q_fatfs_bind(w25qxx_t *flash)
{
    if (flash == 0 || flash->capacity_bytes == 0u ||
        flash->capacity_bytes % W25QXX_SECTOR_SIZE != 0u) {
        return false;
    }
    bound_flash = flash;
    disk_ready = false;
    return true;
}

DSTATUS w25q_fatfs_initialize(BYTE drive)
{
    uint8_t id[3];
    if (drive != 0u || bound_flash == 0 || !w25qxx_read_jedec_id(bound_flash, id) ||
        (id[0] == 0u && id[1] == 0u && id[2] == 0u) ||
        (id[0] == 0xFFu && id[1] == 0xFFu && id[2] == 0xFFu)) {
        disk_ready = false;
    } else {
        disk_ready = true;
    }
    return disk_ready ? 0u : STA_NOINIT;
}

DSTATUS w25q_fatfs_status(BYTE drive)
{
    return drive == 0u && disk_ready ? 0u : STA_NOINIT;
}

DRESULT w25q_fatfs_read(BYTE drive, BYTE *buffer, DWORD sector, UINT count)
{
    uint32_t address;
    size_t length;

    if (drive != 0u || !disk_ready || buffer == 0 || count == 0u ||
        (uint64_t)sector + count > bound_flash->capacity_bytes / FATFS_SECTOR_SIZE) {
        return RES_PARERR;
    }
    address = sector * FATFS_SECTOR_SIZE;
    length = (size_t)count * FATFS_SECTOR_SIZE;
    return w25qxx_read(bound_flash, address, buffer, length) ? RES_OK : RES_ERROR;
}

DRESULT w25q_fatfs_write(BYTE drive, const BYTE *buffer, DWORD sector, UINT count)
{
    uint32_t current_sector;
    UINT index;

    if (drive != 0u || !disk_ready || buffer == 0 || count == 0u ||
        (uint64_t)sector + count > bound_flash->capacity_bytes / FATFS_SECTOR_SIZE) {
        return RES_PARERR;
    }
    current_sector = sector;
    for (index = 0; index < count; ++index, ++current_sector) {
        uint32_t address = current_sector * FATFS_SECTOR_SIZE;
        uint32_t erase_address = address - (address % W25QXX_SECTOR_SIZE);
        uint32_t offset = address - erase_address;
        if (!w25qxx_read(bound_flash, erase_address, physical_sector, sizeof(physical_sector))) {
            return RES_ERROR;
        }
        memcpy(&physical_sector[offset], &buffer[(size_t)index * FATFS_SECTOR_SIZE],
               FATFS_SECTOR_SIZE);
        if (!w25qxx_erase_sector(bound_flash, erase_address) ||
            !w25qxx_program(bound_flash, erase_address, physical_sector, sizeof(physical_sector))) {
            return RES_ERROR;
        }
    }
    return RES_OK;
}

DRESULT w25q_fatfs_ioctl(BYTE drive, BYTE command, void *buffer)
{
    if (drive != 0u || !disk_ready) {
        return RES_NOTRDY;
    }
    switch (command) {
    case CTRL_SYNC:
        return RES_OK;
    case GET_SECTOR_COUNT:
        if (buffer == 0) return RES_PARERR;
        *(DWORD *)buffer = bound_flash->capacity_bytes / FATFS_SECTOR_SIZE;
        return RES_OK;
    case GET_SECTOR_SIZE:
        if (buffer == 0) return RES_PARERR;
        *(WORD *)buffer = FATFS_SECTOR_SIZE;
        return RES_OK;
    case GET_BLOCK_SIZE:
        if (buffer == 0) return RES_PARERR;
        *(DWORD *)buffer = FATFS_BLOCK_SECTORS;
        return RES_OK;
    default:
        return RES_PARERR;
    }
}
