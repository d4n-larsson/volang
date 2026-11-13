# check_func.s: functions as values (research §4-§6, contracts/grammar-v2.md).
#
# Anonymous functions are checked in place with their own frame: parameters first, then one slot
# per captured `let` or parameter of an enclosing function (copied when the anonymous function is
# created), then their own locals. A capture through several nested anonymous functions is
# threaded through each of them. Capturing a `var` is E0505. Their effects are inferred: builtins
# and called functions' declarations collect in the anonymous function's FX_DIRECT.

    .include "defs.inc"

    .equ CX_NODE, 0
    .equ CX_RET, 8
    .equ CX_LOOPS, 16
    .equ CX_LOCALS, 24              # the enclosing frame's local count
    .equ CX_BROKE, 32
    .equ CX_FN, 40
    .equ CX_FX, 48
    .equ CX_SCOPE, 56
    .equ CX_STMT, 64
    .equ CX_SIZE, 72

    .section .bss
    .align 3
    .globl ck_depth
ck_depth:   .space 8                # anonymous-function nesting of the code being checked
ck_ctx:     .space 8                # vec of contexts, one per enclosing anonymous function
    .globl ck_cur_stmt
ck_cur_stmt: .space 8               # the statement being checked (E0505 fixes insert before it)

    .section .rodata
e0110:      .string "E0110"
e0302:      .string "E0302"
e0303:      .string "E0303"
e0301:      .string "E0301"
e0313:      .string "E0313"
e0403:      .string "E0403"
e0505:      .string "E0505"
m_missing_ret: .string "`$1` can reach its end without returning a value."
m_capture:  .string "`$1` is a `var`: anonymous functions cannot capture variables that change. Copy it into a `let` first."
m_not_fn:   .string "`$1` is not a function."
m_not_list: .string "Expected a list, found `$1`."
m_not_ordered: .string "`$1` is not ordered: a sort key must be `int`, `float` or `str`."
s_let:      .string "let "
s_now:      .string "_now"
s_now_eq:   .string "_now = "
s_semi_nl:  .string ";\n"

    .text

# void *ck_ctx_at(long level)
# The context of the anonymous function at nesting level (1 = outermost).
    .globl ck_ctx_at
ck_ctx_at:
    la t0, ck_ctx
    ld t0, 0(t0)
    ld t0, VEC_DATA(t0)
    addi a0, a0, -1
    slli a0, a0, 3
    add t0, t0, a0
    ld a0, 0(t0)
    ret

# void *check_lambda(void *node)
# Checks an anonymous function; returns its function type.
    .globl check_lambda
check_lambda:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    li a0, CX_SIZE              # save the enclosing context
    call alloc
    mv s1, a0
    sd s0, CX_NODE(s1)
    la t0, ck_ret
    ld t0, 0(t0)
    sd t0, CX_RET(s1)
    la t0, ck_loops
    ld t0, 0(t0)
    sd t0, CX_LOOPS(s1)
    la t0, ck_locals
    ld t0, 0(t0)
    sd t0, CX_LOCALS(s1)
    la t0, ck_broke
    ld t0, 0(t0)
    sd t0, CX_BROKE(s1)
    la t0, ck_fn
    ld t0, 0(t0)
    sd t0, CX_FN(s1)
    la t0, ck_scope
    ld t0, 0(t0)
    sd t0, CX_SCOPE(s1)
    la t0, ck_cur_stmt
    ld t0, 0(t0)
    sd t0, CX_STMT(s1)
    la t0, ck_ctx
    ld a0, 0(t0)
    bnez a0, 1f
    call vec_new
    la t0, ck_ctx
    sd a0, 0(t0)
1:
    mv a1, s1
    call vec_push
    la t0, ck_depth
    ld t1, 0(t0)
    addi t1, t1, 1
    sd t1, 0(t0)
    mv a0, s0                   # a fresh effect record and capture list
    call item_fx
    mv s2, a0
    li a0, 0
    call es_new
    sd a0, FX_DIRECT(s2)
    call vec_new
    sd a0, FX_CAPS(s2)
    call vec_new
    sd a0, FX_BADCAPS(s2)
    mv a0, s2
    call fx_swap
    sd a0, CX_FX(s1)
    la t0, ck_locals
    sd zero, 0(t0)
    la t0, ck_loops
    sd zero, 0(t0)
    la t0, ck_broke
    sd zero, 0(t0)
    la t0, ck_fn
    sd s0, 0(t0)
    ld s2, N_B(s0)              # parameters are the first locals
    li s3, 0
2:
    ld t0, VEC_N(s2)
    bge s3, t0, 3f
    mv a0, s2
    mv a1, s3
    call vec_get
    addi s3, s3, 1
    sd a0, 0(sp)
    ld a0, N_B(a0)
    call resolve_type
    ld t0, 0(sp)
    sd a0, N_TYPE(t0)
    mv a1, a0
    ld a0, N_A(t0)
    beqz a0, 2b
    li a2, 0
    call new_local
    ld t0, SY_SLOT(a0)
    ld t1, 0(sp)
    sd t0, N_E(t1)
    li t1, SYF_USED
    sd t1, SY_FLAGS(a0)
    j 2b
3:
    ld a0, N_C(s0)              # the return type (none: void)
    beqz a0, 31f
    call resolve_type
    j 32f
31:
    la t0, t_void
    ld a0, 0(t0)
32:
    sd a0, N_TYPE(s0)
    la t0, ck_ret
    sd a0, 0(t0)
    ld a0, N_D(s0)
    beqz a0, 4f
    call check_block
    ld t0, N_TYPE(s0)           # E0403, as for named functions
    ld t1, TY_KIND(t0)
    li t2, TY_VOID
    beq t1, t2, 4f
    li t2, TY_ERROR
    beq t1, t2, 4f
    ld a0, N_D(s0)
    call terminates
    bnez a0, 4f
    la a0, e0403
    ld a1, N_A(s0)
    la a2, m_missing_ret
    ld t0, N_A(s0)
    ld a3, N_A(t0)
    call ck_err1
4:
    ld t0, CX_STMT(s1)          # the statement holding this function (E0505 fixes go before it)
    la t1, ck_cur_stmt
    sd t0, 0(t1)
    mv a0, s0
    call report_bad_captures
    la t0, ck_locals            # this frame's size; then restore the enclosing context
    ld t0, 0(t0)
    sd t0, N_E(s0)
    ld a0, CX_SCOPE(s1)
    call scope_leave
    ld t0, CX_RET(s1)
    la t1, ck_ret
    sd t0, 0(t1)
    ld t0, CX_LOOPS(s1)
    la t1, ck_loops
    sd t0, 0(t1)
    ld t0, CX_LOCALS(s1)
    la t1, ck_locals
    sd t0, 0(t1)
    ld t0, CX_BROKE(s1)
    la t1, ck_broke
    sd t0, 0(t1)
    ld t0, CX_FN(s1)
    la t1, ck_fn
    sd t0, 0(t1)
    ld t0, CX_STMT(s1)
    la t1, ck_cur_stmt
    sd t0, 0(t1)
    ld a0, CX_FX(s1)
    call fx_swap
    la t0, ck_depth
    ld t1, 0(t0)
    addi t1, t1, -1
    sd t1, 0(t0)
    la t0, ck_ctx
    ld t0, 0(t0)
    ld t1, VEC_N(t0)
    addi t1, t1, -1
    sd t1, VEC_N(t0)
    mv a0, s0
    call ty_func_of
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# long ck_capture(void *sym, void *ident)
# The slot of a local in the code being checked: its own slot, or a capture slot when it was
# defined outside the innermost anonymous function. A captured `var` is recorded for E0505.
    .globl ck_capture
ck_capture:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    mv s1, a1
    la t0, ck_depth
    ld t1, 0(t0)
    ld t2, SY_DEPTH(s0)
    bge t2, t1, 8f
    ld t0, SY_FLAGS(s0)
    andi t0, t0, SYF_MUT
    beqz t0, 1f
    mv a0, t1                   # a var: remember the use in the innermost anonymous function
    call ck_ctx_at
    ld a0, CX_NODE(a0)
    call item_fx
    ld a0, FX_BADCAPS(a0)
    mv a1, s1
    call vec_push
    j 8f
1:
    mv a0, s0
    mv a1, t1
    call ck_capture_at
    j 9f
8:
    ld a0, SY_SLOT(s0)
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# long ck_capture_at(void *sym, long level)
# The capture slot of sym in the anonymous function at level, created (with its enclosing
# levels') on first use.
    .globl ck_capture_at
ck_capture_at:
    addi sp, sp, -64
    sd ra, 56(sp)
    sd s0, 48(sp)
    sd s1, 40(sp)
    sd s2, 32(sp)
    sd s3, 24(sp)
    sd s4, 16(sp)
    mv s0, a0
    mv s1, a1
    mv a0, s1
    call ck_ctx_at
    ld a0, CX_NODE(a0)
    call item_fx
    ld s2, FX_CAPS(a0)
    li s3, 0
1:
    ld t0, VEC_N(s2)
    bge s3, t0, 2f
    mv a0, s2
    mv a1, s3
    call vec_get
    addi s3, s3, 1
    ld t0, CP_SYM(a0)
    bne t0, s0, 1b
    ld a0, CP_INNER(a0)
    j 9f
2:
    ld t0, SY_DEPTH(s0)         # the slot one level out
    addi t1, s1, -1
    ld s4, SY_SLOT(s0)
    beq t0, t1, 3f
    mv a0, s0
    mv a1, t1
    call ck_capture_at
    mv s4, a0
3:
    la t0, ck_depth             # this level's next local: live, or saved by the next level
    ld t0, 0(t0)
    la t1, ck_locals
    beq s1, t0, 4f
    addi a0, s1, 1
    call ck_ctx_at
    addi t1, a0, CX_LOCALS
4:
    ld s3, 0(t1)
    addi t2, s3, 1
    sd t2, 0(t1)
    li a0, CP_SIZE
    call alloc
    sd s0, CP_SYM(a0)
    sd s4, CP_OUTER(a0)
    sd s3, CP_INNER(a0)
    mv a1, a0
    mv a0, s2
    call vec_push
    mv a0, s3
9:
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    addi sp, sp, 64
    ret

# void report_bad_captures(void *lambda)
# E0505 once per captured `var`, with a fix that copies it into a `let` before the statement and
# renames its uses inside the anonymous function.
    .globl report_bad_captures
report_bad_captures:
    addi sp, sp, -112           # 0..39: scratch
    sd ra, 104(sp)
    sd s0, 96(sp)
    sd s1, 88(sp)
    sd s2, 80(sp)
    sd s3, 72(sp)
    sd s4, 64(sp)
    sd s5, 56(sp)
    sd s6, 48(sp)
    sd s7, 40(sp)
    mv s0, a0
    call item_fx
    ld s1, FX_BADCAPS(a0)
    li s2, 0
1:
    ld t0, VEC_N(s1)
    bge s2, t0, 9f
    mv a0, s1
    mv a1, s2
    call vec_get
    addi s2, s2, 1
    mv s3, a0                   # an identifier using a var
    ld s4, N_B(s3)              # its symbol
    li t0, 0                    # reported already (an earlier use of the same var)?
2:
    addi t1, s2, -1
    bge t0, t1, 3f
    ld t2, VEC_DATA(s1)
    slli t3, t0, 3
    add t2, t2, t3
    ld t2, 0(t2)
    addi t0, t0, 1
    ld t2, N_B(t2)
    beq t2, s4, 1b
    j 2b
3:
    la a0, e0505
    mv a1, s3
    la a2, m_capture
    ld a3, N_A(s3)
    call ck_err1
    mv s5, a0
    la t0, ck_cur_stmt          # the fix: `let x_now = x;` + the text up to the function's end
    ld s6, 0(t0)
    beqz s6, 1b
    lwu a0, N_FILE(s6)
    call file_get
    ld s7, FILE_TEXT(a0)
    call sb_new
    sd a0, 0(sp)
    la a1, s_let
    call sb_cstr
    ld a0, 0(sp)
    ld a1, N_A(s3)
    call sb_str
    ld a0, 0(sp)
    la a1, s_now_eq
    call sb_cstr
    ld a0, 0(sp)
    ld a1, N_A(s3)
    call sb_str
    ld a0, 0(sp)
    la a1, s_semi_nl
    call sb_cstr
    lwu t0, N_START(s6)         # the statement's indentation
    mv t1, t0
4:
    beqz t1, 5f
    add t2, s7, t1
    lbu t2, -1(t2)
    li t3, 10
    beq t2, t3, 5f
    addi t1, t1, -1
    j 4b
5:
    ld a0, 0(sp)
    add a1, s7, t1
    sub a2, t0, t1
    call sb_bytes
    lwu t0, N_START(s6)         # copy up to the function's end, renaming the var's uses
    sd t0, 8(sp)
    li t0, 0
    sd t0, 16(sp)
6:
    ld t0, 16(sp)
    ld t1, VEC_N(s1)
    bge t0, t1, 7f
    addi t2, t0, 1
    sd t2, 16(sp)
    mv a0, s1
    mv a1, t0
    call vec_get
    ld t0, N_B(a0)
    bne t0, s4, 6b
    lwu t1, N_START(a0)
    ld t2, 8(sp)
    blt t1, t2, 6b
    sd a0, 24(sp)               # the use
    ld a0, 0(sp)
    add a1, s7, t2
    sub a2, t1, t2
    call sb_bytes
    ld t0, 24(sp)
    lwu t1, N_END(t0)
    sd t1, 8(sp)
    ld a0, 0(sp)
    ld a1, N_A(t0)
    call sb_str
    ld a0, 0(sp)
    la a1, s_now
    call sb_cstr
    j 6b
7:
    ld t0, 8(sp)
    lwu t1, N_END(s0)
    ld a0, 0(sp)
    add a1, s7, t0
    sub a2, t1, t0
    call sb_bytes
    ld a0, 0(sp)
    call sb_finish
    mv a4, a0
    mv a0, s5
    lwu a1, N_FILE(s6)
    lwu a2, N_START(s6)
    lwu a3, N_END(s0)
    call diag_add_fix
    j 1b
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
    addi sp, sp, 112
    ret

# void *call_name(void *call)
# How a call names its callee in messages: the name, or the callee's source text.
    .globl call_name
call_name:
    ld t0, N_A(a0)
    lhu t1, N_KIND(a0)
    li t2, N_CALL
    beq t1, t2, 1f
    lhu t1, N_KIND(t0)
    li t2, N_IDENT
    beq t1, t2, 1f
    mv a0, t0
    tail src_text
1:
    ld a0, N_A(t0)
    ret

# void *check_call_value(void *node, void *expected)
# A call of a function value: `f(x)`, `make()(x)`, `fs[0](x)`, `r.handler(x)`.
    .globl check_call_value
check_call_value:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    mv s3, a1
    lhu t0, N_FLAGS(s0)
    andi t0, t0, NF_METHOD
    beqz t0, 1f
    mv a0, s0
    mv a1, s3
    call check_method_form
    j 9f
1:
    ld a0, N_A(s0)
    li a1, 0
    call check_expr
    mv s1, a0                   # the callee's type
    call ty_bad
    bnez a0, 7f
    ld t0, TY_KIND(s1)
    li t1, TY_FUNC
    beq t0, t1, 2f
    ld a0, N_A(s0)
    call src_text
    mv a3, a0
    la a0, e0303
    ld a1, N_A(s0)
    la a2, m_not_fn
    call ck_err1
    j 7f
2:
    li t0, CV_CLOSURE
    sd t0, N_C(s0)
    ld t0, TY_A(s1)
    mv a0, s0
    ld a1, VEC_N(t0)
    call check_argc
    beqz a0, 4f
    li s2, 0
3:
    ld t0, TY_A(s1)
    ld t1, VEC_N(t0)
    bge s2, t1, 4f
    mv a0, t0
    mv a1, s2
    call vec_get
    mv a2, a0
    mv a0, s0
    mv a1, s2
    call check_arg
    addi s2, s2, 1
    j 3b
4:
    ld a0, TY_DECL(s1)
    call fx_value_call
    ld a0, TY_B(s1)
    j 9f
7:
    ld t0, N_B(s0)              # still check the arguments
    ld s2, VEC_N(t0)
    li s1, 0
71:
    bge s1, s2, 72f
    mv a0, s0
    mv a1, s1
    li a2, 0
    call check_arg
    addi s1, s1, 1
    j 71b
72:
    call bad_type
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# void *check_method_form(void *node, void *expected)
# `obj.name(args)`: a call of a record field of function type, or else v1's E0110 ("vo has no
# methods", fixed as `name(obj, args)`) and the rewritten call is checked instead.
    .globl check_method_form
check_method_form:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    mv s3, a1
    ld s1, N_A(s0)              # the N_FIELD callee
    la t0, diag_mute            # a silent look at the object's type
    li t1, 1
    sd t1, 0(t0)
    ld a0, N_A(s1)
    li a1, 0
    call check_expr
    la t0, diag_mute
    sd zero, 0(t0)
    ld t0, TY_KIND(a0)
    li t1, TY_RECORD
    bne t0, t1, 5f
    ld t0, TY_DECL(a0)          # a field with this name and a function type?
    ld a0, N_B(t0)
    ld t0, N_B(s1)
    ld a1, N_A(t0)
    sd a0, 0(sp)
    call member_index
    bltz a0, 5f
    mv a1, a0
    ld a0, 0(sp)
    call member_at
    ld t0, N_B(a0)
    beqz t0, 5f
    ld t0, N_TYPE(t0)
    beqz t0, 5f
    ld t1, TY_KIND(t0)
    li t2, TY_FUNC
    bne t1, t2, 5f
    lhu t0, N_FLAGS(s0)         # a field call
    li t1, NF_METHOD
    not t1, t1
    and t0, t0, t1
    sh t0, N_FLAGS(s0)
    mv a0, s0
    mv a1, s3
    call check_call_value
    j 9f
5:
    ld s2, N_E(s0)              # the parser's E0110 and its fix
    la a0, e0110
    li a1, SEV_ERROR
    ld a2, 32(s2)
    ld a3, 8(s2)
    ld a4, 16(s2)
    ld a5, 0(s2)
    call diag_add
    ld a4, 24(s2)
    ld a1, 32(s2)
    ld a2, 8(s2)
    ld a3, 16(s2)
    call diag_add_fix
    ld a0, N_D(s0)
    mv a1, s3
    call check_call
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# void *check_hof(void *call, long id)
# sort_by, map, filter, fold, any, all: a list, (fold: a start value,) and a function of its
# elements. The function's effects become the caller's.
    .globl check_hof
check_hof:
    addi sp, sp, -80
    sd ra, 72(sp)
    sd s0, 64(sp)
    sd s1, 56(sp)
    sd s2, 48(sp)
    sd s3, 40(sp)
    sd s4, 32(sp)
    sd s5, 24(sp)
    sd s6, 16(sp)
    mv s0, a0
    mv s1, a1
    li a1, 2
    li t0, B_FOLD
    bne s1, t0, 1f
    li a1, 3
1:
    mv a0, s0
    call check_argc
    beqz a0, 80f
    mv a0, s0                   # the list
    li a1, 0
    li a2, 0
    call check_arg
    mv s2, a0
    call ty_bad
    bnez a0, 81f
    ld t0, TY_KIND(s2)
    li t1, TY_LIST
    beq t0, t1, 2f
    mv a0, s2
    call ty_name
    mv a3, a0
    la a0, e0301
    ld a1, N_B(s0)
    ld a1, VEC_DATA(a1)
    ld a1, 0(a1)
    la a2, m_not_list
    call ck_err1
    j 81f
2:
    ld s3, TY_A(s2)             # T
    li s4, 0                    # fold: A
    li s6, 1                    # the function's argument index
    li t0, B_FOLD
    bne s1, t0, 3f
    mv a0, s0
    li a1, 1
    li a2, 0
    call check_arg
    mv s4, a0
    li s6, 2
3:
    mv a0, s0                   # the function
    mv a1, s6
    li a2, 0
    call check_arg
    mv s5, a0
    call ty_bad
    bnez a0, 81f
    call vec_new                # the parameters it must take: (T) or (A, T)
    sd a0, 0(sp)
    beqz s4, 4f
    mv a1, s4
    call vec_push
4:
    ld a0, 0(sp)
    mv a1, s3
    call vec_push
    ld t0, TY_KIND(s5)          # the return type it must have
    li t1, TY_FUNC
    ld a1, TY_B(s5)
    beq t0, t1, 41f
    la t0, t_error
    ld a1, 0(t0)
41:
    li t0, B_FOLD
    bne s1, t0, 42f
    mv a1, s4
42:
    li t0, B_FILTER
    beq s1, t0, 43f
    li t0, B_ANY
    beq s1, t0, 43f
    li t0, B_ALL
    bne s1, t0, 44f
43:
    la t0, t_bool
    ld a1, 0(t0)
44:
    sd a1, 8(sp)
    li t0, TY_FUNC              # the expected function type, with the argument's own effects
    ld t1, TY_KIND(s5)
    li a0, 0
    bne t1, t0, 45f
    ld a0, TY_DECL(s5)
45:
    bnez a0, 46f
    li a0, 0
    call es_new
46:
    mv a2, a0
    ld a0, 0(sp)
    ld a1, 8(sp)
    call ty_func
    mv s4, a0
    beq s4, s5, 5f
    mv a0, s0
    mv a1, s6
    ld a0, N_B(a0)
    call vec_get
    mv a1, s4
    mv a2, s5
    call ck_mismatch
    j 81f
5:
    ld a0, TY_DECL(s5)
    call fx_value_call
    li t0, B_SORT_BY            # the result
    bne s1, t0, 6f
    ld a0, TY_B(s5)             # the key must be ordered
    ld t0, TY_KIND(a0)
    li t1, TY_INT
    beq t0, t1, 51f
    li t1, TY_FLOAT
    beq t0, t1, 51f
    li t1, TY_STR
    beq t0, t1, 51f
    call ty_name
    mv a3, a0
    la a0, e0313
    mv a1, s0
    ld a1, N_B(a1)
    ld a1, VEC_DATA(a1)
    ld a1, 8(a1)
    la a2, m_not_ordered
    call ck_err1
51:
    la t0, t_void
    ld a0, 0(t0)
    j 99f
6:
    li t0, B_MAP
    bne s1, t0, 7f
    ld a0, TY_B(s5)
    call ty_list
    j 99f
7:
    li t0, B_FILTER
    bne s1, t0, 8f
    mv a0, s2
    j 99f
8:
    li t0, B_FOLD
    bne s1, t0, 82f             # any / all
    ld a0, TY_B(s5)
    j 99f
80:
81:
    li t0, B_ANY                # any / all: bool; otherwise an error type
    beq s1, t0, 82f
    li t0, B_ALL
    beq s1, t0, 82f
    call bad_type
    j 99f
82:
    la t0, t_bool
    ld a0, 0(t0)
99:
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
