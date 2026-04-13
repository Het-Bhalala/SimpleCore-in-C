/*
 * isa.h — SimpleCore Instruction Set Architecture
 *
 * Defines all opcodes, addressing modes, flag bits, register indices,
 * and the memory map for the SimpleCore 16-bit CPU.
 *
 * Instruction format (16-bit fixed width):
 *
 *  15  14  13  12  11  10   9   8   7   6   5   4   3   2   1   0
 * +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 * |       OPCODE (6 bits)     | MODE(2) |   DST(3)  |   SRC(5)    |
 * +---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+---+
 *
 * For immediate / memory-direct addressing modes, the 16-bit operand
 * value is stored in the NEXT word after the instruction word.
 */

#ifndef ISA_H
#define ISA_H

#include <stdint.h>

/* -----------------------------------------------------------------------
 * Instruction word bit-field layout
 * ----------------------------------------------------------------------- */
#define INSTR_OPCODE_SHIFT  10
#define INSTR_OPCODE_MASK   (0x3F << INSTR_OPCODE_SHIFT)   /* bits 15:10 */

#define INSTR_MODE_SHIFT    8
#define INSTR_MODE_MASK     (0x03 << INSTR_MODE_SHIFT)     /* bits  9:8  */

#define INSTR_DST_SHIFT     5
#define INSTR_DST_MASK      (0x07 << INSTR_DST_SHIFT)      /* bits  7:5  */

#define INSTR_SRC_MASK      0x1F                            /* bits  4:0  */

/* Extract fields from an encoded instruction word */
#define INSTR_GET_OPCODE(w) (((w) & INSTR_OPCODE_MASK) >> INSTR_OPCODE_SHIFT)
#define INSTR_GET_MODE(w)   (((w) & INSTR_MODE_MASK)   >> INSTR_MODE_SHIFT)
#define INSTR_GET_DST(w)    (((w) & INSTR_DST_MASK)    >> INSTR_DST_SHIFT)
#define INSTR_GET_SRC(w)    ((w)  & INSTR_SRC_MASK)

/* Build an instruction word from its parts */
#define INSTR_ENCODE(op, mode, dst, src) \
    ((uint16_t)(((op) << INSTR_OPCODE_SHIFT) | \
                ((mode) << INSTR_MODE_SHIFT) | \
                ((dst)  << INSTR_DST_SHIFT)  | \
                ((src)  & INSTR_SRC_MASK)))

/* -----------------------------------------------------------------------
 * Addressing modes (MODE field, 2 bits)
 * ----------------------------------------------------------------------- */
typedef enum {
    MODE_REG = 0,   /* Register direct:   operand is register Rx           */
    MODE_IMM = 1,   /* Immediate:         operand is next word in memory    */
    MODE_MEM = 2,   /* Memory direct:     address is next word in memory    */
    MODE_IND = 3    /* Register indirect: address is value in register Rx  */
} AddrMode;

/* -----------------------------------------------------------------------
 * Register indices (3 bits, values 0–7)
 * ----------------------------------------------------------------------- */
typedef enum {
    REG_R0 = 0,
    REG_R1 = 1,
    REG_R2 = 2,
    REG_R3 = 3,
    REG_R4 = 4,
    REG_R5 = 5,
    REG_R6 = 6,
    REG_R7 = 7,
    REG_COUNT = 8
} RegIndex;

/* -----------------------------------------------------------------------
 * Opcodes (6 bits, values 0–63)
 * ----------------------------------------------------------------------- */
typedef enum {
    OP_NOP   = 0x00,  /* No operation                                      */
    OP_MOV   = 0x01,  /* MOV  Rd, Rs/imm  — copy value into Rd             */
    OP_LOAD  = 0x02,  /* LOAD Rd, [addr]  — load word from memory into Rd  */
    OP_STORE = 0x03,  /* STORE [addr], Rs — store Rs to memory             */
    OP_ADD   = 0x04,  /* ADD  Rd, Rs      — Rd = Rd + Rs                   */
    OP_SUB   = 0x05,  /* SUB  Rd, Rs      — Rd = Rd - Rs                   */
    OP_MUL   = 0x06,  /* MUL  Rd, Rs      — Rd = Rd * Rs                   */
    OP_DIV   = 0x07,  /* DIV  Rd, Rs      — Rd = Rd / Rs                   */
    OP_AND   = 0x08,  /* AND  Rd, Rs      — Rd = Rd & Rs                   */
    OP_OR    = 0x09,  /* OR   Rd, Rs      — Rd = Rd | Rs                   */
    OP_XOR   = 0x0A,  /* XOR  Rd, Rs      — Rd = Rd ^ Rs                   */
    OP_NOT   = 0x0B,  /* NOT  Rd          — Rd = ~Rd                        */
    OP_SHL   = 0x0C,  /* SHL  Rd, imm     — Rd = Rd << imm                 */
    OP_SHR   = 0x0D,  /* SHR  Rd, imm     — Rd = Rd >> imm (logical)       */
    OP_CMP   = 0x0E,  /* CMP  Rd, Rs      — set FLAGS, discard result       */
    OP_JMP   = 0x0F,  /* JMP  addr        — unconditional jump              */
    OP_JEQ   = 0x10,  /* JEQ  addr        — jump if Z flag set              */
    OP_JNE   = 0x11,  /* JNE  addr        — jump if Z flag clear            */
    OP_JLT   = 0x12,  /* JLT  addr        — jump if N flag set              */
    OP_JGT   = 0x13,  /* JGT  addr        — jump if Z=0 and N=0            */
    OP_JLE   = 0x14,  /* JLE  addr        — jump if Z=1 or N=1             */
    OP_JGE   = 0x15,  /* JGE  addr        — jump if Z=1 or N=0             */
    OP_CALL  = 0x16,  /* CALL addr        — push PC, jump to addr           */
    OP_RET   = 0x17,  /* RET              — pop PC from stack               */
    OP_PUSH  = 0x18,  /* PUSH Rs          — push register onto stack        */
    OP_POP   = 0x19,  /* POP  Rd          — pop top of stack into Rd        */
    OP_INC   = 0x1A,  /* INC  Rd          — Rd = Rd + 1                     */
    OP_DEC   = 0x1B,  /* DEC  Rd          — Rd = Rd - 1                     */
    OP_IN    = 0x1C,  /* IN   Rd, port    — read word from I/O port         */
    OP_OUT   = 0x1D,  /* OUT  port, Rs    — write word to I/O port          */
    OP_HLT   = 0x1E   /* HLT              — halt the CPU                    */
} Opcode;

/* -----------------------------------------------------------------------
 * FLAGS register bit positions
 *
 *  Bit 0 — Z  (Zero)     : result of last ALU op was zero
 *  Bit 1 — N  (Negative) : result of last ALU op was negative (MSB set)
 *  Bit 2 — C  (Carry)    : unsigned overflow / borrow
 *  Bit 3 — V  (oVerflow) : signed overflow
 * ----------------------------------------------------------------------- */
#define FLAG_Z  (1 << 0)
#define FLAG_N  (1 << 1)
#define FLAG_C  (1 << 2)
#define FLAG_V  (1 << 3)

/* -----------------------------------------------------------------------
 * Memory map (all addresses are byte addresses, word-addressed in practice)
 *
 *  0x0000 – 0x00FF   Interrupt / reset vectors  (256 bytes)
 *  0x0100 – 0x7FFF   ROM / program area         (~32 KB)
 *  0x8000 – 0xEFFF   RAM                        (~28 KB)
 *  0xF000 – 0xFEFF   Reserved
 *  0xFF00 – 0xFF0F   Memory-mapped I/O
 *  0xFF10 – 0xFFFF   Stack (grows downward from 0xFFFF)
 * ----------------------------------------------------------------------- */
#define MEM_SIZE        0x10000   /* 64 KB total address space              */

#define ROM_START       0x0000
#define ROM_END         0x7FFF

#define RAM_START       0x8000
#define RAM_END         0xEFFF

#define MMIO_START      0xFF00
#define MMIO_END        0xFF0F

#define STACK_TOP       0xFFFF    /* SP starts here, grows downward         */
#define STACK_BOTTOM    0xFF10

/* Memory-mapped I/O port addresses */
#define PORT_CONSOLE_OUT  0xFF00  /* Write a char (low byte) to console     */
#define PORT_CONSOLE_IN   0xFF01  /* Read a char (low byte) from console    */
#define PORT_TIMER_LO     0xFF02  /* Timer counter low byte                 */
#define PORT_TIMER_HI     0xFF03  /* Timer counter high byte                */
#define PORT_TIMER_CTRL   0xFF04  /* Timer control: bit0=run, bit1=reset    */

/* Reset / entry-point address */
#define RESET_VECTOR      0x0100  /* CPU begins execution here after reset  */

#endif /* ISA_H */
