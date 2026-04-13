/*
 * alu.c — SimpleCore Arithmetic Logic Unit
 *
 * All operations are 16-bit unsigned at the hardware level.  Signed
 * interpretation is only relevant for overflow detection and the N flag.
 *
 * FLAG update rules per operation type:
 *
 *   Arithmetic (ADD, SUB, MUL, DIV, INC, DEC)
 *     Z  — result == 0
 *     N  — bit 15 of result == 1
 *     C  — unsigned carry (ADD) or borrow (SUB)
 *     V  — signed two's-complement overflow (ADD, SUB only)
 *
 *   Bitwise (AND, OR, XOR, NOT, SHL, SHR)
 *     Z  — result == 0
 *     N  — bit 15 of result == 1
 *     C  — last bit shifted out (SHL/SHR); cleared for AND/OR/XOR/NOT
 *     V  — always cleared
 *
 *   CMP — identical to SUB but result is discarded
 */

#include "alu.h"
#include "cpu.h"

void alu_update_zn(struct CPU *cpu, uint16_t result) {
    if (result == 0) cpu_set_flag(cpu, FLAG_Z); else cpu_clr_flag(cpu, FLAG_Z);
    if (result & 0x8000) cpu_set_flag(cpu, FLAG_N); else cpu_clr_flag(cpu, FLAG_N);
}

uint16_t alu_add(struct CPU *cpu, uint16_t a, uint16_t b) {
    uint32_t result32 = (uint32_t)a + (uint32_t)b;
    uint16_t result   = (uint16_t)(result32 & 0xFFFF);
    alu_update_zn(cpu, result);
    if (result32 > 0xFFFF) cpu_set_flag(cpu, FLAG_C); else cpu_clr_flag(cpu, FLAG_C);
    int16_t sa = (int16_t)a, sb = (int16_t)b, sr = (int16_t)result;
    if ((sa > 0 && sb > 0 && sr < 0) || (sa < 0 && sb < 0 && sr > 0))
        cpu_set_flag(cpu, FLAG_V); else cpu_clr_flag(cpu, FLAG_V);
    return result;
}

uint16_t alu_sub(struct CPU *cpu, uint16_t a, uint16_t b) {
    uint32_t result32 = (uint32_t)a - (uint32_t)b;
    uint16_t result   = (uint16_t)(result32 & 0xFFFF);
    alu_update_zn(cpu, result);
    if (a < b) cpu_set_flag(cpu, FLAG_C); else cpu_clr_flag(cpu, FLAG_C);
    int16_t sa = (int16_t)a, sb = (int16_t)b, sr = (int16_t)result;
    if ((sa > 0 && sb < 0 && sr < 0) || (sa < 0 && sb > 0 && sr > 0))
        cpu_set_flag(cpu, FLAG_V); else cpu_clr_flag(cpu, FLAG_V);
    return result;
}

uint16_t alu_mul(struct CPU *cpu, uint16_t a, uint16_t b) {
    uint32_t result32 = (uint32_t)a * (uint32_t)b;
    uint16_t result   = (uint16_t)(result32 & 0xFFFF);
    alu_update_zn(cpu, result);
    if (result32 > 0xFFFF) cpu_set_flag(cpu, FLAG_C); else cpu_clr_flag(cpu, FLAG_C);
    cpu_clr_flag(cpu, FLAG_V);
    return result;
}

uint16_t alu_div(struct CPU *cpu, uint16_t a, uint16_t b) {
    if (b == 0) {
        cpu_set_flag(cpu, FLAG_C);
        cpu_clr_flag(cpu, FLAG_V);
        alu_update_zn(cpu, 0xFFFF);
        return 0xFFFF;
    }
    uint16_t result = a / b;
    alu_update_zn(cpu, result);
    cpu_clr_flag(cpu, FLAG_C);
    cpu_clr_flag(cpu, FLAG_V);
    return result;
}

uint16_t alu_and(struct CPU *cpu, uint16_t a, uint16_t b) {
    uint16_t r = a & b;
    alu_update_zn(cpu, r); cpu_clr_flag(cpu, FLAG_C); cpu_clr_flag(cpu, FLAG_V);
    return r;
}

uint16_t alu_or(struct CPU *cpu, uint16_t a, uint16_t b) {
    uint16_t r = a | b;
    alu_update_zn(cpu, r); cpu_clr_flag(cpu, FLAG_C); cpu_clr_flag(cpu, FLAG_V);
    return r;
}

uint16_t alu_xor(struct CPU *cpu, uint16_t a, uint16_t b) {
    uint16_t r = a ^ b;
    alu_update_zn(cpu, r); cpu_clr_flag(cpu, FLAG_C); cpu_clr_flag(cpu, FLAG_V);
    return r;
}

uint16_t alu_not(struct CPU *cpu, uint16_t a) {
    uint16_t r = ~a;
    alu_update_zn(cpu, r); cpu_clr_flag(cpu, FLAG_C); cpu_clr_flag(cpu, FLAG_V);
    return r;
}

uint16_t alu_shl(struct CPU *cpu, uint16_t a, uint16_t shift) {
    if (shift == 0) { alu_update_zn(cpu, a); cpu_clr_flag(cpu, FLAG_C); cpu_clr_flag(cpu, FLAG_V); return a; }
    if (shift <= 16 && (a >> (16 - shift)) & 1) cpu_set_flag(cpu, FLAG_C); else cpu_clr_flag(cpu, FLAG_C);
    uint16_t result = (shift >= 16) ? 0 : (uint16_t)(a << shift);
    alu_update_zn(cpu, result); cpu_clr_flag(cpu, FLAG_V);
    return result;
}

uint16_t alu_shr(struct CPU *cpu, uint16_t a, uint16_t shift) {
    if (shift == 0) { alu_update_zn(cpu, a); cpu_clr_flag(cpu, FLAG_C); cpu_clr_flag(cpu, FLAG_V); return a; }
    if (shift <= 16 && (a >> (shift - 1)) & 1) cpu_set_flag(cpu, FLAG_C); else cpu_clr_flag(cpu, FLAG_C);
    uint16_t result = (shift >= 16) ? 0 : (uint16_t)(a >> shift);
    alu_update_zn(cpu, result); cpu_clr_flag(cpu, FLAG_V);
    return result;
}

void alu_cmp(struct CPU *cpu, uint16_t a, uint16_t b) {
    alu_sub(cpu, a, b);
}
