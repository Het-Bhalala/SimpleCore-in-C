/*
 * memory.c — SimpleCore Memory Subsystem
 *
 * Provides a flat 64 KB word-addressed memory with:
 *   - ROM region protection (after locking, writes are silently ignored)
 *   - Automatic MMIO dispatch for addresses 0xFF00–0xFF0F
 *   - Hex-dump utility for post-mortem inspection
 */

#include <stdio.h>
#include <string.h>
#include "memory.h"
#include "io.h"

void mem_init(Memory *mem, IO *io) {
    memset(mem->data, 0, sizeof(mem->data));
    mem->rom_locked = false;
    mem->io = io;
}

void mem_lock_rom(Memory *mem) {
    mem->rom_locked = true;
}

uint16_t mem_read(Memory *mem, uint16_t addr) {
    if (addr >= MMIO_START && addr <= MMIO_END) {
        return io_read(mem->io, addr);
    }
    return mem->data[addr];
}

void mem_write(Memory *mem, uint16_t addr, uint16_t value) {
    if (addr >= MMIO_START && addr <= MMIO_END) {
        io_write(mem->io, addr, value);
        return;
    }
    if (mem->rom_locked && addr >= ROM_START && addr <= ROM_END) {
        return;
    }
    mem->data[addr] = value;
}

void mem_load(Memory *mem, uint16_t start_addr,
              const uint16_t *words, uint16_t count) {
    for (uint16_t i = 0; i < count; i++) {
        mem->data[(uint32_t)start_addr + i] = words[i];
    }
}

void mem_dump(const Memory *mem, uint16_t start_addr, uint16_t length) {
    const int WORDS_PER_ROW = 8;

    printf("\n--- Memory Dump: 0x%04X - 0x%04X ---\n",
           start_addr, (uint16_t)(start_addr + length - 1));

    for (uint16_t i = 0; i < length; i += WORDS_PER_ROW) {
        uint16_t row_addr = start_addr + i;
        printf("  %04X:  ", row_addr);

        for (int j = 0; j < WORDS_PER_ROW; j++) {
            uint16_t w_addr = row_addr + j;
            if (i + j < length) {
                printf("%04X ", mem->data[w_addr]);
            } else {
                printf("     ");
            }
        }

        printf(" | ");
        for (int j = 0; j < WORDS_PER_ROW; j++) {
            uint16_t w_addr = row_addr + j;
            if (i + j < length) {
                uint8_t lo = (uint8_t)(mem->data[w_addr] & 0xFF);
                uint8_t hi = (uint8_t)(mem->data[w_addr] >> 8);
                putchar(lo >= 0x20 && lo < 0x7F ? lo : '.');
                putchar(hi >= 0x20 && hi < 0x7F ? hi : '.');
            }
        }
        printf(" |\n");
    }
    printf("---\n\n");
}
