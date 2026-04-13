# SimpleCore — A Software CPU in C

A complete software implementation of a 16-bit CPU, including:

- **ISA** — Fixed-width 16-bit instruction set with 31 instructions
- **Emulator** — Full Fetch/Decode/Execute pipeline in C99
- **Assembler** — Two-pass assembler with labels, directives, and all addressing modes
- **Memory-Mapped I/O** — Console and hardware timer peripherals
- **Example Programs** — Timer demo, Hello World, and Fibonacci sequence

---

## Architecture Overview

See `docs/schematic.txt` for the full ASCII schematic.

| Property          | Value                                     |
|-------------------|-------------------------------------------|
| Word size         | 16-bit                                    |
| Address space     | 64 KB (word-addressed)                    |
| Registers         | R0–R7, PC, SP, FLAGS                      |
| Instruction width | Fixed 16-bit (+ optional operand word)    |
| Addressing modes  | REG, IMM, MEM (direct), IND (indirect)    |
| Stack             | 0xFF10–0xFFFF (grows downward)            |
| Reset vector      | 0x0100                                    |

---

## Project Structure

```
simplecore/
├── CMakeLists.txt
├── README.md
├── docs/
│   └── schematic.txt         CPU architecture diagram
├── include/
│   ├── isa.h                 Opcodes, modes, flags, memory map
│   ├── cpu.h                 CPU state and control unit interface
│   ├── alu.h                 ALU operations
│   ├── memory.h              Memory subsystem
│   ├── bus.h                 Internal bus
│   ├── io.h                  Memory-mapped I/O
│   └── assembler.h           Two-pass assembler interface
├── src/
│   ├── main.c                CLI entry point
│   ├── cpu.c                 Fetch/Decode/Execute pipeline
│   ├── alu.c                 ALU: ADD SUB MUL DIV AND OR XOR NOT SHL SHR CMP
│   ├── memory.c              Flat 64KB memory with MMIO dispatch
│   ├── bus.c                 Bus abstraction layer
│   ├── io.c                  Console and timer I/O handlers
│   └── assembler.c           Two-pass assembler
└── programs/
    ├── timer.asm             Timer Fetch/Compute/Store demonstration
    ├── hello.asm             Hello, World
    └── fibonacci.asm         Fibonacci sequence (with decimal printing)
```

---

## Building

Requires CMake 3.10+ and a C99 compiler (GCC, Clang, or MSVC).

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

The `simplecore` executable will be in the `build/` directory.

**Windows (MinGW/MSYS2):**
```bash
gcc -std=c99 -Wall -Iinclude src/*.c -o simplecore
```

---

## Usage

### Assemble a source file

```bash
./simplecore --assemble programs/hello.asm
# produces programs/hello.bin
```

### Run a program

```bash
# Run directly from source (assembled on the fly)
./simplecore --run programs/hello.asm

# Run with execution trace (shows each Fetch/Decode/Execute cycle)
./simplecore --run programs/timer.asm --trace

# Run and dump RAM after halt
./simplecore --run programs/fibonacci.asm --dump --dump-addr 8000 --dump-len 10

# Show register state after halt
./simplecore --run programs/hello.asm --regs

# Limit to 1000 cycles
./simplecore --run programs/timer.asm --max-cycles 1000
```

### Interactive step mode

```bash
./simplecore --step programs/hello.asm
# Press Enter to step one instruction, 'q' to quit
```

---

## Instruction Set Summary

| Mnemonic        | Description                               |
|-----------------|-------------------------------------------|
| NOP             | No operation                              |
| MOV Rd, Rs/imm  | Copy register or immediate into Rd        |
| LOAD Rd, [Rs]   | Load from memory indirect                 |
| LOAD Rd, [addr] | Load from memory direct                   |
| STORE [Rs], Rx  | Store to memory indirect                  |
| STORE [addr], Rx| Store to memory direct                    |
| ADD Rd, Rs      | Rd = Rd + Rs                              |
| SUB Rd, Rs      | Rd = Rd - Rs                              |
| MUL Rd, Rs      | Rd = Rd * Rs                              |
| DIV Rd, Rs      | Rd = Rd / Rs                              |
| AND Rd, Rs      | Rd = Rd & Rs                              |
| OR  Rd, Rs      | Rd = Rd \| Rs                             |
| XOR Rd, Rs      | Rd = Rd ^ Rs                              |
| NOT Rd          | Rd = ~Rd                                  |
| SHL Rd, #n      | Rd = Rd << n                              |
| SHR Rd, #n      | Rd = Rd >> n (logical)                    |
| INC Rd          | Rd = Rd + 1                               |
| DEC Rd          | Rd = Rd - 1                               |
| CMP Rd, Rs      | Set FLAGS from (Rd - Rs), discard result  |
| JMP addr        | Unconditional jump                        |
| JEQ addr        | Jump if Z flag set                        |
| JNE addr        | Jump if Z flag clear                      |
| JLT addr        | Jump if N flag set                        |
| JGT addr        | Jump if Z=0 and N=0                       |
| JLE addr        | Jump if Z=1 or N=1                        |
| JGE addr        | Jump if Z=1 or N=0                        |
| CALL addr       | Push PC, jump to addr                     |
| RET             | Pop PC from stack                         |
| PUSH Rs         | Push register onto stack                  |
| POP  Rd         | Pop top of stack into Rd                  |
| IN   Rd, #port  | Read word from I/O port                   |
| OUT  #port, Rs  | Write word to I/O port                    |
| HLT             | Halt the CPU                              |

---

## Addressing Modes

| Mode | Syntax   | Description                               |
|------|----------|-------------------------------------------|
| REG  | Rx       | Value is the register itself              |
| IMM  | #value   | Value is the next word in the instruction |
| MEM  | [addr]   | Value is read from memory[addr]           |
| IND  | [Rx]     | Value is read from memory[Rx]             |

---

## FLAGS Register

| Bit | Flag | Meaning                               |
|-----|------|---------------------------------------|
| 0   | Z    | Result was zero                       |
| 1   | N    | Result was negative (bit 15 set)      |
| 2   | C    | Carry (ADD) or Borrow (SUB/CMP)       |
| 3   | V    | Signed overflow                       |

---

## Memory-Mapped I/O Ports

| Address | Direction | Description                           |
|---------|-----------|---------------------------------------|
| 0xFF00  | Write     | Console: emit low byte as ASCII char  |
| 0xFF01  | Read      | Console: read one byte from stdin     |
| 0xFF02  | R/W       | Timer counter low 16 bits             |
| 0xFF03  | R/W       | Timer counter high 16 bits            |
| 0xFF04  | Write     | Timer control: bit0=run, bit1=reset   |

---

## Fetch / Compute / Store Cycle

The `timer.asm` program makes the three-stage cycle explicit:

```asm
loop:
    IN   R6, #0xFF02    ; FETCH   -- read timer value from MMIO into R6
    INC  R0             ; COMPUTE -- increment software loop counter
    STORE [R1], R0      ; STORE   -- write result back to RAM
```

Run with `--trace` to see each cycle printed:

```
[0100]  IN   R6, #0xFF02          FLAGS=----  SP=FFFF
[0102]  INC  R0                   FLAGS=----  SP=FFFF
[0103]  STORE [R1], R0            FLAGS=----  SP=FFFF
...
```

---

## License

MIT — free to use, modify, and learn from.
