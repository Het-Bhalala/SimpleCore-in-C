/*
 * io.c — SimpleCore Memory-Mapped I/O Subsystem
 *
 * Handles all accesses to the MMIO region (0xFF00–0xFF0F).
 * The timer increments once per io_tick() call, which is invoked
 * once per CPU cycle from cpu_step().
 */

#include <stdio.h>
#include <string.h>
#include "io.h"

void io_init(IO *io) {
    memset(io, 0, sizeof(*io));
    io->timer.running = false;
    io->timer.counter = 0;
}

void io_tick(IO *io) {
    if (io->timer.running) {
        io->timer.counter++;
    }
}

uint16_t io_read(IO *io, uint16_t port) {
    switch (port) {

        case PORT_CONSOLE_IN: {
            int c = getchar();
            return (c == EOF) ? 0 : (uint16_t)(c & 0xFF);
        }

        case PORT_TIMER_LO:
            return (uint16_t)(io->timer.counter & 0xFFFF);

        case PORT_TIMER_HI:
            return (uint16_t)((io->timer.counter >> 16) & 0xFFFF);

        case PORT_TIMER_CTRL:
            return io->timer.running ? 1 : 0;

        case PORT_CONSOLE_OUT:
            return 0;

        default:
            return 0;
    }
}

void io_write(IO *io, uint16_t port, uint16_t value) {
    switch (port) {

        case PORT_CONSOLE_OUT:
            /* Print the low byte as an ASCII character */
            putchar((int)(value & 0xFF));
            fflush(stdout);
            break;

        case PORT_TIMER_CTRL:
            /* bit 0 : 1 = start timer, 0 = stop timer
             * bit 1 : write 1 to reset counter to zero */
            if (value & 0x02) {
                io->timer.counter = 0;
            }
            io->timer.running = (value & 0x01) ? true : false;
            break;

        case PORT_TIMER_LO:
            io->timer.counter = (io->timer.counter & 0xFFFF0000)
                                | (uint32_t)(value & 0xFFFF);
            break;

        case PORT_TIMER_HI:
            io->timer.counter = (io->timer.counter & 0x0000FFFF)
                                | ((uint32_t)(value & 0xFFFF) << 16);
            break;

        default:
            break;
    }
}
