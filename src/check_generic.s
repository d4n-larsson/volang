# check_generic.s: user-defined generics (research §7, contracts/grammar-v2.md).
#
# Type parameters are symbols (SYM_TPARAM) whose type is a TY_PARAM; generic bodies are checked
# once with them as opaque types. Type arguments are never written at calls: a call unifies its
# parameter types with the argument types (and its return type with the expected type) to bind
# them, E0310 when one stays unbound. The bindings are stored on the call (N_E) for the runtime.

    .include "defs.inc"

    .section .rodata
e0310:      .string "E0310"
e0312:      .string "E0312"
e0313:      .string "E0313"
m_infer:    .string "Cannot infer `$1` for `$2`: give the variable a type."
m_argn:     .string "`$1` takes $2 type argument(s), got $3."
m_unordered: .string "`$1` is not ordered, but `$2` needs `int`, `float` or `str`."
s_new:      .string "new "

    .text

# void tparams_push(void *tparams, void *owner)
# Brings a declaration's type parameters into scope.
    .globl tparams_push
tparams_push:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    mv s1, a1
    beqz s0, 9f
    li s2, 0
1:
    ld t0, VEC_N(s0)
    bge s2, t0, 9f
    mv a0, s0
    mv a1, s2
    call vec_get
    mv s3, a0                   # N_TPARAM
    li a0, TY_PARAM
    mv a1, s2
    ld a2, N_B(s3)
    mv a3, s1
    call ty_make
    mv a3, a0
    ld t0, N_A(s3)
    beqz t0, 2f
    ld a0, N_A(t0)
    li a1, SYM_TPARAM
    mv a2, s1
    ld a4, N_A(s3)
    call sym_new
    li t0, SYF_USED
    sd t0, SY_FLAGS(a0)
    call scope_add
2:
    addi s2, s2, 1
    j 1b
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# int ty_ordered(void *type)
# int, float, str, an `ordered` type parameter, or an error type.
    .globl ty_ordered
ty_ordered:
    ld t0, TY_KIND(a0)
    li t1, TY_INT
    beq t0, t1, 1f
    li t1, TY_FLOAT
    beq t0, t1, 1f
    li t1, TY_STR
    beq t0, t1, 1f
    li t1, TY_ERROR
    beq t0, t1, 1f
    li t1, TY_PARAM
    bne t0, t1, 2f
    ld t0, TY_B(a0)
    li t1, TPC_ORDERED
    beq t0, t1, 1f
2:
    li a0, 0
    ret
1:
    li a0, 1
    ret

# int check_targ(void *type, void *tparam, void *node, void *owner_name)
# E0313 when an `ordered` type parameter gets a type without an order. 1 = fine.
    .globl check_targ
check_targ:
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
    li a0, 1
    ld t0, N_B(s1)
    li t1, TPC_ORDERED
    bne t0, t1, 9f
    mv a0, s0
    call ty_ordered
    bnez a0, 9f
    mv a0, s0
    call ty_name
    mv a1, a0
    la a0, m_unordered
    mv a2, s3
    li a3, 0
    call msg
    mv a2, a0
    la a0, e0313
    mv a1, s2
    call ck_err
    li a0, 0
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# void *ty_argn_error(void *node, void *name, long want, long got)
# E0312; returns the error type.
    .globl ty_argn_error
ty_argn_error:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    mv s1, a1
    mv s2, a2
    mv a0, a3
    call int_to_str
    mv s3, a0
    mv a0, s2
    call int_to_str
    mv a2, a0
    la a0, m_argn
    mv a1, s1
    mv a3, s3
    call msg
    mv a2, a0
    la a0, e0312
    mv a1, s0
    call ck_err
    call bad_type
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# void *resolve_instance(void *node, void *decl, int kind, void *arg_nodes)
# `Name` or `Name[args]` for a record or variant: its type (an instance when generic), checking
# the number of arguments (E0312) and `ordered` parameters (E0313).
    .globl resolve_instance
resolve_instance:
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
    mv s2, a2
    mv s3, a3
    mv a0, s1
    call ty_tparams_of
    mv s4, a0                   # parameters
    li s5, 0
    beqz s4, 1f
    ld s5, VEC_N(s4)
1:
    li s6, 0                    # arguments written
    beqz s3, 2f
    ld s6, VEC_N(s3)
2:
    beq s5, s6, 3f
    mv a0, s0
    ld t0, N_A(s1)
    ld a1, N_A(t0)
    mv a2, s5
    mv a3, s6
    call ty_argn_error
    j 99f
3:
    bnez s5, 4f
    mv a0, s2                   # not generic
    li a1, 0
    li a2, 0
    mv a3, s1
    call ty_make
    j 99f
4:
    call vec_new
    sd a0, 0(sp)
    li s5, 0
5:
    bge s5, s6, 6f
    mv a0, s3
    mv a1, s5
    call vec_get
    sd a0, 8(sp)
    call resolve_type
    mv s0, a0
    mv a0, s4
    mv a1, s5
    call vec_get
    mv a1, a0
    mv a0, s0
    ld a2, 8(sp)
    ld t0, N_A(s1)
    ld a3, N_A(t0)
    call check_targ
    ld a0, 0(sp)
    mv a1, s0
    call vec_push
    addi s5, s5, 1
    j 5b
6:
    mv a0, s2
    mv a1, s1
    ld a2, 0(sp)
    call ty_instance
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

# void *field_type_in(void *record_type, void *field_type)
# A field's type in an instance: the declared type with the instance's arguments substituted.
    .globl field_type_in
field_type_in:
    ld t1, TY_KIND(a0)          # an instance that failed to resolve: no cascading mismatches
    li t2, TY_ERROR
    beq t1, t2, 3f
    ld t0, TY_B(a0)
    beqz t0, 1f
    ld t1, TY_KIND(a0)
    li t2, TY_RECORD
    beq t1, t2, 2f
    li t2, TY_VARIANT
    bne t1, t2, 1f
2:
    mv t3, a0
    mv a0, a1
    ld a1, TY_DECL(t3)
    mv a2, t0
    tail ty_subst
1:
    mv a0, a1
3:
    ret

# void *bindings_vec(long *bindings, long n)
    .globl bindings_vec
bindings_vec:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    mv s1, a1
    call vec_new
    mv s2, a0
    li s3, 0
1:
    bge s3, s1, 9f
    slli t0, s3, 3
    add t0, s0, t0
    ld a1, 0(t0)
    mv a0, s2
    call vec_push
    addi s3, s3, 1
    j 1b
9:
    mv a0, s2
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# int bind_rest(void *node, void *tparams, long *bindings, void *what)
# After inference: effect parameters left unbound are pure; a type parameter left unbound is
# E0310 (it becomes the error type). 1 when everything was bound.
    .globl bind_rest
bind_rest:
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
    li s5, 1
    li s4, 0
1:
    ld t0, VEC_N(s1)
    bge s4, t0, 9f
    slli t0, s4, 3
    add t0, s2, t0
    ld t1, 0(t0)
    addi s4, s4, 1
    bnez t1, 1b
    mv a0, s1
    addi a1, s4, -1
    call vec_get
    sd a0, 0(sp)
    ld t0, N_B(a0)
    li t1, TPC_EFFECTS
    bne t0, t1, 2f
    li a0, 0                    # an unused effect parameter: pure
    call es_new
    mv a3, a0
    li a0, TY_EFFSET
    li a1, 0
    li a2, 0
    call ty_make
    j 3f
2:
    li s5, 0
    ld t0, 0(sp)
    ld t0, N_A(t0)
    ld a1, N_A(t0)
    la a0, m_infer
    mv a2, s3
    li a3, 0
    call msg
    mv a2, a0
    la a0, e0310
    mv a1, s0
    call ck_err
    call bad_type
3:
    addi t0, s4, -1
    slli t0, t0, 3
    add t0, s2, t0
    sd a0, 0(t0)
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

# void *check_generic_call(void *call, void *decl, void *expected)
# A call of a generic function: arguments, inference, constraints, effects; the return type.
    .globl check_generic_call
check_generic_call:
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
    mv s0, a0
    mv s1, a1
    mv s7, a2
    mv a0, s1
    call ty_tparams_of
    mv s2, a0
    ld t0, VEC_N(s2)
    slli a0, t0, 3
    call alloc
    mv s3, a0                   # bindings
    ld t0, N_B(s1)
    mv a0, s0
    ld a1, VEC_N(t0)
    call check_argc
    beqz a0, 50f
    li s4, 0
1:
    ld t0, N_B(s1)              # each argument: unify its type with the parameter's
    ld t1, VEC_N(t0)
    bge s4, t1, 2f
    mv a0, t0
    mv a1, s4
    call vec_get
    ld s5, N_TYPE(a0)
    mv a0, s5
    call ty_has_params
    li a2, 0
    bnez a0, 11f
    mv a2, s5                   # nothing to infer here: check against it directly
11:
    mv a0, s0
    mv a1, s4
    call check_arg
    mv s6, a0
    mv a0, s5
    mv a1, s6
    mv a2, s1
    mv a3, s3
    call ty_unify
    bnez a0, 12f
    mv a0, s5                   # the parameter's type as far as it is known
    mv a1, s1
    mv a2, s3
    ld t0, VEC_N(s2)
    sd t0, 0(sp)
    call ty_subst_bindings
    mv a1, a0
    ld t0, N_B(s0)
    ld t0, VEC_DATA(t0)
    slli t1, s4, 3
    add t0, t0, t1
    ld a0, 0(t0)
    mv a2, s6
    call ck_mismatch
12:
    addi s4, s4, 1
    j 1b
2:
    beqz s7, 3f                 # the expected type can bind what the arguments did not
    ld a0, N_TYPE(s1)
    mv a1, s7
    mv a2, s1
    mv a3, s3
    call ty_unify
3:
    mv a0, s0                   # what stayed unbound
    mv a1, s2
    mv a2, s3
    ld t0, N_A(s0)
    ld a3, N_A(t0)
    call bind_rest
    bnez a0, 31f
    call diag_last              # E0310: an annotation fix when a later use shows the type
    mv a1, a0
    mv a0, s0
    call infer_fix
31:
    li s4, 0
4:
    ld t0, VEC_N(s2)            # `ordered` parameters
    bge s4, t0, 5f
    slli t1, s4, 3
    add t1, s3, t1
    ld a0, 0(t1)
    sd a0, 0(sp)
    mv a0, s2
    mv a1, s4
    call vec_get
    mv a1, a0
    ld a0, 0(sp)
    ld t0, N_B(a1)
    li t1, TPC_EFFECTS
    beq t0, t1, 41f
    mv a2, s0
    ld t0, N_A(s0)
    ld a3, N_A(t0)
    call check_targ
41:
    addi s4, s4, 1
    j 4b
5:
    mv a0, s3                   # bindings for the runtime
    ld a1, VEC_N(s2)
    call bindings_vec
    sd a0, N_E(s0)
    mv s4, a0
    li s5, 0                    # arguments again: function values must fit the instantiated type
6:
    ld t0, N_B(s1)
    ld t1, VEC_N(t0)
    bge s5, t1, 7f
    mv a0, t0
    mv a1, s5
    call vec_get
    ld a0, N_TYPE(a0)
    mv a1, s1
    mv a2, s4
    call ty_subst
    mv s6, a0
    ld t0, N_B(s0)
    mv a0, t0
    mv a1, s5
    call vec_get
    sd a0, 0(sp)
    ld a2, N_TYPE(a0)
    ld t0, TY_KIND(a2)
    li t1, TY_FUNC
    bne t0, t1, 61f
    ld a0, 0(sp)
    mv a1, s6
    call expect_type
61:
    addi s5, s5, 1
    j 6b
7:
    ld t0, N_A(s1)              # effects: the declaration with its effect parameters bound
    ld t0, N_B(t0)
    ld a0, FX_DECLARED(t0)
    beqz a0, 71f
    mv a1, s1
    mv a2, s4
    call es_subst
    call fx_value_call
71:
    ld a0, N_TYPE(s1)
    mv a1, s1
    mv a2, s4
    call ty_subst
    j 99f
50:
    call bad_type
99:
    ld ra, 88(sp)
    ld s0, 80(sp)
    ld s1, 72(sp)
    ld s2, 64(sp)
    ld s3, 56(sp)
    ld s4, 48(sp)
    ld s5, 40(sp)
    ld s6, 32(sp)
    ld s7, 24(sp)
    addi sp, sp, 96
    ret

# void *ty_subst_bindings(void *type, void *owner, long *bindings)
# ty_subst with a bindings array (as many as owner's type parameters).
    .globl ty_subst_bindings
ty_subst_bindings:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    mv s0, a0
    mv s1, a1
    mv s2, a2
    mv a0, s1
    call ty_tparams_of
    ld a1, VEC_N(a0)
    mv a0, s2
    call bindings_vec
    mv a2, a0
    mv a0, s0
    mv a1, s1
    call ty_subst
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    addi sp, sp, 48
    ret

# void *vcase_generic(void *node, void *decl, void *payload_type_node, void *expected)
# `Tree.leaf` / `Tree.node(x)` of a generic variant: arguments from the expected type, then from
# the payload; E0310 for what stays unknown.
    .globl vcase_generic
vcase_generic:
    addi sp, sp, -80
    sd ra, 72(sp)
    sd s0, 64(sp)
    sd s1, 56(sp)
    sd s2, 48(sp)
    sd s3, 40(sp)
    sd s4, 32(sp)
    sd s5, 24(sp)
    mv s0, a0
    mv s1, a1
    mv s2, a2
    mv s3, a3
    mv a0, s1
    call ty_tparams_of
    mv s4, a0
    ld t0, VEC_N(s4)
    slli a0, t0, 3
    call alloc
    mv s5, a0                   # bindings
    beqz s3, 2f                 # the expected type, when it is this variant
    ld t0, TY_KIND(s3)
    li t1, TY_VARIANT
    bne t0, t1, 2f
    ld t0, TY_DECL(s3)
    bne t0, s1, 2f
    ld t0, TY_B(s3)
    beqz t0, 2f
    ld t1, VEC_N(t0)
    ld t0, VEC_DATA(t0)
    mv t2, s5
1:
    beqz t1, 2f
    ld t3, 0(t0)
    sd t3, 0(t2)
    addi t0, t0, 8
    addi t2, t2, 8
    addi t1, t1, -1
    j 1b
2:
    ld t0, N_C(s0)              # the payload
    beqz t0, 4f
    beqz s2, 4f
    ld a0, N_C(s0)
    li a1, 0
    call check_expr
    sd a0, 0(sp)
    ld a0, N_TYPE(s2)
    ld a1, 0(sp)
    mv a2, s1
    mv a3, s5
    call ty_unify
    bnez a0, 4f
    ld a0, N_TYPE(s2)
    mv a1, s1
    mv a2, s5
    call ty_subst_bindings
    mv a1, a0
    ld a0, N_C(s0)
    ld a2, 0(sp)
    call ck_mismatch
4:
    call sb_new                 # `Tree.case` for messages
    sd a0, 8(sp)
    ld t0, N_A(s0)
    ld a1, N_A(t0)
    call sb_str
    ld a0, 8(sp)
    li a1, 46
    call sb_byte
    ld t0, N_B(s0)
    ld a0, 8(sp)
    ld a1, N_A(t0)
    call sb_str
    ld a0, 8(sp)
    call sb_finish
    mv a3, a0
    mv a0, s0
    mv a1, s4
    mv a2, s5
    call bind_rest
    mv a0, s5
    ld a1, VEC_N(s4)
    call bindings_vec
    mv a2, a0
    li a0, TY_VARIANT
    mv a1, s1
    call ty_instance
    ld ra, 72(sp)
    ld s0, 64(sp)
    ld s1, 56(sp)
    ld s2, 48(sp)
    ld s3, 40(sp)
    ld s4, 32(sp)
    ld s5, 24(sp)
    addi sp, sp, 80
    ret

    .section .rodata
e0311:      .string "E0311"
m_no_order: .string "`$1` has no order: declare it as `$1: ordered` to compare or sort its values."
s_ordered_fix: .string ": ordered"

    .text
# int ck_param_order(void *type, void *node)
# For a type parameter where an ordered type is needed: 1 when it is `ordered`; otherwise E0311
# with a fix adding `: ordered` to its declaration, and 2. 0 for other types.
    .globl ck_param_order
ck_param_order:
    addi sp, sp, -48            # 0..15: scratch
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    mv s0, a0
    mv s1, a1
    li a0, 0
    ld t0, TY_KIND(s0)
    li t1, TY_PARAM
    bne t0, t1, 9f
    ld t0, TY_B(s0)
    li a0, 1
    li t1, TPC_ORDERED
    beq t0, t1, 9f
    mv a0, s0
    call ty_param_name
    mv a3, a0
    la a0, e0311
    mv a1, s1
    la a2, m_no_order
    call ck_err1
    sd a0, 0(sp)
    ld a0, TY_DECL(s0)          # the fix: `T` -> `T: ordered` where it is declared
    call ty_tparams_of
    beqz a0, 8f
    ld a1, TY_A(s0)
    call vec_get
    ld t0, N_A(a0)
    beqz t0, 8f
    lwu t1, N_FILE(t0)
    sd t1, 8(sp)
    lwu s1, N_END(t0)
    la a0, s_ordered_fix
    call str_from_cstr
    mv a4, a0
    ld a0, 0(sp)
    ld a1, 8(sp)
    mv a2, s1
    mv a3, s1
    call diag_add_fix
8:
    li a0, 2
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    addi sp, sp, 48
    ret

    .section .rodata
s_colon_sp: .string ": "

    .text
# void infer_fix(void *call, void *diag)
# `let x = f();` where f's type argument cannot be inferred: when the first later statement that
# calls a generic function with `x` as an argument determines x's type, a fix adds `: Type`.
    .globl infer_fix
infer_fix:
    addi sp, sp, -64
    sd ra, 56(sp)
    sd s0, 48(sp)
    sd s1, 40(sp)
    sd s2, 32(sp)
    sd s3, 24(sp)
    sd s4, 16(sp)
    mv s0, a0
    mv s1, a1
    la t0, ck_cur_stmt
    ld s2, 0(t0)                # the let
    beqz s2, 9f
    lhu t0, N_KIND(s2)
    li t1, N_LET
    bne t0, t1, 9f
    ld t0, N_B(s2)
    bnez t0, 9f
    ld t0, N_C(s2)
    bne t0, s0, 9f
    ld t0, N_A(s2)
    beqz t0, 9f
    la t0, ck_blk
    ld s3, 0(t0)
    beqz s3, 9f
    la t0, ck_blk_idx
    ld s4, 0(t0)
1:
    ld t0, VEC_N(s3)
    bge s4, t0, 9f
    mv a0, s3
    mv a1, s4
    call vec_get
    addi s4, s4, 1
    lhu t0, N_KIND(a0)
    li t1, N_EXPR_STMT
    beq t0, t1, 2f
    li t1, N_LET
    bne t0, t1, 1b
    ld a0, N_C(a0)
    j 3f
2:
    ld a0, N_A(a0)
3:
    beqz a0, 1b
    lhu t0, N_KIND(a0)
    li t1, N_CALL
    bne t0, t1, 1b
    ld t0, N_A(s2)
    ld a1, N_A(t0)
    call infer_from_use
    beqz a0, 1b
    call ty_name                # `let x: Type = ...`
    sd a0, 0(sp)
    call sb_new
    sd a0, 8(sp)
    la a1, s_colon_sp
    call sb_cstr
    ld a0, 8(sp)
    ld a1, 0(sp)
    call sb_str
    ld a0, 8(sp)
    call sb_finish
    mv a4, a0
    ld t0, N_A(s2)
    mv a0, s1
    lwu a1, N_FILE(t0)
    lwu a2, N_END(t0)
    mv a3, a2
    call diag_add_fix
9:
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    addi sp, sp, 64
    ret

# void *infer_from_use(void *call, void *name)
# The type `name` must have as an argument of this generic call, from its other arguments (checked
# silently), or 0 when that does not determine it.
    .globl infer_from_use
infer_from_use:
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
    ld t0, N_A(s0)
    ld a0, N_A(t0)
    call scope_find
    beqz a0, 8f
    ld t0, SY_KIND(a0)
    li t1, SYM_FN
    bne t0, t1, 8f
    ld s2, SY_DECL(a0)          # the generic function
    mv a0, s2
    call ty_tparams_of
    beqz a0, 8f
    ld t0, VEC_N(a0)
    slli a0, t0, 3
    call alloc
    mv s3, a0                   # bindings
    ld t0, N_B(s0)
    ld t1, N_B(s2)
    ld t2, VEC_N(t0)
    ld t3, VEC_N(t1)
    bne t2, t3, 8f
    li s4, 0                    # which argument is the variable?
1:
    ld t0, N_B(s0)
    ld t1, VEC_N(t0)
    bge s4, t1, 8f
    mv a0, t0
    mv a1, s4
    call vec_get
    lhu t0, N_KIND(a0)
    li t1, N_IDENT
    bne t0, t1, 11f
    ld a0, N_A(a0)
    mv a1, s1
    call str_eq
    bnez a0, 2f
11:
    addi s4, s4, 1
    j 1b
2:
    la t0, diag_mute            # the other arguments, silently
    li t1, 1
    sd t1, 0(t0)
    li s5, 0
3:
    ld t0, N_B(s0)
    ld t1, VEC_N(t0)
    bge s5, t1, 4f
    beq s5, s4, 31f
    mv a0, t0
    mv a1, s5
    call vec_get
    li a1, 0
    call check_expr
    mv s6, a0
    ld a0, N_B(s2)
    mv a1, s5
    call vec_get
    ld a0, N_TYPE(a0)
    mv a1, s6
    mv a2, s2
    mv a3, s3
    call ty_unify
31:
    addi s5, s5, 1
    j 3b
4:
    la t0, diag_mute
    sd zero, 0(t0)
    ld a0, N_B(s2)
    mv a1, s4
    call vec_get
    ld a0, N_TYPE(a0)
    mv a1, s2
    mv a2, s3
    call ty_subst_bindings
    mv s6, a0
    call ty_has_params
    bnez a0, 8f
    mv a0, s6
    j 9f
8:
    li a0, 0
9:
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
