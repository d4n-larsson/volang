# Runtime primitives of the standard library (spec 007 research R5). They are builtins whose names
# start with `__`, callable only from files under std/ (std_only_ok, checked in check_call).
    .include "defs.inc"

    .text
# long std_only_ok(void *name, void *node)
# 0 when name is a `__` primitive and node is not in a standard-library file; else 1.
    .globl std_only_ok
std_only_ok:
    ld t0, STR_LEN(a0)
    li t1, 2
    blt t0, t1, 1f
    lbu t0, STR_DATA(a0)
    li t1, '_'
    bne t0, t1, 1f
    lbu t0, STR_DATA+1(a0)
    bne t0, t1, 1f
    addi sp, sp, -16
    sd ra, 8(sp)
    lwu a0, N_FILE(a1)
    call file_get
    ld a0, FILE_STD(a0)
    ld ra, 8(sp)
    addi sp, sp, 16
    ret
1:
    li a0, 1
    ret

# ---- exact decimal conversion for std.json (research R5) ----
# The language's own float text (literals, parse_float, str) stays at 15 digits through
# text_to_float/float_to_text. JSON needs 17 digits both ways and exact round trips, so these use
# big integers: fixed 40-limb (2560-bit) little-endian numbers, enough for 2^1074 * 10^324.
    .equ BN_LIMBS, 40
    .equ BN_BYTES, 320

# void bn_set(void *a, long v)
    .globl bn_set
bn_set:
    li t0, BN_LIMBS
    mv t1, a0
1:
    sd zero, 0(t1)
    addi t1, t1, 8
    addi t0, t0, -1
    bnez t0, 1b
    sd a1, 0(a0)
    ret

# void bn_copy(void *dst, void *src)
    .globl bn_copy
bn_copy:
    li t0, BN_LIMBS
1:
    ld t1, 0(a1)
    sd t1, 0(a0)
    addi a0, a0, 8
    addi a1, a1, 8
    addi t0, t0, -1
    bnez t0, 1b
    ret

# void bn_mul_small(void *a, long k)
# a *= k (k below 2^32; the result must fit).
    .globl bn_mul_small
bn_mul_small:
    li t0, BN_LIMBS
    li t1, 0                    # carry
1:
    ld t2, 0(a0)
    mul t3, t2, a1
    mulhu t4, t2, a1
    add t3, t3, t1
    sltu t5, t3, t1
    add t1, t4, t5
    sd t3, 0(a0)
    addi a0, a0, 8
    addi t0, t0, -1
    bnez t0, 1b
    ret

# void bn_shl(void *a, long n)
# a <<= n bits.
    .globl bn_shl
bn_shl:
    srli t0, a1, 6              # whole limbs
    andi t1, a1, 63             # bits
    li t2, 64
    sub t2, t2, t1              # 64 - bits
    li t3, BN_LIMBS - 1         # i, from the top
1:
    bltz t3, 9f
    sub t4, t3, t0              # the source limb
    li t5, 0
    bltz t4, 3f
    slli t6, t4, 3
    add t6, a0, t6
    ld t5, 0(t6)
    sll t5, t5, t1
    beqz t1, 3f
    addi t4, t4, -1             # the bits shifted in from the limb below
    bltz t4, 3f
    ld t6, -8(t6)
    srl t6, t6, t2
    or t5, t5, t6
3:
    slli t6, t3, 3
    add t6, a0, t6
    sd t5, 0(t6)
    addi t3, t3, -1
    j 1b
9:
    ret

# void bn_shr1(void *a)
    .globl bn_shr1
bn_shr1:
    li t0, BN_LIMBS - 1
1:
    ld t1, 0(a0)
    srli t1, t1, 1
    beqz t0, 2f
    ld t2, 8(a0)
    slli t2, t2, 63
    or t1, t1, t2
2:
    sd t1, 0(a0)
    beqz t0, 9f
    addi a0, a0, 8
    addi t0, t0, -1
    j 1b
9:
    ret

# long bn_cmp(void *a, void *b)
# -1, 0 or 1.
    .globl bn_cmp
bn_cmp:
    li t0, BN_LIMBS - 1
1:
    slli t1, t0, 3
    add t2, a0, t1
    ld t2, 0(t2)
    add t3, a1, t1
    ld t3, 0(t3)
    bltu t2, t3, 7f
    bltu t3, t2, 8f
    addi t0, t0, -1
    bgez t0, 1b
    li a0, 0
    ret
7:
    li a0, -1
    ret
8:
    li a0, 1
    ret

# void bn_sub(void *a, void *b)
# a -= b (a >= b).
    .globl bn_sub
bn_sub:
    li t0, BN_LIMBS
    li t1, 0                    # borrow
1:
    ld t2, 0(a0)
    ld t3, 0(a1)
    sltu t4, t2, t3
    sub t2, t2, t3
    sltu t5, t2, t1
    sub t2, t2, t1
    or t1, t4, t5
    sd t2, 0(a0)
    addi a0, a0, 8
    addi a1, a1, 8
    addi t0, t0, -1
    bnez t0, 1b
    ret

# long bn_bitlen(void *a)
# The number of significant bits (0 for zero).
    .globl bn_bitlen
bn_bitlen:
    li t0, BN_LIMBS - 1
1:
    slli t1, t0, 3
    add t1, a0, t1
    ld t1, 0(t1)
    bnez t1, 2f
    addi t0, t0, -1
    bgez t0, 1b
    li a0, 0
    ret
2:
    slli a0, t0, 6
3:
    addi a0, a0, 1
    srli t1, t1, 1
    bnez t1, 3b
    ret

# long bn_pow_mul(void *a, long base, long n)
# a *= base^n (base 5 or 10), in steps that keep the multiplier below 2^32.
    .globl bn_pow_mul
bn_pow_mul:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    sd s2, 0(sp)
    mv s0, a0
    mv s1, a1
    mv s2, a2
1:
    blez s2, 9f
    mv a0, s0
    mv a1, s1
    call bn_mul_small
    addi s2, s2, -1
    j 1b
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    ld s2, 0(sp)
    addi sp, sp, 32
    ret

# long exact_to_double(long w, long q)
# The double nearest to w * 10^q (w >= 0, ties to even), as bits; above the range: infinity.
    .globl exact_to_double
exact_to_double:
    addi sp, sp, -64
    sd ra, 56(sp)
    sd s0, 48(sp)
    sd s1, 40(sp)
    sd s2, 32(sp)
    sd s3, 24(sp)
    sd s4, 16(sp)
    mv s0, a0
    mv s1, a1
    beqz s0, 80f                # zero
    li t0, 310                  # far outside the range: infinity or zero
    bgt s1, t0, 81f
    li t0, -345
    blt s1, t0, 80f
    la a0, bn_n                 # N / D = w * 10^q, scaled by 2^s2
    mv a1, s0
    call bn_set
    la a0, bn_d
    li a1, 1
    call bn_set
    mv s2, s1                   # the power of two: 10^q = 5^q * 2^q
    la a0, bn_n
    bgez s1, 1f
    la a0, bn_d
1:
    li a1, 5
    mv a2, s1
    bgez a2, 2f
    neg a2, a2
2:
    call bn_pow_mul
    la a0, bn_n                 # shift so that N / D has 55 or 56 bits
    call bn_bitlen
    mv s3, a0
    la a0, bn_d
    call bn_bitlen
    addi t0, a0, 55
    sub t0, t0, s3              # s = 55 + bitlen(D) - bitlen(N)
    sub s2, s2, t0
    la a0, bn_n
    mv a1, t0
    bgez t0, 3f
    la a0, bn_d
    neg a1, t0
3:
    call bn_shl
    call bn_div_small_q         # s3 = quotient, s4 = sticky (remainder is not zero)
    mv s3, a0
    la a0, bn_n
    call bn_bitlen
    snez s4, a0
    mv t0, s3                   # the quotient's bit length
    li t1, 0
4:
    beqz t0, 5f
    addi t1, t1, 1
    srli t0, t0, 1
    j 4b
5:
    addi t2, t1, -53            # bits to drop
    add t3, s2, t2              # the exponent of the kept mantissa
    li t4, -1074
    bge t3, t4, 6f
    sub t5, t4, t3              # a subnormal: drop more
    add t2, t2, t5
    mv t3, t4
6:
    bgt t2, t1, 80f             # below half the smallest subnormal
    li t5, 1
    sll t5, t5, t2
    addi t5, t5, -1
    and t6, s3, t5              # the dropped bits
    srl a0, s3, t2              # the mantissa
    addi t2, t2, -1
    li t5, 1
    sll t5, t5, t2              # half
    bltu t6, t5, 7f
    bltu t5, t6, 61f
    bnez s4, 61f                # exactly half: ties to even, unless the remainder says above
    andi t4, a0, 1
    beqz t4, 7f
61:
    addi a0, a0, 1
    li t4, 1
    slli t4, t4, 53
    bne a0, t4, 7f
    srli a0, a0, 1
    addi t3, t3, 1
7:
    li t4, 1                    # encode: a normal number has bit 52 set
    slli t4, t4, 52
    bltu a0, t4, 8f
    addi t5, t3, 1075           # biased exponent
    li t6, 2047
    bge t5, t6, 81f
    sub a0, a0, t4
    slli t5, t5, 52
    or a0, a0, t5
8:
    j 90f
80:
    li a0, 0
    j 90f
81:
    li a0, 2047
    slli a0, a0, 52
90:
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    addi sp, sp, 64
    ret

# long bn_div_small_q(void)
# floor(bn_n / bn_d) when it fits 64 bits; bn_n keeps the remainder.
    .globl bn_div_small_q
bn_div_small_q:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    la a0, bn_n
    call bn_bitlen
    mv s0, a0
    la a0, bn_d
    call bn_bitlen
    sub s0, s0, a0              # the quotient has at most s0 + 1 bits
    la a0, bn_c
    la a1, bn_d
    call bn_copy
    li s1, 0
    bltz s0, 8f
    la a0, bn_c
    mv a1, s0
    call bn_shl
1:
    slli s1, s1, 1
    la a0, bn_n
    la a1, bn_c
    call bn_cmp
    bltz a0, 2f
    la a0, bn_n
    la a1, bn_c
    call bn_sub
    ori s1, s1, 1
2:
    la a0, bn_c
    call bn_shr1
    addi s0, s0, -1
    bgez s0, 1b
8:
    mv a0, s1
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# long exact_digits(long bits, long n, char *out)
# The n significant decimal digits of |x| (x finite, not zero), rounded to nearest with ties to
# even, written to out as ASCII; returns k with |x| about d1.d2...dn * 10^k.
    .globl exact_digits
exact_digits:
    addi sp, sp, -64
    sd ra, 56(sp)
    sd s0, 48(sp)
    sd s1, 40(sp)
    sd s2, 32(sp)
    sd s3, 24(sp)
    sd s4, 16(sp)
    mv s1, a1
    mv s2, a2
    srli t0, a0, 52             # m * 2^e
    andi t0, t0, 2047
    li t1, 1
    slli t1, t1, 52
    addi t2, t1, -1
    and s0, a0, t2              # the fraction
    li s3, -1074
    beqz t0, 1f
    or s0, s0, t1
    addi s3, t0, -1075
1:
    la a0, bn_n
    mv a1, s0
    call bn_set
    la a0, bn_d
    li a1, 1
    call bn_set
    la a0, bn_n
    mv a1, s3
    bgez s3, 2f
    la a0, bn_d
    neg a1, s3
2:
    call bn_shl
    mv t0, s0                   # k: floor(log10(x)) estimated from the bit length, then corrected
    li t1, -1
3:
    beqz t0, 4f
    addi t1, t1, 1
    srli t0, t0, 1
    j 3b
4:
    add t1, t1, s3              # floor(log2(x))
    li t2, 78913                # log10(2) * 2^18
    mul t1, t1, t2
    srai s4, t1, 18
    la a0, bn_d
    mv a2, s4
    bgez s4, 5f
    la a0, bn_n
    neg a2, s4
5:
    li a1, 10
    call bn_pow_mul
6:
    la a0, bn_c                 # N >= 10 D: one more
    la a1, bn_d
    call bn_copy
    la a0, bn_c
    li a1, 10
    call bn_mul_small
    la a0, bn_n
    la a1, bn_c
    call bn_cmp
    bltz a0, 7f
    la a0, bn_d
    li a1, 10
    call bn_mul_small
    addi s4, s4, 1
    j 6b
7:
    la a0, bn_n                 # N < D: one less
    la a1, bn_d
    call bn_cmp
    bgez a0, 8f
    la a0, bn_n
    li a1, 10
    call bn_mul_small
    addi s4, s4, -1
    j 7b
8:
    li s3, 0                    # digit index
9:
    li s0, 0                    # the digit: subtract D while N >= D
10:
    la a0, bn_n
    la a1, bn_d
    call bn_cmp
    bltz a0, 11f
    la a0, bn_n
    la a1, bn_d
    call bn_sub
    addi s0, s0, 1
    j 10b
11:
    add t0, s2, s3
    addi t1, s0, '0'
    sb t1, 0(t0)
    addi s3, s3, 1
    la a0, bn_n
    li a1, 10
    call bn_mul_small
    blt s3, s1, 9b
    la a0, bn_c                 # round: 10 R against 5 D (N is 10 R now)
    la a1, bn_d
    call bn_copy
    la a0, bn_c
    li a1, 5
    call bn_mul_small
    la a0, bn_n
    la a1, bn_c
    call bn_cmp
    bltz a0, 20f
    bgtz a0, 12f
    add t0, s2, s1              # a tie: up when the last digit is odd
    lbu t0, -1(t0)
    andi t0, t0, 1
    beqz t0, 20f
12:
    addi t0, s1, -1             # add one at the last digit
13:
    add t1, s2, t0
    lbu t2, 0(t1)
    li t3, '9'
    bne t2, t3, 14f
    li t3, '0'
    sb t3, 0(t1)
    addi t0, t0, -1
    bgez t0, 13b
    li t3, '1'                  # 99..9 rounded up: 10..0, one more power of ten
    sb t3, 0(s2)
    addi s4, s4, 1
    j 20f
14:
    addi t2, t2, 1
    sb t2, 0(t1)
20:
    mv a0, s4
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    addi sp, sp, 64
    ret

# long std_text_float(void *s)
# __text_float: "-"? digits ["." digits] with at most 19 significant digits, to the nearest
# double (exact_to_double).
    .globl std_text_float
std_text_float:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    addi t0, a0, STR_DATA
    ld t1, STR_LEN(a0)
    add t1, t0, t1              # the end
    li s0, 0                    # negative
    bgeu t0, t1, 1f
    lbu t2, 0(t0)
    li t3, '-'
    bne t2, t3, 1f
    li s0, 1
    addi t0, t0, 1
1:
    mv t2, t0                   # the last non-zero digit
    li t6, 0
2:
    bgeu t2, t1, 3f
    lbu t3, 0(t2)
    addi t2, t2, 1
    li t4, '1'
    bltu t3, t4, 2b
    li t4, '9'
    bgtu t3, t4, 2b
    addi t6, t2, -1
    j 2b
3:
    li a0, 0                    # w
    li a1, 0                    # q
    li t4, 0                    # past '.'
4:
    bgeu t0, t1, 7f
    lbu t3, 0(t0)
    addi t0, t0, 1
    li t5, '.'
    bne t3, t5, 5f
    li t4, 1
    j 4b
5:
    addi t3, t3, -'0'
    beqz t6, 6f                 # no non-zero digit: zero
    addi t2, t0, -1
    bgtu t2, t6, 6f
    li t5, 10
    mul a0, a0, t5
    add a0, a0, t3
    sub a1, a1, t4              # a fraction digit
    j 4b
6:
    bnez t4, 4b                 # trailing zeros: of the integer part, a power of ten
    addi a1, a1, 1
    j 4b
7:
    call exact_to_double
    slli s0, s0, 63
    or a0, a0, s0
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret

# void *std_float_text(long bits)
# __float_text: the shortest of 15, 16 or 17 significant digits that reads back to the same
# double, in plain decimal ("0.000025", "100.0"). NaN and infinities keep float_to_text's text
# (std.json refuses them).
    .globl std_float_text
std_float_text:
    addi sp, sp, -64
    sd ra, 56(sp)
    sd s0, 48(sp)
    sd s1, 40(sp)
    sd s2, 32(sp)
    sd s3, 24(sp)
    sd s4, 16(sp)
    mv s0, a0
    srli t0, s0, 52
    andi t0, t0, 2047
    li t1, 2047
    bne t0, t1, 1f
    call sb_new                 # NaN, infinity
    mv s2, a0
    mv a1, s0
    call float_to_text
    j 80f
1:
    call sb_new
    mv s2, a0
    bgez s0, 2f
    li a1, '-'
    call sb_byte
2:
    slli t0, s0, 1              # zero: 0.0
    bnez t0, 3f
    mv a0, s2
    la a1, s_zero_text
    call sb_cstr
    j 80f
3:
    li s1, 15                   # the digit count
4:
    mv a0, s0
    mv a1, s1
    la a2, digit_buf
    call exact_digits
    mv s3, a0                   # k
    li t0, 17
    beq s1, t0, 6f
    la t0, digit_buf            # w = the digits, q = k - n + 1: same bits?
    li a0, 0
    li t1, 0
5:
    add t2, t0, t1
    lbu t2, 0(t2)
    addi t2, t2, -'0'
    li t3, 10
    mul a0, a0, t3
    add a0, a0, t2
    addi t1, t1, 1
    blt t1, s1, 5b
    sub a1, s3, s1
    addi a1, a1, 1
    call exact_to_double
    slli t0, s0, 1
    srli t0, t0, 1
    beq a0, t0, 6f
    addi s1, s1, 1
    j 4b
6:
    la t0, digit_buf            # drop trailing zero digits
7:
    li t1, 1
    ble s1, t1, 8f
    add t1, t0, s1
    lbu t1, -1(t1)
    li t2, '0'
    bne t1, t2, 8f
    addi s1, s1, -1
    j 7b
8:
    mv a0, s2                   # plain decimal: digit i has the power k - i
    mv a1, s1
    mv a2, s3
    call float_digits_out
80:
    mv a0, s2
    call sb_finish
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    addi sp, sp, 64
    ret

# void float_digits_out(void *sb, long n, long k)
# Appends digit_buf's n digits d1..dn as d1.d2...dn * 10^k in plain decimal, with at least one
# digit after the point.
    .globl float_digits_out
float_digits_out:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    mv s1, a1
    mv s2, a2
    bgez s2, 3f
    mv a0, s0                   # k < 0: "0." then -k-1 zeros, then the digits
    li a1, '0'
    call sb_byte
    mv a0, s0
    li a1, '.'
    call sb_byte
    li s3, -1
1:
    ble s3, s2, 2f
    mv a0, s0
    li a1, '0'
    call sb_byte
    addi s3, s3, -1
    j 1b
2:
    li s3, 0
21:
    bge s3, s1, 9f
    la t0, digit_buf
    add t0, t0, s3
    lbu a1, 0(t0)
    mv a0, s0
    call sb_byte
    addi s3, s3, 1
    j 21b
3:
    li s3, 0                    # k >= 0: digits (or zeros) up to the point, then the rest or 0
4:
    bgt s3, s2, 6f
    li a1, '0'
    bge s3, s1, 5f
    la t0, digit_buf
    add t0, t0, s3
    lbu a1, 0(t0)
5:
    mv a0, s0
    call sb_byte
    addi s3, s3, 1
    j 4b
6:
    mv a0, s0
    li a1, '.'
    call sb_byte
    blt s3, s1, 7f
    mv a0, s0
    li a1, '0'
    call sb_byte
    j 9f
7:
    bge s3, s1, 9f
    la t0, digit_buf
    add t0, t0, s3
    lbu a1, 0(t0)
    mv a0, s0
    call sb_byte
    addi s3, s3, 1
    j 7b
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

    .section .rodata
s_zero_text: .string "0.0"

    .bss
    .align 3
bn_n:      .space BN_BYTES
bn_d:      .space BN_BYTES
bn_c:      .space BN_BYTES
digit_buf: .space 24
