/*
 * assembler.h — SimpleCore Two-Pass Assembler
 *
 * Converts SimpleCore assembly source (.asm) into a binary word image
 * (.bin) that can be loaded directly into the emulator.
 *
 * -----------------------------------------------------------------------
 * Assembly language syntax
 * -----------------------------------------------------------------------
 *
 * Comments     : semicolon (;) to end of line
 * Labels       : identifier followed by colon  e.g.  loop:  ADD R0, R1
 * Registers    : R0 – R7  (case-insensitive)
 *
 * Numeric literals:
 *   Decimal    : 42
 *   Hexadecimal: 0xFF  or  0xff
 *   Binary     : 0b1010
 *
 * Directives:
 *   .org  <addr>         — set current address counter
 *   .word <val> [, ...]  — emit raw 16-bit word(s)
 *   .string "text"       — emit null-terminated ASCII words (one char/word)
 *
 * -----------------------------------------------------------------------
 * Two-pass algorithm
 * -----------------------------------------------------------------------
 *
 * Pass 1: Walk every line. Assign addresses to instructions and data
 *         directives. Record label → address in the symbol table.
 *
 * Pass 2: Walk every line again. Emit binary words. Resolve all labels.
 */

#ifndef ASSEMBLER_H
#define ASSEMBLER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define MAX_IMAGE_WORDS 0x8000   /* max 32K words (covers ROM + some RAM)  */

typedef struct {
    uint16_t words[MAX_IMAGE_WORDS];
    uint16_t start_addr;
    uint16_t word_count;
} AsmImage;

typedef enum {
    ASM_OK            =  0,
    ASM_ERR_FILE      = -1,
    ASM_ERR_SYNTAX    = -2,
    ASM_ERR_UNDEF_SYM = -3,
    ASM_ERR_OVERFLOW  = -4,
    ASM_ERR_INTERNAL  = -5
} AsmResult;

AsmResult asm_assemble_file(const char *source_path, AsmImage *image);
AsmResult asm_assemble_string(const char *source, AsmImage *image);
bool      asm_save_bin(const AsmImage *image, const char *out_path);
bool      asm_load_bin(const char *bin_path, AsmImage *image, uint16_t start_addr);

#endif /* ASSEMBLER_H */
