; =============================================================================
; timer.asm — Timer Example
;
; Demonstrates the Fetch / Compute / Store execution cycle using the
; memory-mapped timer peripheral.
;
; Each loop iteration:
;   FETCH   — IN   R6, #0xFF02    read current timer value from MMIO
;   COMPUTE — INC  R0             increment the software loop counter
;   STORE   — STORE [R1], R0      save counter to RAM
;
; Run with trace to see each stage:
;   simplecore --run programs/timer.asm --trace --dump --dump-addr 8000 --dump-len 10
; =============================================================================

    .org 0x0100

    MOV  R0, #0         ; loop counter
    MOV  R1, #0x8000    ; RAM output pointer

    ; Start the hardware timer
    MOV  R7, #1
    OUT  #0xFF04, R7

loop:
    ; ---- FETCH: read timer value from MMIO ----
    IN   R6, #0xFF02

    ; ---- COMPUTE: increment loop counter ----
    INC  R0

    ; Print counter as ASCII digit
    MOV  R5, #48
    ADD  R5, R0
    OUT  #0xFF00, R5
    MOV  R5, #10
    OUT  #0xFF00, R5

    ; ---- STORE: write counter to RAM ----
    STORE [R1], R0
    INC  R1

    MOV  R4, #10
    CMP  R0, R4
    JNE  loop

    ; Stop the timer
    MOV  R7, #0
    OUT  #0xFF04, R7

    ; Store final timer value
    IN   R6, #0xFF02
    STORE [R1], R6

    HLT
