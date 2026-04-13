/*
 * io.h — SimpleCore Memory-Mapped I/O Subsystem
 *
 * Devices are mapped into the address range 0xFF00–0xFF0F.
 * The Memory subsystem calls io_read / io_write whenever an access
 * falls within that range.
 *
 * Devices implemented:
 *
 *   PORT_CONSOLE_OUT (0xFF00) — write a character to stdout
 *   PORT_CONSOLE_IN  (0xFF01) — read a character from stdin (non-blocking)
 *   PORT_TIMER_LO    (0xFF02) — low  16 bits of a free-running tick counter
 *   PORT_TIMER_HI    (0xFF03) — high 16 bits of a free-running tick counter
 *   PORT_TIMER_CTRL  (0xFF04) — timer control register
 *                               bit 0 : 1 = timer running, 0 = stopped
 *                               bit 1 : write 1 to reset counter to 0
 */

#ifndef IO_H
#define IO_H

#include <stdint.h>
#include <stdbool.h>
#include "isa.h"

typedef struct {
    uint32_t counter;   /* 32-bit free-running tick count                  */
    bool     running;   /* Whether the timer is currently incrementing      */
} Timer;

typedef struct IO {
    Timer timer;
} IO;

void io_init(IO *io);

uint16_t io_read(IO *io, uint16_t port);
void     io_write(IO *io, uint16_t port, uint16_t value);

void io_tick(IO *io);

#endif /* IO_H */
