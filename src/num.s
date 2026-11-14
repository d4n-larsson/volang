# Number <-> text conversion (research §3, §11).
    .include "defs.inc"

    .text
# void int_to_dec(void *sb, long v)
    .globl int_to_dec
int_to_dec:
    addi sp, sp, -64
    sd ra, 56(sp)
    sd s0, 48(sp)
    sd s1, 40(sp)
    sd s2, 32(sp)
    mv s0, a0
    mv s1, a1
    bgez s1, 1f
    li a1, '-'
    call sb_byte
    neg s1, s1                  # INT64_MIN stays 2^63 when read as unsigned
1:
    addi s2, sp, 32             # digits are written backwards below 32(sp)
    li t1, 10
2:
    remu t0, s1, t1
    divu s1, s1, t1
    addi t0, t0, '0'
    addi s2, s2, -1
    sb t0, 0(s2)
    bnez s1, 2b
    mv a0, s0
    mv a1, s2
    addi a2, sp, 32
    sub a2, a2, s2
    call sb_bytes
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    addi sp, sp, 64
    ret

# void *int_to_str(long v)
# The decimal text of v: digits on the stack, then one string (spec 004: one allocation, not
# a builder's three).
    .globl int_to_str
int_to_str:
    addi sp, sp, -48            # 0..31: digits, written backwards from 32(sp)
    sd ra, 40(sp)
    sd zero, 0(sp)              # whole words: no stale pointers for the collector
    sd zero, 8(sp)
    sd zero, 16(sp)
    sd zero, 24(sp)
    mv t0, a0
    addi a0, sp, 32
    li t1, 10
    mv t2, t0
    bgez t2, 1f
    neg t2, t2                  # INT64_MIN stays 2^63 when read as unsigned
1:
    remu t3, t2, t1
    divu t2, t2, t1
    addi t3, t3, '0'
    addi a0, a0, -1
    sb t3, 0(a0)
    bnez t2, 1b
    bgez t0, 2f
    li t3, '-'
    addi a0, a0, -1
    sb t3, 0(a0)
2:
    addi a1, sp, 32
    sub a1, a1, a0
    call str_new
    ld ra, 40(sp)
    addi sp, sp, 48
    ret

# int dec_to_int(char *p, long n, long *out)
# 0 = ok, 1 = overflow, 2 = malformed. Accepts an optional leading '-'.
    .globl dec_to_int
dec_to_int:
    li t6, 0                    # negative?
    blez a1, 8f
    lbu t0, 0(a0)
    li t1, '-'
    bne t0, t1, 1f
    li t6, 1
    addi a0, a0, 1
    addi a1, a1, -1
    blez a1, 8f
1:
    li t2, 0                    # magnitude, unsigned
    li t3, 10
2:
    blez a1, 4f
    lbu t0, 0(a0)
    addi t0, t0, -'0'
    bltz t0, 8f
    li t1, 9
    bgt t0, t1, 8f
    mulhu t4, t2, t3
    bnez t4, 7f
    mul t2, t2, t3
    add t5, t2, t0
    bltu t5, t2, 7f
    mv t2, t5
    addi a0, a0, 1
    addi a1, a1, -1
    j 2b
4:
    li t4, 1
    slli t4, t4, 63             # 2^63
    bnez t6, 5f
    bgeu t2, t4, 7f             # positive limit is 2^63 - 1
    sd t2, 0(a2)
    li a0, 0
    ret
5:
    bgtu t2, t4, 7f             # negative limit is 2^63
    neg t2, t2
    sd t2, 0(a2)
    li a0, 0
    ret
7:
    li a0, 1
    ret
8:
    li a0, 2
    ret

    .section .rodata
    .align 3
pow10:
    .double 1e0, 1e1, 1e2, 1e3, 1e4, 1e5, 1e6, 1e7, 1e8, 1e9, 1e10, 1e11
    .double 1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19, 1e20, 1e21, 1e22
s_nan:  .string "nan"
s_inf:  .string "inf"
s_ninf: .string "-inf"

    .text
# long scale10(long bits, long k)
# bits (an f64) times 10^k, applied in exact steps of at most 10^22.
    .globl scale10
scale10:
    fmv.d.x ft0, a0
    la t0, pow10
    fld ft1, 176(t0)            # 1e22
1:
    li t1, 22
    ble a1, t1, 2f
    fmul.d ft0, ft0, ft1
    addi a1, a1, -22
    j 1b
2:
    li t1, -22
    bge a1, t1, 3f
    fdiv.d ft0, ft0, ft1
    addi a1, a1, 22
    j 2b
3:
    bltz a1, 4f
    slli t1, a1, 3
    add t1, t0, t1
    fld ft2, 0(t1)
    fmul.d ft0, ft0, ft2
    j 5f
4:
    neg t2, a1
    slli t1, t2, 3
    add t1, t0, t1
    fld ft2, 0(t1)
    fdiv.d ft0, ft0, ft2
5:
    fmv.x.d a0, ft0
    ret

# long text_to_float(char *p, long n)
# Decimal text ("-"? digits ["." digits]) to f64 bits. Digits up to the last non-zero one form an
# exact mantissa (at most 15 significant digits); trailing zeros only move the power of ten.
    .globl text_to_float
text_to_float:
    li t3, 0                    # negative
    blez a1, 1f
    lbu t4, 0(a0)
    li t5, '-'
    bne t4, t5, 1f
    li t3, 1
    addi a0, a0, 1
    addi a1, a1, -1
1:
    li t6, -1                   # offset of the last non-zero digit
    li t0, 0
2:
    bge t0, a1, 3f
    add t1, a0, t0
    lbu t1, 0(t1)
    addi t0, t0, 1
    li t2, '1'
    blt t1, t2, 2b
    li t2, '9'
    bgt t1, t2, 2b
    addi t6, t0, -1
    j 2b
3:
    li t0, 0                    # mantissa
    li t1, 0                    # power of ten
    li t2, 0                    # seen '.'
    li a2, 0                    # offset
4:
    bge a2, a1, 7f
    add t4, a0, a2
    lbu t4, 0(t4)
    li t5, '.'
    bne t4, t5, 5f
    li t2, 1
    j 6f
5:
    addi t4, t4, -'0'
    bgt a2, t6, 51f             # a trailing zero
    li t5, 10
    mul t0, t0, t5
    add t0, t0, t4
    sub t1, t1, t2              # fraction digits divide by ten
    j 6f
51:
    bnez t2, 6f                 # trailing zeros of the integer part multiply by ten
    addi t1, t1, 1
6:
    addi a2, a2, 1
    j 4b
7:
    fcvt.d.l ft0, t0
    beqz t3, 8f
    fneg.d ft0, ft0
8:
    fmv.x.d a0, ft0
    mv a1, t1
    tail scale10

# int float_parse(char *p, long n, long *out)
# Validates and converts text for parse_float: 0 = ok, 2 = malformed.
# Accepts "-"? digits ["." digits] with at most 15 significant digits.
    .globl float_parse
float_parse:
    mv t0, a0
    mv t1, a1
    blez t1, 9f
    lbu t2, 0(t0)
    li t3, '-'
    bne t2, t3, 1f
    addi t0, t0, 1
    addi t1, t1, -1
    blez t1, 9f
1:
    li t4, 0                    # digits before '.'
    li t5, 0                    # digits after
    li t6, 0                    # '.' seen
    li a3, 0                    # digits since the first non-zero
    li a4, 0                    # non-zero seen
    li a5, 0                    # significant digits (through the last non-zero digit)
2:
    blez t1, 5f
    lbu t2, 0(t0)
    addi t0, t0, 1
    addi t1, t1, -1
    li t3, '.'
    bne t2, t3, 3f
    bnez t6, 9f
    li t6, 1
    j 2b
3:
    addi t2, t2, -'0'
    bltz t2, 9f
    li t3, 9
    bgt t2, t3, 9f
    bnez t6, 31f
    addi t4, t4, 1
    j 32f
31:
    addi t5, t5, 1
32:
    bnez a4, 33f
    beqz t2, 2b
    li a4, 1
33:
    addi a3, a3, 1              # count up to here; trailing zeros are taken back below
    beqz t2, 34f
    mv a5, a3                   # significant digits through the last non-zero one
34:
    j 2b
5:
    beqz t4, 9f
    beqz t6, 6f
    beqz t5, 9f
6:
    li t3, 15
    bgt a5, t3, 9f
    addi sp, sp, -16
    sd ra, 8(sp)
    sd a2, 0(sp)
    call text_to_float
    ld a2, 0(sp)
    sd a0, 0(a2)
    ld ra, 8(sp)
    addi sp, sp, 16
    li a0, 0
    ret
9:
    li a0, 2
    ret

# void float_to_text(void *sb, long bits)
# ponytail: 15 significant digits, not shortest round-trip; upgrade: Ryu port
    .globl float_to_text
float_to_text:
    addi sp, sp, -96            # 0: retry count, 8..22: digits, 48..95: saved registers
    sd ra, 88(sp)
    sd s0, 80(sp)
    sd s1, 72(sp)
    sd s2, 64(sp)
    sd s3, 56(sp)
    sd s4, 48(sp)
    mv s0, a0
    mv s1, a1
    fmv.d.x ft0, s1
    fclass.d t0, ft0
    andi t1, t0, 0x300          # NaN
    bnez t1, 80f
    andi t1, t0, 1              # -inf
    bnez t1, 81f
    andi t1, t0, 0x80           # +inf
    bnez t1, 82f
    li a0, 400
    call alloc
    mv s2, a0                   # output buffer
    mv s3, a0                   # write cursor
    srli t0, s1, 63
    beqz t0, 1f
    li t1, '-'
    sb t1, 0(s3)
    addi s3, s3, 1
1:
    slli s1, s1, 1              # |x|
    srli s1, s1, 1
    bnez s1, 2f
    li t1, '0'                  # zero
    sb t1, 0(s3)
    li t1, '.'
    sb t1, 1(s3)
    li t1, '0'
    sb t1, 2(s3)
    addi s3, s3, 3
    j 70f
2:
    fmv.d.x ft0, s1             # e = floor(log10(|x|)) by repeated scaling
    li t0, 10
    fcvt.d.l ft1, t0
    li t0, 1
    fcvt.d.l ft2, t0
    li s4, 0
3:
    fle.d t0, ft1, ft0
    beqz t0, 4f
    fdiv.d ft0, ft0, ft1
    addi s4, s4, 1
    j 3b
4:
    flt.d t0, ft0, ft2
    beqz t0, 5f
    fmul.d ft0, ft0, ft1
    addi s4, s4, -1
    j 4b
5:
    sd zero, 0(sp)              # correction rounds
51:
    mv a0, s1
    li a1, 14
    sub a1, a1, s4
    call scale10
    fmv.d.x ft0, a0
    fcvt.l.d t0, ft0, rne
    li t1, 100000000000000      # 10^14
    li t2, 1000000000000000     # 10^15
    ld t3, 0(sp)
    li t4, 3
    bge t3, t4, 53f
    addi t3, t3, 1
    sd t3, 0(sp)
    bge t0, t1, 52f
    addi s4, s4, -1
    j 51b
52:
    blt t0, t2, 53f
    addi s4, s4, 1
    j 51b
53:
    addi t5, sp, 0              # 15 digits of t0 into 8(sp)..22(sp)
    addi t5, t5, 23
    li t6, 15
    li t1, 10
54:
    rem t2, t0, t1
    div t0, t0, t1
    addi t2, t2, '0'
    addi t5, t5, -1
    sb t2, 0(t5)
    addi t6, t6, -1
    bnez t6, 54b
    bltz s4, 60f
    li t6, 0                    # e >= 0: integer part is the first e+1 digits (padded with zeros)
55:
    bgt t6, s4, 57f
    li t2, '0'
    li t1, 15
    bge t6, t1, 56f
    add t1, sp, t6
    lbu t2, 8(t1)
56:
    sb t2, 0(s3)
    addi s3, s3, 1
    addi t6, t6, 1
    j 55b
57:
    li t2, '.'
    sb t2, 0(s3)
    addi s3, s3, 1
    mv a2, s3                   # fraction start
    li t1, 15
58:
    bge t6, t1, 59f
    add t2, sp, t6
    lbu t2, 8(t2)
    sb t2, 0(s3)
    addi s3, s3, 1
    addi t6, t6, 1
    j 58b
59:
    bne s3, a2, 65f
    li t2, '0'
    sb t2, 0(s3)
    addi s3, s3, 1
    j 65f
60:
    li t2, '0'                  # e < 0: "0." then -e-1 zeros then the digits
    sb t2, 0(s3)
    li t2, '.'
    sb t2, 1(s3)
    addi s3, s3, 2
    mv a2, s3
    neg t6, s4
    addi t6, t6, -1
61:
    blez t6, 62f
    li t2, '0'
    sb t2, 0(s3)
    addi s3, s3, 1
    addi t6, t6, -1
    j 61b
62:
    li t6, 0
63:
    li t1, 15
    bge t6, t1, 65f
    add t2, sp, t6
    lbu t2, 8(t2)
    sb t2, 0(s3)
    addi s3, s3, 1
    addi t6, t6, 1
    j 63b
65:
    addi t0, a2, 1              # trim trailing zeros, keep one fraction digit
66:
    ble s3, t0, 70f
    lbu t2, -1(s3)
    li t1, '0'
    bne t2, t1, 70f
    addi s3, s3, -1
    j 66b
70:
    mv a0, s0
    mv a1, s2
    sub a2, s3, s2
    call sb_bytes
    j 90f
80:
    la a1, s_nan
    j 83f
81:
    la a1, s_ninf
    j 83f
82:
    la a1, s_inf
83:
    mv a0, s0
    call sb_cstr
90:
    ld ra, 88(sp)
    ld s0, 80(sp)
    ld s1, 72(sp)
    ld s2, 64(sp)
    ld s3, 56(sp)
    ld s4, 48(sp)
    addi sp, sp, 96
    ret

# void *float_to_str(long bits)
    .globl float_to_str
float_to_str:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s1, a0
    call sb_new
    mv s0, a0
    mv a1, s1
    call float_to_text
    mv a0, s0
    call sb_finish
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret
