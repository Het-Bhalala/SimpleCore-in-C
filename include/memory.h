/*
 * memory.h — SimpleCore Memory Subsystem
 *
 * The memory model is a flat 64 KB array. Reads and writes route through
 * a single interface that enforces the memory map:
 *
 *   - ROM region (0x0000–0x7FFF): writable only during program load
 *   - RAM region (0x8000–0xEFFF): freely readable/writable
 *   - MMIO region (0xFF00–0xFF0F): dispatched to io.h handlers
 *   - Stack region (0xFF10–0xFFFF): RAM-backed
 *
 * All values are 16-bit words stored in little-endian order.
 */

#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stdbool.h>
#include "isa.h"

/* Forward declaration — io.h provides the full definition */
struct IO;

typedef struct Memory {
    uint16_t  data[MEM_SIZE];   /* Flat 64 K word array (word-addressed)   */
    bool      rom_locked;       /* When true, writes to ROM region are NOP  */
    struct IO *io;              /* Pointer to I/O subsystem (for MMIO)      */
} Memory;

void mem_init(Memory *mem, struct IO *io);
void mem_lock_rom(Memory *mem);

uint16_t mem_read(Memory *mem, uint16_t addr);
void     mem_write(Memory *mem, uint16_t addr, uint16_t value);

void mem_load(Memory *mem, uint16_t start_addr,
              const uint16_t *words, uint16_t count);

void mem_dump(const Memory *mem, uint16_t start_addr, uint16_t length);

#endif /* MEMORY_H */
