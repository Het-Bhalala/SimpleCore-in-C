/*
 * bus.h — SimpleCore Internal Bus
 *
 * The Bus is the abstraction layer between the CPU and all memory/IO
 * resources. Every read or write from the CPU goes through the Bus,
 * which then dispatches to Memory (which in turn dispatches to IO for
 * MMIO addresses).
 *
 * Bus width : 16-bit address, 16-bit data
 */

#ifndef BUS_H
#define BUS_H

#include <stdint.h>
#include "memory.h"

typedef struct Bus {
    Memory *mem;            /* The single memory subsystem on this bus      */
    uint32_t read_count;    /* Cumulative bus read operations               */
    uint32_t write_count;   /* Cumulative bus write operations              */
} Bus;

void bus_init(Bus *bus, Memory *mem);

uint16_t bus_read(Bus *bus, uint16_t addr);
void     bus_write(Bus *bus, uint16_t addr, uint16_t value);

void bus_print_stats(const Bus *bus);

#endif /* BUS_H */
