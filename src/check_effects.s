# check_effects.s: effect checking per function and test (research §3, §10).
#
# Each function and test has a side record FX (data-model.md): the declared set (from `uses`),
# the direct set (builtins called in the body, as "any scope" bits) and the computed requirement.
# Calls to user functions are recorded as edges while bodies are checked; after a file's bodies,
# the requirement is the least fixpoint need(f) = direct(f) ∪ ⋃ (declared(g) ∪ need(g)) over its
# calls f→g. Imports are checked before their importers, so imported requirements are final.

    .include "defs.inc"

    .section .bss
    .align 3
ck_fx:      .space 8                # FX of the function or test being checked, or 0
ck_edges:   .space 8                # vec of call edges of the current file
all_edges:  .space 8                # vec of call edges of the whole program
fx_tmp:     .space 8

    .section .rodata
# effect bit per builtin id (check_expr.s builtin_names); 0 = pure
    .globl builtin_effects
builtin_effects:
    .byte 0, EF_CONSOLE, EF_CONSOLE, EF_CONSOLE, EF_INPUT, EF_INPUT, EF_FREAD, EF_FWRITE, 0, 0
    .byte EF_CLOCK, 0, 0, 0, 0, 0, 0, 0, 0, 0
    .byte 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    .byte 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    .byte EF_ENV, EF_RANDOM
    .space 22
e0501: .string "E0501"
e0503: .string "E0503"
e0506: .string "E0506"
e0509: .string "E0509"
w0501: .string "W0501"
m_missing:  .string "`$1` does not declare $2, which it needs."
m_narrow:   .string "`$1` declares too narrow a scope: it needs $2."
m_unused:   .string "`$1` declares $2, which it never needs."
m_declares: .string "`$1` declares $2 here."
m_scope:    .string "\"$1\" is not a valid scope: write a relative directory without `..`, or a non-empty name."
m_effect:   .string "`$1` is not an effect: use console, input, file.read, file.write, net, clock, random or env."
s_uses:     .string "uses "
s_space:    .string " "
s_test_q:   .string "test \""
s_quote:    .string "\""

    .text

# void *item_fx(void *item)
# The FX record of a function or test, or 0 (anything else, or a function without a name).
    .globl item_fx
item_fx:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    mv s0, a0
    lhu t0, N_KIND(s0)
    li t1, N_LAMBDA             # anonymous functions have the same layout
    beq t0, t1, 0f
    li t1, N_FN
    bne t0, t1, 1f
0:
    ld t0, N_A(s0)
    li a0, 0
    beqz t0, 9f
    ld a0, N_B(t0)
    j 9f
1:
    li t1, N_TEST
    li a0, 0
    bne t0, t1, 9f
    ld a0, N_D(s0)
    bnez a0, 9f
    li a0, FX_SIZE
    call alloc
    sd a0, N_D(s0)
    ld t0, N_C(s0)
    sd t0, FX_USES(a0)
9:
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret

# void fx_declare(void *item)
# Builds the declared set from the `uses` clause.
    .globl fx_declare
fx_declare:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    call item_fx
    beqz a0, 9f
    mv s0, a0
    li a0, 0
    call es_new
    sd a0, FX_DIRECT(s0)
    sd zero, FX_REQUIRED(s0)
    ld a0, FX_USES(s0)
    mv a1, s0
    call fx_build_set
    sd a0, FX_DECLARED(s0)
9:
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret

# void *fx_build_set(void *uses, void *fx)
# The set a `uses` clause (or 0) declares: E0506 for invalid scopes, E0509 for unknown names
# (and FX_BAD on fx, when given, so its effect reports are skipped).
    .globl fx_build_set
fx_build_set:
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
    mv s7, a0
    mv s0, a1
    li a0, 0
    call es_new
    mv s1, a0
    beqz s7, 9f
    ld s2, N_A(s7)              # effects
    li s3, 0
1:
    ld t0, VEC_N(s2)
    bge s3, t0, 9f
    mv a0, s2
    mv a1, s3
    call vec_get
    mv s4, a0
    addi s3, s3, 1
    ld s5, N_A(s4)              # bit
    bnez s5, 2f
    ld t0, N_C(s4)
    beqz t0, 10f
    ld a0, N_A(t0)              # an effect parameter in scope?
    call scope_find
    beqz a0, 10f
    ld t0, SY_KIND(a0)
    li t1, SYM_TPARAM
    bne t0, t1, 10f
    ld a1, SY_TYPE(a0)
    ld t0, TY_B(a1)
    li t1, TPC_EFFECTS
    bne t0, t1, 10f
    mv a0, s1
    call es_add_evar
    j 1b
10:
    beqz s0, 11f
    li t0, 1
    sd t0, FX_BAD(s0)
11:
    mv a0, s4
    call fx_unknown_effect
    j 1b
2:
    ld s6, N_B(s4)              # scopes
    bnez s6, 3f
    ld t0, ES_MASK(s1)
    or t0, t0, s5
    sd t0, ES_MASK(s1)
    j 1b
3:
    sd zero, 0(sp)
4:
    ld t0, 0(sp)
    ld t1, VEC_N(s6)
    bge t0, t1, 1b
    addi t1, t0, 1
    sd t1, 0(sp)
    mv a0, s6
    mv a1, t0
    call vec_get
    la t0, fx_tmp
    sd a0, 0(t0)
    ld a2, N_A(a0)
    mv a0, s1
    mv a1, s5
    call es_add_scope
    bnez a0, 4b
    beqz s0, 41f
    li t0, 1
    sd t0, FX_BAD(s0)
41:
    la t0, fx_tmp
    ld a1, 0(t0)
    la a0, e0506
    la a2, m_scope
    ld a3, N_A(a1)
    call ck_err1
    j 4b
9:
    mv a0, s1
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

# void fx_unknown_effect(void *eff)
# E0509, with a fix to the closest effect name within edit distance 2.
    .globl fx_unknown_effect
fx_unknown_effect:
    addi sp, sp, -64
    sd ra, 56(sp)
    sd s0, 48(sp)
    sd s1, 40(sp)
    sd s2, 32(sp)
    sd s3, 24(sp)
    sd s4, 16(sp)
    sd s5, 8(sp)
    mv s0, a0
    la a0, e0509
    mv a1, s0
    la a2, m_effect
    ld a3, N_D(s0)
    call ck_err1
    mv s1, a0                   # the diagnostic
    li s2, 1                    # bit
    li s3, 0                    # best name
    li s4, 3                    # best distance
1:
    li t0, EF_ALL
    bgtu s2, t0, 3f
    mv a0, s2
    call es_bit_name
    mv s5, a0
    call cstr_len
    mv a3, a0
    ld t0, N_D(s0)
    addi a0, t0, STR_DATA
    ld a1, STR_LEN(t0)
    mv a2, s5
    li a4, 2
    call levenshtein
    bgeu a0, s4, 2f
    mv s4, a0
    mv s3, s5
2:
    slli s2, s2, 1
    j 1b
3:
    beqz s3, 9f
    mv a0, s3
    call str_from_cstr
    mv a4, a0
    mv a0, s1
    lwu a1, N_FILE(s0)
    lwu a2, N_START(s0)
    ld t0, N_D(s0)
    ld t0, STR_LEN(t0)
    add a3, a2, t0
    call diag_add_fix
9:
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    ld s5, 8(sp)
    addi sp, sp, 64
    ret

# void fx_begin(void *item)
# Effects found while checking the body go to this function's or test's record.
    .globl fx_begin
fx_begin:
    addi sp, sp, -16
    sd ra, 8(sp)
    call item_fx
    la t0, ck_fx
    sd a0, 0(t0)
    ld ra, 8(sp)
    addi sp, sp, 16
    ret

# void *fx_swap(void *fx)
# Makes fx the record effects go to (anonymous functions); returns the previous one.
    .globl fx_swap
fx_swap:
    la t0, ck_fx
    ld t1, 0(t0)
    sd a0, 0(t0)
    mv a0, t1
    ret

# void fx_value_call(void *effects)
# A call through a function value needs the effects of its type.
    .globl fx_value_call
fx_value_call:
    la t0, ck_fx
    ld t0, 0(t0)
    beqz t0, 9f
    beqz a0, 9f
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    mv s0, t0
    mv a1, a0
    ld a0, FX_DIRECT(s0)
    call es_union
    sd a0, FX_DIRECT(s0)
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
9:
    ret

# void fx_end(void)
    .globl fx_end
fx_end:
    la t0, ck_fx
    sd zero, 0(t0)
    ret

# void fx_builtin(long id)
# A builtin call needs its effect in some scope.
    .globl fx_builtin
fx_builtin:
    la t0, ck_fx
    ld t0, 0(t0)
    beqz t0, 9f
    ld t0, FX_DIRECT(t0)
    beqz t0, 9f
    la t1, builtin_effects
    add t1, t1, a0
    lbu t1, 0(t1)
    ld t2, ES_ANY(t0)
    or t2, t2, t1
    sd t2, ES_ANY(t0)
9:
    ret

# void fx_call(void *call, void *decl)
# Records a call from the current function or test to a user function.
    .globl fx_call
fx_call:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    mv s1, a1
    la t0, ck_fx
    ld t0, 0(t0)
    beqz t0, 9f
    beqz s1, 9f
    mv a0, s1                   # a callee with effect parameters: check_generic_call adds them
    call item_fx
    beqz a0, 9f
    ld a0, FX_DECLARED(a0)
    beqz a0, 4f
    call es_evar_count
    bnez a0, 9f
4:
    la t0, ck_fx
    ld t0, 0(t0)
    ld t1, FX_DECLARED(t0)      # an anonymous function takes the callee's declaration directly
    bnez t1, 0f
    mv a0, s1
    call item_fx
    beqz a0, 9f
    ld a0, FX_DECLARED(a0)
    call fx_value_call
    j 9f
0:
    la t0, ck_edges
    ld t0, 0(t0)
    bnez t0, 1f
    call vec_new
    la t0, ck_edges
    sd a0, 0(t0)
1:
    li a0, 24
    call alloc
    la t0, ck_fx
    ld t0, 0(t0)
    sd t0, 0(a0)
    sd s1, 8(a0)
    sd s0, 16(a0)
    mv s0, a0
    mv a1, a0
    la t0, ck_edges
    ld a0, 0(t0)
    call vec_push
    la t0, all_edges
    ld a0, 0(t0)
    beqz a0, 9f
    mv a1, s0
    call vec_push
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void fx_program_begin(void)
# A new program: forget the call edges of the previous one.
    .globl fx_program_begin
fx_program_begin:
    addi sp, sp, -16
    sd ra, 8(sp)
    call vec_new
    la t0, all_edges
    sd a0, 0(t0)
    ld ra, 8(sp)
    addi sp, sp, 16
    ret

# void *fx_edges(void)
# The program's call edges: {caller FX, callee decl, call node}.
    .globl fx_edges
fx_edges:
    la t0, all_edges
    ld a0, 0(t0)
    ret

# void fx_file_begin(void)
    .globl fx_file_begin
fx_file_begin:
    addi sp, sp, -16
    sd ra, 8(sp)
    call vec_new
    la t0, ck_edges
    sd a0, 0(t0)
    ld ra, 8(sp)
    addi sp, sp, 16
    ret

# void fx_file_end(void *items)
# The requirement fixpoint for this file's functions and tests, then their effect diagnostics.
    .globl fx_file_end
fx_file_end:
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
    li s1, 0                    # requirement starts as the direct set
1:
    ld t0, VEC_N(s0)
    bge s1, t0, 2f
    mv a0, s0
    mv a1, s1
    call vec_get
    addi s1, s1, 1
    call item_fx
    beqz a0, 1b
    ld t0, FX_DIRECT(a0)
    beqz t0, 1b
    sd t0, FX_REQUIRED(a0)
    j 1b
2:
    li s2, 0                    # changed
    la t0, ck_edges
    ld s3, 0(t0)
    li s1, 0
3:
    ld t0, VEC_N(s3)
    bge s1, t0, 5f
    mv a0, s3
    mv a1, s1
    call vec_get
    addi s1, s1, 1
    mv s4, a0                   # edge
    ld a0, 8(s4)
    call item_fx
    beqz a0, 3b
    mv s5, a0                   # callee FX
    ld t0, FX_DECLARED(s5)
    beqz t0, 3b
    ld s6, 0(s4)                # caller FX
    ld t0, FX_REQUIRED(s6)
    beqz t0, 3b
    ld a0, FX_DECLARED(s5)
    ld a1, FX_REQUIRED(s5)
    bnez a1, 4f
    ld a1, FX_DIRECT(s5)
    bnez a1, 4f
    mv a1, a0
4:
    call es_union
    mv a1, a0
    ld a0, FX_REQUIRED(s6)
    call es_union
    mv s4, a0
    ld a1, FX_REQUIRED(s6)
    call es_equal
    bnez a0, 3b
    sd s4, FX_REQUIRED(s6)
    li s2, 1
    j 3b
5:
    bnez s2, 2b
    li s1, 0                    # report
6:
    ld t0, VEC_N(s0)
    bge s1, t0, 9f
    mv a0, s0
    mv a1, s1
    call vec_get
    addi s1, s1, 1
    call fx_report
    j 6b
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

# void *fx_item_name(void *item)
# `name` for a function, `test "name"` for a test, as a str.
    .globl fx_item_name
fx_item_name:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    lhu t0, N_KIND(s0)
    li t1, N_FN
    bne t0, t1, 1f
    ld t0, N_A(s0)
    ld a0, N_A(t0)
    j 9f
1:
    call sb_new
    mv s1, a0
    la a1, s_test_q
    call sb_cstr
    ld a1, N_A(s0)
    beqz a1, 2f
    mv a0, s1
    call sb_str
2:
    mv a0, s1
    la a1, s_quote
    call sb_cstr
    mv a0, s1
    call sb_finish
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void *fx_render(void *set)
# The canonical text of a set as a str.
    .globl fx_render
fx_render:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    call sb_new
    mv s1, a0
    mv a0, s0
    mv a1, s1
    call es_render
    mv a0, s1
    call sb_finish
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void *fx_diag(char *code, int sev, void *item, char *tmpl, void *set)
# A diagnostic on the item's name (functions) or `test` keyword (tests): "$1" the item, "$2" the set.
    .globl fx_diag
fx_diag:
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
    mv a0, a4
    call fx_render
    mv s4, a0
    mv a0, s2
    call fx_item_name
    mv a1, a0
    mv a0, s3
    mv a2, s4
    li a3, 0
    call msg
    mv a5, a0
    lwu a2, N_FILE(s2)
    lhu t0, N_KIND(s2)
    li t1, N_FN
    bne t0, t1, 1f
    ld t0, N_A(s2)
    lwu a3, N_START(t0)
    lwu a4, N_END(t0)
    j 2f
1:
    lwu a3, N_START(s2)
    addi a4, a3, 4
2:
    mv a0, s0
    mv a1, s1
    call diag_add
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    ld s5, 8(sp)
    addi sp, sp, 64
    ret

# void fx_fix_clause(void *diag, void *item, void *set)
# A fix that makes the item's `uses` clause declare exactly set (removing the clause when empty).
    .globl fx_fix_clause
fx_fix_clause:
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
    mv a0, s1
    call item_fx
    ld s3, FX_USES(a0)
    ld t0, N_D(s1)              # body: N_D of a function, N_B of a test
    lhu t1, N_KIND(s1)
    li t2, N_FN
    beq t1, t2, 1f
    ld t0, N_B(s1)
1:
    beqz t0, 9f
    lwu s4, N_START(t0)
    call sb_new
    mv s5, a0
    mv a0, s2
    call es_is_empty
    bnez a0, 2f
    mv a0, s5
    la a1, s_uses
    call sb_cstr
    mv a0, s2
    mv a1, s5
    call es_render
    mv a0, s5
    la a1, s_space
    call sb_cstr
2:
    mv a0, s5
    call sb_finish
    mv a4, a0
    mv a0, s0
    lwu a1, N_FILE(s1)
    mv a2, s4
    beqz s3, 3f
    lwu a2, N_START(s3)
3:
    mv a3, s4
    call diag_add_fix
9:
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    ld s5, 8(sp)
    addi sp, sp, 64
    ret

# void fx_report(void *item)
# E0501 / E0503 when the declaration does not cover the requirement, W0501 for unused effects.
    .globl fx_report
fx_report:
    addi sp, sp, -64
    sd ra, 56(sp)
    sd s0, 48(sp)
    sd s1, 40(sp)
    sd s2, 32(sp)
    sd s3, 24(sp)
    sd s4, 16(sp)
    mv s0, a0
    call item_fx
    beqz a0, 9f
    mv s1, a0
    ld t0, FX_BAD(s1)
    bnez t0, 9f
    ld t0, FX_DECLARED(s1)
    beqz t0, 9f
    ld t0, FX_REQUIRED(s1)
    beqz t0, 9f
    ld a0, FX_DECLARED(s1)
    ld a1, FX_REQUIRED(s1)
    call es_missing
    mv s2, a0
    call es_is_empty
    bnez a0, 5f
    ld a0, FX_DECLARED(s1)
    mv a1, s2
    call es_lacks_bits
    la s3, e0503
    la a3, m_narrow
    beqz a0, 1f
    la s3, e0501
    la a3, m_missing
1:
    mv a0, s3
    li a1, SEV_ERROR
    mv a2, s0
    mv a4, s2
    call fx_diag
    mv s4, a0
    ld a0, FX_DECLARED(s1)
    ld a1, FX_REQUIRED(s1)
    call es_union
    mv a2, a0
    mv a0, s4
    mv a1, s0
    call fx_fix_clause
    mv a0, s4
    mv a1, s1
    call fx_related
    j 9f
5:
    ld a0, FX_DECLARED(s1)
    ld a1, FX_REQUIRED(s1)
    call es_unused
    mv s2, a0
    call es_is_empty
    bnez a0, 9f
    la a0, w0501
    li a1, SEV_WARNING
    mv a2, s0
    la a3, m_unused
    mv a4, s2
    call fx_diag
    mv s4, a0
    ld a0, FX_DECLARED(s1)
    mv a1, s2
    call es_minus
    mv a2, a0
    mv a0, s4
    mv a1, s0
    call fx_fix_clause
9:
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    addi sp, sp, 64
    ret

# void fx_related(void *diag, void *fx)
# One related note per distinct callee whose declaration the caller does not cover.
    .globl fx_related
fx_related:
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
    la t0, ck_edges
    ld s2, 0(t0)
    beqz s2, 9f
    li s3, 0
1:
    ld t0, VEC_N(s2)
    bge s3, t0, 9f
    mv a0, s2
    mv a1, s3
    call vec_get
    addi s3, s3, 1
    ld t0, 0(a0)
    bne t0, s1, 1b
    ld s4, 8(a0)                # callee decl
    li t1, 0                    # seen before in this caller's edges?
2:
    addi t2, s3, -1
    bge t1, t2, 3f
    ld t3, VEC_DATA(s2)
    slli t4, t1, 3
    add t3, t3, t4
    ld t3, 0(t3)
    addi t1, t1, 1
    ld t4, 0(t3)
    bne t4, s1, 2b
    ld t4, 8(t3)
    beq t4, s4, 1b
    j 2b
3:
    mv a0, s4
    call item_fx
    beqz a0, 1b
    mv s5, a0
    ld t0, FX_USES(s5)
    beqz t0, 1b
    ld a1, FX_DECLARED(s5)
    beqz a1, 1b
    ld a0, FX_DECLARED(s1)
    call es_covers
    bnez a0, 1b
    ld a0, FX_DECLARED(s5)
    call fx_render
    mv s6, a0
    mv a0, s4
    call fx_item_name
    mv a1, a0
    la a0, m_declares
    mv a2, s6
    li a3, 0
    call msg
    mv a4, a0
    ld t0, FX_USES(s5)
    mv a0, s0
    lwu a1, N_FILE(t0)
    lwu a2, N_START(t0)
    lwu a3, N_END(t0)
    call diag_add_related
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
    addi sp, sp, 80
    ret
