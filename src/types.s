# types.s: function types (data-model.md "Types"; contracts/grammar-v2.md).
#
# TY_FUNC: TY_A = vec of parameter types, TY_B = return type, TY_DECL = effect set. Interned here
# (structurally: same parameters, return type and effects), so equal types are the same pointer.
# A function value fits an expected function type when the parameters and return type match and
# the expected effects cover the value's.

    .include "defs.inc"

    .section .bss
    .align 3
func_tab:   .space 8                # vec of interned TY_FUNC types
    .globl ck_exp_tnode
ck_exp_tnode: .space 8              # the type node an expected type came from (E0502 fixes), or 0

    .section .rodata
s_fn_open:  .string "fn("
s_comma:    .string ", "
s_arrow:    .string " -> "
s_uses:     .string " uses "
s_uses_fix: .string "uses "
e0502:      .string "E0502"
m_effects:  .string "This function uses $1, which `$2` does not allow."

    .text

# void *ty_func(void *params, void *ret, void *effects)
    .globl ty_func
ty_func:
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
    la t0, func_tab
    ld t0, 0(t0)
    bnez t0, 1f
    call vec_new
    la t0, func_tab
    sd a0, 0(t0)
1:
    li s3, 0
2:
    la t0, func_tab
    ld t0, 0(t0)
    ld t1, VEC_N(t0)
    bge s3, t1, 6f
    mv a0, t0
    mv a1, s3
    call vec_get
    addi s3, s3, 1
    mv s4, a0
    ld t0, TY_B(s4)
    bne t0, s1, 2b
    ld t0, TY_A(s4)             # the same parameter types
    ld t1, VEC_N(t0)
    ld t2, VEC_N(s0)
    bne t1, t2, 2b
    ld t3, VEC_DATA(t0)
    ld t4, VEC_DATA(s0)
3:
    beqz t1, 4f
    ld t5, 0(t3)
    ld t6, 0(t4)
    bne t5, t6, 2b
    addi t3, t3, 8
    addi t4, t4, 8
    addi t1, t1, -1
    j 3b
4:
    ld a0, TY_DECL(s4)
    mv a1, s2
    call es_equal
    beqz a0, 2b
    mv a0, s4
    j 9f
6:
    li a0, TYPE_SIZE
    call alloc
    li t0, TY_FUNC
    sd t0, TY_KIND(a0)
    sd s0, TY_A(a0)
    sd s1, TY_B(a0)
    sd s2, TY_DECL(a0)
    mv s4, a0
    mv a1, a0
    la t0, func_tab
    ld a0, 0(t0)
    call vec_push
    mv a0, s4
9:
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    addi sp, sp, 64
    ret

# void *ty_func_of(void *fn)
# The type of a named function or a checked anonymous function as a value.
    .globl ty_func_of
ty_func_of:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    mv s0, a0
    call vec_new
    mv s1, a0
    ld s2, N_B(s0)              # params
    li t0, 0
    sd t0, 0(sp)
1:
    ld t0, 0(sp)
    ld t1, VEC_N(s2)
    bge t0, t1, 2f
    addi t1, t0, 1
    sd t1, 0(sp)
    mv a0, s2
    mv a1, t0
    call vec_get
    ld a1, N_TYPE(a0)
    mv a0, s1
    call vec_push
    j 1b
2:
    mv a0, s0                   # declared effects (named), or the inferred ones (anonymous)
    call item_fx
    li a2, 0
    beqz a0, 3f
    ld a2, FX_DECLARED(a0)
    bnez a2, 3f
    ld a2, FX_DIRECT(a0)
3:
    bnez a2, 4f
    li a0, 0
    call es_new
    mv a2, a0
4:
    mv a0, s1
    ld a1, N_TYPE(s0)
    call ty_func
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    addi sp, sp, 48
    ret

# int ty_func_fits(void *value, void *expected)
# 0: fits; 1: different parameters or return type; 2: the value has effects the type does not allow.
    .globl ty_func_fits
ty_func_fits:
    beq a0, a1, 8f
    ld t0, TY_KIND(a0)
    li t2, TY_FUNC
    bne t0, t2, 7f
    ld t1, TY_KIND(a1)
    bne t1, t2, 7f
    ld t0, TY_B(a0)
    ld t1, TY_B(a1)
    bne t0, t1, 7f
    ld t0, TY_A(a0)
    ld t1, TY_A(a1)
    ld t2, VEC_N(t0)
    ld t3, VEC_N(t1)
    bne t2, t3, 7f
    ld t0, VEC_DATA(t0)
    ld t1, VEC_DATA(t1)
1:
    beqz t2, 2f
    ld t3, 0(t0)
    ld t4, 0(t1)
    bne t3, t4, 7f
    addi t0, t0, 8
    addi t1, t1, 8
    addi t2, t2, -1
    j 1b
2:
    addi sp, sp, -16
    sd ra, 8(sp)
    mv t0, a0
    ld a0, TY_DECL(a1)
    ld a1, TY_DECL(t0)
    call es_covers
    ld ra, 8(sp)
    addi sp, sp, 16
    li t0, 2
    beqz a0, 3f
    li t0, 0
3:
    mv a0, t0
    ret
7:
    li a0, 1
    ret
8:
    li a0, 0
    ret

# void ty_func_name_into(void *sb, void *type)
# `fn(int, str) -> bool uses console` (no `-> void`, no clause when pure).
    .globl ty_func_name_into
ty_func_name_into:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    mv s1, a1
    la a1, s_fn_open
    call sb_cstr
    ld s2, TY_A(s1)
    li s3, 0
1:
    ld t0, VEC_N(s2)
    bge s3, t0, 2f
    beqz s3, 11f
    mv a0, s0
    la a1, s_comma
    call sb_cstr
11:
    mv a0, s2
    mv a1, s3
    call vec_get
    mv a1, a0
    mv a0, s0
    call ty_name_into
    addi s3, s3, 1
    j 1b
2:
    mv a0, s0
    li a1, 41                   # ')'
    call sb_byte
    ld t0, TY_B(s1)
    ld t1, TY_KIND(t0)
    li t2, TY_VOID
    beq t1, t2, 3f
    mv a0, s0
    la a1, s_arrow
    call sb_cstr
    mv a0, s0
    ld a1, TY_B(s1)
    call ty_name_into
3:
    ld a0, TY_DECL(s1)
    call es_is_empty
    bnez a0, 9f
    mv a0, s0
    la a1, s_uses
    call sb_cstr
    ld a0, TY_DECL(s1)
    mv a1, s0
    call es_render
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# void ty_effects_error(void *node, void *expected, void *found)
# E0502: a function value with effects the expected function type does not allow. The fix adds
# them to that type when the expected type was written as a type in this program (ck_exp_tnode).
    .globl ty_effects_error
ty_effects_error:
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
    ld a0, TY_DECL(s1)
    ld a1, TY_DECL(s2)
    call es_missing
    call fx_render
    mv s3, a0
    mv a0, s1
    call ty_name
    mv a2, a0
    la a0, m_effects
    mv a1, s3
    li a3, 0
    call msg
    mv a2, a0
    la a0, e0502
    mv a1, s0
    call ck_err
    mv s3, a0                   # the diagnostic
    la t0, ck_exp_tnode
    ld s4, 0(t0)
    beqz s4, 9f
    ld t0, N_A(s4)
    li t1, TY_FUNC
    bne t0, t1, 9f
    call sb_new
    sd a0, 0(sp)
    ld t0, N_D(s4)              # an existing clause is rewritten, otherwise one is appended
    bnez t0, 1f
    mv a0, a0
    la a1, s_uses
    call sb_cstr
    j 2f
1:
    ld a0, 0(sp)
    la a1, s_uses_fix
    call sb_cstr
2:
    ld a0, TY_DECL(s1)
    ld a1, TY_DECL(s2)
    call es_union
    ld a1, 0(sp)
    call es_render
    ld a0, 0(sp)
    call sb_finish
    mv a4, a0
    mv a0, s3
    lwu a1, N_FILE(s4)
    ld t0, N_D(s4)
    beqz t0, 3f
    lwu a2, N_START(t0)
    lwu a3, N_END(t0)
    j 4f
3:
    lwu a2, N_END(s4)
    mv a3, a2
4:
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

# ---- generics (research §7) ----
# TY_PARAM: TY_A = index in the owner's type-parameter list, TY_B = constraint (TPC_*), TY_DECL =
# the owner (N_FN, N_RECORD or N_VARIANT). Instances of generic records and variants are
# TY_RECORD/TY_VARIANT with TY_B = vec of type arguments, interned here by declaration and
# arguments. Effect parameters are TY_PARAMs with TPC_EFFECTS in an effect set's ES_EVARS; their
# bindings are effect sets.

    .section .bss
    .align 3
inst_tab:   .space 8

    .section .rodata
s_lbrack:   .string "["
s_rbrack:   .string "]"
s_qmark:    .string "?"

    .text

# void *ty_tparams_of(void *decl)
# The type-parameter list (vec of N_TPARAM) of a function, record or variant, or 0.
    .globl ty_tparams_of
ty_tparams_of:
    beqz a0, 9f
    lhu t0, N_KIND(a0)
    li t1, N_RECORD
    beq t0, t1, 1f
    li t1, N_VARIANT
    beq t0, t1, 1f
    li t1, N_FN
    bne t0, t1, 8f
    ld t0, N_A(a0)
    beqz t0, 8f
    ld t0, N_B(t0)
    beqz t0, 8f
    ld a0, FX_TPARAMS(t0)
    ret
1:
    ld a0, N_C(a0)
    ret
8:
    li a0, 0
9:
    ret

# void *ty_param_name(void *param)
    .globl ty_param_name
ty_param_name:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    mv s0, a0
    ld a0, TY_DECL(s0)
    call ty_tparams_of
    beqz a0, 8f
    ld a1, TY_A(s0)
    call vec_get
    ld a0, N_A(a0)
    ld a0, N_A(a0)
    j 9f
8:
    la a0, s_qmark
    call str_from_cstr
9:
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret

# void *ty_instance(int kind, void *decl, void *args)
# The record or variant type `Name[args]` (interned).
    .globl ty_instance
ty_instance:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    mv s1, a1
    mv s2, a2
    la t0, inst_tab
    ld t0, 0(t0)
    bnez t0, 1f
    call vec_new
    la t0, inst_tab
    sd a0, 0(t0)
1:
    li s3, 0
2:
    la t0, inst_tab
    ld t0, 0(t0)
    ld t1, VEC_N(t0)
    bge s3, t1, 6f
    ld t0, VEC_DATA(t0)
    slli t1, s3, 3
    add t0, t0, t1
    ld a0, 0(t0)
    addi s3, s3, 1
    ld t1, TY_DECL(a0)
    bne t1, s1, 2b
    ld t1, TY_KIND(a0)
    bne t1, s0, 2b
    ld t1, TY_B(a0)
    ld t2, VEC_N(t1)
    ld t3, VEC_N(s2)
    bne t2, t3, 2b
    ld t1, VEC_DATA(t1)
    ld t3, VEC_DATA(s2)
3:
    beqz t2, 9f
    ld t4, 0(t1)
    ld t5, 0(t3)
    bne t4, t5, 2b
    addi t1, t1, 8
    addi t3, t3, 8
    addi t2, t2, -1
    j 3b
6:
    li a0, TYPE_SIZE
    call alloc
    sd s0, TY_KIND(a0)
    sd s2, TY_B(a0)
    sd s1, TY_DECL(a0)
    mv s3, a0
    mv a1, a0
    la t0, inst_tab
    ld a0, 0(t0)
    call vec_push
    mv a0, s3
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# void ty_args_into(void *sb, void *args)
# `[int, str]`
    .globl ty_args_into
ty_args_into:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    sd s2, 0(sp)
    mv s0, a0
    mv s1, a1
    la a1, s_lbrack
    call sb_cstr
    li s2, 0
1:
    ld t0, VEC_N(s1)
    bge s2, t0, 2f
    beqz s2, 11f
    mv a0, s0
    la a1, s_comma
    call sb_cstr
11:
    mv a0, s1
    mv a1, s2
    call vec_get
    mv a1, a0
    mv a0, s0
    call ty_name_into
    addi s2, s2, 1
    j 1b
2:
    mv a0, s0
    la a1, s_rbrack
    call sb_cstr
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    ld s2, 0(sp)
    addi sp, sp, 32
    ret

# int ty_has_params(void *type)
# 1 when the type mentions a type or effect parameter.
    .globl ty_has_params
ty_has_params:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    li a0, 0
    beqz s0, 9f
    ld t0, TY_KIND(s0)
    li t1, TY_PARAM
    li a0, 1
    beq t0, t1, 9f
    li t1, TY_LIST
    beq t0, t1, 1f
    li t1, TY_RESULT
    beq t0, t1, 1f
    li t1, TY_MAP
    beq t0, t1, 2f
    li t1, TY_FUNC
    beq t0, t1, 3f
    li t1, TY_RECORD
    beq t0, t1, 4f
    li t1, TY_VARIANT
    beq t0, t1, 4f
    li t1, TY_EFFSET
    beq t0, t1, 5f
    li a0, 0
    j 9f
5:
    ld a0, TY_DECL(s0)
    call es_evar_count
    snez a0, a0
    j 9f
1:
    ld a0, TY_A(s0)
    call ty_has_params
    j 9f
2:
    ld a0, TY_A(s0)
    call ty_has_params
    bnez a0, 9f
    ld a0, TY_B(s0)
    call ty_has_params
    j 9f
3:
    ld t0, TY_DECL(s0)          # effect parameters
    ld t0, ES_EVARS(t0)
    li a0, 1
    beqz t0, 31f
    ld t0, VEC_N(t0)
    bnez t0, 9f
31:
    ld a0, TY_B(s0)
    call ty_has_params
    bnez a0, 9f
    ld a0, TY_A(s0)
    call ty_vec_has_params
    j 9f
4:
    ld a0, TY_B(s0)
    li t0, 0
    beqz a0, 41f
    call ty_vec_has_params
    j 9f
41:
    li a0, 0
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# int ty_vec_has_params(void *types)
    .globl ty_vec_has_params
ty_vec_has_params:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    li s1, 0
1:
    ld t0, VEC_N(s0)
    li a0, 0
    bge s1, t0, 9f
    mv a0, s0
    mv a1, s1
    call vec_get
    addi s1, s1, 1
    call ty_has_params
    beqz a0, 1b
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void *ty_subst(void *type, void *owner, void *args)
# The type with owner's type parameters replaced by args (an argument 0 leaves the parameter).
    .globl ty_subst
ty_subst:
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
    beqz s0, 90f
    beqz s2, 90f
    ld t0, TY_KIND(s0)
    li t1, TY_PARAM
    beq t0, t1, 1f
    li t1, TY_LIST
    beq t0, t1, 2f
    li t1, TY_RESULT
    beq t0, t1, 3f
    li t1, TY_MAP
    beq t0, t1, 4f
    li t1, TY_FUNC
    beq t0, t1, 5f
    li t1, TY_RECORD
    beq t0, t1, 6f
    li t1, TY_VARIANT
    beq t0, t1, 6f
    li t1, TY_EFFSET
    beq t0, t1, 7f
    j 90f
7:
    ld a0, TY_DECL(s0)
    mv a1, s1
    mv a2, s2
    call es_subst
    mv a3, a0
    li a0, TY_EFFSET
    li a1, 0
    li a2, 0
    call ty_make
    j 99f
1:
    ld t0, TY_DECL(s0)
    li t1, -1                   # owner -1: any owner (at run time, a frame's own parameters)
    beq s1, t1, 11f
    bne t0, s1, 90f
11:
    ld t1, TY_A(s0)
    ld t2, VEC_N(s2)
    bgeu t1, t2, 90f
    ld t2, VEC_DATA(s2)
    slli t1, t1, 3
    add t2, t2, t1
    ld a0, 0(t2)
    beqz a0, 90f
    j 99f
2:
    ld a0, TY_A(s0)
    mv a1, s1
    mv a2, s2
    call ty_subst
    call ty_list
    j 99f
3:
    ld a0, TY_A(s0)
    mv a1, s1
    mv a2, s2
    call ty_subst
    call ty_result
    j 99f
4:
    ld a0, TY_A(s0)
    mv a1, s1
    mv a2, s2
    call ty_subst
    mv s3, a0
    ld a0, TY_B(s0)
    mv a1, s1
    mv a2, s2
    call ty_subst
    mv a2, a0
    mv a1, s3
    li a0, TY_MAP
    li a3, 0
    call ty_make
    j 99f
5:
    ld a0, TY_A(s0)
    mv a1, s1
    mv a2, s2
    call ty_subst_vec
    mv s3, a0
    ld a0, TY_B(s0)
    mv a1, s1
    mv a2, s2
    call ty_subst
    mv s4, a0
    ld a0, TY_DECL(s0)
    mv a1, s1
    mv a2, s2
    call es_subst
    mv a2, a0
    mv a0, s3
    mv a1, s4
    call ty_func
    j 99f
6:
    ld a0, TY_B(s0)
    beqz a0, 90f
    mv a1, s1
    mv a2, s2
    call ty_subst_vec
    mv a2, a0
    ld a0, TY_KIND(s0)
    ld a1, TY_DECL(s0)
    call ty_instance
    j 99f
90:
    mv a0, s0
99:
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    addi sp, sp, 64
    ret

# void *ty_subst_vec(void *types, void *owner, void *args)
    .globl ty_subst_vec
ty_subst_vec:
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
    call vec_new
    mv s3, a0
    li s4, 0
1:
    ld t0, VEC_N(s0)
    bge s4, t0, 9f
    mv a0, s0
    mv a1, s4
    call vec_get
    mv a1, s1
    mv a2, s2
    call ty_subst
    mv a1, a0
    mv a0, s3
    call vec_push
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

# int ty_unify(void *pattern, void *type, void *owner, long *bindings)
# Matches type against pattern, binding owner's parameters (bindings[index]; effect parameters
# bind to effect sets). 1 when they match; errors match anything.
    .globl ty_unify
ty_unify:
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
    li a0, 1
    beqz s0, 99f
    beqz s1, 99f
    ld t0, TY_KIND(s1)
    li t1, TY_ERROR
    beq t0, t1, 99f
    ld t0, TY_KIND(s0)
    li t1, TY_PARAM
    bne t0, t1, 2f
    ld t1, TY_DECL(s0)
    bne t1, s2, 2f
    ld t1, TY_A(s0)             # owner's parameter: bind or compare
    slli t1, t1, 3
    add t1, s3, t1
    ld t2, 0(t1)
    bnez t2, 1f
    sd s1, 0(t1)
    li a0, 1
    j 99f
1:
    sub t0, t2, s1
    seqz a0, t0
    j 99f
2:
    bne s0, s1, 21f             # the same type matches; with parameters it still binds them
    mv a0, s0
    call ty_has_params
    beqz a0, 98f
21:
    ld t0, TY_KIND(s0)
    ld t1, TY_KIND(s1)
    bne t0, t1, 97f
    li t1, TY_LIST
    beq t0, t1, 3f
    li t1, TY_RESULT
    beq t0, t1, 3f
    li t1, TY_MAP
    beq t0, t1, 4f
    li t1, TY_FUNC
    beq t0, t1, 5f
    li t1, TY_RECORD
    beq t0, t1, 7f
    li t1, TY_VARIANT
    beq t0, t1, 7f
    j 97f
3:
    ld a0, TY_A(s0)
    ld a1, TY_A(s1)
    mv a2, s2
    mv a3, s3
    call ty_unify
    j 99f
4:
    ld a0, TY_A(s0)
    ld a1, TY_A(s1)
    mv a2, s2
    mv a3, s3
    call ty_unify
    beqz a0, 99f
    ld a0, TY_B(s0)
    ld a1, TY_B(s1)
    mv a2, s2
    mv a3, s3
    call ty_unify
    j 99f
5:
    ld t0, TY_A(s0)             # same number of parameters
    ld t1, TY_A(s1)
    ld t2, VEC_N(t0)
    ld t3, VEC_N(t1)
    bne t2, t3, 97f
    li s4, 0
51:
    ld t0, TY_A(s0)
    ld t1, VEC_N(t0)
    bge s4, t1, 52f
    mv a0, t0
    mv a1, s4
    call vec_get
    mv s5, a0
    ld a0, TY_A(s1)
    mv a1, s4
    call vec_get
    mv a1, a0
    mv a0, s5
    mv a2, s2
    mv a3, s3
    call ty_unify
    beqz a0, 99f
    addi s4, s4, 1
    j 51b
52:
    ld a0, TY_B(s0)
    ld a1, TY_B(s1)
    mv a2, s2
    mv a3, s3
    call ty_unify
    beqz a0, 99f
    ld t0, TY_DECL(s0)          # an effect parameter binds to the value's effects
    ld t0, ES_EVARS(t0)
    beqz t0, 98f
    ld t1, VEC_N(t0)
    beqz t1, 98f
    ld t0, VEC_DATA(t0)
    ld t0, 0(t0)
    ld t1, TY_DECL(t0)
    bne t1, s2, 98f
    ld t1, TY_A(t0)
    slli t1, t1, 3
    add t1, s3, t1
    ld t2, 0(t1)
    bnez t2, 98f
    sd t1, 0(sp)
    li a0, TY_EFFSET
    li a1, 0
    li a2, 0
    ld a3, TY_DECL(s1)
    call ty_make
    ld t1, 0(sp)
    sd a0, 0(t1)
    j 98f
7:
    ld t0, TY_DECL(s0)
    ld t1, TY_DECL(s1)
    bne t0, t1, 97f
    ld t0, TY_B(s0)
    ld t1, TY_B(s1)
    beqz t0, 97f
    beqz t1, 97f
    li s4, 0
71:
    ld t0, TY_B(s0)
    ld t1, VEC_N(t0)
    bge s4, t1, 98f
    mv a0, t0
    mv a1, s4
    call vec_get
    mv s5, a0
    ld a0, TY_B(s1)
    mv a1, s4
    call vec_get
    mv a1, a0
    mv a0, s5
    mv a2, s2
    mv a3, s3
    call ty_unify
    beqz a0, 99f
    addi s4, s4, 1
    j 71b
97:
    li a0, 0
    j 99f
98:
    li a0, 1
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

# ---- run-time memo (spec 003 research §6, data-model.md "Run-time memo") ----
# Generic code resolves its types at run time. The results are permanent (interned types and
# type-argument vecs in the permanent arena), so they are memoized by the pointers they depend on:
# a hot generic loop pays one table lookup instead of a substitution walk and an allocation.
    .equ MEMO_HASP, 1           # type -> 1 + ty_has_params(type)
    .equ MEMO_HASPV, 2          # vec of types -> 1 + ty_vec_has_params(vec)
    .equ MEMO_TARGS, 3          # (bindings, caller's type arguments) -> type-argument vec
    .equ MEMO_CONCRETE, 4       # (type, type arguments) -> concrete type
    .equ MEMO_ESUBST, 5         # (effect set, type arguments) -> effect set
    .equ ME_A, 0
    .equ ME_B, 8
    .equ ME_KINDW, 16
    .equ ME_VALUE, 24
    .equ MEMO_ENTRY, 32

    .section .bss
    .align 3
memo_tab:   .space 8            # entries {a, b, kind, value}, open addressing, cap a power of two
memo_cap:   .space 8
memo_n:     .space 8
    .text

# long memo_slot(long kind, long a, long b)
# The first probe index for a key (memo_cap must be non-zero).
memo_slot:
    li t0, 0x9E3779B97F4A7C15
    mul t1, a1, t0
    li t0, 0xC2B2AE3D27D4EB4F
    mul t2, a2, t0
    xor t1, t1, t2
    add t1, t1, a0
    srli t2, t1, 29
    xor t1, t1, t2
    la t0, memo_cap
    ld t0, 0(t0)
    addi t0, t0, -1
    and a0, t1, t0
    ret

# long memo_get(long kind, long a, long b)
# The stored value, or 0.
    .globl memo_get
memo_get:
    la t0, memo_cap
    ld t0, 0(t0)
    bnez t0, 1f
    li a0, 0
    ret
1:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    sd s2, 0(sp)
    mv s0, a0
    mv s1, a1
    mv s2, a2
    call memo_slot
    la t0, memo_tab
    ld t0, 0(t0)
    la t1, memo_cap
    ld t1, 0(t1)
    addi t1, t1, -1
2:
    slli t2, a0, 5
    add t2, t0, t2
    ld t3, ME_KINDW(t2)
    beqz t3, 8f
    bne t3, s0, 3f
    ld t4, ME_A(t2)
    bne t4, s1, 3f
    ld t4, ME_B(t2)
    bne t4, s2, 3f
    ld a0, ME_VALUE(t2)
    j 9f
3:
    addi a0, a0, 1
    and a0, a0, t1
    j 2b
8:
    li a0, 0
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    ld s2, 0(sp)
    addi sp, sp, 32
    ret

# void memo_insert(long kind, long a, long b, long value)
# Stores into the current table, which has room (no growth).
memo_insert:
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
    call memo_slot
    la t0, memo_tab
    ld t0, 0(t0)
    la t1, memo_cap
    ld t1, 0(t1)
    addi t1, t1, -1
1:
    slli t2, a0, 5
    add t2, t0, t2
    ld t3, ME_KINDW(t2)
    beqz t3, 2f
    addi a0, a0, 1
    and a0, a0, t1
    j 1b
2:
    sd s1, ME_A(t2)
    sd s2, ME_B(t2)
    sd s0, ME_KINDW(t2)
    sd s3, ME_VALUE(t2)
    la t0, memo_n
    ld t1, 0(t0)
    addi t1, t1, 1
    sd t1, 0(t0)
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# void memo_put(long kind, long a, long b, long value)
# Adds an entry; the table doubles (in the permanent arena) before it is half full.
    .globl memo_put
memo_put:
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
    la t0, memo_n
    ld t0, 0(t0)
    addi t0, t0, 1
    slli t0, t0, 1
    la t1, memo_cap
    ld t1, 0(t1)
    ble t0, t1, 5f
    la t0, memo_tab             # grow: keep the old table, rehash into a new one
    ld s4, 0(t0)
    mv s5, t1                   # old capacity
    slli t1, t1, 1
    li t2, 1024
    bge t1, t2, 1f
    mv t1, t2
1:
    la t0, memo_cap
    sd t1, 0(t0)
    la t0, memo_n
    sd zero, 0(t0)
    slli a0, t1, 5
    call perm_alloc             # zeroed: the arena is never reused
    la t0, memo_tab
    sd a0, 0(t0)
2:
    beqz s5, 5f
    addi s5, s5, -1
    slli t0, s5, 5
    add t0, s4, t0
    ld a0, ME_KINDW(t0)
    beqz a0, 2b
    ld a1, ME_A(t0)
    ld a2, ME_B(t0)
    ld a3, ME_VALUE(t0)
    call memo_insert
    j 2b
5:
    mv a0, s0
    mv a1, s1
    mv a2, s2
    mv a3, s3
    call memo_insert
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    ld s5, 8(sp)
    addi sp, sp, 64
    ret

# int ty_has_params_memo(void *type)
ty_has_params_memo:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    mv s0, a0
    mv a1, a0
    li a0, MEMO_HASP
    li a2, 0
    call memo_get
    bnez a0, 9f
    mv a0, s0
    call ty_has_params
    addi a3, a0, 1
    sd a3, 0(sp)
    li a0, MEMO_HASP
    mv a1, s0
    li a2, 0
    call memo_put
    ld a0, 0(sp)
9:
    addi a0, a0, -1
    ld ra, 24(sp)
    ld s0, 16(sp)
    addi sp, sp, 32
    ret

# void *rt_concrete(void *type)
# At run time: the type with the running frame's type arguments substituted (memoized).
    .globl rt_concrete
rt_concrete:
    beqz a0, 7f
    ld t0, TY_KIND(a0)
    li t1, TY_VOID              # int, float, bool, str, void: nothing to substitute
    bleu t0, t1, 7f
    li t1, TY_PARAM
    bne t0, t1, 1f
    la t1, cur_frame            # a bare type parameter: the frame's argument, if it has one
    ld t1, 0(t1)
    beqz t1, 7f
    ld t1, FR_TARGS(t1)
    beqz t1, 7f
    ld t2, TY_A(a0)
    ld t3, VEC_N(t1)
    bgeu t2, t3, 7f
    ld t1, VEC_DATA(t1)
    slli t2, t2, 3
    add t1, t1, t2
    ld t1, 0(t1)
    beqz t1, 7f
    mv a0, t1
7:
    ret
1:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    call ty_has_params_memo
    beqz a0, 8f
    la t0, cur_frame
    ld t0, 0(t0)
    beqz t0, 8f
    ld s1, FR_TARGS(t0)
    beqz s1, 8f
    li a0, MEMO_CONCRETE
    mv a1, s0
    mv a2, s1
    call memo_get
    bnez a0, 9f
    call perm_begin             # interned types are never collected (spec 003)
    mv a0, s0
    li a1, -1
    mv a2, s1
    call ty_subst
    sd a0, 0(sp)
    call perm_end
    li a0, MEMO_CONCRETE
    mv a1, s0
    mv a2, s1
    ld a3, 0(sp)
    call memo_put
    ld a0, 0(sp)
    j 9f
8:
    mv a0, s0
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void *rt_targs(void *bindings, void *caller_frame)
# A generic call's type arguments, with the caller's own type parameters resolved (memoized, so
# the same call from the same instantiation always yields the same vec).
    .globl rt_targs
rt_targs:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    mv s0, a0
    mv s1, a1
    li a0, MEMO_HASPV
    mv a1, s0
    li a2, 0
    call memo_get
    bnez a0, 1f
    mv a0, s0
    call ty_vec_has_params
    addi a3, a0, 1
    sd a3, 0(sp)
    li a0, MEMO_HASPV
    mv a1, s0
    li a2, 0
    call memo_put
    ld a0, 0(sp)
1:
    li t0, 1
    beq a0, t0, 8f              # no parameters in the bindings
    beqz s1, 8f
    ld s2, FR_TARGS(s1)
    beqz s2, 8f
    li a0, MEMO_TARGS
    mv a1, s0
    mv a2, s2
    call memo_get
    bnez a0, 9f
    call perm_begin             # type-argument vecs outlive the call (spec 003)
    mv a0, s0
    li a1, -1
    mv a2, s2
    call ty_subst_vec
    sd a0, 0(sp)
    call perm_end
    li a0, MEMO_TARGS
    mv a1, s0
    mv a2, s2
    ld a3, 0(sp)
    call memo_put
    ld a0, 0(sp)
    j 9f
8:
    mv a0, s0
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    addi sp, sp, 48
    ret

# void *rt_es_subst(void *set, void *owner, void *args)
# es_subst for a generic call's declared effects, memoized by (set, args).
    .globl rt_es_subst
rt_es_subst:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    mv s0, a0
    mv s1, a1
    mv s2, a2
    call es_evar_count
    beqz a0, 8f
    li a0, MEMO_ESUBST
    mv a1, s0
    mv a2, s2
    call memo_get
    bnez a0, 9f
    mv a0, s0
    mv a1, s1
    mv a2, s2
    call es_subst
    sd a0, 0(sp)
    li a0, MEMO_ESUBST
    mv a1, s0
    mv a2, s2
    ld a3, 0(sp)
    call memo_put
    ld a0, 0(sp)
    j 9f
8:
    mv a0, s0
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    addi sp, sp, 48
    ret
