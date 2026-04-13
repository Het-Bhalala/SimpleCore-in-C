/*
 * bus.c — SimpleCore Internal Bus
 *
 * Thin dispatch layer between the CPU and memory. Maintains read/write
 * counters that can be printed for diagnostics.
 */

#include <stdio.h>
#include "bus.h"

void bus_init(Bus *bus, Memory *mem) {
    bus->mem         = mem;
    bus->read_count  = 0;
    bus->write_count = 0;
}

uint16_t bus_read(Bus *bus, uint16_t addr) {
    bus->read_count++;
    return mem_read(bus->mem, addr);
}

void bus_write(Bus *bus, uint16_t addr, uint16_t value) {
    bus->write_count++;
    mem_write(bus->mem, addr, value);
}

void bus_print_stats(const Bus *bus) {
    printf("Bus stats - reads: %u  writes: %u  total: %u\n",
           bus->read_count,
           bus->write_count,
           bus->read_count + bus->write_count);
}
