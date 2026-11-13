# Runtime lists: header | len | cap | data (slot[cap]). Growth copies (the arena never frees).
    .include "defs.inc"

    .text
# void *list_new(long cap)
    .globl list_new
list_new:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s1, a0
    bnez s1, 1f
    li s1, 4
1:
    li a0, OBJ_LIST
    li a1, LIST_SIZE
    call alloc_obj
    mv s0, a0
    sd zero, L_LEN(s0)
    sd s1, L_CAP(s0)
    slli a0, s1, 3
    call alloc
    sd a0, L_DATA(s0)
    mv a0, s0
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void list_push(void *list, long value)
    .globl list_push
list_push:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    sd s2, 0(sp)
    mv s0, a0
    mv s1, a1
    ld t0, L_LEN(s0)
    ld t1, L_CAP(s0)
    blt t0, t1, 1f
    slli s2, t1, 1
    bnez s2, 2f
    li s2, 4
2:
    slli a0, s2, 3
    call alloc
    ld a1, L_DATA(s0)
    sd a0, L_DATA(s0)
    ld a2, L_LEN(s0)
    slli a2, a2, 3
    call mem_copy8
    sd s2, L_CAP(s0)
1:
    ld t0, L_LEN(s0)
    ld t1, L_DATA(s0)
    slli t2, t0, 3
    add t1, t1, t2
    sd s1, 0(t1)
    addi t0, t0, 1
    sd t0, L_LEN(s0)
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    ld s2, 0(sp)
    addi sp, sp, 32
    ret

# void mem_copy8(long *dst, long *src, long bytes)
# Copies a whole number of 8-byte words.
    .globl mem_copy8
mem_copy8:
1:
    blez a2, 2f
    ld t0, 0(a1)
    sd t0, 0(a0)
    addi a0, a0, 8
    addi a1, a1, 8
    addi a2, a2, -8
    j 1b
2:
    ret

# void *make_result(int tag, long payload)
    .globl make_result
make_result:
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

# void *make_err(char *message)
    .globl make_err
make_err:
    addi sp, sp, -16
    sd ra, 8(sp)
    call str_from_cstr
    mv a1, a0
    li a0, 1
    call make_result
    ld ra, 8(sp)
    addi sp, sp, 16
    ret

    .section .rodata
s_empty: .string "empty"
    .text

# void *list_pop(void *list)
# result[T]: ok(last element) or err("empty").
    .globl list_pop
list_pop:
    ld t0, L_LEN(a0)
    beqz t0, 1f
    addi t0, t0, -1
    sd t0, L_LEN(a0)
    ld t1, L_DATA(a0)
    slli t2, t0, 3
    add t1, t1, t2
    ld a1, 0(t1)
    li a0, 0
    tail make_result
1:
    la a0, s_empty
    tail make_err

# void *list_slice(void *list, long a, long b, void *node)
# Elements a..b (half-open) as a new list; R0003 when out of range.
    .globl list_slice
list_slice:
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
    ld t0, L_LEN(s0)
    bltz s1, 8f
    bgt s1, t0, 8f
    blt s2, s1, 9f
    bgt s2, t0, 9f
    sub a0, s2, s1
    call list_new
    sub t0, s2, s1
    sd t0, L_LEN(a0)
    sd a0, 0(sp)
    ld a0, L_DATA(a0)
    ld a1, L_DATA(s0)
    slli t1, s1, 3
    add a1, a1, t1
    slli a2, t0, 3
    call mem_copy8
    ld a0, 0(sp)
    j 99f
8:
    mv a0, s3
    mv a1, s1
    ld a2, L_LEN(s0)
    call rt_index_error
9:
    mv a0, s3
    mv a1, s2
    ld a2, L_LEN(s0)
    call rt_index_error
99:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# int elem_less(long a, long b, int kind)
# a < b for sortable element kinds (int, float, str).
    .globl elem_less
elem_less:
    li t0, TY_FLOAT
    beq a2, t0, 1f
    li t0, TY_STR
    beq a2, t0, 2f
    slt a0, a0, a1
    ret
1:
    fmv.d.x ft0, a0
    fmv.d.x ft1, a1
    flt.d a0, ft0, ft1
    ret
2:
    addi sp, sp, -16
    sd ra, 8(sp)
    call str_cmp
    slti a0, a0, 0
    ld ra, 8(sp)
    addi sp, sp, 16
    ret

# void list_sort(void *list, int kind)
# Stable bottom-up merge sort into a scratch array.
    .globl list_sort
list_sort:
    addi sp, sp, -112           # 0: list, 8: i, 16: j, 32..111: saved registers
    sd ra, 104(sp)
    sd s0, 96(sp)
    sd s1, 88(sp)
    sd s2, 80(sp)
    sd s3, 72(sp)
    sd s4, 64(sp)
    sd s5, 56(sp)
    sd s6, 48(sp)
    sd s7, 40(sp)
    sd s8, 32(sp)
    mv s7, a1                   # element kind
    ld s0, L_LEN(a0)            # n
    ld s1, L_DATA(a0)           # src
    sd a0, 0(sp)
    li t0, 2
    blt s0, t0, 9f
    slli a0, s0, 3
    call alloc
    mv s2, a0                   # dst
    li s3, 1                    # width
1:
    bge s3, s0, 8f
    li s4, 0                    # left start
2:
    bge s4, s0, 7f
    add s5, s4, s3              # mid
    ble s5, s0, 21f
    mv s5, s0
21:
    slli t0, s3, 1
    add s6, s4, t0              # right end
    ble s6, s0, 22f
    mv s6, s0
22:
    mv t3, s4                   # i
    mv t4, s5                   # j
    mv s8, s4                   # k
3:
    bge s8, s6, 6f
    bge t3, s5, 5f              # left exhausted: take right
    bge t4, s6, 4f              # right exhausted: take left
    sd t3, 8(sp)
    sd t4, 16(sp)
    slli t0, t4, 3              # take right only when right < left (keeps the sort stable)
    add t0, s1, t0
    ld a0, 0(t0)
    slli t1, t3, 3
    add t1, s1, t1
    ld a1, 0(t1)
    mv a2, s7
    call elem_less
    ld t3, 8(sp)
    ld t4, 16(sp)
    bnez a0, 5f
4:
    slli t0, t3, 3
    add t0, s1, t0
    ld t0, 0(t0)
    slli t1, s8, 3
    add t1, s2, t1
    sd t0, 0(t1)
    addi t3, t3, 1
    addi s8, s8, 1
    j 3b
5:
    slli t0, t4, 3
    add t0, s1, t0
    ld t0, 0(t0)
    slli t1, s8, 3
    add t1, s2, t1
    sd t0, 0(t1)
    addi t4, t4, 1
    addi s8, s8, 1
    j 3b
6:
    slli t0, s3, 1
    add s4, s4, t0
    j 2b
7:
    mv t0, s1                   # swap src and dst
    mv s1, s2
    mv s2, t0
    slli s3, s3, 1
    j 1b
8:
    ld t0, 0(sp)                # the sorted data is in src
    sd s1, L_DATA(t0)
    sd s0, L_CAP(t0)
9:
    ld ra, 104(sp)
    ld s0, 96(sp)
    ld s1, 88(sp)
    ld s2, 80(sp)
    ld s3, 72(sp)
    ld s4, 64(sp)
    ld s5, 56(sp)
    ld s6, 48(sp)
    ld s7, 40(sp)
    ld s8, 32(sp)
    addi sp, sp, 112
    ret
