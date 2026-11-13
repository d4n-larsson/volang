# Memory and string helpers, the growable byte builder, UTF-8, JSON escaping, message templates.
    .include "defs.inc"

    .text
# void mem_copy(char *dst, char *src, long n)
    .globl mem_copy
mem_copy:
    beqz a2, 2f
1:
    lbu t0, 0(a1)
    sb t0, 0(a0)
    addi a0, a0, 1
    addi a1, a1, 1
    addi a2, a2, -1
    bnez a2, 1b
2:
    ret

# int mem_eq(char *a, char *b, long n)
    .globl mem_eq
mem_eq:
    beqz a2, 2f
1:
    lbu t0, 0(a0)
    lbu t1, 0(a1)
    bne t0, t1, 3f
    addi a0, a0, 1
    addi a1, a1, 1
    addi a2, a2, -1
    bnez a2, 1b
2:
    li a0, 1
    ret
3:
    li a0, 0
    ret

# long cstr_len(char *s)
    .globl cstr_len
cstr_len:
    mv t0, a0
1:
    lbu t1, 0(t0)
    beqz t1, 2f
    addi t0, t0, 1
    j 1b
2:
    sub a0, t0, a0
    ret

# void *str_new(char *p, long n)
    .globl str_new
str_new:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    mv s1, a1
    li a0, OBJ_STR
    addi a1, s1, 9              # len field + bytes + NUL (the arena is zeroed)
    call alloc_obj
    sd s1, STR_LEN(a0)
    sd a0, 0(sp)
    addi a0, a0, STR_DATA
    mv a1, s0
    mv a2, s1
    call mem_copy
    ld a0, 0(sp)
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void *str_from_cstr(char *s)
    .globl str_from_cstr
str_from_cstr:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    mv s0, a0
    call cstr_len
    mv a1, a0
    mv a0, s0
    call str_new
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret

# int str_eq(void *a, void *b)
    .globl str_eq
str_eq:
    ld t0, STR_LEN(a0)
    ld t1, STR_LEN(a1)
    bne t0, t1, 1f
    addi a0, a0, STR_DATA
    addi a1, a1, STR_DATA
    mv a2, t0
    tail mem_eq
1:
    li a0, 0
    ret

# int str_eq_bytes(void *s, char *p, long n)
    .globl str_eq_bytes
str_eq_bytes:
    ld t0, STR_LEN(a0)
    bne t0, a2, 1f
    addi a0, a0, STR_DATA
    tail mem_eq
1:
    li a0, 0
    ret

# int str_cmp(void *a, void *b)
# Byte-wise comparison: -1, 0 or 1.
    .globl str_cmp
str_cmp:
    ld t0, STR_LEN(a0)
    ld t1, STR_LEN(a1)
    addi a0, a0, STR_DATA
    addi a1, a1, STR_DATA
    mv t2, t0
    bleu t0, t1, 1f
    mv t2, t1
1:
    beqz t2, 3f
    lbu t3, 0(a0)
    lbu t4, 0(a1)
    bltu t3, t4, 4f
    bltu t4, t3, 5f
    addi a0, a0, 1
    addi a1, a1, 1
    addi t2, t2, -1
    j 1b
3:
    bltu t0, t1, 4f
    bltu t1, t0, 5f
    li a0, 0
    ret
4:
    li a0, -1
    ret
5:
    li a0, 1
    ret

# void *sb_new(void)
    .globl sb_new
sb_new:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    li a0, SB_SIZE
    call alloc
    mv s0, a0
    li a0, 64
    call alloc_bytes
    sd a0, SB_PTR(s0)
    sd zero, SB_LEN(s0)
    li t0, 64
    sd t0, SB_CAP(s0)
    mv a0, s0
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret

# void sb_reserve(void *sb, long n)
# Makes room for n more bytes.
    .globl sb_reserve
sb_reserve:
    ld t0, SB_LEN(a0)
    ld t1, SB_CAP(a0)
    add t2, t0, a1
    bgtu t2, t1, 1f
    ret
1:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    slli s1, t1, 1
    bgeu s1, t2, 2f
    mv s1, t2
2:
    mv a0, s1
    call alloc_bytes
    sd a0, 0(sp)
    ld a1, SB_PTR(s0)
    ld a2, SB_LEN(s0)
    call mem_copy
    ld t0, 0(sp)
    sd t0, SB_PTR(s0)
    sd s1, SB_CAP(s0)
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void sb_byte(void *sb, int b)
    .globl sb_byte
sb_byte:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    mv s1, a1
    li a1, 1
    call sb_reserve
    ld t0, SB_PTR(s0)
    ld t1, SB_LEN(s0)
    add t0, t0, t1
    sb s1, 0(t0)
    addi t1, t1, 1
    sd t1, SB_LEN(s0)
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void sb_bytes(void *sb, char *p, long n)
    .globl sb_bytes
sb_bytes:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    sd s2, 0(sp)
    mv s0, a0
    mv s1, a1
    mv s2, a2
    srli t0, s2, 12             # one work unit per 4 KiB copied
    addi t0, t0, 1
    la t6, rt_budget          # POLL t0 units: --timeout budget
    ld t5, 0(t6)
    sub t5, t5, t0
    sd t5, 0(t6)
    bgtz t5, 1000f
    call rt_poll
1000:
    mv a0, s0
    mv a1, s2
    call sb_reserve
    ld t0, SB_PTR(s0)
    ld t1, SB_LEN(s0)
    add a0, t0, t1
    add t1, t1, s2
    sd t1, SB_LEN(s0)
    mv a1, s1
    mv a2, s2
    call mem_copy
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    ld s2, 0(sp)
    addi sp, sp, 32
    ret

# void sb_cstr(void *sb, char *s)
    .globl sb_cstr
sb_cstr:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    mv s1, a1
    mv a0, a1
    call cstr_len
    mv a2, a0
    mv a0, s0
    mv a1, s1
    call sb_bytes
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void sb_str(void *sb, void *str)
    .globl sb_str
sb_str:
    beqz a1, 1f
    ld a2, STR_LEN(a1)
    addi a1, a1, STR_DATA
    tail sb_bytes
1:
    ret

# void *sb_finish(void *sb)
    .globl sb_finish
sb_finish:
    ld a1, SB_LEN(a0)
    ld a0, SB_PTR(a0)
    tail str_new

# long utf8_seq_len(char *p, long n)
# Length of the valid UTF-8 sequence starting at p (1-4), or 0 if it is invalid.
# ponytail: checks lead and continuation bytes only, not overlong 3/4-byte forms or surrogates
    .globl utf8_seq_len
utf8_seq_len:
    blez a1, 9f
    lbu t0, 0(a0)
    li t1, 0x80
    bltu t0, t1, 1f
    li t1, 0xC2
    bltu t0, t1, 9f
    li t1, 0xE0
    li t2, 2
    bltu t0, t1, 2f
    li t1, 0xF0
    li t2, 3
    bltu t0, t1, 2f
    li t1, 0xF5
    li t2, 4
    bltu t0, t1, 2f
    j 9f
1:
    li a0, 1
    ret
2:
    bgt t2, a1, 9f
    li t3, 1
3:
    bge t3, t2, 4f
    add t4, a0, t3
    lbu t4, 0(t4)
    andi t4, t4, 0xC0
    li t5, 0x80
    bne t4, t5, 9f
    addi t3, t3, 1
    j 3b
4:
    mv a0, t2
    ret
9:
    li a0, 0
    ret

# long utf8_validate(char *p, long n)
# -1 when valid, otherwise the offset of the first invalid byte.
    .globl utf8_validate
utf8_validate:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    sd s2, 0(sp)
    mv s0, a0
    mv s1, a1
    li s2, 0
1:
    bge s2, s1, 2f
    add a0, s0, s2
    sub a1, s1, s2
    call utf8_seq_len
    beqz a0, 3f
    add s2, s2, a0
    j 1b
2:
    li a0, -1
    j 4f
3:
    mv a0, s2
4:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    ld s2, 0(sp)
    addi sp, sp, 32
    ret

# long cp_count(char *p, long n)
    .globl cp_count
cp_count:
    li t0, 0
1:
    blez a1, 2f
    lbu t1, 0(a0)
    andi t1, t1, 0xC0
    li t2, 0x80
    beq t1, t2, 3f
    addi t0, t0, 1
3:
    addi a0, a0, 1
    addi a1, a1, -1
    j 1b
2:
    mv a0, t0
    ret

    .section .rodata
hexdig: .string "0123456789abcdef"
esc_fffd: .string "\\ufffd"
    .text

# void json_escape_into(void *sb, char *p, long n)
    .globl json_escape_into
json_escape_into:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    mv s1, a1
    mv s2, a2
1:
    blez s2, 9f
    lbu s3, 0(s1)
    li t0, 0x80
    bgeu s3, t0, 6f
    li t0, '"'
    beq s3, t0, 2f
    li t0, '\\'
    beq s3, t0, 2f
    li t0, 10
    beq s3, t0, 3f
    li t0, 9
    beq s3, t0, 4f
    li t0, 0x20
    bltu s3, t0, 5f
    mv a0, s0
    mv a1, s3
    call sb_byte
    addi s1, s1, 1
    addi s2, s2, -1
    j 1b
2:
    mv a0, s0
    li a1, '\\'
    call sb_byte
    mv a0, s0
    mv a1, s3
    call sb_byte
    addi s1, s1, 1
    addi s2, s2, -1
    j 1b
3:
    li s3, 'n'
    j 2b
4:
    li s3, 't'
    j 2b
5:
    mv a0, s0
    li a1, '\\'
    call sb_byte
    mv a0, s0
    li a1, 'u'
    call sb_byte
    mv a0, s0
    li a1, '0'
    call sb_byte
    mv a0, s0
    li a1, '0'
    call sb_byte
    la t0, hexdig
    srli t1, s3, 4
    add t1, t0, t1
    lbu a1, 0(t1)
    mv a0, s0
    call sb_byte
    la t0, hexdig
    andi t1, s3, 15
    add t1, t0, t1
    lbu a1, 0(t1)
    mv a0, s0
    call sb_byte
    addi s1, s1, 1
    addi s2, s2, -1
    j 1b
6:
    mv a0, s1
    mv a1, s2
    call utf8_seq_len
    beqz a0, 7f
    mv s3, a0
    mv a0, s0
    mv a1, s1
    mv a2, s3
    call sb_bytes
    add s1, s1, s3
    sub s2, s2, s3
    j 1b
7:
    mv a0, s0
    la a1, esc_fffd
    call sb_cstr
    addi s1, s1, 1
    addi s2, s2, -1
    j 1b
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# void json_str_into(void *sb, void *str)
# Appends str as a quoted, escaped JSON string.
    .globl json_str_into
json_str_into:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    mv s1, a1
    li a1, '"'
    call sb_byte
    beqz s1, 1f
    mv a0, s0
    addi a1, s1, STR_DATA
    ld a2, STR_LEN(s1)
    call json_escape_into
1:
    mv a0, s0
    li a1, '"'
    call sb_byte
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void *msg(char *tmpl, void *a1, void *a2, void *a3)
# Builds a str from tmpl, replacing $1, $2 and $3 with the given str objects.
    .globl msg
msg:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    sd s4, 0(sp)
    mv s1, a0
    mv s2, a1
    mv s3, a2
    mv s4, a3
    call sb_new
    mv s0, a0
1:
    lbu t0, 0(s1)
    beqz t0, 9f
    li t1, '$'
    bne t0, t1, 5f
    lbu t2, 1(s1)
    li t1, '1'
    beq t2, t1, 2f
    li t1, '2'
    beq t2, t1, 3f
    li t1, '3'
    beq t2, t1, 4f
    j 5f
2:
    mv a1, s2
    j 6f
3:
    mv a1, s3
    j 6f
4:
    mv a1, s4
6:
    mv a0, s0
    call sb_str
    addi s1, s1, 2
    j 1b
5:
    mv a0, s0
    lbu a1, 0(s1)
    call sb_byte
    addi s1, s1, 1
    j 1b
9:
    mv a0, s0
    call sb_finish
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    ld s4, 0(sp)
    addi sp, sp, 48
    ret
