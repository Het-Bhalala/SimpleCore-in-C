/*
 * alu.h — SimpleCore Arithmetic Logic Unit
 *
 * The ALU is a stateless functional unit. Every operation takes two
 * 16-bit operands (a, b), performs the requested computation, updates
 * the CPU FLAGS register, and returns the 16-bit result.
 *
 * FLAGS updated after each ALU operation:
 *
 *   Z  (Zero)     — result == 0
 *   N  (Negative) — bit 15 of result is set
 *   C  (Carry)    — unsigned overflow on add; borrow on subtract
 *   V  (oVerflow) — signed two's-complement overflow on add/subtract
 */

#ifndef ALU_H
#define ALU_H

#include <stdint.h>
#include "isa.h"

/* Forward declaration — cpu.h provides the full CPU struct */
struct CPU;

uint16_t alu_add(struct CPU *cpu, uint16_t a, uint16_t b);
uint16_t alu_sub(struct CPU *cpu, uint16_t a, uint16_t b);
uint16_t alu_mul(struct CPU *cpu, uint16_t a, uint16_t b);
uint16_t alu_div(struct CPU *cpu, uint16_t a, uint16_t b);

uint16_t alu_and(struct CPU *cpu, uint16_t a, uint16_t b);
uint16_t alu_or (struct CPU *cpu, uint16_t a, uint16_t b);
uint16_t alu_xor(struct CPU *cpu, uint16_t a, uint16_t b);
uint16_t alu_not(struct CPU *cpu, uint16_t a);

uint16_t alu_shl(struct CPU *cpu, uint16_t a, uint16_t shift);
uint16_t alu_shr(struct CPU *cpu, uint16_t a, uint16_t shift);

void alu_cmp(struct CPU *cpu, uint16_t a, uint16_t b);
void alu_update_zn(struct CPU *cpu, uint16_t result);

#endif /* ALU_H */
