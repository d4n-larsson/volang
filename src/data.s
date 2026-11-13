# Embedded documents (the single source of truth for `vo reference`, `vo explain` and code names).
    .include "defs.inc"

    .section .rodata
    .globl ref_md
ref_md:
    .incbin "docs/reference.md"
ref_md_end:
    .globl errors_md
errors_md:
    .incbin "docs/errors.md"
errors_md_end:
    .byte 0
    .align 3
    .globl ref_md_len
ref_md_len:     .dword ref_md_end - ref_md
    .globl errors_md_len
errors_md_len:  .dword errors_md_end - errors_md

    .text
# char *catalog_find(char *code)
# Start of the "## CODE " heading line in errors.md, or 0. code is 5 characters.
    .globl catalog_find
catalog_find:
    la t0, errors_md
    la t1, errors_md_end
1:
    addi t2, t0, 9
    bgt t2, t1, 8f
    lbu t3, 0(t0)
    li t4, 35
    bne t3, t4, 6f
    lbu t3, 1(t0)
    bne t3, t4, 6f
    lbu t3, 2(t0)
    li t4, ' '
    bne t3, t4, 6f
    lbu t3, 8(t0)
    bne t3, t4, 6f
    li t5, 0                    # compare 5 code characters
2:
    add t3, t0, t5
    lbu t3, 3(t3)
    add t4, a0, t5
    lbu t4, 0(t4)
    bne t3, t4, 6f
    addi t5, t5, 1
    li t6, 5
    blt t5, t6, 2b
    mv a0, t0
    ret
6:
    lbu t3, 0(t0)               # skip to the next line
    addi t0, t0, 1
    li t4, 10
    beq t3, t4, 1b
    blt t0, t1, 6b
8:
    li a0, 0
    ret

# void *catalog_name(char *code)
# The code's snake_case name as a str, or 0 when the code is unknown.
    .globl catalog_name
catalog_name:
    addi sp, sp, -16
    sd ra, 8(sp)
    call catalog_find
    beqz a0, 9f
    addi a0, a0, 9
    mv t0, a0
1:
    lbu t1, 0(t0)
    li t2, 10
    beq t1, t2, 3f
    beqz t1, 3f
    addi t0, t0, 1
    j 1b
3:
    sub a1, t0, a0
    call str_new
9:
    ld ra, 8(sp)
    addi sp, sp, 16
    ret

# void *catalog_section(char *code)
# The whole "## CODE ..." section up to the next "## " heading, as a str; 0 if unknown.
    .globl catalog_section
catalog_section:
    addi sp, sp, -16
    sd ra, 8(sp)
    call catalog_find
    beqz a0, 9f
    la t1, errors_md_end
    addi t0, a0, 1
1:
    bge t0, t1, 3f
    lbu t2, -1(t0)
    li t3, 10
    bne t2, t3, 2f
    lbu t2, 0(t0)
    li t3, 35
    bne t2, t3, 2f
    lbu t2, 1(t0)
    bne t2, t3, 2f
    lbu t2, 2(t0)
    li t3, ' '
    beq t2, t3, 3f
2:
    addi t0, t0, 1
    j 1b
3:
    sub a1, t0, a0
    call str_new
9:
    ld ra, 8(sp)
    addi sp, sp, 16
    ret

# int catalog_fixable(char *code)
# 1 when the code's section says "Fixable: yes" or "Fixable: sometimes" (a fix may be offered).
    .globl catalog_fixable
catalog_fixable:
    addi sp, sp, -16
    sd ra, 8(sp)
    call catalog_section
    beqz a0, 8f
    ld t1, STR_LEN(a0)
    addi a0, a0, STR_DATA
    add t1, a0, t1
1:
    addi t2, a0, 13
    bgt t2, t1, 8f
    lbu t3, 0(a0)
    li t4, 'F'
    bne t3, t4, 2f
    lbu t3, 7(a0)
    li t4, ':'
    bne t3, t4, 2f
    lbu t3, 9(a0)
    li t4, 'n'                  # yes or sometimes: a fix may be offered
    beq t3, t4, 8f
    li a0, 1
    j 9f
2:
    addi a0, a0, 1
    j 1b
8:
    li a0, 0
9:
    ld ra, 8(sp)
    addi sp, sp, 16
    ret
