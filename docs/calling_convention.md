# Function Calls & Recursion on SimpleCore

This document walks through `programs/factorial.asm` step by step, showing
exactly how the CPU handles function calls, stack frames, and deep recursion.

---

## 1. The C Program

```c
// Iterative multiply: adds 'a' to itself 'b' times
int multiply(int a, int b) {
    int result = 0;
    while (b > 0) { result += a; b--; }
    return result;
}

// Recursive factorial: calls itself, then calls multiply
int factorial(int n) {
    if (n <= 1) return 1;
    return multiply(n, factorial(n - 1));
}

int main() {
    int r = factorial(5);   // r = 120
    print_uint(r);          // prints "120" to console
}
```

The call graph for `factorial(5)` looks like this:

```
main
 └─ factorial(5)
      └─ factorial(4)
           └─ factorial(3)
                └─ factorial(2)
                     └─ factorial(1)  ← base case, returns 1
                     └─ multiply(2, 1)  → 2
                └─ multiply(3, 2)  → 6
           └─ multiply(4, 6)  → 24
      └─ multiply(5, 24)  → 120
```

---

## 2. Calling Convention (SimpleCore ABI)

| Role              | Register | Notes                                  |
|-------------------|----------|----------------------------------------|
| Argument 1        | R0       | Also the return value                  |
| Argument 2        | R1       |                                        |
| Scratch / clobber | R2–R7    | Caller must save these if needed later |
| Stack pointer     | SP       | Starts at 0xFFFF, grows **downward**   |

**CALL addr**
1. Reads the target address from the word after the instruction.
2. Pushes the return address (the PC of the instruction **after** CALL) onto the stack.
3. Sets PC = target address.

**RET**
1. Pops the return address from the stack.
2. Sets PC = that address.

**PUSH Rx** — writes `Rx` to `mem[SP]`, then SP--.  
**POP  Rx** — SP++, then reads `mem[SP]` into `Rx`.

So the stack always grows down: a PUSH decrements SP after writing, and
a POP increments SP before reading. **SP always points to the next free slot**
(one below the last-written word).

---

## 3. ROM Memory Layout

After assembly the program occupies the ROM area starting at the reset
vector (0x0100). Each word is 16 bits.

```
Address   Words   Contents
────────────────────────────────────────────────────────────────
0x0100      12    main
              0x0100  MOV  R0, #5          ; n = 5
              0x0102  CALL factorial       ; call, ret addr = 0x0104
              0x0104  MOV  R5, R0          ; move result to print arg
              0x0105  CALL print_uint      ; call, ret addr = 0x0107
              0x0107  MOV  R5, #10         ; newline
              0x0109  OUT  #0xFF00, R5     ; emit newline
              0x010B  HLT

0x010C      17    factorial
              0x010C  MOV  R1, #1
              0x010E  CMP  R0, R1
              0x010F  JGT  fact_recurse    ; → 0x0114
              0x0111  MOV  R0, #1          ; base case
              0x0113  RET

              0x0114  PUSH R0              ; fact_recurse: save n
              0x0115  DEC  R0              ; n-1
              0x0116  CALL factorial       ; call, ret addr = 0x0118
              0x0118  MOV  R1, R0          ; R1 = factorial(n-1)
              0x0119  POP  R0              ; R0 = n (restored)
              0x011A  CALL multiply        ; call, ret addr = 0x011C
              0x011C  RET

0x011D      14    multiply
              0x011D  MOV  R2, #0          ; result = 0
              0x011F  MOV  R3, R1          ; counter = b
              0x0120  MOV  R4, #0          ; zero constant

              0x0122  CMP  R3, R4          ; mul_loop: b == 0?
              0x0123  JEQ  mul_done        ; → 0x0129
              0x0125  ADD  R2, R0          ; result += a
              0x0126  DEC  R3              ; b--
              0x0127  JMP  mul_loop        ; → 0x0122

              0x0129  MOV  R0, R2          ; mul_done: return result
              0x012A  RET

0x012B      46    print_uint              (decimal print helper)
  …
0x0158       1    pu_done: RET
────────────────────────────────────────────────────────────────
Total ROM used: 0x0100 – 0x0158  (89 words = 178 bytes)

0x8000  …    RAM  (data, scratch; start of writable region)
0x8100  …    RAM  (print_uint digit buffer, 5 words)
  …
0xFF10  …    Stack region bottom
0xFFFF       Stack top — SP initialised here
```

---

## 4. C Function → Assembly, Side by Side

### 4a. `multiply` (non-recursive)

```
C                               SimpleCore Assembly
─────────────────────────────   ────────────────────────────────────
int multiply(int a, int b) {    multiply:                  ; 0x011D
    int result = 0;                 MOV  R2, #0            ; R2 = 0
                                    MOV  R3, R1            ; R3 = b
                                    MOV  R4, #0            ; R4 = 0 (const)
    while (b > 0) {             mul_loop:
                                    CMP  R3, R4            ; b == 0?
                                    JEQ  mul_done          ; yes → done
        result += a;                ADD  R2, R0            ; R2 += a
        b--;                        DEC  R3                ; R3--
    }                               JMP  mul_loop
    return result;              mul_done:
}                                   MOV  R0, R2            ; R0 = result
                                    RET
```

### 4b. `factorial` (recursive)

```
C                               SimpleCore Assembly
─────────────────────────────   ────────────────────────────────────
int factorial(int n) {          factorial:                 ; 0x010C
    if (n <= 1)                     MOV  R1, #1
        return 1;                   CMP  R0, R1            ; n vs 1
                                    JGT  fact_recurse      ; n > 1?
                                    MOV  R0, #1            ; return 1
                                    RET

    return                      fact_recurse:              ; 0x0114
        multiply(                   PUSH R0                ; save n
            n,                      DEC  R0                ; n-1
            factorial(n-1)          CALL factorial         ; R0=factorial(n-1)
        );                          MOV  R1, R0            ; R1=factorial(n-1)
}                                   POP  R0                ; R0=n (restored)
                                    CALL multiply          ; R0=n*factorial(n-1)
                                    RET
```

---

## 5. The Call Stack — Visualised

The stack grows **downward** from 0xFFFF.  Each row below is one 16-bit word.
The table shows the state at the **deepest point** of recursion — just before
`factorial(1)` executes its base-case `RET`.

```
Address   Value    Pushed by                           What it is
────────────────────────────────────────────────────────────────────────
0xFFFF    0x0104   CALL factorial  (main, 0x0102)      ← return to main
0xFFFE    0x0005   PUSH R0         (factorial n=5)     ← saved  n = 5
0xFFFD    0x0118   CALL factorial  (fact_recurse n=5)  ← return into factorial
0xFFFC    0x0004   PUSH R0         (factorial n=4)     ← saved  n = 4
0xFFFB    0x0118   CALL factorial  (fact_recurse n=4)  ← return into factorial
0xFFFA    0x0003   PUSH R0         (factorial n=3)     ← saved  n = 3
0xFFF9    0x0118   CALL factorial  (fact_recurse n=3)  ← return into factorial
0xFFF8    0x0002   PUSH R0         (factorial n=2)     ← saved  n = 2
0xFFF7    0x0118   CALL factorial  (fact_recurse n=2)  ← return into factorial
          ↑
          SP = 0xFFF6  (next free slot)
────────────────────────────────────────────────────────────────────────

Depth: 5 activation records, 9 words on the stack (9 × 2 bytes = 18 bytes).
```

Each **activation record** for `factorial` is exactly **2 words**:

```
  ┌─────────────────────────────┐ ← higher address
  │  saved n  (PUSH R0)         │
  ├─────────────────────────────┤
  │  return address (CALL)      │
  └─────────────────────────────┘ ← lower address  (SP points here after)
```

Every `multiply` call also pushes 1 word (its return address), but since
`multiply` is a leaf function (it doesn't call anything), those words come
and go without further nesting.

---

## 6. Step-by-Step Execution Trace

Below is the sequence of significant events.  SP values are shown before
each push/pop.

```
Event                         SP before   Action                SP after
──────────────────────────────────────────────────────────────────────────
main: CALL factorial(5)        0xFFFF     push 0x0104           0xFFFE
  factorial(5): PUSH R0=5      0xFFFE     push 5                0xFFFD
  factorial(5): CALL fact(4)   0xFFFD     push 0x0118           0xFFFC
    factorial(4): PUSH R0=4    0xFFFC     push 4                0xFFFB
    factorial(4): CALL fact(3) 0xFFFB     push 0x0118           0xFFFA
      factorial(3): PUSH R0=3  0xFFFA     push 3                0xFFF9
      factorial(3): CALL fact(2) 0xFFF9   push 0x0118           0xFFF8
        factorial(2): PUSH R0=2 0xFFF8    push 2                0xFFF7
        factorial(2): CALL fact(1) 0xFFF7 push 0x0118           0xFFF6
          factorial(1): base case R0=1, RET
        factorial(2) RET@0x0118: SP       pop  → 0x0118         0xFFF7
          MOV R1,R0 → R1=1
          POP R0 → n=2                    pop  → 2              0xFFF8
          CALL multiply(2,1)              push 0x011C           0xFFF7
            multiply: 1 loop → R0=2, RET pop  → 0x011C         0xFFF8
          factorial(2) RET                pop  → 0x0118         0xFFF9
      factorial(3) @0x0118: R0=2
          MOV R1,R0 → R1=2
          POP R0 → n=3                    pop  → 3              0xFFFA
          CALL multiply(3,2)              push 0x011C           0xFFF9
            multiply: 2 loops → R0=6, RET pop  → 0x011C        0xFFFA
          factorial(3) RET                pop  → 0x0118         0xFFFB
    factorial(4) @0x0118: R0=6
        MOV R1,R0 → R1=6
        POP R0 → n=4                      pop  → 4              0xFFFC
        CALL multiply(4,6)                push 0x011C           0xFFFB
          multiply: 6 loops → R0=24, RET  pop  → 0x011C        0xFFFC
        factorial(4) RET                  pop  → 0x0118         0xFFFD
  factorial(5) @0x0118: R0=24
      MOV R1,R0 → R1=24
      POP R0 → n=5                        pop  → 5              0xFFFE
      CALL multiply(5,24)                 push 0x011C           0xFFFD
        multiply: 24 loops → R0=120, RET  pop  → 0x011C        0xFFFE
      factorial(5) RET                    pop  → 0x0104         0xFFFF
main @0x0104: R0=120
  MOV R5, R0; CALL print_uint → prints "120"
  HLT
──────────────────────────────────────────────────────────────────────────
SP returned to 0xFFFF — stack perfectly balanced.
```

---

## 7. How Recursion Works — The Key Insight

Each recursive call to `factorial` follows exactly the same instruction
sequence, but operates on **different data** because it has its own copy
of `n` saved on the stack.

**Winding** (building up the stack):

```
factorial(5) → saves 5, calls factorial(4)
  factorial(4) → saves 4, calls factorial(3)
    factorial(3) → saves 3, calls factorial(2)
      factorial(2) → saves 2, calls factorial(1)
        factorial(1) → base case, returns 1
                        ↑
                        no PUSH, no recursive CALL
```

**Unwinding** (each level consumes its saved value):

```
factorial(1) returns 1
factorial(2) pops 2, calls multiply(2, 1) → returns 2
factorial(3) pops 3, calls multiply(3, 2) → returns 6
factorial(4) pops 4, calls multiply(4, 6) → returns 24
factorial(5) pops 5, calls multiply(5,24) → returns 120
main receives 120
```

Without the `PUSH` / `POP` around the recursive `CALL`, each activation
would overwrite the previous level's `n` in R0.  The stack is the only
reason independent copies of `n` survive while deeper calls are running.

---

## 8. Running the Program

```bat
cd E:\simplecore

REM --- Normal run (prints "120") ---
simplecore.exe --run programs\factorial.asm

REM --- Full instruction trace ---
simplecore.exe --run programs\factorial.asm --trace

REM --- Show register dump after HALT ---
simplecore.exe --run programs\factorial.asm --regs

REM --- Show bus read/write statistics ---
simplecore.exe --run programs\factorial.asm --bus-stats

REM --- Interactive single-step (watch stack pointer change) ---
simplecore.exe --step programs\factorial.asm
```

Expected output (normal run):
```
Assembling 'programs\factorial.asm'...
  -> 89 words assembled, load address 0x0100
Running 'programs\factorial.asm'...

120

Halted after N cycles.
```

With `--regs` you should see SP = 0xFFFF (stack fully unwound) and R0 = 120.
