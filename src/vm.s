# The bytecode engine (spec 004): runs compiled functions (src/compile.s) with a direct-threaded
# dispatch loop. Frames live on the value stack (data-model.md "Frame"); vo-to-vo calls stay in
# the loop, and builtins that call back into vo re-enter it through call_fn.
#
# Inside vm_run: s1 = pc (the instruction being executed), s2 = the current frame, s3 = its slots
# (s2 + FR_SLOTS). Slot operands are byte offsets from s3. Handlers are local labels reached only
# through vm_ops; each ends by loading the next instruction's handler and jumping to it.
# ponytail: fixed 32-byte instructions with the source node inline (about 4x a packed encoding);
# upgrade: 8-byte instructions with an opcode table and a side table of nodes, if memory matters.
    .include "defs.inc"

    .section .rodata
    .align 3
    .globl vm_ops
vm_ops:
    .dword op_mov, op_loadk
    .dword op_add_int, op_sub_int, op_mul_int, op_div_int, op_rem_int
    .dword op_add_float, op_sub_float, op_mul_float, op_div_float
    .dword op_concat, op_eq, op_ne
    .dword op_lt_int, op_le_int, op_gt_int, op_ge_int
    .dword op_lt_float, op_le_float, op_gt_float, op_ge_float
    .dword op_lt_str, op_le_str, op_gt_str, op_ge_str
    .dword op_not, op_neg_int, op_neg_float
    .dword op_jmp, op_jt, op_jf
    .dword op_for_range, op_for_list, op_inc, op_tick
    .dword op_call, op_call_builtin, op_ret, op_cmp_param
    .dword op_new_list, op_list_push, op_new_map, op_new_record, op_set_field, op_get_field
    .dword op_index_list, op_index_map, op_set_index_list, op_set_index_map
    .dword op_new_variant, op_set_payload, op_jtag_ne, op_payload, op_try
    .dword op_closure, op_fnval, op_call_value
    .dword op_add_int_k, op_sub_int_k, op_mul_int_k, op_div_int_k, op_rem_int_k
    .dword op_eq_int, op_ne_int
    .dword op_jlt, op_jle, op_jgt, op_jge, op_jeq, op_jne
    .dword op_jlt_k, op_jle_k, op_jgt_k, op_jge_k, op_jeq_k, op_jne_k
    .dword op_for_step, op_for_list_step, op_call_fast
    .dword op_len, op_push, op_has, op_call_generic

s_not_compiled: .string "error[T0003]: internal error: a function has no compiled code\n"

    .text
# ponytail: under qemu-user, translation blocks end at 4 KiB page boundaries, so where the VM's code
# falls within a page decides whether hot handlers get split (up to 20% on tests/speed). The VM
# starts at a fixed page offset, the one spec 004 was measured at, so code added elsewhere cannot
# move it. Upgrade: lay the hot handlers out to avoid page crossings, if qemu speed matters more.
    .balign 4096
    .skip 0xc7c
# void vm_not_compiled(void)
# Every function, lambda and test is compiled before a program runs (compile_program): one
# without code is a toolchain bug (T0003, exit 125).
    .globl vm_not_compiled
vm_not_compiled:
    addi sp, sp, -16
    sd ra, 8(sp)
    li a0, 2
    la a1, s_not_compiled
    call out_cstr
    li a0, EXIT_INTERNAL
    call vo_exit
    ld ra, 8(sp)
    addi sp, sp, 16
    ret

# long vm_run(void *frame)
# Runs the compiled function of a frame made by vm_push until that frame returns; the result.
    .globl vm_run
vm_run:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s1, 32(sp)
    sd s2, 24(sp)
    sd s3, 16(sp)
    sd zero, 8(sp)              # words never written: no stale pointers for the collector
    sd zero, 0(sp)
    mv s2, a0
    addi s3, s2, FR_SLOTS
    ld t0, FR_CODE(s2)
    ld s1, CF_CODE(t0)
    ld t6, I_HANDLER(s1)
    jr t6

op_mov:
    lw t1, I_B(s1)
    add t1, s3, t1
    ld t1, 0(t1)
    lw t0, I_A(s1)
    add t0, s3, t0
    sd t1, 0(t0)
    addi s1, s1, I_SIZE
    ld t6, I_HANDLER(s1)
    jr t6

op_loadk:
    ld t1, I_C(s1)
    lw t0, I_A(s1)
    add t0, s3, t0
    sd t1, 0(t0)
    addi s1, s1, I_SIZE
    ld t6, I_HANDLER(s1)
    jr t6

# ---- integer arithmetic: t1 = b, t2 = c, the result in t3 ----
op_add_int:                     # overflow iff (t < a) != (b < 0)
    lw t1, I_B(s1)
    add t1, s3, t1
    ld t1, 0(t1)
    ld t2, I_C(s1)
    add t2, s3, t2
    ld t2, 0(t2)
    add t3, t1, t2
    slt t4, t3, t1
    slti t5, t2, 0
    bne t4, t5, vm_overflow
    j vm_store_t3
op_sub_int:                     # overflow iff (t < a) != (b > 0)
    lw t1, I_B(s1)
    add t1, s3, t1
    ld t1, 0(t1)
    ld t2, I_C(s1)
    add t2, s3, t2
    ld t2, 0(t2)
    sub t3, t1, t2
    slt t4, t3, t1
    sgtz t5, t2
    bne t4, t5, vm_overflow
    j vm_store_t3
op_mul_int:                     # overflow iff the high word is not the low word's sign
    lw t1, I_B(s1)
    add t1, s3, t1
    ld t1, 0(t1)
    ld t2, I_C(s1)
    add t2, s3, t2
    ld t2, 0(t2)
    mul t3, t1, t2
    mulh t4, t1, t2
    srai t5, t3, 63
    bne t4, t5, vm_overflow
    j vm_store_t3
op_div_int:
    lw t1, I_B(s1)
    add t1, s3, t1
    ld t1, 0(t1)
    ld t2, I_C(s1)
    add t2, s3, t2
    ld t2, 0(t2)
    beqz t2, vm_div_zero
    li t4, -1
    bne t2, t4, 1f
    li t4, 1
    slli t4, t4, 63
    beq t1, t4, vm_overflow     # MIN / -1
1:
    div t3, t1, t2
    j vm_store_t3
op_rem_int:
    lw t1, I_B(s1)
    add t1, s3, t1
    ld t1, 0(t1)
    ld t2, I_C(s1)
    add t2, s3, t2
    ld t2, 0(t2)
    beqz t2, vm_div_zero
    rem t3, t1, t2
    j vm_store_t3

# ---- floats ----
op_add_float:
    lw t1, I_B(s1)
    add t1, s3, t1
    fld ft0, 0(t1)
    ld t2, I_C(s1)
    add t2, s3, t2
    fld ft1, 0(t2)
    fadd.d ft0, ft0, ft1
    fmv.x.d t3, ft0
    j vm_store_t3
op_sub_float:
    lw t1, I_B(s1)
    add t1, s3, t1
    fld ft0, 0(t1)
    ld t2, I_C(s1)
    add t2, s3, t2
    fld ft1, 0(t2)
    fsub.d ft0, ft0, ft1
    fmv.x.d t3, ft0
    j vm_store_t3
op_mul_float:
    lw t1, I_B(s1)
    add t1, s3, t1
    fld ft0, 0(t1)
    ld t2, I_C(s1)
    add t2, s3, t2
    fld ft1, 0(t2)
    fmul.d ft0, ft0, ft1
    fmv.x.d t3, ft0
    j vm_store_t3
op_div_float:
    lw t1, I_B(s1)
    add t1, s3, t1
    fld ft0, 0(t1)
    ld t2, I_C(s1)
    add t2, s3, t2
    fld ft1, 0(t2)
    fdiv.d ft0, ft0, ft1
    fmv.x.d t3, ft0
    j vm_store_t3

# ---- strings and equality (runtime calls; the operands stay in their slots for the collector) ----
op_concat:
    ld t0, I_NODE(s1)
    la t1, cur_node
    sd t0, 0(t1)
    lw t1, I_B(s1)
    add t1, s3, t1
    ld a0, 0(t1)
    ld t2, I_C(s1)
    add t2, s3, t2
    ld a1, 0(t2)
    call str_concat
    mv t3, a0
    j vm_store_t3
op_eq:
    ld t0, I_NODE(s1)           # a type parameter bound to int, bool or void: inline (US1)
    ld t0, N_A(t0)
    ld t0, N_TYPE(t0)
    ld t1, TY_KIND(t0)
    li t2, TY_PARAM
    bne t1, t2, 8f
    ld t3, FR_TARGS(s2)
    beqz t3, 8f
    ld t4, TY_A(t0)
    ld t5, VEC_N(t3)
    bgeu t4, t5, 8f
    ld t3, VEC_DATA(t3)
    slli t4, t4, 3
    add t3, t3, t4
    ld t3, 0(t3)
    beqz t3, 8f
    ld t1, TY_KIND(t3)
    li t2, TY_INT
    beq t1, t2, 1f
    li t2, TY_BOOL
    beq t1, t2, 1f
    li t2, TY_VOID
    bne t1, t2, 8f
1:
    ld t0, I_NODE(s1)           # `==` sets cur_node; values_equal: POLL 1, a raw compare
    la t1, cur_node
    sd t0, 0(t1)
    la t6, rt_budget
    ld t5, 0(t6)
    addi t5, t5, -1
    sd t5, 0(t6)
    bgtz t5, 2f
    call rt_poll
2:
    lw t1, I_B(s1)
    add t1, s3, t1
    ld t1, 0(t1)
    ld t2, I_C(s1)
    add t2, s3, t2
    ld t2, 0(t2)
    sub t3, t1, t2
    seqz t3, t3
    j vm_store_t3
8:
    call vm_equal
    mv t3, a0
    j vm_store_t3
op_ne:
    ld t0, I_NODE(s1)           # a type parameter bound to int, bool or void: inline (US1)
    ld t0, N_A(t0)
    ld t0, N_TYPE(t0)
    ld t1, TY_KIND(t0)
    li t2, TY_PARAM
    bne t1, t2, 8f
    ld t3, FR_TARGS(s2)
    beqz t3, 8f
    ld t4, TY_A(t0)
    ld t5, VEC_N(t3)
    bgeu t4, t5, 8f
    ld t3, VEC_DATA(t3)
    slli t4, t4, 3
    add t3, t3, t4
    ld t3, 0(t3)
    beqz t3, 8f
    ld t1, TY_KIND(t3)
    li t2, TY_INT
    beq t1, t2, 1f
    li t2, TY_BOOL
    beq t1, t2, 1f
    li t2, TY_VOID
    bne t1, t2, 8f
1:
    ld t0, I_NODE(s1)           # `==` sets cur_node; values_equal: POLL 1, a raw compare
    la t1, cur_node
    sd t0, 0(t1)
    la t6, rt_budget
    ld t5, 0(t6)
    addi t5, t5, -1
    sd t5, 0(t6)
    bgtz t5, 2f
    call rt_poll
2:
    lw t1, I_B(s1)
    add t1, s3, t1
    ld t1, 0(t1)
    ld t2, I_C(s1)
    add t2, s3, t2
    ld t2, 0(t2)
    sub t3, t1, t2
    snez t3, t3
    j vm_store_t3
8:
    call vm_equal
    xori t3, a0, 1
    j vm_store_t3

# ---- comparisons: 0 or 1 ----
op_lt_int:
    lw t1, I_B(s1)
    add t1, s3, t1
    ld t1, 0(t1)
    ld t2, I_C(s1)
    add t2, s3, t2
    ld t2, 0(t2)
    slt t3, t1, t2
    j vm_store_t3
op_le_int:
    lw t1, I_B(s1)
    add t1, s3, t1
    ld t1, 0(t1)
    ld t2, I_C(s1)
    add t2, s3, t2
    ld t2, 0(t2)
    slt t3, t2, t1
    xori t3, t3, 1
    j vm_store_t3
op_gt_int:
    lw t1, I_B(s1)
    add t1, s3, t1
    ld t1, 0(t1)
    ld t2, I_C(s1)
    add t2, s3, t2
    ld t2, 0(t2)
    slt t3, t2, t1
    j vm_store_t3
op_ge_int:
    lw t1, I_B(s1)
    add t1, s3, t1
    ld t1, 0(t1)
    ld t2, I_C(s1)
    add t2, s3, t2
    ld t2, 0(t2)
    slt t3, t1, t2
    xori t3, t3, 1
    j vm_store_t3
op_lt_float:
    lw t1, I_B(s1)
    add t1, s3, t1
    fld ft0, 0(t1)
    ld t2, I_C(s1)
    add t2, s3, t2
    fld ft1, 0(t2)
    flt.d t3, ft0, ft1
    j vm_store_t3
op_le_float:
    lw t1, I_B(s1)
    add t1, s3, t1
    fld ft0, 0(t1)
    ld t2, I_C(s1)
    add t2, s3, t2
    fld ft1, 0(t2)
    fle.d t3, ft0, ft1
    j vm_store_t3
op_gt_float:
    lw t1, I_B(s1)
    add t1, s3, t1
    fld ft0, 0(t1)
    ld t2, I_C(s1)
    add t2, s3, t2
    fld ft1, 0(t2)
    flt.d t3, ft1, ft0
    j vm_store_t3
op_ge_float:
    lw t1, I_B(s1)
    add t1, s3, t1
    fld ft0, 0(t1)
    ld t2, I_C(s1)
    add t2, s3, t2
    fld ft1, 0(t2)
    fle.d t3, ft1, ft0
    j vm_store_t3
op_lt_str:
    call vm_str_cmp
    slti t3, a0, 0
    j vm_store_t3
op_le_str:
    call vm_str_cmp
    slti t3, a0, 1
    j vm_store_t3
op_gt_str:
    call vm_str_cmp
    sgtz t3, a0
    j vm_store_t3
op_ge_str:
    call vm_str_cmp
    slti t3, a0, 0
    xori t3, t3, 1
    j vm_store_t3

# a <op> on an `ordered` type parameter: the concrete kind picks the int, float or str handler,
# which then runs on this same instruction (as the tree walker did with OP_*_PARAM).
op_cmp_param:
    ld t0, I_NODE(s1)
    ld t0, N_A(t0)
    ld a0, N_TYPE(t0)
    ld t1, TY_KIND(a0)          # rt_concrete's bare-parameter case, inline
    li t2, TY_PARAM
    bne t1, t2, 2f
    ld t3, FR_TARGS(s2)
    beqz t3, 2f
    ld t4, TY_A(a0)
    ld t5, VEC_N(t3)
    bgeu t4, t5, 2f
    ld t3, VEC_DATA(t3)
    slli t4, t4, 3
    add t3, t3, t4
    ld t3, 0(t3)
    beqz t3, 2f
    ld t0, TY_KIND(t3)
    j 3f
2:
    call rt_concrete
    ld t0, TY_KIND(a0)
3:
    ld t1, I_NODE(s1)
    ld t1, N_D(t1)
    addi t1, t1, -OP_LT_PARAM
    li t2, TY_FLOAT
    li t3, OP_LT_FLOAT
    beq t0, t2, 1f
    li t2, TY_STR
    li t3, OP_LT_STR
    beq t0, t2, 1f
    li t3, OP_LT_INT
1:
    add t1, t1, t3
    addi t1, t1, OPC_ADD_INT - OP_ADD_INT
    slli t1, t1, 3
    la t2, vm_ops
    add t2, t2, t1
    ld t6, 0(t2)
    jr t6

# ---- unary ----
op_not:
    lw t1, I_B(s1)
    add t1, s3, t1
    ld t1, 0(t1)
    xori t3, t1, 1
    j vm_store_t3
op_neg_int:
    lw t1, I_B(s1)
    add t1, s3, t1
    ld t1, 0(t1)
    li t4, 1
    slli t4, t4, 63
    beq t1, t4, vm_overflow     # -MIN
    neg t3, t1
    j vm_store_t3
op_neg_float:
    lw t1, I_B(s1)
    add t1, s3, t1
    ld t1, 0(t1)
    li t4, 1
    slli t4, t4, 63
    xor t3, t1, t4
    j vm_store_t3

# ---- control ----
op_jmp:
    ld s1, I_C(s1)
    ld t6, I_HANDLER(s1)
    jr t6
op_jt:
    lw t1, I_B(s1)
    add t1, s3, t1
    ld t1, 0(t1)
    bnez t1, op_jmp
    addi s1, s1, I_SIZE
    ld t6, I_HANDLER(s1)
    jr t6
op_jf:
    lw t1, I_B(s1)
    add t1, s3, t1
    ld t1, 0(t1)
    beqz t1, op_jmp
    addi s1, s1, I_SIZE
    ld t6, I_HANDLER(s1)
    jr t6

# for over a range: counter at b, end at b+8 (both hidden temporaries)
op_for_range:
    lw t1, I_B(s1)
    add t1, s3, t1
    ld t2, 0(t1)
    ld t3, 8(t1)
    bge t2, t3, op_jmp
    ld a0, I_NODE(s1)
    call tick
    lw t1, I_B(s1)
    add t1, s3, t1
    ld t3, 0(t1)
    j vm_store_t3

# for over a list: the list at b, the index at b+8; the length and data are read every time
op_for_list:
    lw t1, I_B(s1)
    add t1, s3, t1
    ld t2, 0(t1)
    ld t3, 8(t1)
    ld t4, L_LEN(t2)
    bge t3, t4, op_jmp
    ld a0, I_NODE(s1)
    call tick
    lw t1, I_B(s1)
    add t1, s3, t1
    ld t2, 0(t1)
    ld t3, 8(t1)
    ld t2, L_DATA(t2)
    slli t3, t3, 3
    add t2, t2, t3
    ld t3, 0(t2)
    j vm_store_t3

op_inc:
    lw t0, I_A(s1)
    add t0, s3, t0
    ld t1, 0(t0)
    addi t1, t1, 1
    sd t1, 0(t0)
    addi s1, s1, I_SIZE
    ld t6, I_HANDLER(s1)
    jr t6

op_tick:
    la t4, steps
    ld t5, 0(t4)
    addi t5, t5, 1
    sd t5, 0(t4)
    slli t5, t5, 48
    bnez t5, vm_next
    call vm_tick_slow
    j vm_next

# ---- calls ----
# a = c(arguments in b, b+8, ...): c is the callee's declaration. A compiled callee gets a frame
# and runs in this loop; one not compiled (yet) runs through call_fn.
op_call:
    ld a1, I_C(s1)
    ld t0, N_A(a1)
    ld t0, N_B(t0)
    ld a0, FX_CODE(t0)
    beqz a0, 5f
    ld a2, I_NODE(s1)
    lw t1, I_B(s1)
    add a3, s3, t1
    li a4, 0
    call vm_push
vm_enter:                       # a0: the callee's new frame; return here to the next instruction
    sd s1, FR_RPC(a0)
    lw t1, I_A(s1)
    sd t1, FR_RDST(a0)
    mv s2, a0
    addi s3, s2, FR_SLOTS
    ld a0, I_NODE(s1)           # the call counts as a step (in the callee's frame, as before)
    call tick
    ld t0, FR_CODE(s2)
    ld s1, CF_CODE(t0)
    ld t6, I_HANDLER(s1)
    jr t6
5:
    mv a0, a1
    ld a1, I_NODE(s1)
    li a2, 0
    lw t1, I_B(s1)
    add a3, s3, t1
    li a4, 0
    call call_fn
    mv t3, a0
    j vm_store_t3

# a = builtin(b, ...): c = argument count | second slot << 16 | third slot << 40; the node the call
op_call_builtin:
    ld a0, I_NODE(s1)
    li a1, 0
    li a2, 0
    li a3, 0
    ld t0, I_C(s1)
    andi t2, t0, 0xFF
    beqz t2, 1f
    lw t1, I_B(s1)
    add t1, s3, t1
    ld a1, 0(t1)
    addi t2, t2, -1
    beqz t2, 1f
    slli t1, t0, 24
    srli t1, t1, 40
    add t1, s3, t1
    ld a2, 0(t1)
    addi t2, t2, -1
    beqz t2, 1f
    srli t1, t0, 40
    add t1, s3, t1
    ld a3, 0(t1)
1:
    call builtin_call
    mv t3, a0
    j vm_store_t3

# return b (b < 0: 0): pop the frame; back to the caller's next instruction, or out of vm_run
op_ret:
    lw t0, I_B(s1)
    li a0, 0
    bltz t0, 1f
    add t0, s3, t0
    ld a0, 0(t0)
1:
vm_return_a0:                   # return a0 from the current frame
    ld t1, FR_PARENT(s2)
    la t2, cur_frame
    sd t1, 0(t2)
    la t2, vsp
    sd s2, 0(t2)
    ld t3, FR_RPC(s2)
    beqz t3, vm_exit
    ld t4, FR_RDST(s2)
    mv s2, t1
    addi s3, s2, FR_SLOTS
    mv s1, t3
    add t4, s3, t4
    sd a0, 0(t4)
    addi s1, s1, I_SIZE
    ld t6, I_HANDLER(s1)
    jr t6
vm_exit:                        # the frame vm_run was entered with returned: a0 is the result
    ld ra, 40(sp)
    ld s1, 32(sp)
    ld s2, 24(sp)
    ld s3, 16(sp)
    addi sp, sp, 48
    ret

# ---- data (the same runtime calls and checks as the tree walker) ----
op_new_list:
    ld a0, I_C(s1)
    call list_new
    mv t3, a0
    j vm_store_t3
op_list_push:
    lw t0, I_A(s1)
    add t0, s3, t0
    ld a0, 0(t0)
    lw t1, I_B(s1)
    add t1, s3, t1
    ld a1, 0(t1)
    call list_push
    j vm_next
op_new_map:
    call map_new
    mv t3, a0
    j vm_store_t3
op_new_record:
    li a0, OBJ_RECORD
    ld a1, I_C(s1)
    slli a1, a1, 3
    call alloc_obj
    mv t3, a0
    j vm_store_t3
op_set_field:
    lw t0, I_A(s1)
    add t0, s3, t0
    ld t0, 0(t0)
    lw t1, I_B(s1)
    add t1, s3, t1
    ld t1, 0(t1)
    ld t2, I_C(s1)
    add t0, t0, t2
    sd t1, 0(t0)
    j vm_next
op_get_field:
    lw t1, I_B(s1)
    add t1, s3, t1
    ld a0, 0(t1)
.ifdef GC_STRESS_BUILD
    li a1, OBJ_RECORD
    call gc_check               # stress build: a live record (spec 003 FR-011)
.endif
    ld t2, I_C(s1)
    add t2, a0, t2
    ld t3, 0(t2)
    j vm_store_t3
op_index_list:
    lw t1, I_B(s1)
    add t1, s3, t1
    ld t1, 0(t1)
    ld t2, I_C(s1)
    add t2, s3, t2
    ld a1, 0(t2)
    ld a2, L_LEN(t1)
    bltz a1, 1f
    bge a1, a2, 1f
    ld t1, L_DATA(t1)
    slli t2, a1, 3
    add t1, t1, t2
    ld t3, 0(t1)
    j vm_store_t3
1:
    ld a0, I_NODE(s1)
    call rt_index_error
    j vm_exit                   # (not reached)
op_index_map:
    call vm_map_find
    bltz a0, 1f
    lw t1, I_B(s1)
    add t1, s3, t1
    ld t1, 0(t1)
    ld t1, M_ENTRIES(t1)
    li t2, ME_SIZE
    mul t2, t2, a0
    add t1, t1, t2
    ld t3, ME_VAL(t1)
    j vm_store_t3
1:
    ld a0, I_NODE(s1)
    ld t0, N_A(a0)
    ld t0, N_TYPE(t0)
    ld a2, TY_A(t0)
    ld t2, I_C(s1)
    add t2, s3, t2
    ld a1, 0(t2)
    call rt_key_error
    j vm_exit                   # (not reached)
op_set_index_list:
    lw t0, I_A(s1)
    add t0, s3, t0
    ld t0, 0(t0)
    lw t1, I_B(s1)
    add t1, s3, t1
    ld a1, 0(t1)
    ld a2, L_LEN(t0)
    bltz a1, 1f
    bge a1, a2, 1f
    ld t2, I_C(s1)
    add t2, s3, t2
    ld t2, 0(t2)
    ld t0, L_DATA(t0)
    slli t3, a1, 3
    add t0, t0, t3
    sd t2, 0(t0)
    j vm_next
1:
    ld a0, I_NODE(s1)
    call rt_index_error
    j vm_exit                   # (not reached)
op_set_index_map:
    ld t0, I_NODE(s1)           # the `set` statement
    la t1, cur_node
    sd t0, 0(t1)
    ld t0, N_A(t0)              # its place: the key type of the map
    ld t0, N_A(t0)
    ld t0, N_TYPE(t0)
    ld a3, TY_A(t0)
    lw t0, I_A(s1)
    add t0, s3, t0
    ld a0, 0(t0)
    lw t1, I_B(s1)
    add t1, s3, t1
    ld a1, 0(t1)
    ld t2, I_C(s1)
    add t2, s3, t2
    ld a2, 0(t2)
    call map_set
    j vm_next
op_new_variant:
    li a0, OBJ_VARIANT
    li a1, VARIANT_SIZE
    call alloc_obj
    ld t0, I_C(s1)
    sw t0, V_TAG(a0)
    mv t3, a0
    j vm_store_t3
op_set_payload:
    lw t0, I_A(s1)
    add t0, s3, t0
    ld t0, 0(t0)
    lw t1, I_B(s1)
    add t1, s3, t1
    ld t1, 0(t1)
    sd t1, V_PAY(t0)
    j vm_next
op_jtag_ne:
    lw t1, I_B(s1)
    add t1, s3, t1
    ld t1, 0(t1)
    lwu t1, V_TAG(t1)
    lw t2, I_A(s1)
    bne t1, t2, op_jmp
    j vm_next
op_payload:
    lw t1, I_B(s1)
    add t1, s3, t1
    ld t1, 0(t1)
    ld t3, V_PAY(t1)
    j vm_store_t3
op_try:                         # ok: unwrap; err: the err value is the function's result
    lw t1, I_B(s1)
    add t1, s3, t1
    ld a0, 0(t1)
    lwu t2, V_TAG(a0)
    bnez t2, vm_return_a0
    ld t3, V_PAY(a0)
    j vm_store_t3

# ---- function values ----
op_closure:
    ld a0, I_NODE(s1)
    call make_closure
    mv t3, a0
    j vm_store_t3
op_fnval:
    ld a0, I_C(s1)
    call fn_closure
    mv t3, a0
    j vm_store_t3
# a = closure(c)(arguments in b, ...): callee then arguments (as before), then the call
op_call_value:
    ld t2, I_C(s1)
    add t2, s3, t2
    ld a4, 0(t2)                # the closure
.ifdef GC_STRESS_BUILD
    mv a0, a4
    li a1, OBJ_CLOSURE
    call gc_check               # stress build: a live OBJ_CLOSURE (spec 003 FR-011)
    ld t2, I_C(s1)
    add t2, s3, t2
    ld a4, 0(t2)
.endif
    ld a1, CL_FN(a4)
    ld t0, N_A(a1)
    ld t0, N_B(t0)
    ld a0, FX_CODE(t0)
    beqz a0, 5f
    ld a2, I_NODE(s1)
    lw t1, I_B(s1)
    add a3, s3, t1
    call vm_push
    j vm_enter
5:
    mv a0, a4
    ld a1, I_NODE(s1)
    lw t1, I_B(s1)
    add a2, s3, t1
    call call_value
    mv t3, a0
    j vm_store_t3

# ---- constant operands (US1): c is the constant ----
op_add_int_k:
    lw t1, I_B(s1)
    add t1, s3, t1
    ld t1, 0(t1)
    ld t2, I_C(s1)
    add t3, t1, t2
    slt t4, t3, t1
    slti t5, t2, 0
    bne t4, t5, vm_overflow
    j vm_store_t3
op_sub_int_k:
    lw t1, I_B(s1)
    add t1, s3, t1
    ld t1, 0(t1)
    ld t2, I_C(s1)
    sub t3, t1, t2
    slt t4, t3, t1
    sgtz t5, t2
    bne t4, t5, vm_overflow
    j vm_store_t3
op_mul_int_k:
    lw t1, I_B(s1)
    add t1, s3, t1
    ld t1, 0(t1)
    ld t2, I_C(s1)
    mul t3, t1, t2
    mulh t4, t1, t2
    srai t5, t3, 63
    bne t4, t5, vm_overflow
    j vm_store_t3
op_div_int_k:
    lw t1, I_B(s1)
    add t1, s3, t1
    ld t1, 0(t1)
    ld t2, I_C(s1)
    div t3, t1, t2
    j vm_store_t3
op_rem_int_k:
    lw t1, I_B(s1)
    add t1, s3, t1
    ld t1, 0(t1)
    ld t2, I_C(s1)
    rem t3, t1, t2
    j vm_store_t3
op_eq_int:
    ld t0, I_NODE(s1)           # `==` sets cur_node (out-of-memory reports)
    la t1, cur_node
    sd t0, 0(t1)
    lw t1, I_B(s1)
    add t1, s3, t1
    ld t1, 0(t1)
    ld t2, I_C(s1)
    add t2, s3, t2
    ld t2, 0(t2)
    sub t3, t1, t2
    seqz t3, t3
    j vm_store_t3
op_ne_int:
    ld t0, I_NODE(s1)           # `==` sets cur_node (out-of-memory reports)
    la t1, cur_node
    sd t0, 0(t1)
    lw t1, I_B(s1)
    add t1, s3, t1
    ld t1, 0(t1)
    ld t2, I_C(s1)
    add t2, s3, t2
    ld t2, 0(t2)
    sub t3, t1, t2
    snez t3, t3
    j vm_store_t3
# ---- fused compare-and-branch (US1): if a <cond> b: pc = c ----
op_jlt:
    lw t1, I_A(s1)
    add t1, s3, t1
    ld t1, 0(t1)
    lw t2, I_B(s1)
    add t2, s3, t2
    ld t2, 0(t2)
    blt t1, t2, op_jmp
    j vm_next
op_jle:
    lw t1, I_A(s1)
    add t1, s3, t1
    ld t1, 0(t1)
    lw t2, I_B(s1)
    add t2, s3, t2
    ld t2, 0(t2)
    bge t2, t1, op_jmp
    j vm_next
op_jgt:
    lw t1, I_A(s1)
    add t1, s3, t1
    ld t1, 0(t1)
    lw t2, I_B(s1)
    add t2, s3, t2
    ld t2, 0(t2)
    blt t2, t1, op_jmp
    j vm_next
op_jge:
    lw t1, I_A(s1)
    add t1, s3, t1
    ld t1, 0(t1)
    lw t2, I_B(s1)
    add t2, s3, t2
    ld t2, 0(t2)
    bge t1, t2, op_jmp
    j vm_next
op_jeq:
    ld t0, I_NODE(s1)           # (from `==`: sets cur_node)
    la t1, cur_node
    sd t0, 0(t1)
    lw t1, I_A(s1)
    add t1, s3, t1
    ld t1, 0(t1)
    lw t2, I_B(s1)
    add t2, s3, t2
    ld t2, 0(t2)
    beq t1, t2, op_jmp
    j vm_next
op_jne:
    ld t0, I_NODE(s1)           # (from `==`: sets cur_node)
    la t1, cur_node
    sd t0, 0(t1)
    lw t1, I_A(s1)
    add t1, s3, t1
    ld t1, 0(t1)
    lw t2, I_B(s1)
    add t2, s3, t2
    ld t2, 0(t2)
    bne t1, t2, op_jmp
    j vm_next
op_jlt_k:
    lw t1, I_A(s1)
    add t1, s3, t1
    ld t1, 0(t1)
    lw t2, I_B(s1)
    blt t1, t2, op_jmp
    j vm_next
op_jle_k:
    lw t1, I_A(s1)
    add t1, s3, t1
    ld t1, 0(t1)
    lw t2, I_B(s1)
    bge t2, t1, op_jmp
    j vm_next
op_jgt_k:
    lw t1, I_A(s1)
    add t1, s3, t1
    ld t1, 0(t1)
    lw t2, I_B(s1)
    blt t2, t1, op_jmp
    j vm_next
op_jge_k:
    lw t1, I_A(s1)
    add t1, s3, t1
    ld t1, 0(t1)
    lw t2, I_B(s1)
    bge t1, t2, op_jmp
    j vm_next
op_jeq_k:
    ld t0, I_NODE(s1)           # (from `==`: sets cur_node)
    la t1, cur_node
    sd t0, 0(t1)
    lw t1, I_A(s1)
    add t1, s3, t1
    ld t1, 0(t1)
    lw t2, I_B(s1)
    beq t1, t2, op_jmp
    j vm_next
op_jne_k:
    ld t0, I_NODE(s1)           # (from `==`: sets cur_node)
    la t1, cur_node
    sd t0, 0(t1)
    lw t1, I_A(s1)
    add t1, s3, t1
    ld t1, 0(t1)
    lw t2, I_B(s1)
    bne t1, t2, op_jmp
    j vm_next
# ---- loop back-edges (US1): one instruction per iteration, the step counted inline ----
# range: counter b, end b+8; c is the first instruction of the body
op_for_step:
    lw t1, I_B(s1)
    add t1, s3, t1
    ld t2, 0(t1)
    addi t2, t2, 1
    sd t2, 0(t1)
    ld t3, 8(t1)
    bge t2, t3, vm_next
    la t4, steps                # the step (tick's fast half)
    ld t5, 0(t4)
    addi t5, t5, 1
    sd t5, 0(t4)
    slli t5, t5, 48
    beqz t5, 2f
1:
    lw t1, I_B(s1)
    add t1, s3, t1
    ld t3, 0(t1)
    lw t0, I_A(s1)
    add t0, s3, t0
    sd t3, 0(t0)
    ld s1, I_C(s1)
    ld t6, I_HANDLER(s1)
    jr t6
2:
    call vm_tick_slow           # every 65,536th step: tick's clock check
    j 1b
# list: list b, index b+8
op_for_list_step:
    lw t1, I_B(s1)
    add t1, s3, t1
    ld t2, 8(t1)
    addi t2, t2, 1
    sd t2, 8(t1)
    ld t3, 0(t1)
    ld t3, L_LEN(t3)
    bge t2, t3, vm_next
    la t4, steps
    ld t5, 0(t4)
    addi t5, t5, 1
    sd t5, 0(t4)
    slli t5, t5, 48
    beqz t5, 2f
1:
    lw t1, I_B(s1)
    add t1, s3, t1
    ld t2, 0(t1)
    ld t3, 8(t1)
    ld t2, L_DATA(t2)
    slli t3, t3, 3
    add t2, t2, t3
    ld t3, 0(t2)
    lw t0, I_A(s1)
    add t0, s3, t0
    sd t3, 0(t0)
    ld s1, I_C(s1)
    ld t6, I_HANDLER(s1)
    jr t6
2:
    call vm_tick_slow
    j 1b

# CALL of a named, non-generic function (US1): the frame of vm_push, set up inline. A callee
# without code, or a frame that does not fit (R0005), takes op_call's path.
op_call_fast:
    ld a1, I_C(s1)              # decl
    ld t0, N_A(a1)
    ld a5, N_B(t0)              # its FX record
    ld a0, FX_CODE(a5)          # cf
    beqz a0, op_call
    la t3, vsp
    ld a2, 0(t3)                # the new frame
    ld t5, CF_SLOTS(a0)
    slli t5, t5, 3
    addi t5, t5, FR_SLOTS
    add t5, a2, t5              # its end
    la t6, vstack_end
    ld t6, 0(t6)
    bgeu t5, t6, op_call
    sd t5, 0(t3)
    addi t1, a2, FR_SLOTS       # the parameters, then zeros
    lw t2, I_B(s1)
    add t2, s3, t2
    ld t3, CF_NPARAMS(a0)
1:
    beqz t3, 2f
    ld t4, 0(t2)
    sd t4, 0(t1)
    addi t1, t1, 8
    addi t2, t2, 8
    addi t3, t3, -1
    j 1b
2:
    bgeu t1, t5, 3f
    sd zero, 0(t1)
    addi t1, t1, 8
    j 2b
3:
    sd s2, FR_PARENT(a2)
    sd a1, FR_FN(a2)
    ld t0, I_NODE(s1)
    sd t0, FR_CALL(a2)
    sd zero, FR_TARGS(a2)
    sd a0, FR_CODE(a2)
    sd s1, FR_RPC(a2)
    lw t1, I_A(s1)
    sd t1, FR_RDST(a2)
    ld t1, FX_DECLARED(a5)      # effects: the declaration, or (none declared) the caller's
    bnez t1, 4f
    ld t1, FR_EFFECTS(s2)
4:
    sd t1, FR_EFFECTS(a2)
    la t0, cur_frame
    sd a2, 0(t0)
    mv s2, a2
    addi s3, s2, FR_SLOTS
    la t4, steps                # the call counts as a step
    ld t5, 0(t4)
    addi t5, t5, 1
    sd t5, 0(t4)
    slli t5, t5, 48
    beqz t5, 6f
5:
    ld t0, FR_CODE(s2)
    ld s1, CF_CODE(t0)
    ld t6, I_HANDLER(s1)
    jr t6
6:
    call vm_tick_slow           # (s1 is still the call: its node)
    j 5b

# ---- hot pure builtins (US1): b_len and b_push without builtin_call; cur_node as before ----
op_len:
    ld t0, I_NODE(s1)
    la t1, cur_node
    sd t0, 0(t1)
    lw t1, I_B(s1)
    add t1, s3, t1
    ld t1, 0(t1)
    ld t3, L_LEN(t1)
    j vm_store_t3
op_push:
    ld t0, I_NODE(s1)
    la t1, cur_node
    sd t0, 0(t1)
    lw t0, I_A(s1)
    add t0, s3, t0
    ld a0, 0(t0)
    lw t1, I_B(s1)
    add t1, s3, t1
    ld a1, 0(t1)
    call list_push
    j vm_next

op_has:                         # b_has: map_find with the key type of the first argument
    ld t0, I_NODE(s1)
    la t1, cur_node
    sd t0, 0(t1)
    ld t0, N_B(t0)
    ld t0, VEC_DATA(t0)
    ld t0, 0(t0)
    ld t0, N_TYPE(t0)
    ld a2, TY_A(t0)
    lw t1, I_B(s1)
    add t1, s3, t1
    ld a0, 0(t1)
    ld t2, I_C(s1)
    add t2, s3, t2
    ld a1, 0(t2)
    call map_find
    slti t3, a0, 0
    xori t3, t3, 1
    j vm_store_t3

# a = generic call: CALL_FAST's inline frame, with type arguments and effects from the call
# site's record (vm_push's rt_targs / rt_es_subst, recomputed only when the caller's type
# arguments or effect set differ from the last call's). c is the record.
op_call_generic:
    ld a6, I_C(s1)              # record
    ld a1, GS_DECL(a6)
    ld t0, N_A(a1)
    ld a5, N_B(t0)              # FX
    ld a0, FX_CODE(a5)
    beqz a0, 8f
    ld t1, FX_DECLARED(a5)      # the effect set before substitution
    bnez t1, 1f
    ld t1, FR_EFFECTS(s2)
1:
    ld t2, FR_TARGS(s2)
    ld t3, GS_VALID(a6)
    beqz t3, 7f
    ld t3, GS_CTARGS(a6)
    bne t3, t2, 7f
    ld t3, GS_EFF0(a6)
    bne t3, t1, 7f
2:
    la t3, vsp
    ld a2, 0(t3)                # the new frame
    ld t5, CF_SLOTS(a0)
    slli t5, t5, 3
    addi t5, t5, FR_SLOTS
    add t5, a2, t5
    la t6, vstack_end
    ld t6, 0(t6)
    bgeu t5, t6, op_call        # (R0005 through vm_push)
    sd t5, 0(t3)
    addi t1, a2, FR_SLOTS
    lw t2, I_B(s1)
    add t2, s3, t2
    ld t3, CF_NPARAMS(a0)
3:
    beqz t3, 4f
    ld t4, 0(t2)
    sd t4, 0(t1)
    addi t1, t1, 8
    addi t2, t2, 8
    addi t3, t3, -1
    j 3b
4:
    bgeu t1, t5, 5f
    sd zero, 0(t1)
    addi t1, t1, 8
    j 4b
5:
    sd s2, FR_PARENT(a2)
    sd a1, FR_FN(a2)
    ld t0, I_NODE(s1)
    sd t0, FR_CALL(a2)
    ld t0, GS_TARGS(a6)
    sd t0, FR_TARGS(a2)
    ld t0, GS_EFF(a6)
    sd t0, FR_EFFECTS(a2)
    sd a0, FR_CODE(a2)
    sd s1, FR_RPC(a2)
    lw t1, I_A(s1)
    sd t1, FR_RDST(a2)
    la t0, cur_frame
    sd a2, 0(t0)
    mv s2, a2
    addi s3, s2, FR_SLOTS
    la t4, steps                # the call counts as a step
    ld t5, 0(t4)
    addi t5, t5, 1
    sd t5, 0(t4)
    slli t5, t5, 48
    beqz t5, 6f
61:
    ld t0, FR_CODE(s2)
    ld s1, CF_CODE(t0)
    ld t6, I_HANDLER(s1)
    jr t6
6:
    call vm_tick_slow
    j 61b
7:
    mv a0, a6                   # (re)fill the record, then as above
    mv a1, t1
    call vm_generic_resolve
    ld a6, I_C(s1)
    ld a1, GS_DECL(a6)
    ld t0, N_A(a1)
    ld a5, N_B(t0)
    ld a0, FX_CODE(a5)
    j 2b
8:
    mv a0, a1                   # no code: through call_fn
    ld a1, I_NODE(s1)
    li a2, 0
    lw t1, I_B(s1)
    add a3, s3, t1
    li a4, 0
    call call_fn
    mv t3, a0
    j vm_store_t3

# ---- shared tails ----
vm_next:
    addi s1, s1, I_SIZE
    ld t6, I_HANDLER(s1)
    jr t6
vm_store_t3:                    # a = t3, next instruction
    lw t0, I_A(s1)
    add t0, s3, t0
    sd t3, 0(t0)
    addi s1, s1, I_SIZE
    ld t6, I_HANDLER(s1)
    jr t6
vm_overflow:
    ld a0, I_NODE(s1)
    call rt_overflow
vm_div_zero:
    ld a0, I_NODE(s1)
    call rt_div_zero
    j vm_exit                   # (not reached: runtime errors do not return)

# int vm_equal(void)
# `==` of the current instruction's operands, with the left operand's static type (as
# the tree walker); sets cur_node, since a long comparison can time out (R0009 points here).
# Reads the VM registers s1 (pc) and s3 (slots) of the vm_run that calls it.
    .globl vm_equal
vm_equal:
    addi sp, sp, -16
    sd ra, 8(sp)
    ld t0, I_NODE(s1)
    la t1, cur_node
    sd t0, 0(t1)
    ld t0, N_A(t0)
    ld a2, N_TYPE(t0)
    lw t1, I_B(s1)
    add t1, s3, t1
    ld a0, 0(t1)
    ld t2, I_C(s1)
    add t2, s3, t2
    ld a1, 0(t2)
    call values_equal
    ld ra, 8(sp)
    addi sp, sp, 16
    ret

# long vm_map_find(void)
# map_find of the current instruction's map (b) and key (c), with the map's key type.
    .globl vm_map_find
vm_map_find:
    addi sp, sp, -16
    sd ra, 8(sp)
    ld t0, I_NODE(s1)
    ld t0, N_A(t0)
    ld t0, N_TYPE(t0)
    ld a2, TY_A(t0)
    lw t1, I_B(s1)
    add t1, s3, t1
    ld a0, 0(t1)
    ld t2, I_C(s1)
    add t2, s3, t2
    ld a1, 0(t2)
    call map_find
    ld ra, 8(sp)
    addi sp, sp, 16
    ret

# void vm_tick_slow(void)
# The inline step just made steps a multiple of 65,536: undo it and let tick count it and check
# the clock, at the current instruction's node (s1 of the calling vm_run).
    .globl vm_tick_slow
vm_tick_slow:
    addi sp, sp, -16
    sd ra, 8(sp)
    la t0, steps
    ld t1, 0(t0)
    addi t1, t1, -1
    sd t1, 0(t0)
    ld a0, I_NODE(s1)
    call tick
    ld ra, 8(sp)
    addi sp, sp, 16
    ret

# void vm_generic_resolve(void *record, long eff0)
# Fills a generic call site's record for the running frame (s2) and the current instruction (s1,
# the call): the caller's type arguments and eff0 as the key, rt_targs and rt_es_subst as values.
    .globl vm_generic_resolve
vm_generic_resolve:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s4, 16(sp)
    sd s5, 8(sp)
    mv s4, a0
    mv s5, a1
    ld a0, I_NODE(s1)
    ld a0, N_E(a0)
    mv a1, s2
    call rt_targs
    sd a0, GS_TARGS(s4)
    sd zero, GS_EFF(s4)
    beqz s5, 1f
    mv a2, a0
    mv a0, s5
    ld a1, GS_DECL(s4)
    call rt_es_subst
    sd a0, GS_EFF(s4)
1:
    ld t0, FR_TARGS(s2)
    sd t0, GS_CTARGS(s4)
    sd s5, GS_EFF0(s4)
    li t0, 1
    sd t0, GS_VALID(s4)
    ld ra, 24(sp)
    ld s4, 16(sp)
    ld s5, 8(sp)
    addi sp, sp, 32
    ret

# int vm_str_cmp(void)
# str_cmp of the current instruction's operands (s1 and s3 of the calling vm_run).
    .globl vm_str_cmp
vm_str_cmp:
    addi sp, sp, -16
    sd ra, 8(sp)
    lw t1, I_B(s1)
    add t1, s3, t1
    ld a0, 0(t1)
    ld t2, I_C(s1)
    add t2, s3, t2
    ld a1, 0(t2)
    call str_cmp
    ld ra, 8(sp)
    addi sp, sp, 16
    ret

# void *vm_push(void *cf, void *decl, void *call, long *vals, void *closure)
# A new frame for compiled function cf at the top of the value stack (research R3, R4): R0005 at
# the call when it does not fit, zeroed slots, the arguments copied from vals, then today's rules
# for effects, type arguments and captured values. Sets cur_frame; FR_RPC 0 and FR_RDST -1 (the
# caller fills them in for a call inside the loop).
    .globl vm_push
vm_push:
    addi sp, sp, -80
    sd ra, 72(sp)
    sd s0, 64(sp)
    sd s1, 56(sp)
    sd s2, 48(sp)
    sd s3, 40(sp)
    sd s4, 32(sp)
    sd s5, 24(sp)
    sd s6, 16(sp)
    sd zero, 8(sp)              # words never written: no stale pointers for the collector
    sd zero, 0(sp)
    mv s0, a0                   # cf
    mv s1, a1                   # decl
    mv s2, a2                   # call node | 0
    mv s3, a3                   # argument values | 0
    mv s4, a4                   # closure | 0
    la t0, vsp
    ld s5, 0(t0)                # the new frame
    ld t1, CF_SLOTS(s0)
    slli t1, t1, 3
    addi t1, t1, FR_SLOTS
    add t2, s5, t1              # its end
    la t3, vstack_end
    ld t3, 0(t3)
    bgeu t2, t3, 90f
    sd t2, 0(t0)                # reserve it
    addi t3, s5, FR_SLOTS       # zero the slots
2:
    bgeu t3, t2, 3f
    sd zero, 0(t3)
    addi t3, t3, 8
    j 2b
3:
    beqz s3, 5f                 # the arguments
    ld t0, N_B(s1)
    ld t0, VEC_N(t0)
    li t1, 0
4:
    bge t1, t0, 5f
    slli t2, t1, 3
    add t3, s3, t2
    ld t3, 0(t3)
    add t2, s5, t2
    sd t3, FR_SLOTS(t2)
    addi t1, t1, 1
    j 4b
5:
    la t0, cur_frame
    ld s6, 0(t0)                # the caller's frame
    sd s6, FR_PARENT(s5)
    sd s1, FR_FN(s5)
    sd s2, FR_CALL(s5)
    sd zero, FR_TARGS(s5)
    sd zero, FR_EFFECTS(s5)
    sd s0, FR_CODE(s5)
    sd zero, FR_RPC(s5)
    li t0, -1
    sd t0, FR_RDST(s5)
    lhu t0, N_KIND(s1)          # effects: a test's grant; a function's declaration, or for an
    li t1, N_TEST               # anonymous function its caller's
    bne t0, t1, 60f
    ld t1, N_D(s1)
    beqz t1, 66f
    ld t1, FX_DECLARED(t1)
    j 65f
60:
    ld t1, N_A(s1)
    beqz t1, 66f
    ld t1, N_B(t1)
    beqz t1, 66f
    ld t1, FX_DECLARED(t1)
    bnez t1, 65f
    beqz s6, 66f
    ld t1, FR_EFFECTS(s6)
65:
    sd t1, FR_EFFECTS(s5)
66:
    beqz s2, 69f                # a generic call: its type arguments (the caller's resolved)
    lhu t1, N_KIND(s2)
    li t2, N_CALL
    bne t1, t2, 69f
    ld a0, N_E(s2)
    beqz a0, 69f
    mv a1, s6
    call rt_targs
    sd a0, FR_TARGS(s5)
    mv a2, a0                   # its declared effects with the effect parameters bound
    ld a0, FR_EFFECTS(s5)
    beqz a0, 69f
    mv a1, s1
    call rt_es_subst
    sd a0, FR_EFFECTS(s5)
69:
    beqz s4, 8f                 # a closure: its type arguments and captured values
    ld t1, CL_TARGS(s4)
    sd t1, FR_TARGS(s5)
    ld t1, N_A(s1)
    ld t1, N_B(t1)
    ld t2, FX_CAPS(t1)
    beqz t2, 8f
    ld t4, CL_N(s4)
    li t3, 0
7:
    bge t3, t4, 8f
    ld t5, VEC_DATA(t2)
    slli t6, t3, 3
    add t5, t5, t6
    ld t5, 0(t5)
    ld t5, CP_INNER(t5)
    slli t5, t5, 3
    add t5, s5, t5
    add t6, s4, t6
    ld t6, CL_CAPS(t6)
    sd t6, FR_SLOTS(t5)
    addi t3, t3, 1
    j 7b
8:
    la t0, cur_frame
    sd s5, 0(t0)
    mv a0, s5
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
90:
    mv a0, s2                   # R0005 at the call (does not return)
    call rt_stack_full
    li a0, 0
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
