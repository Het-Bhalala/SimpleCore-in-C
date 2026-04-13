/*
 * main.c — SimpleCore Emulator Entry Point
 *
 * Usage:
 *   simplecore --assemble <file.asm> [-o <file.bin>]
 *   simplecore --run  <file.asm|file.bin> [options]
 *   simplecore --step <file.asm|file.bin>
 *
 * Options:
 *   -o <file>            Output .bin file (--assemble only)
 *   --trace              Trace each instruction (shows Fetch/Decode/Execute)
 *   --dump               Hex dump RAM after halt
 *   --dump-addr <hex>    Start address for dump (default: 0x8000)
 *   --dump-len  <hex>    Number of words to dump (default: 0x40)
 *   --max-cycles <n>     Stop after N cycles (0 = unlimited)
 *   --regs               Print registers after halt
 *   --bus-stats          Print bus statistics after halt
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "isa.h"
#include "io.h"
#include "memory.h"
#include "bus.h"
#include "cpu.h"
#include "assembler.h"

static void print_usage(const char *prog) {
    printf("SimpleCore CPU Emulator\n");
    printf("Usage:\n");
    printf("  %s --assemble <file.asm> [-o <file.bin>]\n", prog);
    printf("  %s --run  <file.asm|file.bin> [options]\n", prog);
    printf("  %s --step <file.asm|file.bin>\n", prog);
    printf("\nOptions:\n");
    printf("  -o <file>            Output .bin file (--assemble only)\n");
    printf("  --trace              Trace each instruction\n");
    printf("  --dump               Hex dump RAM after halt\n");
    printf("  --dump-addr <hex>    Start address for dump (default: 0x8000)\n");
    printf("  --dump-len  <hex>    Number of words to dump (default: 0x40)\n");
    printf("  --max-cycles <n>     Stop after N cycles (0 = unlimited)\n");
    printf("  --regs               Print registers after halt\n");
    printf("  --bus-stats          Print bus statistics after halt\n");
}

static bool is_asm_file(const char *path) {
    size_t len = strlen(path);
    if (len < 4) return false;
    const char *ext = path + len - 4;
    return (ext[0] == '.' &&
            (ext[1] == 'a' || ext[1] == 'A') &&
            (ext[2] == 's' || ext[2] == 'S') &&
            (ext[3] == 'm' || ext[3] == 'M'));
}

static void make_bin_path(const char *asm_path, char *bin_path, size_t buf_len) {
    strncpy(bin_path, asm_path, buf_len - 1);
    bin_path[buf_len - 1] = '\0';
    size_t len = strlen(bin_path);
    if (len >= 4 && strcmp(bin_path + len - 4, ".asm") == 0) {
        strcpy(bin_path + len - 4, ".bin");
    } else {
        strncat(bin_path, ".bin", buf_len - strlen(bin_path) - 1);
    }
}

static bool load_program(const char *path, Memory *mem) {
    AsmImage image;

    if (is_asm_file(path)) {
        printf("Assembling '%s'...\n", path);
        AsmResult r = asm_assemble_file(path, &image);
        if (r != ASM_OK) {
            fprintf(stderr, "Assembly failed (error %d)\n", r);
            return false;
        }
        printf("  -> %u words assembled, load address 0x%04X\n",
               image.word_count, image.start_addr);
    } else {
        if (!asm_load_bin(path, &image, RESET_VECTOR)) return false;
        printf("Loaded '%s': %u words at 0x%04X\n",
               path, image.word_count, image.start_addr);
    }

    mem_load(mem, image.start_addr, image.words, image.word_count);
    return true;
}

static int cmd_assemble(const char *src_path, const char *out_path) {
    AsmImage image;
    AsmResult r = asm_assemble_file(src_path, &image);
    if (r != ASM_OK) { fprintf(stderr, "Assembly failed (error %d)\n", r); return 1; }

    char bin_path[512];
    if (out_path) {
        strncpy(bin_path, out_path, sizeof(bin_path) - 1);
        bin_path[sizeof(bin_path) - 1] = '\0';
    } else {
        make_bin_path(src_path, bin_path, sizeof(bin_path));
    }

    if (!asm_save_bin(&image, bin_path)) return 1;

    printf("Assembled '%s' -> '%s'  (%u words, %u bytes)\n",
           src_path, bin_path, image.word_count, image.word_count * 2);
    return 0;
}

static int cmd_run(const char *prog_path,
                   bool trace, bool dump, bool show_regs, bool bus_stats,
                   uint16_t dump_addr, uint16_t dump_len, uint64_t max_cycles) {
    IO io; Memory mem; Bus bus; CPU cpu;

    io_init(&io);
    mem_init(&mem, &io);
    bus_init(&bus, &mem);
    cpu_init(&cpu, &bus);

    if (!load_program(prog_path, &mem)) return 1;
    mem_lock_rom(&mem);

    printf("Running '%s'...\n\n", prog_path);
    uint64_t cycles = cpu_run(&cpu, max_cycles, trace);
    printf("\n\nHalted after %llu cycles.\n", (unsigned long long)cycles);

    if (show_regs)  cpu_dump_regs(&cpu);
    if (bus_stats)  bus_print_stats(&bus);
    if (dump)       mem_dump(&mem, dump_addr, dump_len);

    return 0;
}

static int cmd_step(const char *prog_path) {
    IO io; Memory mem; Bus bus; CPU cpu;

    io_init(&io);
    mem_init(&mem, &io);
    bus_init(&bus, &mem);
    cpu_init(&cpu, &bus);

    if (!load_program(prog_path, &mem)) return 1;
    mem_lock_rom(&mem);

    printf("\nInteractive step mode. Press Enter to step, 'q' + Enter to quit.\n\n");

    while (!cpu.halted) {
        char dis[64];
        cpu_disasm(&cpu, cpu.pc, dis, sizeof(dis));
        printf("  [%04X] %-28s  FLAGS=%c%c%c%c  SP=%04X\n",
               cpu.pc, dis,
               (cpu.flags & FLAG_Z) ? 'Z' : '-',
               (cpu.flags & FLAG_N) ? 'N' : '-',
               (cpu.flags & FLAG_C) ? 'C' : '-',
               (cpu.flags & FLAG_V) ? 'V' : '-',
               cpu.sp);
        printf("  R0=%04X R1=%04X R2=%04X R3=%04X R4=%04X R5=%04X R6=%04X R7=%04X\n",
               cpu.regs[0], cpu.regs[1], cpu.regs[2], cpu.regs[3],
               cpu.regs[4], cpu.regs[5], cpu.regs[6], cpu.regs[7]);
        printf("  [Enter=step, q=quit] > ");
        fflush(stdout);

        char line[16];
        if (!fgets(line, sizeof(line), stdin)) break;
        if (line[0] == 'q' || line[0] == 'Q') break;

        cpu_step(&cpu, false);
    }

    printf("\n");
    cpu_dump_regs(&cpu);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 2) { print_usage(argv[0]); return 1; }

    const char *mode     = NULL;
    const char *input    = NULL;
    const char *out_file = NULL;
    bool  trace      = false;
    bool  dump       = false;
    bool  show_regs  = false;
    bool  bus_stats  = false;
    uint16_t dump_addr  = RAM_START;
    uint16_t dump_len   = 0x40;
    uint64_t max_cycles = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--assemble") == 0 ||
            strcmp(argv[i], "--run")      == 0 ||
            strcmp(argv[i], "--step")     == 0) {
            mode = argv[i];
            if (i + 1 < argc) input = argv[++i];
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            out_file = argv[++i];
        } else if (strcmp(argv[i], "--trace") == 0)      { trace      = true; }
        else if (strcmp(argv[i], "--dump") == 0)         { dump       = true; }
        else if (strcmp(argv[i], "--regs") == 0)         { show_regs  = true; }
        else if (strcmp(argv[i], "--bus-stats") == 0)    { bus_stats  = true; }
        else if (strcmp(argv[i], "--dump-addr") == 0 && i + 1 < argc)
            dump_addr = (uint16_t)strtol(argv[++i], NULL, 16);
        else if (strcmp(argv[i], "--dump-len") == 0 && i + 1 < argc)
            dump_len  = (uint16_t)strtol(argv[++i], NULL, 16);
        else if (strcmp(argv[i], "--max-cycles") == 0 && i + 1 < argc)
            max_cycles = (uint64_t)strtoull(argv[++i], NULL, 10);
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
            { print_usage(argv[0]); return 0; }
    }

    if (!mode || !input) { print_usage(argv[0]); return 1; }

    if (strcmp(mode, "--assemble") == 0)
        return cmd_assemble(input, out_file);
    if (strcmp(mode, "--run") == 0)
        return cmd_run(input, trace, dump, show_regs, bus_stats,
                       dump_addr, dump_len, max_cycles);
    if (strcmp(mode, "--step") == 0)
        return cmd_step(input);

    print_usage(argv[0]);
    return 1;
}
