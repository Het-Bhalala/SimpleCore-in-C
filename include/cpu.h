/*
 * cpu.h — SimpleCore CPU (Control Unit + Register File)
 *
 * The CPU structure holds the complete architectural state of the processor:
 *
 *   R0–R7   — eight 16-bit general-purpose registers
 *   PC      — Program Counter (points to the next instruction word)
 *   SP      — Stack Pointer (points to the last pushed word; init 0xFFFF)
 *   FLAGS   — condition codes: Z N C V (see isa.h for bit definitions)
 *   halted  — set to true by HLT; stops the run loop
 *
 * Execution model — each cpu_step() performs one full Fetch/Decode/Execute
 * cycle:
 *
 *   FETCH   — read the 16-bit instruction word at [PC]; PC += 1
 *             if the instruction needs a second word (immediate / address),
 *             read [PC]; PC += 1
 *   DECODE  — extract OPCODE, MODE, DST, SRC fields from the instruction
 *   EXECUTE — dispatch to the appropriate handler; update registers/memory
 */

#ifndef CPU_H
#define CPU_H

#include <stdint.h>
#include <stdbool.h>
#include "isa.h"
#include "bus.h"

typedef struct CPU {
    uint16_t regs[REG_COUNT];   /* General-purpose registers R0–R7         */
    uint16_t pc;                /* Program Counter                         */
    uint16_t sp;                /* Stack Pointer                           */
    uint8_t  flags;             /* Condition flags: Z N C V                */
    bool     halted;            /* Execution stopped when true             */
    uint64_t cycle_count;       /* Total cycles executed (for profiling)   */
    Bus     *bus;               /* Pointer to the system bus               */
} CPU;

void cpu_init(CPU *cpu, Bus *bus);
void cpu_reset(CPU *cpu);

bool     cpu_step(CPU *cpu, bool trace);
uint64_t cpu_run(CPU *cpu, uint64_t max_cycles, bool trace);

void cpu_dump_regs(const CPU *cpu);
void cpu_disasm(const CPU *cpu, uint16_t addr, char *buf, int buf_len);

static inline void cpu_set_flag(CPU *cpu, uint8_t mask)   { cpu->flags |=  mask; }
static inline void cpu_clr_flag(CPU *cpu, uint8_t mask)   { cpu->flags &= ~mask; }
static inline bool cpu_get_flag(const CPU *cpu, uint8_t mask) {
    return (cpu->flags & mask) != 0;
}

#endif /* CPU_H */
