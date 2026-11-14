# Property tests (spec 005 US2, research R6-R10): which types can be generated, the generator,
# deep copies, type-directed shrinking and the property runner.
# Generated values live in the collected heap. They stay alive through the conservative roots:
# the s registers and native stack of these functions, and pr_args/pr_best/pr_cand in .bss.
# ponytail: a fixed 100 cases / 1,000 attempts per property; an option if real properties need more.
# ponytail: at most 2,000 shrink runs per property; raise it if counterexamples come out unshrunk.
# ponytail: greedy shrinking (first candidate that still fails); locally minimal, not globally.
    .include "defs.inc"

    .equ PR_CASES, 100
    .equ PR_ATTEMPTS, 1000
    .equ PR_BUDGET, 2000
    .equ PR_DEPTH, 6
    # pr_try outcomes
    .equ O_PASS, 0
    .equ O_DISCARD, 1
    .equ O_FAIL, 2
    .equ O_ERROR, 3
    .equ O_TIMEOUT, 4
    .equ O_OOM, 5
    # test statuses (testrun.s)
    .equ ST_PASS, 0
    .equ ST_FAIL, 1
    .equ ST_ERROR, 2
    .equ ST_TIMEOUT, 3

    .section .bss
    .align 3
pg_state:   .space 8                # the generator (SplitMix64), separate from random_int's
    .globl pr_args
pr_args:    .space 8                # list: the arguments being generated or run
    .globl pr_best
pr_best:    .space 8                # list: the smallest failing arguments so far
    .globl pr_cand
pr_cand:    .space 8                # the candidate list of the parameter being shrunk
    .globl pr_cases
pr_cases:   .space 8                # accepted cases run (including the failing one)
    .globl pr_discarded
pr_discarded: .space 8
    .globl pr_shrinks
pr_shrinks: .space 8
    .globl pr_stopped
pr_stopped: .space 8                # 1 when the time limit cut shrinking short
    .globl pr_starved
pr_starved: .space 8                # 1 when too many inputs were discarded
pr_kind:    .space 8                # the last failure: the expect node, or the runtime code

    .section .rodata
    .globl assume_code
assume_code: .string "ASSUME"       # the internal discard raised by assume(false); never printed
code_r0006: .string "R0006"
code_r0009: .string "R0009"
alphabet:   .string "abcdeabcde0123456789 ,;:-_#"
    .equ ALPHABET_N, 27
mb_e:       .string "\303\251"              # é
mb_ss:      .string "\303\237"              # ß
mb_ri:      .string "\346\227\245"          # 日
mb_smile:   .string "\360\237\230\200"      # 😀
s_a:        .string "a"
s_empty:    .string ""
    .align 3
multibyte:  .dword mb_e, mb_ss, mb_ri, mb_smile
int_edges:  .dword 0, 1, -1, 0x7fffffffffffffff, 0x8000000000000000, 0x7ffffffffffffffe
            .dword 0x8000000000000001
float_edges: .dword 0, 0x8000000000000000, 0x3ff0000000000000, 0xbff0000000000000
            .dword 0x3fe0000000000000, 0x430c6bf526340000, 0xc30c6bf526340000

    .text

# ---- the generator ----

# long pr_next(void)
# The next 64 bits (SplitMix64).
pr_next:
    la t0, pg_state
    ld a0, 0(t0)
    li t1, 0x9e3779b97f4a7c15
    add a0, a0, t1
    sd a0, 0(t0)
    srli t1, a0, 30
    xor a0, a0, t1
    li t1, 0xbf58476d1ce4e5b9
    mul a0, a0, t1
    srli t1, a0, 27
    xor a0, a0, t1
    li t1, 0x94d049bb133111eb
    mul a0, a0, t1
    srli t1, a0, 31
    xor a0, a0, t1
    ret

# long pr_below(long n)
# Uniform in 0..n-1 (n > 0).
pr_below:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    mv s0, a0
    call pr_next
    remu a0, a0, s0
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret

# void prop_seed(long seed, long item, long case_no)
# Case case_no of the property at item index item: any case can be regenerated alone.
    .globl prop_seed
prop_seed:
    slli a1, a1, 32
    xor a0, a0, a1
    xor a0, a0, a2
    la t0, pg_state
    sd a0, 0(t0)
    ret

# ---- types ----

# long pg_nmembers(void *type)
# The number of fields of a record type or cases of a variant type.
pg_nmembers:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    ld t0, TY_DECL(a0)
    ld s0, N_B(t0)
    li s1, 0
1:
    mv a0, s0
    mv a1, s1
    call member_at
    beqz a0, 9f
    addi s1, s1, 1
    j 1b
9:
    mv a0, s1
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void *pg_member(void *type, long i)
# Field i's type (records) or case i's payload type, 0 when it has none (variants).
pg_member:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    mv s0, a0
    ld t0, TY_KIND(a0)
    li t1, TY_RECORD
    beq t0, t1, 1f
    call case_payload
    j 9f
1:
    ld t0, TY_DECL(s0)
    ld a0, N_B(t0)
    call member_at
    ld t0, N_B(a0)
    ld a1, N_TYPE(t0)
    mv a0, s0
    call field_type_in
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    addi sp, sp, 32
    ret

# int pg_on_stack(void *stack, void *type)
pg_on_stack:
    ld t0, VEC_N(a0)
    ld t1, VEC_DATA(a0)
1:
    beqz t0, 2f
    ld t2, 0(t1)
    beq t2, a1, 3f
    addi t1, t1, 8
    addi t0, t0, -1
    j 1b
2:
    li a0, 0
    ret
3:
    li a0, 1
    ret

# int pg_find(void *type, void *target, void *stack)
# target 0: 1 when type contains a function type or a type parameter. Otherwise 1 when type
# contains target. Each record or variant is entered once along a path.
pg_find:
    addi sp, sp, -64
    sd ra, 56(sp)
    sd s0, 48(sp)
    sd s1, 40(sp)
    sd s2, 32(sp)
    sd s3, 24(sp)
    sd s4, 16(sp)
    mv s0, a0
    mv s1, a1
    mv s2, a2
    li a0, 0
    beqz s0, 9f
    li a0, 1
    beq s0, s1, 9f
    ld t0, TY_KIND(s0)
    li t1, TY_FUNC
    beq t0, t1, 1f
    li t1, TY_PARAM
    beq t0, t1, 1f
    li t1, TY_LIST
    beq t0, t1, 2f
    li t1, TY_RESULT
    beq t0, t1, 2f
    li t1, TY_MAP
    beq t0, t1, 3f
    li t1, TY_RECORD
    beq t0, t1, 4f
    li t1, TY_VARIANT
    beq t0, t1, 4f
    li a0, 0
    j 9f
1:
    seqz a0, s1                 # functions matter only when looking for them
    j 9f
2:
    ld a0, TY_A(s0)
    mv a1, s1
    mv a2, s2
    call pg_find
    j 9f
3:
    ld a0, TY_A(s0)
    mv a1, s1
    mv a2, s2
    call pg_find
    bnez a0, 9f
    ld a0, TY_B(s0)
    mv a1, s1
    mv a2, s2
    call pg_find
    j 9f
4:
    mv a0, s2
    mv a1, s0
    call pg_on_stack
    bnez a0, 8f
    mv a0, s2
    mv a1, s0
    call vec_push
    mv a0, s0
    call pg_nmembers
    mv s4, a0
    li s3, 0
5:
    li a0, 0
    bge s3, s4, 6f
    mv a0, s0
    mv a1, s3
    call pg_member
    addi s3, s3, 1
    beqz a0, 5b
    mv a1, s1
    mv a2, s2
    call pg_find
    beqz a0, 5b
6:
    ld t0, VEC_N(s2)            # pop
    addi t0, t0, -1
    sd t0, VEC_N(s2)
    j 9f
8:
    li a0, 0
9:
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    addi sp, sp, 64
    ret

# int pg_finite(void *type, void *stack)
# 1 when a finite value of type exists (least fixpoint: a type already being entered counts as
# having none along that path).
pg_finite:
    addi sp, sp, -64
    sd ra, 56(sp)
    sd s0, 48(sp)
    sd s1, 40(sp)
    sd s2, 32(sp)
    sd s3, 24(sp)
    sd s4, 16(sp)
    mv s0, a0
    mv s1, a1
    li a0, 1
    ld t0, TY_KIND(s0)
    li t1, TY_RECORD
    beq t0, t1, 1f
    li t1, TY_VARIANT
    bne t0, t1, 9f
1:
    mv a0, s1
    mv a1, s0
    call pg_on_stack
    bnez a0, 8f
    mv a0, s1
    mv a1, s0
    call vec_push
    mv a0, s0
    call pg_nmembers
    mv s4, a0
    li s3, 0
    ld t0, TY_KIND(s0)
    li t1, TY_RECORD
    li s2, 1                    # s2 = 1: a record (every field finite), 0: a variant (one case)
    beq t0, t1, 2f
    li s2, 0
2:
    mv a0, s2                   # no member left: a record is finite, a variant is not
    bge s3, s4, 6f
    mv a0, s0
    mv a1, s3
    call pg_member
    addi s3, s3, 1
    beqz a0, 3f                 # a case without a payload
    mv a1, s1
    call pg_finite
    bnez s2, 4f
    beqz a0, 2b
    j 6f                        # variant: a finite case
3:
    li a0, 1
    j 6f
4:
    bnez a0, 2b                 # record: every field must be finite
6:
    ld t0, VEC_N(s1)            # pop
    addi t0, t0, -1
    sd t0, VEC_N(s1)
    j 9f
8:
    li a0, 0
9:
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    addi sp, sp, 64
    ret

# int prop_generatable(void *type)
# 0 when values of type can be generated; 1 when it contains a function type (or a type
# parameter); 2 when it has no finite value (research R6).
    .globl prop_generatable
prop_generatable:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    li a0, 0
    beqz s0, 9f
    ld t0, TY_KIND(s0)
    li t1, TY_ERROR
    beq t0, t1, 9f
    call vec_new
    mv s1, a0
    mv a0, s0
    li a1, 0
    mv a2, s1
    call pg_find
    bnez a0, 9f
    mv a0, s0
    mv a1, s1
    call pg_finite
    li t0, 1                    # finite: 0, not: 2
    sub a0, t0, a0
    slli a0, a0, 1
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# ---- generation (research R7) ----

# void *pg_str(long size)
# A string of length 0..min(size/4 + 1, 32): mostly a-e, digits and separators, 1 in 16 multi-byte.
pg_str:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    srli a0, a0, 2
    addi a0, a0, 1
    li t0, 32
    ble a0, t0, 1f
    li a0, 32
1:
    addi a0, a0, 1
    call pr_below
    mv s1, a0
    call sb_new
    mv s0, a0
2:
    beqz s1, 9f
    addi s1, s1, -1
    li a0, 16
    call pr_below
    bnez a0, 3f
    li a0, 4
    call pr_below
    la t0, multibyte
    slli a0, a0, 3
    add t0, t0, a0
    ld a1, 0(t0)
    mv a0, s0
    call sb_cstr
    j 2b
3:
    li a0, ALPHABET_N
    call pr_below
    la t0, alphabet
    add t0, t0, a0
    lbu a1, 0(t0)
    mv a0, s0
    call sb_byte
    j 2b
9:
    mv a0, s0
    call sb_finish
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# long pg_count(long size)
# A collection length: uniform in 0..min(size/8 + 1, 16).
pg_count:
    addi sp, sp, -16
    sd ra, 8(sp)
    srli a0, a0, 3
    addi a0, a0, 1
    li t0, 16
    ble a0, t0, 1f
    li a0, 16
1:
    addi a0, a0, 1
    call pr_below
    ld ra, 8(sp)
    addi sp, sp, 16
    ret

# long pg_small_cases(void *type, long pick)
# Variants at the depth limit: the pick-th case without a payload or with a payload that does not
# contain the variant again; returns the case index, or -1 - (the number of such cases) when
# pick is past them.
pg_small_cases:
    addi sp, sp, -64
    sd ra, 56(sp)
    sd s0, 48(sp)
    sd s1, 40(sp)
    sd s2, 32(sp)
    sd s3, 24(sp)
    sd s4, 16(sp)
    mv s0, a0
    mv s1, a1
    call pg_nmembers
    mv s4, a0
    li s2, 0                    # case
    li s3, 0                    # small cases seen
1:
    bge s2, s4, 8f
    mv a0, s0
    mv a1, s2
    call pg_member
    beqz a0, 2f
    sd a0, 0(sp)
    call vec_new
    mv a2, a0
    ld a0, 0(sp)
    mv a1, s0
    call pg_find
    bnez a0, 3f
2:
    mv a0, s2
    beq s3, s1, 9f
    addi s3, s3, 1
3:
    addi s2, s2, 1
    j 1b
8:
    li a0, -1
    sub a0, a0, s3
9:
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    addi sp, sp, 64
    ret

# long prop_gen(void *type, long size, long depth)
# A new value of type (research R7). depth bounds variant nesting.
    .globl prop_gen
prop_gen:
    addi sp, sp, -64
    sd ra, 56(sp)
    sd s0, 48(sp)
    sd s1, 40(sp)
    sd s2, 32(sp)
    sd s3, 24(sp)
    sd s4, 16(sp)
    sd s5, 8(sp)
    sd zero, 0(sp)
    mv s0, a0
    mv s1, a1
    mv s2, a2
    bgez s1, 1f
    li s1, 0
1:
    ld t0, TY_KIND(s0)
    li t1, TY_INT
    beq t0, t1, 10f
    li t1, TY_FLOAT
    beq t0, t1, 20f
    li t1, TY_BOOL
    beq t0, t1, 30f
    li t1, TY_STR
    beq t0, t1, 40f
    li t1, TY_LIST
    beq t0, t1, 50f
    li t1, TY_MAP
    beq t0, t1, 60f
    li t1, TY_RESULT
    beq t0, t1, 70f
    li t1, TY_RECORD
    beq t0, t1, 80f
    li t1, TY_VARIANT
    beq t0, t1, 90f
    li a0, 0
    j 99f
10:                             # int: 1 in 8 an edge, else -size..size
    li a0, 8
    call pr_below
    bnez a0, 11f
    li a0, 7
    call pr_below
    la t0, int_edges
    slli a0, a0, 3
    add t0, t0, a0
    ld a0, 0(t0)
    j 99f
11:
    slli a0, s1, 1
    addi a0, a0, 1
    call pr_below
    sub a0, a0, s1
    j 99f
20:                             # float: 1 in 8 an edge, else (-size..size) / 2^(0..6)
    li a0, 8
    call pr_below
    bnez a0, 21f
    li a0, 7
    call pr_below
    la t0, float_edges
    slli a0, a0, 3
    add t0, t0, a0
    ld a0, 0(t0)
    j 99f
21:
    slli a0, s1, 1
    addi a0, a0, 1
    call pr_below
    sub s3, a0, s1
    li a0, 7
    call pr_below
    li t0, 1
    sll t0, t0, a0
    fcvt.d.l ft0, s3
    fcvt.d.l ft1, t0
    fdiv.d ft0, ft0, ft1
    fmv.x.d a0, ft0
    j 99f
30:
    li a0, 2
    call pr_below
    j 99f
40:
    mv a0, s1
    call pg_str
    j 99f
50:                             # list[T]
    mv a0, s1
    call pg_count
    mv s4, a0
    call list_new
    mv s3, a0
51:
    beqz s4, 52f
    addi s4, s4, -1
    ld a0, TY_A(s0)
    srli a1, s1, 1
    mv a2, s2
    call prop_gen
    mv a1, a0
    mv a0, s3
    call list_push
    j 51b
52:
    mv a0, s3
    j 99f
60:                             # map[K, V]: duplicate keys collapse
    mv a0, s1
    call pg_count
    mv s4, a0
    call map_new
    mv s3, a0
61:
    beqz s4, 52b
    addi s4, s4, -1
    ld a0, TY_A(s0)
    srli a1, s1, 1
    mv a2, s2
    call prop_gen
    mv s5, a0
    ld a0, TY_B(s0)
    srli a1, s1, 1
    mv a2, s2
    call prop_gen
    mv a2, a0
    mv a0, s3
    mv a1, s5
    ld a3, TY_A(s0)
    call map_set
    j 61b
70:                             # result[T]: 1 in 4 err
    li a0, 4
    call pr_below
    bnez a0, 71f
    mv a0, s1
    call pg_str
    mv a1, a0
    li a0, 1
    call make_result
    j 99f
71:
    li s3, 0
    ld a0, TY_A(s0)
    ld t0, TY_KIND(a0)
    li t1, TY_VOID
    beq t0, t1, 72f
    mv a1, s1
    mv a2, s2
    call prop_gen
    mv s3, a0
72:
    li a0, 0
    mv a1, s3
    call make_result
    j 99f
80:                             # record: each field with size - 1
    mv a0, s0
    call pg_nmembers
    mv s4, a0
    li a0, OBJ_RECORD
    slli a1, s4, 3
    call alloc_obj
    mv s3, a0
    li s5, 0
81:
    bge s5, s4, 82f
    slli t0, s5, 3
    add t0, s3, t0
    sd zero, R_FIELDS(t0)
    addi s5, s5, 1
    j 81b
82:
    li s5, 0
83:
    bge s5, s4, 52b
    mv a0, s0
    mv a1, s5
    call pg_member
    addi a1, s1, -1
    mv a2, s2
    call prop_gen
    slli t0, s5, 3
    add t0, s3, t0
    sd a0, R_FIELDS(t0)
    addi s5, s5, 1
    j 83b
90:                             # variant: past the depth budget only the small cases
    bgtz s2, 92f
    mv a0, s0
    li a1, -1                   # count them
    call pg_small_cases
    li t0, -1
    sub a0, t0, a0
    beqz a0, 92f                # none: any case
    call pr_below
    mv a1, a0
    mv a0, s0
    call pg_small_cases
    mv s4, a0
    j 93f
92:
    mv a0, s0
    call pg_nmembers
    call pr_below
    mv s4, a0
93:
    li s3, 0
    mv a0, s0
    mv a1, s4
    call pg_member
    beqz a0, 94f
    srli a1, s1, 1
    addi a2, s2, -1
    call prop_gen
    mv s3, a0
94:
    li a0, OBJ_VARIANT
    li a1, VARIANT_SIZE
    call alloc_obj
    sw s4, V_TAG(a0)
    sd s3, V_PAY(a0)
99:
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    ld s5, 8(sp)
    addi sp, sp, 64
    ret

# long prop_copy(long value, void *type)
# A deep copy: lists, maps, records, variants and results are new; strings are shared.
    .globl prop_copy
prop_copy:
    addi sp, sp, -64
    sd ra, 56(sp)
    sd s0, 48(sp)
    sd s1, 40(sp)
    sd s2, 32(sp)
    sd s3, 24(sp)
    sd s4, 16(sp)
    sd s5, 8(sp)
    sd zero, 0(sp)
    mv s0, a0
    mv s1, a1
    ld t0, TY_KIND(s1)
    li t1, TY_LIST
    beq t0, t1, 10f
    li t1, TY_MAP
    beq t0, t1, 20f
    li t1, TY_RESULT
    beq t0, t1, 30f
    li t1, TY_RECORD
    beq t0, t1, 40f
    li t1, TY_VARIANT
    beq t0, t1, 50f
    mv a0, s0
    j 99f
10:
    ld a0, L_LEN(s0)
    call list_new
    mv s2, a0
    li s3, 0
11:
    ld t0, L_LEN(s0)
    bge s3, t0, 12f
    ld t0, L_DATA(s0)
    slli t1, s3, 3
    add t0, t0, t1
    ld a0, 0(t0)
    ld a1, TY_A(s1)
    call prop_copy
    mv a1, a0
    mv a0, s2
    call list_push
    addi s3, s3, 1
    j 11b
12:
    mv a0, s2
    j 99f
20:
    call map_new
    mv s2, a0
    li s3, 0
21:
    ld t0, M_USED(s0)
    bge s3, t0, 12b
    li t1, ME_SIZE
    mul t1, s3, t1
    ld t0, M_ENTRIES(s0)
    add s4, t0, t1
    addi s3, s3, 1
    lwu t0, ME_LIVE(s4)
    beqz t0, 21b
    ld a0, ME_VAL(s4)
    ld a1, TY_B(s1)
    call prop_copy
    mv a2, a0
    mv a0, s2
    ld a1, ME_KEY(s4)
    ld a3, TY_A(s1)
    call map_set
    j 21b
30:
    lwu s3, V_TAG(s0)
    ld s2, V_PAY(s0)
    bnez s3, 31f                # err: the str is shared
    ld a1, TY_A(s1)
    ld t0, TY_KIND(a1)
    li t1, TY_VOID
    beq t0, t1, 31f
    mv a0, s2
    call prop_copy
    mv s2, a0
31:
    mv a0, s3
    mv a1, s2
    call make_result
    j 99f
40:
    mv a0, s1
    call pg_nmembers
    mv s4, a0
    li a0, OBJ_RECORD
    slli a1, s4, 3
    call alloc_obj
    mv s2, a0
    li s3, 0
41:
    bge s3, s4, 12b
    slli t0, s3, 3
    add t1, s2, t0
    add t0, s0, t0
    ld t0, R_FIELDS(t0)
    sd t0, R_FIELDS(t1)         # until the copy replaces it: the collector sees no garbage
    mv a0, s1
    mv a1, s3
    call pg_member
    mv a1, a0
    slli t0, s3, 3
    add t0, s0, t0
    ld a0, R_FIELDS(t0)
    call prop_copy
    slli t0, s3, 3
    add t0, s2, t0
    sd a0, R_FIELDS(t0)
    addi s3, s3, 1
    j 41b
50:
    lwu s3, V_TAG(s0)
    ld s2, V_PAY(s0)
    mv a0, s1
    mv a1, s3
    call pg_member
    beqz a0, 51f
    mv a1, a0
    mv a0, s2
    call prop_copy
    mv s2, a0
51:
    li a0, OBJ_VARIANT
    li a1, VARIANT_SIZE
    call alloc_obj
    sw s3, V_TAG(a0)
    sd s2, V_PAY(a0)
99:
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    ld s5, 8(sp)
    addi sp, sp, 64
    ret

# ---- shrinking (research R8) ----

# void *pg_list_with(void *list, long i, long value)
# A copy of list with element i replaced by value.
pg_list_with:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    sd s4, 0(sp)
    mv s0, a0
    mv s1, a1
    mv s2, a2
    ld a0, L_LEN(s0)
    call list_new
    mv s3, a0
    li s4, 0
1:
    ld t0, L_LEN(s0)
    bge s4, t0, 9f
    ld t0, L_DATA(s0)
    slli t1, s4, 3
    add t0, t0, t1
    ld a1, 0(t0)
    bne s4, s1, 2f
    mv a1, s2
2:
    mv a0, s3
    call list_push
    addi s4, s4, 1
    j 1b
9:
    mv a0, s3
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    ld s4, 0(sp)
    addi sp, sp, 48
    ret

# void *pg_list_without(void *list, long i)
# A copy of list without element i.
pg_list_without:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s3, 8(sp)
    sd s4, 0(sp)
    mv s0, a0
    mv s1, a1
    ld a0, L_LEN(s0)
    call list_new
    mv s3, a0
    li s4, 0
1:
    ld t0, L_LEN(s0)
    bge s4, t0, 9f
    beq s4, s1, 2f
    ld t0, L_DATA(s0)
    slli t1, s4, 3
    add t0, t0, t1
    ld a1, 0(t0)
    mv a0, s3
    call list_push
2:
    addi s4, s4, 1
    j 1b
9:
    mv a0, s3
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s3, 8(sp)
    ld s4, 0(sp)
    addi sp, sp, 48
    ret

# void *pg_map_edit(void *map, long entry, long value, void *map_type)
# A copy of map where entry index `entry` gets value `value`, or, when entry is
# (1 << 62 | index), where that entry is dropped.
pg_map_edit:
    addi sp, sp, -64
    sd ra, 56(sp)
    sd s0, 48(sp)
    sd s1, 40(sp)
    sd s2, 32(sp)
    sd s3, 24(sp)
    sd s4, 16(sp)
    sd s5, 8(sp)
    mv s0, a0
    mv s1, a1
    mv s2, a2
    mv s3, a3
    call map_new
    mv s5, a0
    li s4, 0
1:
    ld t0, M_USED(s0)
    bge s4, t0, 9f
    li t1, ME_SIZE
    mul t1, s4, t1
    ld t0, M_ENTRIES(s0)
    add t0, t0, t1
    lwu t1, ME_LIVE(t0)
    beqz t1, 3f
    ld a1, ME_KEY(t0)
    ld a2, ME_VAL(t0)
    li t1, 1
    slli t1, t1, 62
    or t1, t1, s4
    beq t1, s1, 3f              # dropped
    bne s4, s1, 2f
    mv a2, s2                   # replaced
2:
    mv a0, s5
    ld a3, TY_A(s3)
    call map_set
3:
    addi s4, s4, 1
    j 1b
9:
    mv a0, s5
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    ld s5, 8(sp)
    addi sp, sp, 64
    ret

# void *pg_new_variant(long tag, long payload)
pg_new_variant:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    mv s1, a1
    li a0, OBJ_VARIANT
    li a1, VARIANT_SIZE
    call alloc_obj
    sw s0, V_TAG(a0)
    sd s1, V_PAY(a0)
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void *pg_seq(void *list, void *elem_type, int chars)
# Candidates for a list (or, with chars, a string as its list of characters): empty, the two
# halves, each element removed (from the end), then each element shrunk in place (chars: replaced
# by "a").
pg_seq:
    addi sp, sp, -80
    sd ra, 72(sp)
    sd s0, 64(sp)
    sd s1, 56(sp)
    sd s2, 48(sp)
    sd s3, 40(sp)
    sd s4, 32(sp)
    sd s5, 24(sp)
    sd s6, 16(sp)
    sd zero, 8(sp)
    sd zero, 0(sp)
    mv s0, a0
    mv s1, a1
    mv s6, a2
    call vec_new
    mv s2, a0
    ld s3, L_LEN(s0)
    beqz s3, 9f
    li a0, 0
    call list_new
    mv a1, a0
    mv a0, s2
    call vec_push
    li t0, 2
    blt s3, t0, 4f
    mv a0, s0                   # halves
    li a1, 0
    srli a2, s3, 1
    li a3, 0
    call list_slice
    mv a1, a0
    mv a0, s2
    call vec_push
    mv a0, s0
    srli a1, s3, 1
    mv a2, s3
    li a3, 0
    call list_slice
    mv a1, a0
    mv a0, s2
    call vec_push
    addi s4, s3, -1             # each element removed, from the end
2:
    bltz s4, 4f
    mv a0, s0
    mv a1, s4
    call pg_list_without
    mv a1, a0
    mv a0, s2
    call vec_push
    addi s4, s4, -1
    j 2b
4:
    li s4, 0                    # each element shrunk in place
5:
    bge s4, s3, 9f
    ld t0, L_DATA(s0)
    slli t1, s4, 3
    add t0, t0, t1
    ld s5, 0(t0)
    beqz s6, 6f
    ld t0, STR_LEN(s5)          # a character: "a" unless it is "a"
    li t1, 1
    bne t0, t1, 51f
    lbu t0, STR_DATA(s5)
    li t1, 'a'
    beq t0, t1, 8f
51:
    la a0, s_a
    call str_from_cstr
    mv a2, a0
    mv a0, s0
    mv a1, s4
    call pg_list_with
    mv a1, a0
    mv a0, s2
    call vec_push
    j 8f
6:
    mv a0, s5
    mv a1, s1
    call prop_shrinks
    sd a0, 8(sp)
    li s5, 0
7:
    ld t0, 8(sp)
    ld t1, VEC_N(t0)
    bge s5, t1, 8f
    mv a0, t0
    mv a1, s5
    call vec_get
    mv a2, a0
    mv a0, s0
    mv a1, s4
    call pg_list_with
    mv a1, a0
    mv a0, s2
    call vec_push
    addi s5, s5, 1
    j 7b
8:
    addi s4, s4, 1
    j 5b
9:
    mv a0, s2
    ld ra, 72(sp)
    ld s0, 64(sp)
    ld s1, 56(sp)
    ld s2, 48(sp)
    ld s3, 40(sp)
    ld s4, 32(sp)
    ld s5, 24(sp)
    ld s6, 16(sp)
    addi sp, sp, 80
    ret

# void *prop_shrinks(long value, void *type)
# The shrink candidates of value in research R8's order: a vec of new values (value itself is
# never changed).
    .globl prop_shrinks
prop_shrinks:
    addi sp, sp, -80
    sd ra, 72(sp)
    sd s0, 64(sp)
    sd s1, 56(sp)
    sd s2, 48(sp)
    sd s3, 40(sp)
    sd s4, 32(sp)
    sd s5, 24(sp)
    sd s6, 16(sp)
    sd zero, 8(sp)
    sd zero, 0(sp)
    mv s0, a0
    mv s1, a1
    call vec_new
    mv s2, a0
    ld t0, TY_KIND(s1)
    li t1, TY_INT
    beq t0, t1, 10f
    li t1, TY_FLOAT
    beq t0, t1, 20f
    li t1, TY_BOOL
    beq t0, t1, 30f
    li t1, TY_STR
    beq t0, t1, 40f
    li t1, TY_LIST
    beq t0, t1, 50f
    li t1, TY_MAP
    beq t0, t1, 60f
    li t1, TY_RESULT
    beq t0, t1, 70f
    li t1, TY_RECORD
    beq t0, t1, 80f
    li t1, TY_VARIANT
    beq t0, t1, 90f
    j 99f
10:                             # int: 0, x/2, x - sign(x)
    beqz s0, 99f
    mv a0, s2
    li a1, 0
    call vec_push
    li t0, 2
    div s3, s0, t0
    beqz s3, 11f
    mv a0, s2
    mv a1, s3
    call vec_push
11:
    addi s4, s0, -1
    bgtz s0, 12f
    addi s4, s0, 1
12:
    beqz s4, 99f
    beq s4, s3, 99f
    mv a0, s2
    mv a1, s4
    call vec_push
    j 99f
20:                             # float: 0.0, the truncated value, x/2
    fmv.d.x ft0, s0
    fmv.d.x ft1, zero
    feq.d t0, ft0, ft1
    bnez t0, 99f
    mv a0, s2
    li a1, 0
    call vec_push
    fmv.d.x ft0, s0
    fcvt.l.d s3, ft0, rtz
    fcvt.d.l ft1, s3
    feq.d t0, ft0, ft1
    li s4, 0
    bnez t0, 21f
    beqz s3, 21f
    fmv.x.d s4, ft1
    mv a0, s2
    mv a1, s4
    call vec_push
21:
    fmv.d.x ft0, s0
    li t0, 0x3fe0000000000000
    fmv.d.x ft1, t0
    fmul.d ft0, ft0, ft1
    fmv.x.d s3, ft0
    fmv.d.x ft1, zero
    feq.d t0, ft0, ft1
    bnez t0, 99f
    beq s3, s4, 99f
    mv a0, s2
    mv a1, s3
    call vec_push
    j 99f
30:
    beqz s0, 99f
    mv a0, s2
    li a1, 0
    call vec_push
    j 99f
40:                             # str: as its list of characters, joined back
    mv a0, s0
    call rt_chars
    li a1, 0
    li a2, 1
    call pg_seq
    mv s3, a0
    la a0, s_empty
    call str_from_cstr
    mv s5, a0
    li s4, 0
41:
    ld t0, VEC_N(s3)
    bge s4, t0, 99f
    mv a0, s3
    mv a1, s4
    call vec_get
    mv a1, s5
    call rt_join
    mv a1, a0
    mv a0, s2
    call vec_push
    addi s4, s4, 1
    j 41b
50:
    mv a0, s0
    ld a1, TY_A(s1)
    li a2, 0
    call pg_seq
    mv s2, a0
    j 99f
60:                             # map: empty, each entry removed, each value shrunk
    ld t0, M_COUNT(s0)
    beqz t0, 99f
    call map_new
    mv a1, a0
    mv a0, s2
    call vec_push
    li s4, 0
61:
    ld t0, M_USED(s0)
    bge s4, t0, 63f
    li t1, ME_SIZE
    mul t1, s4, t1
    ld t0, M_ENTRIES(s0)
    add t0, t0, t1
    lwu t1, ME_LIVE(t0)
    beqz t1, 62f
    li a1, 1
    slli a1, a1, 62
    or a1, a1, s4
    mv a0, s0
    li a2, 0
    mv a3, s1
    call pg_map_edit
    mv a1, a0
    mv a0, s2
    call vec_push
62:
    addi s4, s4, 1
    j 61b
63:
    li s4, 0
64:
    ld t0, M_USED(s0)
    bge s4, t0, 99f
    li t1, ME_SIZE
    mul t1, s4, t1
    ld t0, M_ENTRIES(s0)
    add t0, t0, t1
    lwu t1, ME_LIVE(t0)
    beqz t1, 67f
    ld a0, ME_VAL(t0)
    ld a1, TY_B(s1)
    call prop_shrinks
    mv s3, a0
    li s5, 0
65:
    ld t0, VEC_N(s3)
    bge s5, t0, 67f
    mv a0, s3
    mv a1, s5
    call vec_get
    mv a2, a0
    mv a0, s0
    mv a1, s4
    mv a3, s1
    call pg_map_edit
    mv a1, a0
    mv a0, s2
    call vec_push
    addi s5, s5, 1
    j 65b
67:
    addi s4, s4, 1
    j 64b
70:                             # result: the payload shrunk, in the same case
    lwu s4, V_TAG(s0)
    la s5, t_str                # err carries a str
    ld s5, 0(s5)
    bnez s4, 71f
    ld s5, TY_A(s1)
    ld t0, TY_KIND(s5)
    li t1, TY_VOID
    beq t0, t1, 99f
71:
    ld a0, V_PAY(s0)
    mv a1, s5
    call prop_shrinks
    mv s3, a0
    li s5, 0
72:
    ld t0, VEC_N(s3)
    bge s5, t0, 99f
    mv a0, s3
    mv a1, s5
    call vec_get
    mv a1, a0
    mv a0, s4
    call make_result
    mv a1, a0
    mv a0, s2
    call vec_push
    addi s5, s5, 1
    j 72b
80:                             # record: each field shrunk in place
    mv a0, s1
    call pg_nmembers
    mv s6, a0
    li s4, 0
81:
    bge s4, s6, 99f
    mv a0, s1
    mv a1, s4
    call pg_member
    mv a1, a0
    slli t0, s4, 3
    add t0, s0, t0
    ld a0, R_FIELDS(t0)
    call prop_shrinks
    mv s3, a0
    li s5, 0
82:
    ld t0, VEC_N(s3)
    bge s5, t0, 84f
    li a0, OBJ_RECORD
    slli a1, s6, 3
    call alloc_obj
    sd a0, 8(sp)
    li t0, 0                    # a copy of the record ...
83:
    bge t0, s6, 831f
    slli t1, t0, 3
    add t2, s0, t1
    ld t2, R_FIELDS(t2)
    add t3, a0, t1
    sd t2, R_FIELDS(t3)
    addi t0, t0, 1
    j 83b
831:
    mv a0, s3                   # ... with field s4 replaced
    mv a1, s5
    call vec_get
    ld t0, 8(sp)
    slli t1, s4, 3
    add t1, t0, t1
    sd a0, R_FIELDS(t1)
    mv a0, s2
    mv a1, t0
    call vec_push
    addi s5, s5, 1
    j 82b
84:
    addi s4, s4, 1
    j 81b
90:                             # variant: earlier cases without a payload, subtrees, the payload
    lwu s4, V_TAG(s0)
    li s5, 0
91:
    bge s5, s4, 92f
    mv a0, s1
    mv a1, s5
    call pg_member
    bnez a0, 911f
    mv a0, s5
    li a1, 0
    call pg_new_variant
    mv a1, a0
    mv a0, s2
    call vec_push
911:
    addi s5, s5, 1
    j 91b
92:
    ld t0, V_PAY(s0)
    beqz t0, 99f
    mv a0, s1
    mv a1, s4
    call pg_member
    mv s6, a0                   # payload type
    bne s6, s1, 93f
    mv a0, s2                   # the payload is itself a value of this variant
    ld a1, V_PAY(s0)
    call vec_push
    j 95f
93:
    ld t0, TY_KIND(s6)
    li t1, TY_RECORD
    bne t0, t1, 95f
    mv a0, s6                   # fields of the payload record that are values of this variant
    call pg_nmembers
    sd a0, 0(sp)
    li s5, 0
94:
    ld t0, 0(sp)
    bge s5, t0, 95f
    mv a0, s6
    mv a1, s5
    call pg_member
    addi s5, s5, 1
    bne a0, s1, 94b
    ld t0, V_PAY(s0)
    slli t1, s5, 3
    add t0, t0, t1
    ld a1, R_FIELDS-8(t0)
    mv a0, s2
    call vec_push
    j 94b
95:
    ld a0, V_PAY(s0)
    mv a1, s6
    call prop_shrinks
    mv s3, a0
    li s5, 0
96:
    ld t0, VEC_N(s3)
    bge s5, t0, 99f
    mv a0, s3
    mv a1, s5
    call vec_get
    mv a1, a0
    mv a0, s4
    call pg_new_variant
    mv a1, a0
    mv a0, s2
    call vec_push
    addi s5, s5, 1
    j 96b
99:
    mv a0, s2
    ld ra, 72(sp)
    ld s0, 64(sp)
    ld s1, 56(sp)
    ld s2, 48(sp)
    ld s3, 40(sp)
    ld s4, 32(sp)
    ld s5, 24(sp)
    ld s6, 16(sp)
    addi sp, sp, 80
    ret

# ---- running a property (research R9, R10) ----

# int pr_streq(char *a, char *b)
pr_streq:
1:
    lbu t0, 0(a0)
    lbu t1, 0(a1)
    bne t0, t1, 2f
    addi a0, a0, 1
    addi a1, a1, 1
    bnez t0, 1b
    li a0, 1
    ret
2:
    li a0, 0
    ret

# int pr_try(void *test, void *args)
# One run of the property body on a deep copy of args, under the test runner's recovery point.
# The outcome (O_*); a failure's expect node or runtime code goes to pr_kind.
pr_try:
    addi sp, sp, -160           # 0..111: recovery buffer
    sd ra, 152(sp)
    sd s0, 144(sp)
    sd s1, 136(sp)
    sd s2, 128(sp)
    sd s3, 120(sp)
    sd s4, 112(sp)
    mv s0, a0
    mv s1, a1
    ld t0, N_D(s0)
    ld s4, FX_PARAMS(t0)
    ld a0, L_LEN(s1)
    call list_new
    mv s2, a0
    li s3, 0
1:
    ld t0, L_LEN(s1)
    bge s3, t0, 2f
    mv a0, s4
    mv a1, s3
    call vec_get
    ld a1, N_TYPE(a0)
    ld t0, L_DATA(s1)
    slli t1, s3, 3
    add t0, t0, t1
    ld a0, 0(t0)
    call prop_copy
    mv a1, a0
    mv a0, s2
    call list_push
    addi s3, s3, 1
    j 1b
2:
    la t0, tf_node
    sd zero, 0(t0)
    la t0, rt_err_code
    sd zero, 0(t0)
    addi a0, sp, 0
    la t0, recover_point
    sd a0, 0(t0)
    call jmp_save
    bnez a0, 3f
    li a0, 1                    # every case starts from seed and clock defaults
    call rt_effects_start
    mv a0, s0                   # spec 006: the trace's test record, for each case
    call tr_test
    ld t0, N_D(s0)
    ld a0, FX_CODE(t0)
    mv a1, s0
    li a2, 0
    ld a3, L_DATA(s2)
    li a4, 0
    call vm_push
    call vm_run
    li a0, 0
3:
    mv s3, a0                   # 0 ran through, 1 expect failed, 2 runtime error
    la t0, recover_point
    sd zero, 0(t0)
    call interp_reset
    li a0, O_PASS
    beqz s3, 9f
    li t0, 1
    bne s3, t0, 4f
    la t0, tf_node
    ld t0, 0(t0)
    la t1, pr_kind
    sd t0, 0(t1)
    li a0, O_FAIL
    j 9f
4:
    la t0, rt_err_code
    ld s3, 0(t0)
    li a0, O_DISCARD
    la t0, assume_code
    beq s3, t0, 9f
    mv a0, s3
    la a1, code_r0009
    call pr_streq
    mv t0, a0
    li a0, O_TIMEOUT
    bnez t0, 9f
    mv a0, s3
    la a1, code_r0006
    call pr_streq
    mv t0, a0
    li a0, O_OOM
    bnez t0, 9f
    la t1, pr_kind
    sd s3, 0(t1)
    li a0, O_ERROR
9:
    ld ra, 152(sp)
    ld s0, 144(sp)
    ld s1, 136(sp)
    ld s2, 128(sp)
    ld s3, 120(sp)
    ld s4, 112(sp)
    addi sp, sp, 160
    ret

# void pr_shrink(void *test, int outcome, void *kind)
# Greedy shrinking of pr_best, parameter by parameter: the first candidate that fails the same way
# (same outcome; same expect node, or same runtime code) replaces it, and the search restarts.
# Stops at the budget or when the time limit is reached (pr_stopped).
pr_shrink:
    addi sp, sp, -80
    sd ra, 72(sp)
    sd s0, 64(sp)
    sd s1, 56(sp)
    sd s2, 48(sp)
    sd s3, 40(sp)
    sd s4, 32(sp)
    sd s5, 24(sp)
    sd s6, 16(sp)
    sd s7, 8(sp)
    mv s0, a0
    mv s1, a1
    mv s7, a2
    ld t0, N_D(s0)
    ld s2, FX_PARAMS(t0)
    ld s3, VEC_N(s2)
    li s6, 0                    # runs
    li s4, 0                    # parameter
1:
    bge s4, s3, 9f
2:
    mv a0, s2
    mv a1, s4
    call vec_get
    ld a1, N_TYPE(a0)
    la t0, pr_best
    ld t0, 0(t0)
    ld t0, L_DATA(t0)
    slli t1, s4, 3
    add t0, t0, t1
    ld a0, 0(t0)
    call prop_shrinks
    la t0, pr_cand
    sd a0, 0(t0)
    li s5, 0
3:
    la t0, pr_cand
    ld t0, 0(t0)
    ld t1, VEC_N(t0)
    bge s5, t1, 8f
    li t0, PR_BUDGET
    bge s6, t0, 9f
    la t0, pr_cand
    ld a0, 0(t0)
    mv a1, s5
    call vec_get
    mv a2, a0
    la t0, pr_best
    ld a0, 0(t0)
    mv a1, s4
    call pg_list_with
    la t0, pr_args
    sd a0, 0(t0)
    mv a1, a0
    mv a0, s0
    call pr_try
    addi s6, s6, 1
    addi s5, s5, 1
    li t0, O_TIMEOUT            # the property's time limit: keep what we have
    beq a0, t0, 7f
    bne a0, s1, 3b
    la t0, pr_kind
    ld a0, 0(t0)
    beq a0, s7, 4f
    li t0, O_ERROR
    bne s1, t0, 3b
    mv a1, s7
    call pr_streq
    beqz a0, 3b
4:
    la t0, pr_args              # still fails the same way: the new best
    ld t0, 0(t0)
    la t1, pr_best
    sd t0, 0(t1)
    la t0, pr_shrinks
    ld t1, 0(t0)
    addi t1, t1, 1
    sd t1, 0(t0)
    j 2b
7:
    la t0, pr_stopped
    li t1, 1
    sd t1, 0(t0)
    j 9f
8:
    addi s4, s4, 1
    j 1b
9:
    ld ra, 72(sp)
    ld s0, 64(sp)
    ld s1, 56(sp)
    ld s2, 48(sp)
    ld s3, 40(sp)
    ld s4, 32(sp)
    ld s5, 24(sp)
    ld s6, 16(sp)
    ld s7, 8(sp)
    addi sp, sp, 80
    ret

# int prop_run(void *test, long item)
# Runs property `test` (item index `item` in its file) until 100 cases pass or 1,000 attempts are
# made, shrinking the first failure. Returns the test status (ST_*); the report reads pr_*.
# The caller has started the heap and set the deadline for the whole property.
    .globl prop_run
prop_run:
    addi sp, sp, -80
    sd ra, 72(sp)
    sd s0, 64(sp)
    sd s1, 56(sp)
    sd s2, 48(sp)
    sd s3, 40(sp)
    sd s4, 32(sp)
    sd s5, 24(sp)
    sd s6, 16(sp)
    sd s7, 8(sp)
    mv s0, a0
    mv s1, a1
    ld t0, N_D(s0)
    ld s2, FX_PARAMS(t0)
    ld s3, VEC_N(s2)
    la t0, pr_cases
    sd zero, 0(t0)
    la t0, pr_discarded
    sd zero, 0(t0)
    la t0, pr_shrinks
    sd zero, 0(t0)
    la t0, pr_stopped
    sd zero, 0(t0)
    la t0, pr_starved
    sd zero, 0(t0)
    la t0, pr_best
    sd zero, 0(t0)
    li s4, 0                    # attempts
1:
    la t0, pr_cases
    ld t0, 0(t0)
    li a0, ST_PASS
    li t1, PR_CASES
    bge t0, t1, 99f
    li t0, PR_ATTEMPTS
    bge s4, t0, 70f
    la t0, opt_seed_set         # the case's inputs: --seed (default 1), item, attempt
    ld t0, 0(t0)
    li a0, 1
    beqz t0, 2f
    la t0, opt_seed
    ld a0, 0(t0)
2:
    mv a1, s1
    mv a2, s4
    call prop_seed
    mv a0, s3
    call list_new
    la t0, pr_args
    sd a0, 0(t0)
    li s6, 0
3:
    bge s6, s3, 4f
    mv a0, s2
    mv a1, s6
    call vec_get
    ld a0, N_TYPE(a0)
    li t0, 100                  # size 1..100: early cases are tiny
    rem a1, s4, t0
    addi a1, a1, 1
    li a2, PR_DEPTH
    call prop_gen
    mv a1, a0
    la t0, pr_args
    ld a0, 0(t0)
    call list_push
    addi s6, s6, 1
    j 3b
4:
    mv a0, s0
    la t0, pr_args
    ld a1, 0(t0)
    call pr_try
    mv s5, a0
    addi s4, s4, 1
    li t0, O_DISCARD
    bne s5, t0, 5f
    la t0, pr_discarded
    ld t1, 0(t0)
    addi t1, t1, 1
    sd t1, 0(t0)
    j 1b
5:
    la t0, pr_cases
    ld t1, 0(t0)
    addi t1, t1, 1
    sd t1, 0(t0)
    beqz s5, 1b                 # O_PASS
    la t0, pr_args              # the counterexample starts as this input
    ld t0, 0(t0)
    la t1, pr_best
    sd t0, 0(t1)
    li a0, ST_TIMEOUT           # a timeout or out of memory: reported with the input, unshrunk
    li t0, O_TIMEOUT
    beq s5, t0, 99f
    li a0, ST_ERROR
    li t0, O_OOM
    beq s5, t0, 99f
    mv a0, s0
    mv a1, s5
    la t0, pr_kind
    ld a2, 0(t0)
    call pr_shrink
    la t0, deadline_ms          # the report describes the final counterexample: run it again
    sd zero, 0(t0)
    mv a0, s0
    la t0, pr_best
    ld a1, 0(t0)
    call pr_try
    li a0, ST_FAIL
    li t0, O_FAIL
    beq s5, t0, 99f
    li a0, ST_ERROR
    j 99f
70:                             # too many discards: a failure at the `prop` keyword
    la t0, pr_starved
    li t1, 1
    sd t1, 0(t0)
    call sb_new
    mv s7, a0
    la a1, m_starved1
    call sb_cstr
    mv a0, s7
    la t0, pr_cases
    ld a1, 0(t0)
    call int_to_dec
    mv a0, s7
    la a1, m_starved2
    call sb_cstr
    mv a0, s7
    call sb_finish
    la t0, tf_msg
    sd a0, 0(t0)
    la t0, tf_node
    sd s0, 0(t0)
    la t0, tf_expected
    sd zero, 0(t0)
    la t0, tf_actual
    sd zero, 0(t0)
    li a0, ST_FAIL
99:
    ld ra, 72(sp)
    ld s0, 64(sp)
    ld s1, 56(sp)
    ld s2, 48(sp)
    ld s3, 40(sp)
    ld s4, 32(sp)
    ld s5, 24(sp)
    ld s6, 16(sp)
    ld s7, 8(sp)
    addi sp, sp, 80
    ret

# ---- reports (contracts/reports.md) ----

# void *pr_value_text(long i, void *test)
# Parameter i's value in pr_best, rendered in vo syntax (a str).
pr_value_text:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s1, a0
    ld t0, N_D(a1)
    ld a0, FX_PARAMS(t0)
    mv a1, s1
    call vec_get
    ld a0, N_TYPE(a0)
    sd a0, 0(sp)
    call sb_new
    mv s0, a0
    la t0, pr_best
    ld t0, 0(t0)
    ld t0, L_DATA(t0)
    slli t1, s1, 3
    add t0, t0, t1
    ld a1, 0(t0)
    ld a2, 0(sp)
    mv a0, s0
    call render_value
    mv a0, s0
    call sb_finish
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void *pr_param_name(long i, void *test)
pr_param_name:
    addi sp, sp, -16
    sd ra, 8(sp)
    ld t0, N_D(a1)
    mv a1, a0
    ld a0, FX_PARAMS(t0)
    call vec_get
    ld a0, N_A(a0)
    ld a0, N_A(a0)
    ld ra, 8(sp)
    addi sp, sp, 16
    ret

# void prop_json(void *sb, void *test, int status)
# The property members of a `test` record, after "ms".
    .globl prop_json
prop_json:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    mv s1, a1
    mv s2, a2
    la a1, j_prop
    call sb_cstr
    la t0, pr_cases
    ld a1, 0(t0)
    mv a0, s0
    call int_to_dec
    la t0, pr_discarded
    ld t0, 0(t0)
    beqz t0, 1f
    mv a0, s0
    la a1, j_discarded
    call sb_cstr
    la t0, pr_discarded
    ld a1, 0(t0)
    mv a0, s0
    call int_to_dec
1:
    beqz s2, 9f
    la t0, pr_starved
    ld t0, 0(t0)
    bnez t0, 9f
    mv a0, s0
    la a1, j_shrinks
    call sb_cstr
    la t0, pr_shrinks
    ld a1, 0(t0)
    mv a0, s0
    call int_to_dec
    mv a0, s0
    la a1, j_cex
    call sb_cstr
    li s3, 0
2:
    ld t0, N_D(s1)
    ld t0, FX_PARAMS(t0)
    ld t0, VEC_N(t0)
    bge s3, t0, 3f
    mv a0, s0
    la a1, j_name
    beqz s3, 21f
    la a1, j_name2
21:
    call sb_cstr
    mv a0, s3
    mv a1, s1
    call pr_param_name
    mv a1, a0
    mv a0, s0
    call json_str_into
    mv a0, s0
    la a1, j_value
    call sb_cstr
    mv a0, s3
    mv a1, s1
    call pr_value_text
    mv a1, a0
    mv a0, s0
    call json_str_into
    mv a0, s0
    li a1, '}'
    call sb_byte
    addi s3, s3, 1
    j 2b
3:
    mv a0, s0
    li a1, ']'
    call sb_byte
    la t0, pr_stopped
    ld t0, 0(t0)
    beqz t0, 9f
    mv a0, s0
    la a1, j_stopped
    call sb_cstr
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# void prop_cases_text(void *sb)
# "N cases, " inside a property's text line.
    .globl prop_cases_text
prop_cases_text:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    mv s0, a0
    la t0, pr_cases
    ld a1, 0(t0)
    call int_to_dec
    la a1, h_cases
    la t0, pr_cases
    ld t0, 0(t0)
    li t1, 1
    bne t0, t1, 1f
    la a1, h_case
1:
    mv a0, s0
    call sb_cstr
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret

# void prop_text(void *sb, void *test, int status)
# After a failing property's details: one counterexample line per parameter, then the shrink line.
    .globl prop_text
prop_text:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    mv s1, a1
    mv s2, a2
    beqz s2, 9f
    la t0, pr_starved
    ld t0, 0(t0)
    bnez t0, 9f
    li s3, 0
1:
    ld t0, N_D(s1)
    ld t0, FX_PARAMS(t0)
    ld t0, VEC_N(t0)
    bge s3, t0, 2f
    mv a0, s0
    la a1, h_cex
    call sb_cstr
    mv a0, s3
    mv a1, s1
    call pr_param_name
    mv a1, a0
    mv a0, s0
    call sb_str
    mv a0, s0
    la a1, h_eq
    call sb_cstr
    mv a0, s3
    mv a1, s1
    call pr_value_text
    mv a1, a0
    mv a0, s0
    call sb_str
    mv a0, s0
    li a1, 10
    call sb_byte
    addi s3, s3, 1
    j 1b
2:
    li t0, ST_TIMEOUT
    beq s2, t0, 9f
    la a1, h_shrunk
    la t0, pr_stopped
    ld t0, 0(t0)
    beqz t0, 3f
    la a1, h_stopped
3:
    mv a0, s0
    call sb_cstr
    la t0, pr_shrinks
    ld a1, 0(t0)
    mv a0, s0
    call int_to_dec
    la a1, h_steps
    la t0, pr_shrinks
    ld t0, 0(t0)
    li t1, 1
    bne t0, t1, 4f
    la a1, h_step
4:
    mv a0, s0
    call sb_cstr
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

    .section .rodata
m_starved1: .string "too many inputs were discarded by `assume` ("
m_starved2: .string " of 1000 accepted)"
j_prop:     .string ",\"prop\":true,\"cases\":"
j_discarded: .string ",\"discarded\":"
j_shrinks:  .string ",\"shrinks\":"
j_cex:      .string ",\"counterexample\":["
j_name:     .string "{\"name\":"
j_name2:    .string ",{\"name\":"
j_value:    .string ",\"value\":"
j_stopped:  .string ",\"shrink_stopped\":true"
h_cases:    .string " cases, "
h_case:     .string " case, "
h_cex:      .string "    counterexample: "
h_eq:       .string " = "
h_shrunk:   .string "    shrunk in "
h_stopped:  .string "    shrinking stopped at the time limit after "
h_steps:    .string " steps\n"
h_step:     .string " step\n"
