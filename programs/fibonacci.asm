; =============================================================================
; fibonacci.asm — Fibonacci Sequence
;
; Computes the first 12 terms: 0, 1, 1, 2, 3, 5, 8, 13, 21, 34, 55, 89
;
; Each term is stored in RAM at 0x8000+ and printed to the console.
;
; Register usage:
;   R0 = F(n-2), R1 = F(n-1), R2 = scratch/F(n)
;   R3 = RAM output pointer, R4 = loop counter
;   R5/R6/R7 = scratch for print_uint subroutine
;
; Scratch buffer for decimal printing: 0x8100-0x8104
;
; Run:
;   simplecore --run programs/fibonacci.asm
;   simplecore --run programs/fibonacci.asm --dump --dump-addr 8000 --dump-len 12
; =============================================================================

    .org 0x0100

    MOV  R0, #0          ; F(n-2) = 0
    MOV  R1, #1          ; F(n-1) = 1
    MOV  R3, #0x8000     ; RAM output pointer
    MOV  R4, #12         ; produce 12 terms

fib_loop:
    STORE [R3], R0       ; store current term to RAM
    INC   R3

    ; Print current term
    PUSH  R0
    PUSH  R1
    PUSH  R4
    MOV   R5, R0
    CALL  print_uint
    MOV   R5, #10        ; newline
    OUT   #0xFF00, R5
    POP   R4
    POP   R1
    POP   R0

    ; Compute next: F(n) = F(n-2) + F(n-1)
    MOV  R2, R0
    ADD  R2, R1          ; R2 = F(n)
    MOV  R0, R1          ; slide: F(n-2) <- F(n-1)
    MOV  R1, R2          ;         F(n-1) <- F(n)

    DEC  R4
    MOV  R6, #0
    CMP  R4, R6
    JNE  fib_loop

    HLT

; =============================================================================
; print_uint — print R5 as an unsigned decimal integer
;
; Uses a buffer at 0x8100-0x8104 (5 words).  Divides by 10 repeatedly,
; stores remainders right-to-left, then prints left-to-right.
;
; Input  : R5 = value to print
; Clobbers: R2, R5, R6, R7
; =============================================================================
print_uint:
    MOV  R6, #0
    CMP  R5, R6
    JNE  pu_nonzero
    MOV  R7, #48
    OUT  #0xFF00, R7
    RET

pu_nonzero:
    MOV  R6, #0x8105     ; write pointer (one past buffer end)

pu_extract:
    MOV  R7, #0          ; quotient = 0

pu_div:
    MOV  R2, #10
    CMP  R5, R2
    JLT  pu_div_done
    SUB  R5, R2
    INC  R7
    JMP  pu_div

pu_div_done:
    ; R5 = digit (0-9), R7 = quotient
    MOV  R2, #48
    ADD  R5, R2          ; convert to ASCII
    DEC  R6
    STORE [R6], R5       ; write digit to buffer
    MOV  R5, R7          ; next dividend = quotient
    MOV  R7, #0
    CMP  R5, R7
    JNE  pu_extract

    MOV  R7, #0x8105     ; end of buffer

pu_print:
    CMP  R6, R7
    JGE  pu_done
    LOAD R5, [R6]
    OUT  #0xFF00, R5
    INC  R6
    JMP  pu_print

pu_done:
    RET
