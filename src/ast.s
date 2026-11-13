# Growable pointer vectors and AST node construction.
    .include "defs.inc"

    .section .bss
    .align 3
    .globl cur_file
cur_file:   .space 8                # file id stamped on new nodes

    .text
# void *vec_new(void)
    .globl vec_new
vec_new:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    li a0, VEC_SIZE
    call alloc
    mv s0, a0
    li a0, 64
    call alloc
    sd a0, VEC_DATA(s0)
    li t0, 8
    sd t0, VEC_CAP(s0)
    sd zero, VEC_N(s0)
    mv a0, s0
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret

# void vec_push(void *v, void *item)
    .globl vec_push
vec_push:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    sd s2, 0(sp)
    mv s0, a0
    mv s1, a1
    ld t0, VEC_N(s0)
    ld t1, VEC_CAP(s0)
    blt t0, t1, 1f
    slli s2, t1, 1
    slli a0, s2, 3
    call alloc
    ld a1, VEC_DATA(s0)
    sd a0, VEC_DATA(s0)
    ld a2, VEC_N(s0)
    slli a2, a2, 3
    call mem_copy
    sd s2, VEC_CAP(s0)
1:
    ld t0, VEC_N(s0)
    ld t1, VEC_DATA(s0)
    slli t2, t0, 3
    add t1, t1, t2
    sd s1, 0(t1)
    addi t0, t0, 1
    sd t0, VEC_N(s0)
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    ld s2, 0(sp)
    addi sp, sp, 32
    ret

# void *vec_get(void *v, long i)
    .globl vec_get
vec_get:
    ld t0, VEC_DATA(a0)
    slli a1, a1, 3
    add t0, t0, a1
    ld a0, 0(t0)
    ret

# long vec_len(void *v)
    .globl vec_len
vec_len:
    beqz a0, 1f
    ld a0, VEC_N(a0)
1:
    ret

# void *node_new(int kind, long start, long end)
    .globl node_new
node_new:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    sd s2, 0(sp)
    mv s0, a0
    mv s1, a1
    mv s2, a2
    li a0, NODE_SIZE
    call alloc
    sh s0, N_KIND(a0)
    la t0, cur_file
    ld t0, 0(t0)
    sw t0, N_FILE(a0)
    sw s1, N_START(a0)
    sw s2, N_END(a0)
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    ld s2, 0(sp)
    addi sp, sp, 32
    ret
