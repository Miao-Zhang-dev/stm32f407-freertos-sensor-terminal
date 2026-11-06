#include "w25qxx.h"

#define W25QXX_CMD_READ_ID       0x9Fu
#define W25QXX_CMD_READ          0x03u
#define W25QXX_CMD_PAGE_PROGRAM  0x02u
#define W25QXX_CMD_WRITE_ENABLE  0x06u
#define W25QXX_CMD_READ_STATUS   0x05u
#define W25QXX_CMD_SECTOR_ERASE  0x20u
#define W25QXX_STATUS_BUSY       0x01u
#define W25QXX_STATUS_WEL        0x02u
#define W25QXX_IO_TIMEOUT_MS     100u
#define W25QXX_PROGRAM_TIMEOUT_MS 20u
#define W25QXX_ERASE_TIMEOUT_MS  3000u

static bool command_begin(w25qxx_t *flash, uint8_t command)
{
    return flash->select(flash->context, true) &&
           flash->transfer(flash->context, &command, 0, 1u, W25QXX_IO_TIMEOUT_MS);
}

static bool command_end(w25qxx_t *flash, bool success)
{
    return flash->select(flash->context, false) && success;
}

static bool send_address(w25qxx_t *flash, uint32_t address)
{
    uint8_t bytes[3];
    bytes[0] = (uint8_t)(address >> 16);
    bytes[1] = (uint8_t)(address >> 8);
    bytes[2] = (uint8_t)address;
    return flash->transfer(flash->context, bytes, 0, sizeof(bytes), W25QXX_IO_TIMEOUT_MS);
}

static bool read_status(w25qxx_t *flash, uint8_t *status)
{
    const uint8_t dummy = 0xFFu;
    bool ok = command_begin(flash, W25QXX_CMD_READ_STATUS);
    if (ok) {
        ok = flash->transfer(flash->context, &dummy, status, 1u, W25QXX_IO_TIMEOUT_MS);
    }
    return command_end(flash, ok);
}

static bool write_enable(w25qxx_t *flash)
{
    uint8_t status;
    bool ok = command_begin(flash, W25QXX_CMD_WRITE_ENABLE);
    ok = command_end(flash, ok);
    return ok && read_status(flash, &status) && (status & W25QXX_STATUS_WEL) != 0u;
}

static bool wait_ready(w25qxx_t *flash, uint32_t timeout_ms)
{
    uint32_t start = flash->tick_ms(flash->context);
    uint8_t status;

    do {
        if (!read_status(flash, &status)) {
            return false;
        }
        if ((status & W25QXX_STATUS_BUSY) == 0u) {
            return true;
        }
        flash->delay(flash->context, 1u);
    } while ((uint32_t)(flash->tick_ms(flash->context) - start) < timeout_ms);
    return false;
}

static bool valid_device(const w25qxx_t *flash)
{
    return flash != 0 && flash->capacity_bytes != 0u && flash->select != 0 &&
           flash->transfer != 0 && flash->tick_ms != 0 && flash->delay != 0;
}

bool w25qxx_read_jedec_id(w25qxx_t *flash, uint8_t id[3])
{
    uint8_t dummy[3] = { 0xFFu, 0xFFu, 0xFFu };
    bool ok;
    if (!valid_device(flash) || id == 0) {
        return false;
    }
    ok = command_begin(flash, W25QXX_CMD_READ_ID);
    if (ok) {
        ok = flash->transfer(flash->context, dummy, id, sizeof(dummy), W25QXX_IO_TIMEOUT_MS);
    }
    return command_end(flash, ok);
}

bool w25qxx_read(w25qxx_t *flash, uint32_t address, uint8_t *data, size_t length)
{
    uint8_t command = W25QXX_CMD_READ;
    bool ok;
    if (!valid_device(flash) || (length != 0u && data == 0) ||
        address > flash->capacity_bytes || length > flash->capacity_bytes - address) {
        return false;
    }
    if (length == 0u) {
        return true;
    }
    ok = wait_ready(flash, W25QXX_IO_TIMEOUT_MS) && flash->select(flash->context, true);
    if (ok) {
        ok = flash->transfer(flash->context, &command, 0, 1u, W25QXX_IO_TIMEOUT_MS) &&
             send_address(flash, address) &&
             flash->transfer(flash->context, 0, data, length, W25QXX_IO_TIMEOUT_MS);
    }
    return command_end(flash, ok);
}

bool w25qxx_program(w25qxx_t *flash, uint32_t address,
                    const uint8_t *data, size_t length)
{
    size_t chunk;
    size_t offset = 0u;
    bool ok;
    uint8_t command = W25QXX_CMD_PAGE_PROGRAM;

    if (!valid_device(flash) || (length != 0u && data == 0) ||
        address > flash->capacity_bytes || length > flash->capacity_bytes - address) {
        return false;
    }
    while (offset < length) {
        chunk = W25QXX_PAGE_SIZE - ((address + (uint32_t)offset) % W25QXX_PAGE_SIZE);
        if (chunk > length - offset) {
            chunk = length - offset;
        }
        if (!wait_ready(flash, W25QXX_PROGRAM_TIMEOUT_MS) || !write_enable(flash)) {
            return false;
        }
        ok = flash->select(flash->context, true);
        if (ok) {
            ok = flash->transfer(flash->context, &command, 0, 1u, W25QXX_IO_TIMEOUT_MS) &&
                 send_address(flash, address + (uint32_t)offset) &&
                 flash->transfer(flash->context, &data[offset], 0, chunk, W25QXX_IO_TIMEOUT_MS);
        }
        if (!command_end(flash, ok) || !wait_ready(flash, W25QXX_PROGRAM_TIMEOUT_MS)) {
            return false;
        }
        offset += chunk;
    }
    return true;
}

bool w25qxx_erase_sector(w25qxx_t *flash, uint32_t address)
{
    uint8_t command = W25QXX_CMD_SECTOR_ERASE;
    uint32_t sector_address;
    bool ok;

    if (!valid_device(flash) || address >= flash->capacity_bytes) {
        return false;
    }
    sector_address = address - (address % W25QXX_SECTOR_SIZE);
    if (!wait_ready(flash, W25QXX_IO_TIMEOUT_MS) || !write_enable(flash)) {
        return false;
    }
    ok = flash->select(flash->context, true);
    if (ok) {
        ok = flash->transfer(flash->context, &command, 0, 1u, W25QXX_IO_TIMEOUT_MS) &&
             send_address(flash, sector_address);
    }
    return command_end(flash, ok) && wait_ready(flash, W25QXX_ERASE_TIMEOUT_MS);
}
