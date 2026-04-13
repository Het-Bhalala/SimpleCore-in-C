/*
 * assembler.c — SimpleCore Two-Pass Assembler
 *
 * Pass 1 — Scan every line, assign addresses, populate symbol table.
 * Pass 2 — Re-scan every line, emit binary words, resolve all labels.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <stdbool.h>
#include "assembler.h"
#include "isa.h"

#define MAX_LABELS       1024
#define MAX_LINE_LEN     512
#define MAX_TOKENS       16
#define MAX_SOURCE_LINES 65536

typedef struct { char name[64]; uint16_t addr; } Symbol;
typedef struct { Symbol entries[MAX_LABELS]; int count; } SymTable;

static void sym_add(SymTable *st, const char *name, uint16_t addr) {
    if (st->count >= MAX_LABELS) { fprintf(stderr, "Assembler: symbol table full\n"); return; }
    strncpy(st->entries[st->count].name, name, 63);
    st->entries[st->count].name[63] = '\0';
    st->entries[st->count].addr     = addr;
    st->count++;
}

static uint16_t sym_lookup(const SymTable *st, const char *name, bool *found) {
    for (int i = 0; i < st->count; i++) {
        if (strcmp(st->entries[i].name, name) == 0) { *found = true; return st->entries[i].addr; }
    }
    *found = false;
    return 0;
}

/* Tokeniser: splits on whitespace/commas, strips ';' comments */
static int tokenise(char *line, char *tokens[], int max_tokens) {
    char *comment = strchr(line, ';');
    if (comment) *comment = '\0';
    int count = 0;
    char *p = line;
    while (*p && count < max_tokens) {
        while (*p && (isspace((unsigned char)*p) || *p == ',')) p++;
        if (!*p) break;
        tokens[count++] = p;
        if (*p == '"') {
            p++;
            while (*p && *p != '"') p++;
            if (*p == '"') p++;
        } else {
            while (*p && !isspace((unsigned char)*p) && *p != ',') p++;
        }
        if (*p) *p++ = '\0';
    }
    return count;
}

/* Parse decimal / hex (0x) / binary (0b) numbers, optional '#' prefix */
static bool parse_number(const char *s, uint16_t *out) {
    if (!s || !*s) return false;
    if (*s == '#') s++;
    char *end; long val;
    if (s[0]=='0' && (s[1]=='x'||s[1]=='X')) val = strtol(s+2, &end, 16);
    else if (s[0]=='0' && (s[1]=='b'||s[1]=='B')) val = strtol(s+2, &end, 2);
    else val = strtol(s, &end, 10);
    if (end == s || (*end != '\0' && !isspace((unsigned char)*end))) return false;
    *out = (uint16_t)(val & 0xFFFF);
    return true;
}

static int parse_reg(const char *s) {
    if (!s) return -1;
    if ((s[0]=='R'||s[0]=='r') && s[1]>='0' && s[1]<='7' && s[2]=='\0') return s[1]-'0';
    return -1;
}

typedef struct {
    uint8_t  mode;
    int      reg;
    uint16_t value;
    bool     is_label;
    char     label[64];
} Operand;

static bool parse_operand(const char *tok, Operand *op, const SymTable *st, int pass, int line_num) {
    memset(op, 0, sizeof(*op)); op->reg = -1;
    if (!tok || !*tok) return false;

    if (tok[0] == '[') {
        size_t len = strlen(tok);
        if (tok[len-1] != ']') { fprintf(stderr, "Line %d: missing ']'\n", line_num); return false; }
        char inner[64];
        strncpy(inner, tok+1, len-2); inner[len-2] = '\0';
        int r = parse_reg(inner);
        if (r >= 0) { op->mode = MODE_IND; op->reg = r; return true; }
        uint16_t addr = 0;
        if (!parse_number(inner, &addr)) {
            bool found = false;
            addr = sym_lookup(st, inner, &found);
            if (!found && pass == 2) { fprintf(stderr, "Line %d: undefined label '%s'\n", line_num, inner); return false; }
        }
        op->mode = MODE_MEM; op->value = addr; return true;
    }

    int r = parse_reg(tok);
    if (r >= 0) { op->mode = MODE_REG; op->reg = r; return true; }

    op->mode = MODE_IMM;
    const char *s = (tok[0]=='#') ? tok+1 : tok;
    uint16_t num = 0;
    if (parse_number(s, &num)) { op->value = num; return true; }
    op->is_label = true;
    strncpy(op->label, s, 63); op->label[63] = '\0';
    if (pass == 2) {
        bool found = false;
        op->value = sym_lookup(st, op->label, &found);
        if (!found) { fprintf(stderr, "Line %d: undefined label '%s'\n", line_num, op->label); return false; }
    }
    return true;
}

typedef struct { AsmImage *img; uint16_t pc; int pass; } EmitCtx;

static bool emit_word(EmitCtx *ctx, uint16_t word) {
    if (ctx->pass == 2) {
        uint16_t offset = ctx->pc - ctx->img->start_addr;
        if (offset >= MAX_IMAGE_WORDS) { fprintf(stderr, "Assembler: image overflow at 0x%04X\n", ctx->pc); return false; }
        ctx->img->words[offset] = word;
        if (offset + 1 > ctx->img->word_count) ctx->img->word_count = offset + 1;
    }
    ctx->pc++;
    return true;
}

static bool emit_instr(EmitCtx *ctx, uint8_t op, uint8_t mode,
                        uint8_t dst, uint8_t src, bool has_extra, uint16_t extra) {
    if (!emit_word(ctx, INSTR_ENCODE(op, mode, dst, src))) return false;
    if (has_extra && !emit_word(ctx, extra)) return false;
    return true;
}

typedef struct { const char *name; uint8_t opcode; } MnemonicEntry;
static const MnemonicEntry MNEMONICS[] = {
    {"NOP",OP_NOP},{"HLT",OP_HLT},{"MOV",OP_MOV},{"LOAD",OP_LOAD},{"STORE",OP_STORE},
    {"ADD",OP_ADD},{"SUB",OP_SUB},{"MUL",OP_MUL},{"DIV",OP_DIV},{"AND",OP_AND},
    {"OR",OP_OR},{"XOR",OP_XOR},{"NOT",OP_NOT},{"SHL",OP_SHL},{"SHR",OP_SHR},
    {"INC",OP_INC},{"DEC",OP_DEC},{"CMP",OP_CMP},{"JMP",OP_JMP},{"JEQ",OP_JEQ},
    {"JNE",OP_JNE},{"JLT",OP_JLT},{"JGT",OP_JGT},{"JLE",OP_JLE},{"JGE",OP_JGE},
    {"CALL",OP_CALL},{"RET",OP_RET},{"PUSH",OP_PUSH},{"POP",OP_POP},
    {"IN",OP_IN},{"OUT",OP_OUT},{NULL,0}
};

static int lookup_mnemonic(const char *name) {
    char upper[16]; int i = 0;
    while (name[i] && i < 15) { upper[i] = (char)toupper((unsigned char)name[i]); i++; }
    upper[i] = '\0';
    for (const MnemonicEntry *e = MNEMONICS; e->name; e++)
        if (strcmp(e->name, upper) == 0) return (int)e->opcode;
    return -1;
}

/* -----------------------------------------------------------------------
 * Instruction size calculator
 *
 * In pass 1 we need to know how many words each instruction emits so we
 * can correctly assign addresses to subsequent labels.
 *
 * MOV:   1 word if source is a register, 2 words for any immediate/label
 * LOAD:  1 word for [Rs] indirect, 2 words for [addr] direct
 * STORE: 1 word for [Rs] indirect, 2 words for [addr] direct
 * All jumps/calls/IN/OUT: always 2 words (need address/port operand word)
 * Everything else: 1 word
 * ----------------------------------------------------------------------- */
static bool tok_is_reg(const char *t) {
    return t && (t[0]=='R'||t[0]=='r') && t[1]>='0' && t[1]<='7' && t[2]=='\0';
}
static bool tok_is_ind_reg(const char *t) {
    if (!t || t[0]!='[') return false;
    size_t l = strlen(t);
    if (l < 4 || t[l-1] != ']') return false;
    char inner[8] = {0};
    strncpy(inner, t+1, l-2 < 7 ? l-2 : 7);
    return tok_is_reg(inner);
}
static int quick_word_count(int opcode, const char *op1, const char *op2) {
    switch (opcode) {
        case OP_MOV:   return tok_is_reg(op2) ? 1 : 2;
        case OP_LOAD:  return tok_is_ind_reg(op2) ? 1 : 2;
        case OP_STORE: return tok_is_ind_reg(op1) ? 1 : 2;
        case OP_JMP: case OP_JEQ: case OP_JNE: case OP_JLT:
        case OP_JGT: case OP_JLE: case OP_JGE: case OP_CALL:
        case OP_IN:  case OP_OUT: return 2;
        default: return 1;
    }
}

static bool process_line(char *line_buf, int line_num,
                          SymTable *st, EmitCtx *ctx, bool *error_out) {
    *error_out = false;
    char *tokens[MAX_TOKENS];
    int ntok = tokenise(line_buf, tokens, MAX_TOKENS);
    if (ntok == 0) return true;

    int tok_idx = 0;
    if (tokens[0][strlen(tokens[0])-1] == ':') {
        char label[64];
        strncpy(label, tokens[0], 63);
        label[strlen(label)-1] = '\0'; label[63] = '\0';
        if (ctx->pass == 1) {
            bool found = false; sym_lookup(st, label, &found);
            if (found) { fprintf(stderr, "Line %d: duplicate label '%s'\n", line_num, label); *error_out = true; return false; }
            sym_add(st, label, ctx->pc);
        }
        tok_idx++;
    }
    if (tok_idx >= ntok) return true;

    char *first = tokens[tok_idx];

    if (first[0] == '.') {
        char dir[16]; strncpy(dir, first+1, 15); dir[15] = '\0';
        for (int i = 0; dir[i]; i++) dir[i] = (char)toupper((unsigned char)dir[i]);

        if (strcmp(dir, "ORG") == 0) {
            if (tok_idx+1 >= ntok) { fprintf(stderr, "Line %d: .org needs an address\n", line_num); *error_out = true; return false; }
            uint16_t addr = 0;
            if (!parse_number(tokens[tok_idx+1], &addr)) { fprintf(stderr, "Line %d: .org bad address\n", line_num); *error_out = true; return false; }
            ctx->pc = addr;
            if (ctx->pass == 2 && ctx->img->word_count == 0) ctx->img->start_addr = addr;
        } else if (strcmp(dir, "WORD") == 0) {
            for (int i = tok_idx+1; i < ntok; i++) {
                uint16_t val = 0;
                if (!parse_number(tokens[i], &val)) {
                    bool found = false; val = sym_lookup(st, tokens[i], &found);
                    if (!found && ctx->pass == 2) { fprintf(stderr, "Line %d: undefined symbol '%s'\n", line_num, tokens[i]); *error_out = true; return false; }
                }
                if (!emit_word(ctx, val)) { *error_out = true; return false; }
            }
        } else if (strcmp(dir, "STRING") == 0) {
            if (tok_idx+1 >= ntok) { fprintf(stderr, "Line %d: .string needs a quoted string\n", line_num); *error_out = true; return false; }
            const char *s = tokens[tok_idx+1];
            if (*s == '"') s++;
            while (*s && *s != '"') {
                char c = *s++;
                if (c == '\\' && *s) {
                    switch (*s++) { case 'n': c='\n'; break; case 't': c='\t'; break;
                                    case '\\': c='\\'; break; case '"': c='"'; break;
                                    default: c=*(s-1); break; }
                }
                if (!emit_word(ctx, (uint16_t)(unsigned char)c)) { *error_out = true; return false; }
            }
            if (!emit_word(ctx, 0)) { *error_out = true; return false; }
        } else {
            fprintf(stderr, "Line %d: unknown directive '.%s'\n", line_num, dir);
            *error_out = true; return false;
        }
        return true;
    }

    int opcode = lookup_mnemonic(first);
    if (opcode < 0) { fprintf(stderr, "Line %d: unknown mnemonic '%s'\n", line_num, first); *error_out = true; return false; }

    if (ctx->pass == 1) {
        const char *op1 = (tok_idx+1 < ntok) ? tokens[tok_idx+1] : NULL;
        const char *op2 = (tok_idx+2 < ntok) ? tokens[tok_idx+2] : NULL;
        ctx->pc += quick_word_count(opcode, op1, op2);
        return true;
    }

#define TOK(n) ((tok_idx+1+(n)) < ntok ? tokens[tok_idx+1+(n)] : NULL)
    Operand op1 = {0}, op2 = {0};
    bool ok1, ok2;

    switch (opcode) {
        case OP_NOP: case OP_HLT: case OP_RET:
            return emit_instr(ctx, (uint8_t)opcode, MODE_REG, 0, 0, false, 0);

        case OP_MOV:
            ok1 = parse_operand(TOK(0), &op1, st, ctx->pass, line_num);
            ok2 = parse_operand(TOK(1), &op2, st, ctx->pass, line_num);
            if (!ok1 || !ok2 || op1.reg < 0) { fprintf(stderr, "Line %d: MOV dst must be register\n", line_num); *error_out = true; return false; }
            if (op2.mode == MODE_REG) return emit_instr(ctx, OP_MOV, MODE_REG, (uint8_t)op1.reg, (uint8_t)op2.reg, false, 0);
            else                      return emit_instr(ctx, OP_MOV, MODE_IMM, (uint8_t)op1.reg, 0, true, op2.value);

        case OP_LOAD:
            ok1 = parse_operand(TOK(0), &op1, st, ctx->pass, line_num);
            ok2 = parse_operand(TOK(1), &op2, st, ctx->pass, line_num);
            if (!ok1 || !ok2 || op1.reg < 0) { fprintf(stderr, "Line %d: LOAD dst must be register\n", line_num); *error_out = true; return false; }
            if (op2.mode == MODE_IND) return emit_instr(ctx, OP_LOAD, MODE_IND, (uint8_t)op1.reg, (uint8_t)op2.reg, false, 0);
            else                      return emit_instr(ctx, OP_LOAD, MODE_MEM, (uint8_t)op1.reg, 0, true, op2.value);

        case OP_STORE:
            ok1 = parse_operand(TOK(0), &op1, st, ctx->pass, line_num);
            ok2 = parse_operand(TOK(1), &op2, st, ctx->pass, line_num);
            if (!ok1 || !ok2 || op2.reg < 0) { fprintf(stderr, "Line %d: STORE src must be register\n", line_num); *error_out = true; return false; }
            if (op1.mode == MODE_IND) return emit_instr(ctx, OP_STORE, MODE_IND, (uint8_t)op1.reg, (uint8_t)op2.reg, false, 0);
            else                      return emit_instr(ctx, OP_STORE, MODE_MEM, 0, (uint8_t)op2.reg, true, op1.value);

        case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV:
        case OP_AND: case OP_OR:  case OP_XOR: case OP_CMP:
            ok1 = parse_operand(TOK(0), &op1, st, ctx->pass, line_num);
            ok2 = parse_operand(TOK(1), &op2, st, ctx->pass, line_num);
            if (!ok1 || !ok2 || op1.reg < 0 || op2.reg < 0) { fprintf(stderr, "Line %d: needs two registers\n", line_num); *error_out = true; return false; }
            return emit_instr(ctx, (uint8_t)opcode, MODE_REG, (uint8_t)op1.reg, (uint8_t)op2.reg, false, 0);

        case OP_NOT: case OP_INC: case OP_DEC:
            ok1 = parse_operand(TOK(0), &op1, st, ctx->pass, line_num);
            if (!ok1 || op1.reg < 0) { fprintf(stderr, "Line %d: needs a register\n", line_num); *error_out = true; return false; }
            return emit_instr(ctx, (uint8_t)opcode, MODE_REG, (uint8_t)op1.reg, 0, false, 0);

        case OP_SHL: case OP_SHR: {
            ok1 = parse_operand(TOK(0), &op1, st, ctx->pass, line_num);
            if (!ok1 || op1.reg < 0) { *error_out = true; return false; }
            uint16_t shift = 0;
            if (!parse_number(TOK(1), &shift)) { fprintf(stderr, "Line %d: SHL/SHR needs numeric shift\n", line_num); *error_out = true; return false; }
            return emit_instr(ctx, (uint8_t)opcode, MODE_REG, (uint8_t)op1.reg, (uint8_t)(shift & 0x0F), false, 0);
        }

        case OP_PUSH:
            ok1 = parse_operand(TOK(0), &op1, st, ctx->pass, line_num);
            if (!ok1 || op1.reg < 0) { *error_out = true; return false; }
            return emit_instr(ctx, OP_PUSH, MODE_REG, 0, (uint8_t)op1.reg, false, 0);

        case OP_POP:
            ok1 = parse_operand(TOK(0), &op1, st, ctx->pass, line_num);
            if (!ok1 || op1.reg < 0) { *error_out = true; return false; }
            return emit_instr(ctx, OP_POP, MODE_REG, (uint8_t)op1.reg, 0, false, 0);

        case OP_JMP: case OP_JEQ: case OP_JNE: case OP_JLT:
        case OP_JGT: case OP_JLE: case OP_JGE: case OP_CALL:
            ok1 = parse_operand(TOK(0), &op1, st, ctx->pass, line_num);
            if (!ok1) { *error_out = true; return false; }
            return emit_instr(ctx, (uint8_t)opcode, MODE_IMM, 0, 0, true, op1.value);

        case OP_IN:
            ok1 = parse_operand(TOK(0), &op1, st, ctx->pass, line_num);
            ok2 = parse_operand(TOK(1), &op2, st, ctx->pass, line_num);
            if (!ok1 || op1.reg < 0) { fprintf(stderr, "Line %d: IN dst must be register\n", line_num); *error_out = true; return false; }
            return emit_instr(ctx, OP_IN, MODE_IMM, (uint8_t)op1.reg, 0, true, ok2 ? op2.value : 0);

        case OP_OUT:
            ok1 = parse_operand(TOK(0), &op1, st, ctx->pass, line_num);
            ok2 = parse_operand(TOK(1), &op2, st, ctx->pass, line_num);
            if (!ok2 || op2.reg < 0) { fprintf(stderr, "Line %d: OUT src must be register\n", line_num); *error_out = true; return false; }
            return emit_instr(ctx, OP_OUT, MODE_IMM, 0, (uint8_t)op2.reg, true, ok1 ? op1.value : 0);

        default:
            fprintf(stderr, "Line %d: unhandled opcode in pass 2\n", line_num);
            *error_out = true; return false;
    }
#undef TOK
}

static AsmResult run_two_pass(char **lines, int num_lines, AsmImage *image) {
    SymTable sym = {0};
    memset(image, 0, sizeof(*image));
    image->start_addr = RESET_VECTOR;

    /* Pass 1 — build symbol table */
    { EmitCtx ctx; ctx.img = image; ctx.pc = RESET_VECTOR; ctx.pass = 1;
      for (int i = 0; i < num_lines; i++) {
          bool err = false; char buf[MAX_LINE_LEN];
          strncpy(buf, lines[i], MAX_LINE_LEN-1); buf[MAX_LINE_LEN-1] = '\0';
          if (!process_line(buf, i+1, &sym, &ctx, &err)) return ASM_ERR_SYNTAX;
      }
    }

    /* Pass 2 — emit binary */
    { EmitCtx ctx; ctx.img = image; ctx.pc = RESET_VECTOR; ctx.pass = 2;
      image->start_addr = RESET_VECTOR; image->word_count = 0;
      for (int i = 0; i < num_lines; i++) {
          bool err = false; char buf[MAX_LINE_LEN];
          strncpy(buf, lines[i], MAX_LINE_LEN-1); buf[MAX_LINE_LEN-1] = '\0';
          if (!process_line(buf, i+1, &sym, &ctx, &err))
              return err ? ASM_ERR_SYNTAX : ASM_ERR_INTERNAL;
      }
    }

    return ASM_OK;
}

AsmResult asm_assemble_string(const char *source, AsmImage *image) {
    char *src_copy = (char *)malloc(strlen(source) + 1);
    if (!src_copy) return ASM_ERR_INTERNAL;
    strcpy(src_copy, source);

    char *line_ptrs[MAX_SOURCE_LINES]; int num_lines = 0;
    char *p = src_copy; line_ptrs[num_lines++] = p;
    while (*p && num_lines < MAX_SOURCE_LINES) {
        if (*p == '\n') { *p = '\0'; if (*(p+1)) line_ptrs[num_lines++] = p+1; }
        p++;
    }

    AsmResult result = run_two_pass(line_ptrs, num_lines, image);
    free(src_copy);
    return result;
}

AsmResult asm_assemble_file(const char *source_path, AsmImage *image) {
    FILE *f = fopen(source_path, "r");
    if (!f) { fprintf(stderr, "Assembler: cannot open '%s'\n", source_path); return ASM_ERR_FILE; }
    fseek(f, 0, SEEK_END); long file_size = ftell(f); fseek(f, 0, SEEK_SET);
    if (file_size <= 0) { fclose(f); return ASM_ERR_FILE; }
    char *buf = (char *)malloc((size_t)file_size + 1);
    if (!buf) { fclose(f); return ASM_ERR_INTERNAL; }
    size_t nread = fread(buf, 1, (size_t)file_size, f);
    buf[nread] = '\0'; fclose(f);
    AsmResult result = asm_assemble_string(buf, image);
    free(buf);
    return result;
}

bool asm_save_bin(const AsmImage *image, const char *out_path) {
    FILE *f = fopen(out_path, "wb");
    if (!f) { fprintf(stderr, "Cannot write '%s'\n", out_path); return false; }
    for (uint16_t i = 0; i < image->word_count; i++) {
        fputc((int)(image->words[i] & 0xFF), f);
        fputc((int)(image->words[i] >> 8),   f);
    }
    fclose(f); return true;
}

bool asm_load_bin(const char *bin_path, AsmImage *image, uint16_t start_addr) {
    FILE *f = fopen(bin_path, "rb");
    if (!f) { fprintf(stderr, "Cannot open '%s'\n", bin_path); return false; }
    memset(image, 0, sizeof(*image)); image->start_addr = start_addr;
    uint16_t idx = 0;
    while (idx < MAX_IMAGE_WORDS) {
        int lo = fgetc(f); if (lo == EOF) break;
        int hi = fgetc(f); if (hi == EOF) { image->words[idx++] = (uint16_t)lo; break; }
        image->words[idx++] = (uint16_t)((hi << 8) | lo);
    }
    image->word_count = idx; fclose(f); return true;
}
