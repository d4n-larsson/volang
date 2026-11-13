# String builtins and conversions.
    .include "defs.inc"

    .section .rodata
r0003: .string "R0003"
r0007: .string "R0007"
r0001: .string "R0001"
m_ord:          .string "`ord` needs exactly one character, got $1."
m_chr:          .string "`chr` needs a Unicode code point, got $1."
m_split_utf8:   .string "Slice boundary $1 splits a UTF-8 character."
m_split_empty:  .string "`split` needs a non-empty separator."
m_to_int:       .string "Cannot convert $1 to int."
m_bad_int:      .string "invalid int: $1"
m_bad_float:    .string "invalid float: $1"
s_true:         .string "true"
s_false:        .string "false"
    .align 3
two63:          .double 9223372036854775808.0

    .text
# void *str_concat(void *a, void *b)
    .globl str_concat
str_concat:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    mv s0, a0
    mv s1, a1
.ifdef GC_STRESS_BUILD
    mv a0, s0
    li a1, OBJ_STR
    call gc_check              # stress build: a live OBJ_STR (spec 003 FR-011)
.endif
.ifdef GC_STRESS_BUILD
    mv a0, s1
    li a1, OBJ_STR
    call gc_check              # stress build: a live OBJ_STR (spec 003 FR-011)
.endif
    ld t0, STR_LEN(s0)
    ld t1, STR_LEN(s1)
    add s2, t0, t1
    li a0, OBJ_STR
    addi a1, s2, 9
    call alloc_obj
    sd s2, STR_LEN(a0)
    sd a0, 0(sp)
    addi a0, a0, STR_DATA
    addi a1, s0, STR_DATA
    ld a2, STR_LEN(s0)
    call mem_copy
    ld a0, 0(sp)
    ld t0, STR_LEN(s0)
    addi a0, a0, STR_DATA
    add a0, a0, t0
    addi a1, s1, STR_DATA
    ld a2, STR_LEN(s1)
    call mem_copy
    ld a0, 0(sp)
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    addi sp, sp, 48
    ret

# int is_cont(void *s, long i)
# 1 when byte i of s is a UTF-8 continuation byte (i < len).
    .globl is_cont
is_cont:
    ld t0, STR_LEN(a0)
    bge a1, t0, 1f
    add t1, a0, a1
    lbu t1, STR_DATA(t1)
    andi t1, t1, 0xC0
    li t2, 0x80
    sub t1, t1, t2
    seqz a0, t1
    ret
1:
    li a0, 0
    ret

# void *rt_slice_str(void *s, long a, long b, void *node)
    .globl rt_slice_str
rt_slice_str:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    mv s1, a1
    mv s2, a2
    mv s3, a3
    ld t0, STR_LEN(s0)
    bltz s1, 8f
    bgt s1, t0, 8f
    blt s2, s1, 9f
    bgt s2, t0, 9f
    mv a0, s0
    mv a1, s1
    call is_cont
    bnez a0, 6f
    mv a0, s0
    mv a1, s2
    call is_cont
    bnez a0, 7f
    addi a0, s0, STR_DATA
    add a0, a0, s1
    sub a1, s2, s1
    call str_new
    j 99f
6:
    mv s2, s1
7:
    mv a0, s2                   # R0007 on the boundary that splits a character
    call int_to_str
    mv a1, a0
    la a0, m_split_utf8
    li a2, 0
    li a3, 0
    call msg
    mv a1, a0
    la a0, r0007
    mv a2, s3
    call rt_raise
8:
    mv a0, s3
    mv a1, s1
    ld a2, STR_LEN(s0)
    call rt_index_error
9:
    mv a0, s3
    mv a1, s2
    ld a2, STR_LEN(s0)
    call rt_index_error
99:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# long str_find_from(void *s, void *sub, long from)
# Byte offset of the first occurrence at or after from, or -1.
    .globl str_find_from
str_find_from:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    sd s4, 0(sp)
    mv s0, a0
    mv s1, a1
    ld s2, STR_LEN(s0)
    ld s4, STR_LEN(s1)
    sub s2, s2, s4              # last start
    mv s3, a2
1:
    bgt s3, s2, 8f
    li t3, 0
2:
    bge t3, s4, 9f
    add t4, s0, s3
    add t4, t4, t3
    lbu t4, STR_DATA(t4)
    add t5, s1, t3
    lbu t5, STR_DATA(t5)
    bne t4, t5, 3f
    addi t3, t3, 1
    j 2b
3:
    addi s3, s3, 1
    addi t3, t3, 1              # one work unit per byte compared
    la t6, rt_budget          # POLL t3 units: --timeout budget
    ld t5, 0(t6)
    sub t5, t5, t3
    sd t5, 0(t6)
    bgtz t5, 1000f
    call rt_poll
1000:
    j 1b
8:
    li a0, -1
    j 99f
9:
    mv a0, s3
99:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    ld s4, 0(sp)
    addi sp, sp, 48
    ret

# long rt_find(void *s, void *sub)
    .globl rt_find
rt_find:
    li a2, 0
    tail str_find_from

# void *rt_split(void *s, void *sep, void *node)
    .globl rt_split
rt_split:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    sd s4, 0(sp)
    mv s0, a0
    mv s1, a1
    ld t0, STR_LEN(s1)
    bnez t0, 1f
    mv a2, a2
    la a0, r0007
    la a1, m_split_empty
    call rt_raise_c
1:
    li a0, 4
    call list_new
    mv s2, a0
    li s3, 0                    # piece start
2:
    la t6, rt_budget          # POLL 1: --timeout budget
    ld t5, 0(t6)
    addi t5, t5, -1
    sd t5, 0(t6)
    bgtz t5, 1000f
    call rt_poll
1000:
    mv a0, s0
    mv a1, s1
    mv a2, s3
    call str_find_from
    bltz a0, 3f
    mv s4, a0
    addi a0, s0, STR_DATA
    add a0, a0, s3
    sub a1, s4, s3
    call str_new
    mv a1, a0
    mv a0, s2
    call list_push
    ld t0, STR_LEN(s1)
    add s3, s4, t0
    j 2b
3:
    addi a0, s0, STR_DATA
    add a0, a0, s3
    ld t0, STR_LEN(s0)
    sub a1, t0, s3
    call str_new
    mv a1, a0
    mv a0, s2
    call list_push
    mv a0, s2
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    ld s4, 0(sp)
    addi sp, sp, 48
    ret

# void *rt_join(void *list, void *sep)
    .globl rt_join
rt_join:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    mv s1, a1
    call sb_new
    mv s2, a0
    li s3, 0
1:
    la t6, rt_budget          # POLL 1: --timeout budget
    ld t5, 0(t6)
    addi t5, t5, -1
    sd t5, 0(t6)
    bgtz t5, 1000f
    call rt_poll
1000:
    ld t0, L_LEN(s0)
    bge s3, t0, 9f
    beqz s3, 2f
    mv a0, s2
    mv a1, s1
    call sb_str
2:
    ld t0, L_DATA(s0)
    slli t1, s3, 3
    add t0, t0, t1
    ld a1, 0(t0)
    mv a0, s2
    call sb_str
    addi s3, s3, 1
    j 1b
9:
    mv a0, s2
    call sb_finish
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# int is_space(int c)
    .globl is_space
is_space:
    li t0, ' '
    beq a0, t0, 1f
    li t0, 9
    blt a0, t0, 2f
    li t0, 13
    ble a0, t0, 1f
2:
    li a0, 0
    ret
1:
    li a0, 1
    ret

# void *rt_trim(void *s)
    .globl rt_trim
rt_trim:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    sd s2, 0(sp)
    mv s0, a0
    li s1, 0
    ld s2, STR_LEN(s0)
1:
    bge s1, s2, 3f
    add t0, s0, s1
    lbu a0, STR_DATA(t0)
    call is_space
    beqz a0, 2f
    addi s1, s1, 1
    j 1b
2:
    ble s2, s1, 3f
    add t0, s0, s2
    lbu a0, STR_DATA-1(t0)
    call is_space
    beqz a0, 3f
    addi s2, s2, -1
    j 2b
3:
    addi a0, s0, STR_DATA
    add a0, a0, s1
    sub a1, s2, s1
    call str_new
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    ld s2, 0(sp)
    addi sp, sp, 32
    ret

# void *rt_chars(void *s)
    .globl rt_chars
rt_chars:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    ld a0, STR_LEN(s0)
    call list_new
    mv s1, a0
    li s2, 0
1:
    ld t0, STR_LEN(s0)
    bge s2, t0, 9f
    addi a0, s0, STR_DATA
    add a0, a0, s2
    sub a1, t0, s2
    call utf8_seq_len
    bnez a0, 2f
    li a0, 1
2:
    mv s3, a0
    addi a0, s0, STR_DATA
    add a0, a0, s2
    mv a1, s3
    call str_new
    mv a1, a0
    mv a0, s1
    call list_push
    add s2, s2, s3
    j 1b
9:
    mv a0, s1
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# void *bool_str(long b)
    .globl bool_str
bool_str:
    la t0, s_false
    beqz a0, 1f
    la t0, s_true
1:
    mv a0, t0
    tail str_from_cstr

# void *rt_parse_int(void *s)
# result[int]: ok(value) or err("invalid int: <text>").
    .globl rt_parse_int
rt_parse_int:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    mv s0, a0
    addi a0, s0, STR_DATA
    ld a1, STR_LEN(s0)
    addi a2, sp, 0
    call dec_to_int
    bnez a0, 1f
    li a0, 0
    ld a1, 0(sp)
    call make_result
    j 9f
1:
    la a0, m_bad_int
    mv a1, s0
    li a2, 0
    li a3, 0
    call msg
    mv a1, a0
    li a0, 1
    call make_result
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    addi sp, sp, 32
    ret

# void *rt_parse_float(void *s)
    .globl rt_parse_float
rt_parse_float:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    mv s0, a0
    addi a0, s0, STR_DATA
    ld a1, STR_LEN(s0)
    addi a2, sp, 0
    call float_parse
    bnez a0, 1f
    li a0, 0
    ld a1, 0(sp)
    call make_result
    j 9f
1:
    la a0, m_bad_float
    mv a1, s0
    li a2, 0
    li a3, 0
    call msg
    mv a1, a0
    li a0, 1
    call make_result
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    addi sp, sp, 32
    ret

# long rt_int_of_float(long bits, void *node)
# Truncates toward zero; R0007 for NaN, infinities and values outside the int range.
    .globl rt_int_of_float
rt_int_of_float:
    fmv.d.x ft0, a0
    feq.d t0, ft0, ft0
    beqz t0, 1f
    la t1, two63
    fld ft1, 0(t1)
    fle.d t0, ft1, ft0          # x >= 2^63
    bnez t0, 1f
    fneg.d ft2, ft1
    flt.d t0, ft0, ft2          # x < -2^63
    bnez t0, 1f
    fcvt.l.d a0, ft0, rtz
    ret
1:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    mv s0, a1
    call float_to_str
    mv a1, a0
    la a0, m_to_int
    li a2, 0
    li a3, 0
    call msg
    mv a1, a0
    la a0, r0007
    mv a2, s0
    call rt_raise
    li a0, 0
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret

# long rt_float_of_int(long i)
    .globl rt_float_of_int
rt_float_of_int:
    fcvt.d.l ft0, a0
    fmv.x.d a0, ft0
    ret

# long rt_abs(long v, int kind, void *node)
    .globl rt_abs
rt_abs:
    li t0, TY_FLOAT
    bne a1, t0, 1f
    slli a0, a0, 1              # clear the sign bit
    srli a0, a0, 1
    ret
1:
    bgez a0, 2f
    li t0, 1
    slli t0, t0, 63
    beq a0, t0, 3f              # abs(MIN) overflows
    neg a0, a0
2:
    ret
3:
    mv a0, a2
    tail rt_overflow

# long rt_minmax(long a, long b, int kind, int want_max)
    .globl rt_minmax
rt_minmax:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    sd s2, 0(sp)
    mv s0, a0
    mv s1, a1
    mv s2, a3
    li t0, TY_FLOAT
    beq a2, t0, 2f
    li t0, TY_STR
    beq a2, t0, 3f
    slt t1, s1, s0              # b < a
    j 4f
2:
    fmv.d.x ft0, s0
    fmv.d.x ft1, s1
    flt.d t1, ft1, ft0
    j 4f
3:
    mv a0, s1
    mv a1, s0
    call str_cmp
    slti t1, a0, 0
4:
    xor t1, t1, s2              # min: take b when b < a; max: take b when not (b < a)
    mv a0, s0
    beqz t1, 9f
    mv a0, s1
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    ld s2, 0(sp)
    addi sp, sp, 32
    ret

# void *rt_case(void *s, int upper)
# ASCII letters to upper (1) or lower (0) case; other bytes unchanged.
    .globl rt_case
rt_case:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s1, a1
    addi a0, a0, STR_DATA
    ld a1, STR_LEN-STR_DATA(a0)
    call str_new
    mv s0, a0
    ld t0, STR_LEN(s0)
    addi t1, s0, STR_DATA
1:
    beqz t0, 9f
    lbu t2, 0(t1)
    beqz s1, 2f
    li t3, 'a'
    blt t2, t3, 3f
    li t3, 'z'
    bgt t2, t3, 3f
    addi t2, t2, -32
    j 3f
2:
    li t3, 'A'
    blt t2, t3, 3f
    li t3, 'Z'
    bgt t2, t3, 3f
    addi t2, t2, 32
3:
    sb t2, 0(t1)
    addi t1, t1, 1
    addi t0, t0, -1
    j 1b
9:
    mv a0, s0
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# long rt_ord(void *s, void *node)
# The code point of a one-character string (R0007 otherwise).
    .globl rt_ord
rt_ord:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    sd s2, 0(sp)
    mv s0, a0
    mv s1, a1
    addi a0, s0, STR_DATA
    ld a1, STR_LEN(s0)
    call utf8_seq_len
    beqz a0, 8f
    ld t0, STR_LEN(s0)
    bne a0, t0, 8f              # more (or less) than one character
    lbu t1, STR_DATA(s0)
    li t2, 1
    beq a0, t2, 5f
    li t2, 2
    andi t3, t1, 0x1f
    beq a0, t2, 2f
    li t2, 3
    andi t3, t1, 0x0f
    beq a0, t2, 2f
    andi t3, t1, 0x07
2:
    li t4, 1                    # add the continuation bytes
3:
    bge t4, a0, 4f
    add t5, s0, t4
    lbu t5, STR_DATA(t5)
    andi t5, t5, 0x3f
    slli t3, t3, 6
    or t3, t3, t5
    addi t4, t4, 1
    j 3b
4:
    mv a0, t3
    j 9f
5:
    mv a0, t1
    j 9f
8:
    mv a0, s0                   # R0007 with the string in vo syntax
    call sb_new
    mv s2, a0
    mv a1, s0
    call render_str_lit
    mv a0, s2
    call sb_finish
    mv a1, a0
    la a0, m_ord
    li a2, 0
    li a3, 0
    call msg
    mv a1, a0
    la a0, r0007
    mv a2, s1
    call rt_raise
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    ld s2, 0(sp)
    addi sp, sp, 32
    ret

# void *rt_chr(long cp, void *node)
# The one-character string for a code point (R0007 for negatives, surrogates, > 0x10FFFF).
    .globl rt_chr
rt_chr:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    mv s1, a1
    bltz s0, 8f
    li t0, 0x10FFFF
    bgt s0, t0, 8f
    li t0, 0xD800
    blt s0, t0, 1f
    li t0, 0xDFFF
    ble s0, t0, 8f
1:
    li t0, 0x80                 # encode into 0(sp)..3(sp)
    bge s0, t0, 2f
    mv t1, s0                   # (via t1: storing s0 itself to the stack reads as a partial save)
    sb t1, 0(sp)
    li a1, 1
    j 7f
2:
    li t0, 0x800
    bge s0, t0, 3f
    srli t1, s0, 6
    ori t1, t1, 0xC0
    sb t1, 0(sp)
    andi t1, s0, 0x3f
    ori t1, t1, 0x80
    sb t1, 1(sp)
    li a1, 2
    j 7f
3:
    li t0, 0x10000
    bge s0, t0, 4f
    srli t1, s0, 12
    ori t1, t1, 0xE0
    sb t1, 0(sp)
    srli t1, s0, 6
    andi t1, t1, 0x3f
    ori t1, t1, 0x80
    sb t1, 1(sp)
    andi t1, s0, 0x3f
    ori t1, t1, 0x80
    sb t1, 2(sp)
    li a1, 3
    j 7f
4:
    srli t1, s0, 18
    ori t1, t1, 0xF0
    sb t1, 0(sp)
    srli t1, s0, 12
    andi t1, t1, 0x3f
    ori t1, t1, 0x80
    sb t1, 1(sp)
    srli t1, s0, 6
    andi t1, t1, 0x3f
    ori t1, t1, 0x80
    sb t1, 2(sp)
    andi t1, s0, 0x3f
    ori t1, t1, 0x80
    sb t1, 3(sp)
    li a1, 4
7:
    mv a0, sp
    call str_new
    j 9f
8:
    mv a0, s0
    call int_to_str
    mv a1, a0
    la a0, m_chr
    li a2, 0
    li a3, 0
    call msg
    mv a1, a0
    la a0, r0007
    mv a2, s1
    call rt_raise
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret
