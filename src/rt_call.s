# Builtin dispatch: runs a builtin call on its argument values (ids: check_expr.s).
# ponytail: the bytecode engine reaches most builtins through builtin_call (cur_node, the effect
# check, the jump table), so builtin-heavy code gains little from spec 004; len, push and has have
# their own instructions. Upgrade: more dedicated instructions where measurements show they pay.
    .include "defs.inc"

    .text
# long builtin_call(void *call, long a0, long a1, long a2)
# Runs builtin call `call` on argument values (spec 004 research R8): sets cur_node, checks the
# effect, then the body. Bodies see s0 = the call node, s3 = the first argument, the arguments at
# 0/8/16(sp) and a scratch word at 24(sp).
    .globl builtin_call
builtin_call:
    addi sp, sp, -64
    sd ra, 56(sp)
    sd s0, 48(sp)
    sd s3, 40(sp)
    sd zero, 32(sp)             # words a body may not write: no stale pointers for the collector
    sd zero, 24(sp)
    mv s0, a0
    sd a1, 0(sp)
    sd a2, 8(sp)
    sd a3, 16(sp)
    la t0, cur_node
    sd s0, 0(t0)
    ld a0, N_D(s0)              # the effect must be declared (pure builtins: nothing to check)
    la t0, builtin_effects
    add t0, t0, a0
    lbu t0, 0(t0)
    beqz t0, 1f
    call rt_check_bit
1:
    ld s3, 0(sp)                # first argument
    ld a0, 0(sp)
    ld a1, 8(sp)
    ld a2, 16(sp)
    ld t0, N_D(s0)              # builtin id -> jump table
    addi t0, t0, -1
    li t1, B_ENS_FAIL
    bgeu t0, t1, 98f
    la t1, btab
    slli t0, t0, 3
    add t1, t1, t0
    ld t1, 0(t1)
    jr t1
b_todo:                         # R0011: "Hole reached: <desc>."
    call sb_new
    sd a0, 24(sp)
    la a1, s_hole
    call sb_cstr
    ld a0, 24(sp)
    mv a1, s3
    call sb_str
    ld a0, 24(sp)
    li a1, '.'
    call sb_byte
    ld a0, 24(sp)
    call sb_finish
    mv a1, a0
    la a0, r0011
    mv a2, s0
    call rt_raise
    j 98f
b_assume:                       # false: discard this property case (prop.s)
    bnez s3, 98f
    la a0, assume_code
    li a1, 0
    mv a2, s0
    call rt_raise
    j 98f
b_req_fail:                     # R0012 at the caller's call, the trace from the caller
    mv a0, s0
    la a1, s_pre
    call contract_msg
    mv a1, a0
    la t0, cur_frame
    ld t1, 0(t0)
    ld a2, FR_CALL(t1)
    bnez a2, 1f
    mv a2, s0                   # no caller node (main): the condition itself
1:
    ld t1, FR_PARENT(t1)
    sd t1, 0(t0)
    la a0, r0012
    call rt_raise
    j 98f
b_ens_fail:                     # R0013 at the exit (s3: its node), the trace from the function
    mv a0, s0
    la a1, s_post
    call contract_msg
    mv a1, a0
    la a0, r0013
    mv a2, s3
    call rt_raise
    j 98f
b_print:
    mv a1, a0
    li a0, 1
    li a2, 1
    call rt_print
    j 98f
b_write:
    mv a1, a0
    li a0, 1
    li a2, 0
    call rt_print
    j 98f
b_eprint:
    mv a1, a0
    li a0, 2
    li a2, 1
    call rt_print
    j 98f
b_read_line:                    # spec 006: the io_* wrappers record or replay these inputs
    mv a0, s0
    call io_read_line
    j 99f
b_read_all:
    mv a0, s0
    call io_read_all
    j 99f
b_read_file:
    mv a0, s3
    call io_read_file
    j 99f
b_write_file:
    mv a0, s3
    ld a1, 8(sp)
    call io_write_file
    j 99f
b_args:
    call rt_args
    j 99f
b_exit:
    mv a1, s0
    call rt_exit
    j 98f
b_now:
    call io_now_ms
    j 99f
b_env:
    call io_env
    j 99f
b_random_int:
    call rt_random_int
    j 99f
b_sort_by:
    mv a2, s0
    call hof_sort_by
    j 98f
b_map:
    mv a2, s0
    call hof_map
    j 99f
b_filter:
    mv a2, s0
    call hof_filter
    j 99f
b_fold:
    mv a3, s0
    call hof_fold
    j 99f
b_any:
    mv a2, s0
    li a3, 0
    call hof_any
    j 99f
b_all:
    mv a2, s0
    li a3, 1
    call hof_any
    j 99f
b_str:
    ld t0, N_B(s0)              # by the argument's type
    ld t0, VEC_DATA(t0)
    ld t0, 0(t0)
    ld t0, N_TYPE(t0)
    ld t0, TY_KIND(t0)
    li t1, TY_FLOAT
    beq t0, t1, 31f
    li t1, TY_BOOL
    beq t0, t1, 32f
    call int_to_str
    j 99f
31:
    call float_to_str
    j 99f
32:
    call bool_str
    j 99f
b_int:
    mv a1, s0
    call rt_int_of_float
    j 99f
b_float:
    call rt_float_of_int
    j 99f
b_parse_int:
    call rt_parse_int
    j 99f
b_parse_float:
    call rt_parse_float
    j 99f
b_len:
    ld t0, N_B(s0)
    ld t0, VEC_DATA(t0)
    ld t0, 0(t0)
    ld t0, N_TYPE(t0)
    ld t0, TY_KIND(t0)
    li t1, TY_STR
    beq t0, t1, 41f
    li t1, TY_MAP
    beq t0, t1, 42f
    ld a0, L_LEN(s3)
    j 99f
41:
    ld a0, STR_LEN(s3)
    j 99f
42:
    ld a0, M_COUNT(s3)
    j 99f
b_slice:
    mv a3, s0
    ld t0, N_B(s0)
    ld t0, VEC_DATA(t0)
    ld t0, 0(t0)
    ld t0, N_TYPE(t0)
    ld t0, TY_KIND(t0)
    li t1, TY_STR
    beq t0, t1, 51f
    call list_slice
    j 99f
51:
    call rt_slice_str
    j 99f
b_find:
    call rt_find
    j 99f
b_split:
    mv a2, s0
    call rt_split
    j 99f
b_join:
    call rt_join
    j 99f
b_trim:
    call rt_trim
    j 99f
b_chars:
    call rt_chars
    j 99f
b_push:
    call list_push
    j 98f
b_pop:
    call list_pop
    j 99f
b_sort:
    ld t0, N_B(s0)
    ld t0, VEC_DATA(t0)
    ld t0, 0(t0)
    ld t0, N_TYPE(t0)
    ld a0, TY_A(t0)             # element type
    call rt_concrete
    ld a1, TY_KIND(a0)
    mv a0, s3
    call list_sort
    j 98f
b_has:
    call map_key_type
    mv a2, a0
    ld a0, 0(sp)
    ld a1, 8(sp)
    call map_find
    slti a0, a0, 0
    xori a0, a0, 1
    j 99f
b_remove:
    call map_key_type
    mv a2, a0
    ld a0, 0(sp)
    ld a1, 8(sp)
    call map_remove
    j 98f
b_keys:
    call map_keys
    j 99f
b_expect:
    mv a1, s0
    call rt_expect
    j 98f
b_expect_eq:
    mv a2, s0
    call rt_expect_eq
    j 98f
b_ok:
    mv a1, a0
    li a0, 0
    call make_result
    j 99f
b_err:
    mv a1, a0
    li a0, 1
    call make_result
    j 99f
b_abs:
    call arg0_kind
    mv a1, a0
    ld a0, 0(sp)
    mv a2, s0
    call rt_abs
    j 99f
b_min:
    li a3, 0
    j 61f
b_max:
    li a3, 1
61:
    sd a3, 24(sp)
    call arg0_kind
    mv a2, a0
    ld a0, 0(sp)
    ld a1, 8(sp)
    ld a3, 24(sp)
    call rt_minmax
    j 99f
b_upper:
    li a1, 1
    call rt_case
    j 99f
b_lower:
    li a1, 0
    call rt_case
    j 99f
b_ord:
    mv a1, s0
    call rt_ord
    j 99f
b_chr:
    mv a1, s0
    call rt_chr
    j 99f
98:
    li a0, 0
99:
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s3, 40(sp)
    addi sp, sp, 64
    ret

# void *contract_msg(void *node, char *kind)
# "<kind> `<condition, whitespace runs as one space>` of `<function>` does not hold." for a
# compiled contract failure node (N_A the condition, N_C the function).
contract_msg:
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
    mv a1, s1
    call sb_cstr
    mv a0, s2
    li a1, '`'
    call sb_byte
    ld a0, N_A(s0)
    call src_text
    mv s1, a0
    li s3, 0                    # 1 after a whitespace character
    addi s1, s1, STR_DATA
    ld t0, STR_LEN-STR_DATA(s1)
    add t0, s1, t0
    sd t0, 0(sp)
1:
    ld t0, 0(sp)
    bgeu s1, t0, 4f
    lbu a1, 0(s1)
    addi s1, s1, 1
    li t1, ' '
    beq a1, t1, 2f
    li t1, 10
    beq a1, t1, 2f
    li t1, 9
    beq a1, t1, 2f
    li s3, 0
    mv a0, s2
    call sb_byte
    j 1b
2:
    bnez s3, 1b
    li s3, 1
    mv a0, s2
    li a1, ' '
    call sb_byte
    j 1b
4:
    mv a0, s2
    la a1, s_of
    call sb_cstr
    ld t0, N_C(s0)
    ld t0, N_A(t0)
    ld a1, N_A(t0)
    mv a0, s2
    call sb_str
    mv a0, s2
    la a1, s_not_hold
    call sb_cstr
    mv a0, s2
    call sb_finish
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

    .section .rodata
r0012:  .string "R0012"
r0013:  .string "R0013"
s_pre:  .string "Precondition "
s_post: .string "Postcondition "
s_of:   .string "` of `"
s_not_hold: .string "` does not hold."
r0011:  .string "R0011"
s_hole: .string "Hole reached: "
    .align 3
btab:
    .dword b_print, b_write, b_eprint, b_read_line, b_read_all, b_read_file, b_write_file, b_args
    .dword b_exit, b_now, b_str, b_int, b_float, b_parse_int, b_parse_float, b_len
    .dword b_slice, b_find, b_split, b_join, b_trim, b_chars, b_push, b_pop
    .dword b_sort, b_has, b_remove, b_keys, b_expect, b_expect_eq, b_ok, b_err
    .dword b_abs, b_min, b_max, b_upper, b_lower, b_ord, b_chr, b_env, b_random_int
    .dword b_sort_by, b_map, b_filter, b_fold, b_any, b_all, b_todo, b_assume, b_req_fail
    .dword b_ens_fail
    .text

# void *map_key_type(void)
# Key type of the map passed as the call's first argument (s0 of builtin_call is not visible here,
# so the caller passes nothing: this reads cur_node).
    .globl map_key_type
map_key_type:
    la t0, cur_node
    ld t0, 0(t0)
    ld t0, N_B(t0)
    ld t0, VEC_DATA(t0)
    ld t0, 0(t0)
    ld t0, N_TYPE(t0)
    ld a0, TY_A(t0)
    ret

# int arg0_kind(void)
# The type kind of the current builtin call's first argument.
    .globl arg0_kind
arg0_kind:
    addi sp, sp, -16
    sd ra, 8(sp)
    la t0, cur_node
    ld t0, 0(t0)
    ld t0, N_B(t0)
    ld t0, VEC_DATA(t0)
    ld t0, 0(t0)
    ld a0, N_TYPE(t0)
    call rt_concrete
    ld a0, TY_KIND(a0)
    ld ra, 8(sp)
    addi sp, sp, 16
    ret
