#ifndef W25QXX_H
#define W25QXX_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define W25QXX_PAGE_SIZE 256u
#define W25QXX_SECTOR_SIZE 4096u

typedef bool (*w25qxx_select_fn)(void *context, bool selected);
/* A NULL transmit sends 0xFF dummy bytes; a NULL receive discards incoming bytes. */
typedef bool (*w25qxx_transfer_fn)(void *context, const uint8_t *transmit,
                                   uint8_t *receive, size_t length,
                                   uint32_t timeout_ms);
typedef uint32_t (*w25qxx_tick_fn)(void *context);
typedef void (*w25qxx_delay_fn)(void *context, uint32_t delay_ms);

typedef struct {
    void *context;
    uint32_t capacity_bytes;
    w25qxx_select_fn select;
    w25qxx_transfer_fn transfer;
    w25qxx_tick_fn tick_ms;
    w25qxx_delay_fn delay;
} w25qxx_t;

bool w25qxx_read_jedec_id(w25qxx_t *flash, uint8_t id[3]);
bool w25qxx_read(w25qxx_t *flash, uint32_t address, uint8_t *data, size_t length);
bool w25qxx_program(w25qxx_t *flash, uint32_t address,
                    const uint8_t *data, size_t length);
bool w25qxx_erase_sector(w25qxx_t *flash, uint32_t address);

#endif
