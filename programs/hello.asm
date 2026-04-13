; =============================================================================
; hello.asm — Hello, World
;
; Demonstrates:
;   - .string directive for embedded null-terminated text
;   - Indirect register addressing [R1] to walk through a string
;   - Conditional jump (JEQ) for null-terminator check
;   - MMIO console output via OUT to PORT_CONSOLE_OUT (0xFF00)
;
; Run:
;   simplecore --run programs/hello.asm
; =============================================================================

    .org 0x0100

    MOV  R1, #msg       ; R1 = address of string

loop:
    LOAD R0, [R1]       ; R0 = character at pointer
    MOV  R2, #0
    CMP  R0, R2         ; null terminator?
    JEQ  done
    OUT  #0xFF00, R0    ; print character
    INC  R1
    JMP  loop

done:
    HLT

msg:
    .string "Hello, World!\n"
