; =============================================================================
; factorial.asm  —  Recursive Factorial on SimpleCore 16-bit CPU
; =============================================================================
;
; MEMORY MAP:
;   0x0100 – 0x011F   main  (entry point, loop N=0..10)
;   0x0120 – 0x013F   factorial(N)  recursive subroutine
;   0x0140 – 0x016F   multiply(A,B) repeated-addition helper
;   0x8000 – 0x800A   RAM results: mem[0x8000 + N] = N!
;
; CALLING CONVENTION:
;   R0  — argument (N), also return value (N!)
;   R1  — second argument to multiply (B)
;   R2  — main loop counter  [saved on stack inside factorial]
;   R3  — main RAM pointer   [saved on stack inside factorial]
;   SP  — hardware stack, grows downward from 0xFFFF
;
; STACK FRAME inside factorial(N) for N > 1:
;   after CALL:   [SP]   = return address  (auto-pushed by CALL)
;   after PUSH R2:[SP-1] = saved R2
;   after PUSH R3:[SP-2] = saved R3
;   after PUSH R0:[SP-3] = saved N
;   on return: POP R0, POP R3, POP R2, RET
; =============================================================================

; ─────────────────────────────────────────────────────────────────────────────
; MAIN
; ─────────────────────────────────────────────────────────────────────────────
        .org 0x0100

main:
        MOV  R2, #0             ; R2 = N, starts at 0
        MOV  R3, #0x8000        ; R3 = RAM pointer for results

loop:
        MOV  R0, R2             ; argument = N
        CALL factorial          ; R0 = N!

        STORE [R3], R0          ; mem[R3] = N!
        INC  R3                 ; advance RAM pointer

        INC  R2                 ; N++
        MOV  R5, #11            ; upper limit
        CMP  R2, R5             ; N < 11?
        JLT  loop               ; yes — keep going

done:
        ; load and print 10! from RAM
        MOV  R6, #0x8009        ; address of result[9] = 9! (0-indexed)
        LOAD R7, [R6]           ; R7 = 9!
        OUT  #0xFF00, R7        ; print to console
        HLT

; ─────────────────────────────────────────────────────────────────────────────
; FACTORIAL  —  recursive
;
; INPUT  : R0 = N
; OUTPUT : R0 = N!
;
;   if N <= 1  →  return 1
;   else       →  return N * factorial(N-1)
; ─────────────────────────────────────────────────────────────────────────────
        .org 0x0120

factorial:
        ; base case: N <= 1 → return 1
        MOV  R1, #1
        CMP  R0, R1             ; N compared to 1
        JGT  fact_recurse       ; N > 1 → recurse

        MOV  R0, #1             ; return value = 1
        RET

fact_recurse:
        ; save registers that the recursive call or multiply will clobber
        PUSH R2                 ; save main loop counter
        PUSH R3                 ; save main RAM pointer
        PUSH R0                 ; save N  (needed after recursive call returns)

        ; recursive call: factorial(N-1)
        DEC  R0                 ; R0 = N - 1
        CALL factorial          ; R0 = factorial(N-1)

        ; multiply:  N * factorial(N-1)
        MOV  R1, R0             ; R1 = factorial(N-1)
        POP  R0                 ; restore N into R0
        POP  R3                 ; restore R3
        POP  R2                 ; restore R2

        CALL multiply           ; R0 = N * factorial(N-1)
        RET

; ─────────────────────────────────────────────────────────────────────────────
; MULTIPLY  —  repeated addition
;
; INPUT  : R0 = A,  R1 = B
; OUTPUT : R0 = A * B
;
; Saves and restores R2, R3 on stack.
; ─────────────────────────────────────────────────────────────────────────────
        .org 0x0140

multiply:
        PUSH R2                 ; save R2
        PUSH R3                 ; save R3

        MOV  R2, #0             ; accumulator = 0
        MOV  R3, R1             ; counter = B

        MOV  R4, #0
        CMP  R3, R4
        JEQ  mul_done           ; B == 0 → result is 0

mul_loop:
        ADD  R2, R0             ; accumulator += A
        DEC  R3                 ; counter--
        MOV  R4, #0
        CMP  R3, R4
        JNE  mul_loop           ; repeat while counter > 0

mul_done:
        MOV  R0, R2             ; return result in R0
        POP  R3
        POP  R2
        RET

; =============================================================================
; END
; =============================================================================
