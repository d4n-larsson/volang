# rt_func.s: the builtins that take functions (contracts/grammar-v2.md "New builtins").
# Each calls its function value through call_value; the list is read afresh on every step, so a
# callback that changes the list cannot make the loop read past its end.

    .include "defs.inc"

    .text

# void *hof_map(void *list, void *fn, void *call)
    .globl hof_map
hof_map:
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
    add a2, t0, t1
    mv a0, s1
    mv a1, s2
    call call_value
    mv a1, a0
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

# void *hof_filter(void *list, void *fn, void *call)
    .globl hof_filter
hof_filter:
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
    li a0, 0
    call list_new
    mv s3, a0
    li s4, 0
1:
    ld t0, L_LEN(s0)
    bge s4, t0, 9f
    ld t0, L_DATA(s0)
    slli t1, s4, 3
    add a2, t0, t1
    mv a0, s1
    mv a1, s2
    call call_value
    beqz a0, 2f
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
    ld s2, 16(sp)
    ld s3, 8(sp)
    ld s4, 0(sp)
    addi sp, sp, 48
    ret

# long hof_fold(void *list, long init, void *fn, void *call)
    .globl hof_fold
hof_fold:
    addi sp, sp, -64            # 0..15: the two argument values
    sd ra, 56(sp)
    sd s0, 48(sp)
    sd s1, 40(sp)
    sd s2, 32(sp)
    sd s3, 24(sp)
    sd s4, 16(sp)
    mv s0, a0
    mv s1, a1                   # the accumulator
    mv s2, a2
    mv s3, a3
    li s4, 0
1:
    ld t0, L_LEN(s0)
    bge s4, t0, 9f
    ld t0, L_DATA(s0)
    slli t1, s4, 3
    add t0, t0, t1
    ld t0, 0(t0)
    sd s1, 0(sp)
    sd t0, 8(sp)
    mv a0, s2
    mv a1, s3
    addi a2, sp, 0
    call call_value
    mv s1, a0
    addi s4, s4, 1
    j 1b
9:
    mv a0, s1
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    addi sp, sp, 64
    ret

# int hof_any(void *list, void *fn, void *call, int all)
# any: stops at the first true; all (all = 1): stops at the first false.
    .globl hof_any
hof_any:
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
    mv s3, a3
    li s4, 0
1:
    ld t0, L_LEN(s0)
    bge s4, t0, 8f
    ld t0, L_DATA(s0)
    slli t1, s4, 3
    add a2, t0, t1
    mv a0, s1
    mv a1, s2
    call call_value
    addi s4, s4, 1
    seqz t0, s3
    beq a0, t0, 9f              # any: true, all: false
    j 1b
8:
    mv a0, s3                   # none decided it: any is false, all is true
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    ld s4, 0(sp)
    addi sp, sp, 48
    ret

# void hof_sort_by(void *list, void *fn, void *call)
# Stable, in place: each key is computed once, then a merge sort of the positions by key.
    .globl hof_sort_by
hof_sort_by:
    addi sp, sp, -96
    sd ra, 88(sp)
    sd s0, 80(sp)
    sd s1, 72(sp)
    sd s2, 64(sp)
    sd s3, 56(sp)
    sd s4, 48(sp)
    sd s5, 40(sp)
    sd s6, 32(sp)
    sd s7, 24(sp)
    sd s8, 16(sp)
    mv s0, a0
    mv s1, a1
    ld t0, N_B(a2)              # the key's type: the function argument's return type
    ld t0, VEC_DATA(t0)
    ld t0, 8(t0)
    ld t0, N_TYPE(t0)
    ld t0, TY_B(t0)
    ld s8, TY_KIND(t0)
    mv s7, a2
    ld s2, L_LEN(s0)            # n
    slli a0, s2, 3
    call alloc
    mv s3, a0                   # keys
    slli a0, s2, 3
    call alloc
    mv s4, a0                   # positions
    slli a0, s2, 3
    call alloc
    mv s5, a0                   # scratch
    li s6, 0
1:
    bge s6, s2, 2f
    slli t0, s6, 3
    add t1, s4, t0
    sd s6, 0(t1)
    ld t1, L_DATA(s0)
    add a2, t1, t0
    mv a0, s1
    mv a1, s7
    call call_value
    slli t0, s6, 3
    add t0, s3, t0
    sd a0, 0(t0)
    addi s6, s6, 1
    j 1b
2:
    li s6, 1                    # width
3:
    bge s6, s2, 7f
    li s7, 0                    # left start
4:
    bge s7, s2, 6f
    add t0, s7, s6              # mid
    mv t1, s2
    blt t0, t1, 41f
    mv t0, s2
41:
    slli t2, s6, 1
    add t2, s7, t2              # end
    blt t2, s2, 42f
    mv t2, s2
42:
    sd t0, 0(sp)
    sd t2, 8(sp)
    mv a0, s7                   # merge [s7, mid) and [mid, end) into scratch
    mv a1, t0
    mv a2, t2
    mv a3, s3
    mv a4, s4
    mv a5, s5
    mv a6, s8
    call hof_merge
    ld t2, 8(sp)
    mv s7, t2
    j 4b
6:
    mv t0, s4                   # the merged run is in scratch now
    mv s4, s5
    mv s5, t0
    slli s6, s6, 1
    j 3b
7:
    slli a0, s2, 3              # permute the elements
    call alloc
    mv s5, a0
    ld t0, L_DATA(s0)
    li t1, 0
8:
    bge t1, s2, 81f
    slli t2, t1, 3
    add t3, s4, t2
    ld t3, 0(t3)
    slli t3, t3, 3
    add t3, t0, t3
    ld t3, 0(t3)
    add t4, s5, t2
    sd t3, 0(t4)
    addi t1, t1, 1
    j 8b
81:
    li t1, 0
82:
    bge t1, s2, 9f
    slli t2, t1, 3
    add t3, s5, t2
    ld t3, 0(t3)
    add t4, t0, t2
    sd t3, 0(t4)
    addi t1, t1, 1
    j 82b
9:
    ld ra, 88(sp)
    ld s0, 80(sp)
    ld s1, 72(sp)
    ld s2, 64(sp)
    ld s3, 56(sp)
    ld s4, 48(sp)
    ld s5, 40(sp)
    ld s6, 32(sp)
    ld s7, 24(sp)
    ld s8, 16(sp)
    addi sp, sp, 96
    ret

# void hof_merge(long lo, long mid, long hi, long *keys, long *from, long *to, int kind)
# Merges the sorted position runs from[lo, mid) and from[mid, hi) into to[lo, hi), by key; on
# equal keys the left run first (stable).
    .globl hof_merge
hof_merge:
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
    sd s8, 0(sp)
    mv s0, a0                   # i (left)
    mv s1, a1                   # j (right)
    mv s2, a1                   # mid
    mv s3, a2                   # hi
    mv s4, a3
    mv s5, a4
    mv s6, a5
    mv s7, a6
    mv s8, a0                   # k (out)
1:
    bge s8, s3, 9f
    bge s0, s2, 3f              # left run done: take right
    bge s1, s3, 2f              # right run done: take left
    slli t0, s1, 3              # right < left? then right, else left
    add t0, s5, t0
    ld t0, 0(t0)
    slli t0, t0, 3
    add t0, s4, t0
    ld a0, 0(t0)
    slli t1, s0, 3
    add t1, s5, t1
    ld t1, 0(t1)
    slli t1, t1, 3
    add t1, s4, t1
    ld a1, 0(t1)
    mv a2, s7
    call elem_less
    bnez a0, 3f
2:
    slli t0, s0, 3
    add t0, s5, t0
    ld t0, 0(t0)
    addi s0, s0, 1
    j 4f
3:
    slli t0, s1, 3
    add t0, s5, t0
    ld t0, 0(t0)
    addi s1, s1, 1
4:
    slli t1, s8, 3
    add t1, s6, t1
    sd t0, 0(t1)
    addi s8, s8, 1
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
    ld s8, 0(sp)
    addi sp, sp, 80
    ret
