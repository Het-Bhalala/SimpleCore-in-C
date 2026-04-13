/*
 * cpu.c — SimpleCore Control Unit
 *
 * Implements the Fetch / Decode / Execute pipeline for every instruction.
 *
 * Fetch/Decode/Execute cycle (one call to cpu_step):
 *
 *   FETCH   1. Read instruction word from memory[PC]; PC++
 *           2. If the instruction needs a second (operand) word,
 *              read memory[PC]; PC++
 *
 *   DECODE  3. Extract OPCODE, MODE, DST, SRC fields from the word.
 *           4. Resolve the effective operand value based on MODE:
 *                MODE_REG — value = regs[SRC]
 *                MODE_IMM — value = second word
 *                MODE_MEM — value = mem[second word]
 *                MODE_IND — value = mem[regs[SRC]]
 *
 *   EXECUTE 5. Dispatch to the opcode handler.
 *           6. Write results back to destination register or memory.
 *           7. Advance cycle counter, tick I/O.
 */

#include <stdio.h>
#include <string.h>
#include "cpu.h"
#include "alu.h"
#include "io.h"

void cpu_init(CPU *cpu, Bus *bus) {
    memset(cpu, 0, sizeof(*cpu));
    cpu->pc          = RESET_VECTOR;
    cpu->sp          = STACK_TOP;
    cpu->flags       = 0;
    cpu->halted      = false;
    cpu->cycle_count = 0;
    cpu->bus         = bus;
}

void cpu_reset(CPU *cpu) {
    Bus *saved_bus = cpu->bus;
    cpu_init(cpu, saved_bus);
}

/* Stack grows downward */
static void stack_push(CPU *cpu, uint16_t value) {
    bus_write(cpu->bus, cpu->sp, value);
    cpu->sp--;
}

static uint16_t stack_pop(CPU *cpu) {
    cpu->sp++;
    return bus_read(cpu->bus, cpu->sp);
}

/*
 * resolve_src_value — return the 16-bit value of the source operand.
 * For MODE_IMM and MODE_MEM the second word is consumed from [PC].
 */
static uint16_t resolve_src_value(CPU *cpu, uint8_t mode, uint8_t src,
                                   uint16_t *extra_word_out) {
    *extra_word_out = 0;
    switch (mode) {
        case MODE_REG: return cpu->regs[src & 0x07];
        case MODE_IMM: { uint16_t imm = bus_read(cpu->bus, cpu->pc++); *extra_word_out = imm; return imm; }
        case MODE_MEM: { uint16_t addr = bus_read(cpu->bus, cpu->pc++); *extra_word_out = addr; return bus_read(cpu->bus, addr); }
        case MODE_IND: return bus_read(cpu->bus, cpu->regs[src & 0x07]);
        default:       return 0;
    }
}

/*
 * resolve_addr — return the effective address for STORE/LOAD.
 * Consumes the extra word from PC for MODE_MEM.
 */
static uint16_t resolve_addr(CPU *cpu, uint8_t mode, uint8_t reg) {
    if (mode == MODE_MEM) {
        return bus_read(cpu->bus, cpu->pc++);
    }
    return cpu->regs[reg & 0x07];
}

/* -----------------------------------------------------------------------
 * Disassembler — one instruction at addr → human-readable string
 * ----------------------------------------------------------------------- */
static const char *reg_name(uint8_t r) {
    static const char *names[] = {"R0","R1","R2","R3","R4","R5","R6","R7"};
    return names[r & 0x07];
}

void cpu_disasm(const CPU *cpu, uint16_t addr, char *buf, int buf_len) {
    uint16_t word  = bus_read((Bus *)cpu->bus, addr);
    uint8_t  op    = INSTR_GET_OPCODE(word);
    uint8_t  mode  = INSTR_GET_MODE(word);
    uint8_t  dst   = INSTR_GET_DST(word);
    uint8_t  src   = INSTR_GET_SRC(word) & 0x07;
    uint16_t extra = bus_read((Bus *)cpu->bus, (uint16_t)(addr + 1));

    switch (op) {
        case OP_NOP:   snprintf(buf, buf_len, "NOP"); break;
        case OP_HLT:   snprintf(buf, buf_len, "HLT"); break;
        case OP_RET:   snprintf(buf, buf_len, "RET"); break;
        case OP_MOV:
            if (mode == MODE_REG) snprintf(buf, buf_len, "MOV  %s, %s",  reg_name(dst), reg_name(src));
            else                  snprintf(buf, buf_len, "MOV  %s, #0x%04X", reg_name(dst), extra);
            break;
        case OP_LOAD:
            if (mode == MODE_MEM) snprintf(buf, buf_len, "LOAD %s, [0x%04X]", reg_name(dst), extra);
            else                  snprintf(buf, buf_len, "LOAD %s, [%s]",      reg_name(dst), reg_name(src));
            break;
        case OP_STORE:
            if (mode == MODE_MEM) snprintf(buf, buf_len, "STORE [0x%04X], %s", extra, reg_name(src));
            else                  snprintf(buf, buf_len, "STORE [%s], %s",      reg_name(dst), reg_name(src));
            break;
        case OP_ADD:  snprintf(buf, buf_len, "ADD  %s, %s", reg_name(dst), reg_name(src)); break;
        case OP_SUB:  snprintf(buf, buf_len, "SUB  %s, %s", reg_name(dst), reg_name(src)); break;
        case OP_MUL:  snprintf(buf, buf_len, "MUL  %s, %s", reg_name(dst), reg_name(src)); break;
        case OP_DIV:  snprintf(buf, buf_len, "DIV  %s, %s", reg_name(dst), reg_name(src)); break;
        case OP_AND:  snprintf(buf, buf_len, "AND  %s, %s", reg_name(dst), reg_name(src)); break;
        case OP_OR:   snprintf(buf, buf_len, "OR   %s, %s", reg_name(dst), reg_name(src)); break;
        case OP_XOR:  snprintf(buf, buf_len, "XOR  %s, %s", reg_name(dst), reg_name(src)); break;
        case OP_NOT:  snprintf(buf, buf_len, "NOT  %s", reg_name(dst)); break;
        case OP_INC:  snprintf(buf, buf_len, "INC  %s", reg_name(dst)); break;
        case OP_DEC:  snprintf(buf, buf_len, "DEC  %s", reg_name(dst)); break;
        case OP_CMP:  snprintf(buf, buf_len, "CMP  %s, %s", reg_name(dst), reg_name(src)); break;
        case OP_SHL:  snprintf(buf, buf_len, "SHL  %s, #%u", reg_name(dst), src); break;
        case OP_SHR:  snprintf(buf, buf_len, "SHR  %s, #%u", reg_name(dst), src); break;
        case OP_JMP:  snprintf(buf, buf_len, "JMP  0x%04X", extra); break;
        case OP_JEQ:  snprintf(buf, buf_len, "JEQ  0x%04X", extra); break;
        case OP_JNE:  snprintf(buf, buf_len, "JNE  0x%04X", extra); break;
        case OP_JLT:  snprintf(buf, buf_len, "JLT  0x%04X", extra); break;
        case OP_JGT:  snprintf(buf, buf_len, "JGT  0x%04X", extra); break;
        case OP_JLE:  snprintf(buf, buf_len, "JLE  0x%04X", extra); break;
        case OP_JGE:  snprintf(buf, buf_len, "JGE  0x%04X", extra); break;
        case OP_CALL: snprintf(buf, buf_len, "CALL 0x%04X", extra); break;
        case OP_PUSH: snprintf(buf, buf_len, "PUSH %s", reg_name(src)); break;
        case OP_POP:  snprintf(buf, buf_len, "POP  %s", reg_name(dst)); break;
        case OP_IN:   snprintf(buf, buf_len, "IN   %s, #0x%04X", reg_name(dst), extra); break;
        case OP_OUT:  snprintf(buf, buf_len, "OUT  #0x%04X, %s", extra, reg_name(src)); break;
        default:      snprintf(buf, buf_len, "??? (0x%04X)", word); break;
    }
}

/* -----------------------------------------------------------------------
 * cpu_step — one Fetch / Decode / Execute cycle
 * ----------------------------------------------------------------------- */
bool cpu_step(CPU *cpu, bool trace) {
    if (cpu->halted) return false;

    /* ---- FETCH ---- */
    uint16_t instr_pc = cpu->pc;
    uint16_t word     = bus_read(cpu->bus, cpu->pc++);

    uint8_t op   = INSTR_GET_OPCODE(word);
    uint8_t mode = (uint8_t)INSTR_GET_MODE(word);
    uint8_t dst  = (uint8_t)INSTR_GET_DST(word);
    uint8_t src  = (uint8_t)(INSTR_GET_SRC(word) & 0x07);

    /* ---- TRACE ---- */
    if (trace) {
        char dis[64];
        cpu_disasm(cpu, instr_pc, dis, sizeof(dis));
        printf("  [%04X]  %-28s  FLAGS=%c%c%c%c  SP=%04X\n",
               instr_pc, dis,
               (cpu->flags & FLAG_Z) ? 'Z' : '-',
               (cpu->flags & FLAG_N) ? 'N' : '-',
               (cpu->flags & FLAG_C) ? 'C' : '-',
               (cpu->flags & FLAG_V) ? 'V' : '-',
               cpu->sp);
    }

    /* ---- DECODE + EXECUTE ---- */
    uint16_t extra = 0;
    uint16_t val   = 0;
    uint16_t addr  = 0;

    switch (op) {
        case OP_NOP: break;
        case OP_HLT: cpu->halted = true; break;

        case OP_MOV:
            val = resolve_src_value(cpu, mode, src, &extra);
            cpu->regs[dst] = val;
            break;

        case OP_LOAD:
            addr = resolve_addr(cpu, mode, src);
            cpu->regs[dst] = bus_read(cpu->bus, addr);
            break;

        case OP_STORE:
            addr = resolve_addr(cpu, mode, dst);
            bus_write(cpu->bus, addr, cpu->regs[src]);
            break;

        case OP_ADD: cpu->regs[dst] = alu_add(cpu, cpu->regs[dst], cpu->regs[src]); break;
        case OP_SUB: cpu->regs[dst] = alu_sub(cpu, cpu->regs[dst], cpu->regs[src]); break;
        case OP_MUL: cpu->regs[dst] = alu_mul(cpu, cpu->regs[dst], cpu->regs[src]); break;
        case OP_DIV: cpu->regs[dst] = alu_div(cpu, cpu->regs[dst], cpu->regs[src]); break;
        case OP_AND: cpu->regs[dst] = alu_and(cpu, cpu->regs[dst], cpu->regs[src]); break;
        case OP_OR:  cpu->regs[dst] = alu_or (cpu, cpu->regs[dst], cpu->regs[src]); break;
        case OP_XOR: cpu->regs[dst] = alu_xor(cpu, cpu->regs[dst], cpu->regs[src]); break;
        case OP_NOT: cpu->regs[dst] = alu_not(cpu, cpu->regs[dst]); break;

        case OP_SHL:
            cpu->regs[dst] = alu_shl(cpu, cpu->regs[dst],
                                     (uint16_t)(INSTR_GET_SRC(word) & 0x0F));
            break;
        case OP_SHR:
            cpu->regs[dst] = alu_shr(cpu, cpu->regs[dst],
                                     (uint16_t)(INSTR_GET_SRC(word) & 0x0F));
            break;

        case OP_CMP: alu_cmp(cpu, cpu->regs[dst], cpu->regs[src]); break;
        case OP_INC: cpu->regs[dst] = alu_add(cpu, cpu->regs[dst], 1); break;
        case OP_DEC: cpu->regs[dst] = alu_sub(cpu, cpu->regs[dst], 1); break;

        case OP_JMP: extra = bus_read(cpu->bus, cpu->pc++); cpu->pc = extra; break;
        case OP_JEQ: extra = bus_read(cpu->bus, cpu->pc++); if ( cpu_get_flag(cpu, FLAG_Z))                           cpu->pc = extra; break;
        case OP_JNE: extra = bus_read(cpu->bus, cpu->pc++); if (!cpu_get_flag(cpu, FLAG_Z))                           cpu->pc = extra; break;
        case OP_JLT: extra = bus_read(cpu->bus, cpu->pc++); if ( cpu_get_flag(cpu, FLAG_N))                           cpu->pc = extra; break;
        case OP_JGT: extra = bus_read(cpu->bus, cpu->pc++); if (!cpu_get_flag(cpu, FLAG_Z) && !cpu_get_flag(cpu, FLAG_N)) cpu->pc = extra; break;
        case OP_JLE: extra = bus_read(cpu->bus, cpu->pc++); if ( cpu_get_flag(cpu, FLAG_Z) ||  cpu_get_flag(cpu, FLAG_N)) cpu->pc = extra; break;
        case OP_JGE: extra = bus_read(cpu->bus, cpu->pc++); if ( cpu_get_flag(cpu, FLAG_Z) || !cpu_get_flag(cpu, FLAG_N)) cpu->pc = extra; break;

        case OP_CALL:
            extra = bus_read(cpu->bus, cpu->pc++);
            stack_push(cpu, cpu->pc);
            cpu->pc = extra;
            break;

        case OP_RET:  cpu->pc = stack_pop(cpu); break;
        case OP_PUSH: stack_push(cpu, cpu->regs[src]); break;
        case OP_POP:  cpu->regs[dst] = stack_pop(cpu); break;

        case OP_IN:
            extra          = bus_read(cpu->bus, cpu->pc++);
            cpu->regs[dst] = bus_read(cpu->bus, extra);
            break;

        case OP_OUT:
            extra = bus_read(cpu->bus, cpu->pc++);
            bus_write(cpu->bus, extra, cpu->regs[src]);
            break;

        default:
            fprintf(stderr, "CPU: unknown opcode 0x%02X at PC=0x%04X\n", op, instr_pc);
            cpu->halted = true;
            break;
    }

    cpu->cycle_count++;
    io_tick(cpu->bus->mem->io);   /* advance timer once per cycle */
    return !cpu->halted;
}

uint64_t cpu_run(CPU *cpu, uint64_t max_cycles, bool trace) {
    uint64_t start = cpu->cycle_count;

    if (trace) {
        printf("=== Execution Trace ===\n");
        printf("  %-6s  %-28s  %-8s  %-6s\n", "PC", "Instruction", "FLAGS", "SP");
        printf("  %s\n", "------------------------------------------------------------");
    }

    while (!cpu->halted) {
        if (max_cycles > 0 && (cpu->cycle_count - start) >= max_cycles) break;
        cpu_step(cpu, trace);
    }

    return cpu->cycle_count - start;
}

void cpu_dump_regs(const CPU *cpu) {
    printf("\n=== CPU Register Dump ===\n");
    for (int i = 0; i < REG_COUNT; i++) {
        printf("  R%d = 0x%04X (%5u)\n", i, cpu->regs[i], cpu->regs[i]);
    }
    printf("  PC = 0x%04X\n", cpu->pc);
    printf("  SP = 0x%04X\n", cpu->sp);
    printf("  FLAGS = 0x%02X  [%c%c%c%c]\n",
           cpu->flags,
           (cpu->flags & FLAG_Z) ? 'Z' : '-',
           (cpu->flags & FLAG_N) ? 'N' : '-',
           (cpu->flags & FLAG_C) ? 'C' : '-',
           (cpu->flags & FLAG_V) ? 'V' : '-');
    printf("  Cycles = %llu\n", (unsigned long long)cpu->cycle_count);
    printf("=========================\n\n");
}
