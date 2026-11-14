# Checker: types, scopes, declarations, statements and control flow (check_expr.s: expressions).
    .include "defs.inc"

    .section .bss
    .align 3
    .globl t_int
t_int:      .space 8
    .globl t_float
t_float:    .space 8
    .globl t_bool
t_bool:     .space 8
    .globl t_str
t_str:      .space 8
    .globl t_void
t_void:     .space 8
    .globl t_error
t_error:    .space 8
type_tab:   .space 8                # vec of interned types
    .globl ck_scope
ck_scope:   .space 8                # innermost symbol (chain through SY_NEXT)
    .globl ck_ret
ck_ret:     .space 8                # return type of the function being checked (0 in tests)
    .globl ck_in_test
ck_in_test: .space 8
    .globl ck_loops
ck_loops:   .space 8
    .globl ck_locals
ck_locals:  .space 8
    .globl ck_mode
ck_mode:    .space 8
    .globl ck_broke
ck_broke:   .space 8                # a `break` was seen in the innermost loop
    .globl ck_fn
ck_fn:      .space 8
    .globl ck_blk
ck_blk:     .space 8                # the statements of the innermost block being checked
    .globl ck_blk_idx
ck_blk_idx: .space 8                # the index after the statement being checked

    .section .rodata
e0201: .string "E0201"
e0202: .string "E0202"
e0203: .string "E0203"
e0207: .string "E0207"
e0208: .string "E0208"
e0301: .string "E0301"
e0309: .string "E0309"
e0401: .string "E0401"
e0402: .string "E0402"
e0403: .string "E0403"
e0404: .string "E0404"
e0405: .string "E0405"
e0602: .string "E0602"
e0603: .string "E0603"
m_impure: .string "A contract condition must be pure, but this one needs $1."
s_ret: .string "ret"
m_ungen_fn: .string "Cannot generate values of `$1` for `$2`: it contains a function type."
m_ungen_inf: .string "Cannot generate values of `$1` for `$2`: it has no finite value."
e0407: .string "E0407"
e0408: .string "E0408"
e0409: .string "E0409"
w0201: .string "W0201"
w0401: .string "W0401"
s_int: .string "int"
s_float: .string "float"
s_bool: .string "bool"
s_str: .string "str"
s_void: .string "void"
s_list: .string "list["
s_map: .string "map["
s_result: .string "result["
s_comma: .string ", "
s_errtype: .string "<error>"
s_main: .string "main"
s_var: .string "var"
s_try: .string "try "
s_let_: .string "let _ = "
m_undefined:    .string "`$1` is not defined."
m_not_type:     .string "`$1` is not a type."
m_dup:          .string "`$1` is already defined."
m_dup_builtin:  .string "`$1` is a builtin and cannot be redefined."
m_dup_first:    .string "`$1` is first defined here."
m_dup_test:     .string "A test named \"$1\" already exists."
m_shadow:       .string "`$1` is already defined in an outer scope."
m_shadow_rel:   .string "`$1` is defined here."
m_naming_snake: .string "`$1` should be snake_case: `$2`."
m_naming_upper: .string "`$1` should be UpperCamel: `$2`."
m_let_set:      .string "`$1` is declared with `let` and cannot be changed. Declare it with `var`."
m_param_set:    .string "`$1` is a parameter and cannot be changed."
m_not_place:    .string "Only variables, fields and list or map elements can be assigned."
m_mismatch:     .string "Expected `$1`, found `$2`."
m_key_type:     .string "Map keys must be `int` or `str`, found `$1`."
m_cond:         .string "A condition must be `bool`, found `$1`."
m_for_list:     .string "`for ... in` needs a list or a range `a..b`, found `$1`."
m_match_type:   .string "`match` needs a variant or result value, found `$1`."
m_no_case:      .string "`$1` is not a case of `$2`."
m_no_payload:   .string "Case `$1` has no payload to bind."
m_missing_cases: .string "This `match` is missing cases: $1."
m_dup_case:     .string "Case `$1` is handled twice."
m_missing_ret:  .string "`$1` can reach its end without returning a value."
m_ret_void:     .string "`$1` returns nothing, but a value is returned here."
m_ret_value:    .string "`$1` must return `$2`."
m_unhandled:    .string "The `$1` returned here is ignored. Handle it with `try` or `match`."
m_unused_val:   .string "The `$1` returned by this call is ignored. Use it or write `let _ = ...`."
m_break:        .string "`$1` can only be used inside a loop."
m_no_main:      .string "`vo run` needs a `fn main()`."
m_bad_main:     .string "`main` must be `fn main()` or `fn main() -> result[void]`."
m_unused:       .string "`$1` is never used."
m_unreachable:  .string "This statement can never run."
m_void_value:   .string "Expected a value, found `void`."
m_break_kw:     .string "break"
m_continue_kw:  .string "continue"

    .text
# void ck_init(void)
    .globl ck_init
ck_init:
    addi sp, sp, -16
    sd ra, 8(sp)
    la t0, type_tab
    ld t0, 0(t0)
    bnez t0, 9f
    call vec_new
    la t0, type_tab
    sd a0, 0(t0)
    li a0, TY_INT
    call ty_basic
    la t0, t_int
    sd a0, 0(t0)
    li a0, TY_FLOAT
    call ty_basic
    la t0, t_float
    sd a0, 0(t0)
    li a0, TY_BOOL
    call ty_basic
    la t0, t_bool
    sd a0, 0(t0)
    li a0, TY_STR
    call ty_basic
    la t0, t_str
    sd a0, 0(t0)
    li a0, TY_VOID
    call ty_basic
    la t0, t_void
    sd a0, 0(t0)
    li a0, TY_ERROR
    call ty_basic
    la t0, t_error
    sd a0, 0(t0)
9:
    ld ra, 8(sp)
    addi sp, sp, 16
    ret

# void *ty_basic(int kind)
    .globl ty_basic
ty_basic:
    li a1, 0
    li a2, 0
    li a3, 0
    tail ty_make

# void *ty_make(int kind, void *a, void *b, void *decl)
# The interned type with these fields.
    .globl ty_make
ty_make:
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
    mv s3, a3
    la t0, type_tab
    ld t0, 0(t0)
    ld t1, VEC_N(t0)
    ld t2, VEC_DATA(t0)
    li s4, 0
1:
    bge s4, t1, 2f
    slli t3, s4, 3
    add t3, t2, t3
    ld t3, 0(t3)
    addi s4, s4, 1
    ld t4, TY_KIND(t3)
    bne t4, s0, 1b
    ld t4, TY_A(t3)
    bne t4, s1, 1b
    ld t4, TY_B(t3)
    bne t4, s2, 1b
    ld t4, TY_DECL(t3)
    bne t4, s3, 1b
    mv a0, t3
    j 9f
2:
    li a0, TYPE_SIZE
    call alloc
    sd s0, TY_KIND(a0)
    sd s1, TY_A(a0)
    sd s2, TY_B(a0)
    sd s3, TY_DECL(a0)
    sd a0, 0(sp)
    mv a1, a0
    la t0, type_tab
    ld a0, 0(t0)
    call vec_push
    ld a0, 0(sp)
9:
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    addi sp, sp, 64
    ret

# void *ty_list(void *elem)
    .globl ty_list
ty_list:
    mv a1, a0
    li a0, TY_LIST
    li a2, 0
    li a3, 0
    tail ty_make

# void *ty_result(void *inner)
    .globl ty_result
ty_result:
    mv a1, a0
    li a0, TY_RESULT
    li a2, 0
    li a3, 0
    tail ty_make

# void ty_name_into(void *sb, void *type)
    .globl ty_name_into
ty_name_into:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    mv s1, a1
    ld t0, TY_KIND(s1)
    li t1, TY_INT
    la a1, s_int
    beq t0, t1, 1f
    li t1, TY_FLOAT
    la a1, s_float
    beq t0, t1, 1f
    li t1, TY_BOOL
    la a1, s_bool
    beq t0, t1, 1f
    li t1, TY_STR
    la a1, s_str
    beq t0, t1, 1f
    li t1, TY_VOID
    la a1, s_void
    beq t0, t1, 1f
    li t1, TY_LIST
    beq t0, t1, 2f
    li t1, TY_MAP
    beq t0, t1, 3f
    li t1, TY_RESULT
    beq t0, t1, 4f
    li t1, TY_RECORD
    beq t0, t1, 5f
    li t1, TY_VARIANT
    beq t0, t1, 5f
    li t1, TY_FUNC
    beq t0, t1, 6f
    li t1, TY_PARAM
    beq t0, t1, 7f
    la a1, s_errtype
1:
    call sb_cstr
    j 9f
7:
    mv a0, s1
    call ty_param_name
    mv a1, a0
    mv a0, s0
    call sb_str
    j 9f
6:
    mv a1, s1
    call ty_func_name_into
    j 9f
2:
    la a1, s_list
    call sb_cstr
    mv a0, s0
    ld a1, TY_A(s1)
    call ty_name_into
    j 8f
3:
    la a1, s_map
    call sb_cstr
    mv a0, s0
    ld a1, TY_A(s1)
    call ty_name_into
    mv a0, s0
    la a1, s_comma
    call sb_cstr
    mv a0, s0
    ld a1, TY_B(s1)
    call ty_name_into
    j 8f
4:
    la a1, s_result
    call sb_cstr
    mv a0, s0
    ld a1, TY_A(s1)
    call ty_name_into
    j 8f
5:
    ld t0, TY_DECL(s1)
    ld t0, N_A(t0)              # name node
    ld a1, N_A(t0)
    call sb_str
    ld a1, TY_B(s1)             # an instance: Name[args]
    beqz a1, 9f
    mv a0, s0
    call ty_args_into
    j 9f
8:
    mv a0, s0
    li a1, ']'
    call sb_byte
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void *ty_name(void *type)
    .globl ty_name
ty_name:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s1, a0
    call sb_new
    mv s0, a0
    mv a1, s1
    call ty_name_into
    mv a0, s0
    call sb_finish
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# int ty_is(void *type, int kind)
    .globl ty_is
ty_is:
    beqz a0, 1f
    ld t0, TY_KIND(a0)
    sub t0, t0, a1
    seqz a0, t0
    ret
1:
    li a0, 0
    ret

# int ty_bad(void *type)
# 1 when the type is the error type (or missing): no further diagnostics.
    .globl ty_bad
ty_bad:
    beqz a0, 1f
    ld t0, TY_KIND(a0)
    li t1, TY_ERROR
    sub t0, t0, t1
    seqz a0, t0
    ret
1:
    li a0, 1
    ret

# void *ck_err(char *code, void *node, void *message)
    .globl ck_err
ck_err:
    mv a5, a2
    lwu a3, N_START(a1)
    lwu a4, N_END(a1)
    lwu a2, N_FILE(a1)
    li a1, SEV_ERROR
    tail diag_add

# void *ck_warn(char *code, void *node, void *message)
    .globl ck_warn
ck_warn:
    mv a5, a2
    lwu a3, N_START(a1)
    lwu a4, N_END(a1)
    lwu a2, N_FILE(a1)
    li a1, SEV_WARNING
    tail diag_add

# void *ck_err1(char *code, void *node, char *tmpl, void *arg)
# ck_err with a message built from a template and one argument.
    .globl ck_err1
ck_err1:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    mv s1, a1
    mv a0, a2
    mv a1, a3
    li a2, 0
    li a3, 0
    call msg
    mv a2, a0
    mv a0, s0
    mv a1, s1
    call ck_err
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void *ck_mismatch(void *node, void *expected, void *found)
# E0301 "Expected `X`, found `Y`." with expected/found set.
    .globl ck_mismatch
ck_mismatch:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    mv s2, a2
    mv a0, a1
    call ty_name
    mv s1, a0
    mv a0, s2
    call ty_name
    mv s2, a0
    la a0, m_mismatch
    mv a1, s1
    mv a2, s2
    li a3, 0
    call msg
    mv a2, a0
    la a0, e0301
    mv a1, s0
    call ck_err
    mv s3, a0
    mv a1, s1
    mv a2, s2
    call diag_set_ef
    mv a0, s3
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# void *sym_new(void *name, int kind, void *decl, void *type, void *name_node)
    .globl sym_new
sym_new:
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
    mv s4, a4
    li a0, SYM_SIZE
    call alloc
    sd s0, SY_NAME(a0)
    sd s1, SY_KIND(a0)
    sd s2, SY_DECL(a0)
    sd s3, SY_TYPE(a0)
    sd s4, SY_NODE(a0)
    mv s0, a0
    call q_note_decl            # spec 006 reference log
    mv a0, s0
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    ld s4, 0(sp)
    addi sp, sp, 48
    ret

# void scope_add(void *sym)
    .globl scope_add
scope_add:
    la t0, ck_scope
    ld t1, 0(t0)
    sd t1, SY_NEXT(a0)
    sd a0, 0(t0)
    ret

# void *scope_find(void *name)
# The innermost symbol with this name, or 0.
    .globl scope_find
scope_find:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    la t0, ck_scope
    ld s1, 0(t0)
1:
    beqz s1, 8f
    ld a0, SY_NAME(s1)
    mv a1, s0
    call str_eq
    bnez a0, 9f
    ld s1, SY_NEXT(s1)
    j 1b
8:
    li s1, 0
9:
    mv a0, s1
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void scope_leave(void *saved)
# Pops symbols back to `saved`, warning (W0201) about locals that were never used.
    .globl scope_leave
scope_leave:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    la t0, ck_scope
    ld s1, 0(t0)
1:
    beq s1, s0, 9f
    beqz s1, 9f
    ld t0, SY_KIND(s1)
    li t1, SYM_LOCAL
    bne t0, t1, 2f
    ld t0, SY_FLAGS(s1)
    andi t0, t0, SYF_USED
    bnez t0, 2f
    ld t0, SY_SLOT(s1)
    bltz t0, 2f                 # parameters (slot stored negative-flagged) are not reported
    la t0, ck_fx                # a function with a hole is unfinished: no W0201
    ld t0, 0(t0)
    beqz t0, 3f
    ld t0, FX_HOLES(t0)
    bnez t0, 2f
3:
    la a0, w0201
    ld a1, SY_NODE(s1)
    beqz a1, 2f
    sd a1, 0(sp)
    ld a1, SY_NAME(s1)
    mv a3, a1
    la a2, m_unused
    mv a0, a2
    mv a1, a3
    li a2, 0
    li a3, 0
    call msg
    mv a2, a0
    la a0, w0201
    ld a1, 0(sp)
    call ck_warn
2:
    ld s1, SY_NEXT(s1)
    j 1b
9:
    la t0, ck_scope
    sd s0, 0(t0)
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void *convert_name(void *name, int upper)
# The name converted to snake_case (upper = 0) or UpperCamel (upper = 1).
    .globl convert_name
convert_name:
    addi sp, sp, -64
    sd ra, 56(sp)
    sd s0, 48(sp)
    sd s1, 40(sp)
    sd s2, 32(sp)
    sd s3, 24(sp)
    sd s4, 16(sp)
    addi s1, a0, STR_DATA
    ld s2, STR_LEN(a0)
    mv s3, a1
    call sb_new
    mv s0, a0
    li s4, 1                    # at a word start (for UpperCamel)
1:
    beqz s2, 9f
    lbu a1, 0(s1)
    addi s1, s1, 1
    addi s2, s2, -1
    beqz s3, 4f
    li t0, '_'                  # UpperCamel: drop '_', capitalize word starts
    bne a1, t0, 2f
    li s4, 1
    j 1b
2:
    beqz s4, 3f
    li t0, 'a'
    blt a1, t0, 3f
    li t0, 'z'
    bgt a1, t0, 3f
    addi a1, a1, -32
3:
    li s4, 0
    mv a0, s0
    call sb_byte
    j 1b
4:
    li t0, 'A'                  # snake_case: '_' before inner capitals, lower everything
    blt a1, t0, 6f
    li t0, 'Z'
    bgt a1, t0, 6f
    addi a1, a1, 32
    ld t0, SB_LEN(s0)
    beqz t0, 6f
    ld t1, SB_PTR(s0)
    add t1, t1, t0
    lbu t1, -1(t1)
    li t2, '_'
    beq t1, t2, 6f
    sd a1, 8(sp)
    mv a0, s0
    li a1, '_'
    call sb_byte
    ld a1, 8(sp)
6:
    mv a0, s0
    call sb_byte
    j 1b
9:
    mv a0, s0
    call sb_finish
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    addi sp, sp, 64
    ret

# void check_naming(void *name_node, int upper)
# E0207 when a name breaks the convention, with the converted name as a fix.
    .globl check_naming
check_naming:
    beqz a0, 99f
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    mv s1, a1
    ld s2, N_A(s0)              # name str
    mv a0, s2
    mv a1, s1
    call convert_name
    mv s3, a0
    mv a0, s2
    mv a1, s3
    call str_eq
    bnez a0, 9f
    beqz s1, 1f                 # UpperCamel must start with a capital
    j 2f
1:
    ld t0, STR_LEN(s2)          # snake_case: a lone leading '_' word like `_x` is also wrong
2:
    la a0, m_naming_snake
    beqz s1, 3f
    la a0, m_naming_upper
3:
    mv a1, s2
    mv a2, s3
    li a3, 0
    call msg
    mv a2, a0
    la a0, e0207
    mv a1, s0
    call ck_err
    lwu a1, N_FILE(s0)
    lwu a2, N_START(s0)
    lwu a3, N_END(s0)
    mv a4, s3
    call diag_add_fix
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
99:
    ret

# void *resolve_type(void *type_node)
# The semantic type of a syntax type (t_error after reporting a problem).
    .globl resolve_type
resolve_type:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    mv s0, a0
    beqz s0, 80f
    ld t0, N_A(s0)
    li t1, TY_INT
    la t2, t_int
    beq t0, t1, 1f
    li t1, TY_FLOAT
    la t2, t_float
    beq t0, t1, 1f
    li t1, TY_BOOL
    la t2, t_bool
    beq t0, t1, 1f
    li t1, TY_STR
    la t2, t_str
    beq t0, t1, 1f
    li t1, TY_VOID
    la t2, t_void
    beq t0, t1, 1f
    li t1, TY_LIST
    beq t0, t1, 2f
    li t1, TY_MAP
    beq t0, t1, 3f
    li t1, TY_RESULT
    beq t0, t1, 4f
    li t1, TY_NAMED
    beq t0, t1, 5f
    li t1, TY_FUNC
    beq t0, t1, 6f
    j 80f
6:                              # fn(params) -> ret uses effects
    call vec_new
    mv s1, a0
    sd zero, 0(sp)
61:
    ld t0, N_B(s0)
    ld t1, VEC_N(t0)
    ld t2, 0(sp)
    bge t2, t1, 62f
    addi t3, t2, 1
    sd t3, 0(sp)
    mv a0, t0
    mv a1, t2
    call vec_get
    call resolve_type
    mv a1, a0
    mv a0, s1
    call vec_push
    j 61b
62:
    la t0, t_void
    ld s2, 0(t0)
    ld a0, N_C(s0)
    beqz a0, 63f
    call resolve_type
    mv s2, a0
63:
    ld a0, N_D(s0)
    li a1, 0
    call fx_build_set
    mv a2, a0
    mv a0, s1
    mv a1, s2
    call ty_func
    j 90f
1:
    ld a0, 0(t2)
    j 90f
2:
    ld a0, N_B(s0)
    call resolve_type
    call ty_list
    j 90f
3:
    ld a0, N_B(s0)
    call resolve_type
    mv s1, a0
    ld a0, N_C(s0)
    call resolve_type
    mv s2, a0
    mv a0, s1
    call ty_bad
    bnez a0, 31f
    mv a0, s1
    li a1, TY_INT
    call ty_is
    bnez a0, 31f
    mv a0, s1
    li a1, TY_STR
    call ty_is
    bnez a0, 31f
    mv a0, s1
    call ty_name
    mv a3, a0
    la a0, e0309
    ld a1, N_B(s0)
    la a2, m_key_type
    call ck_err1
    j 80f
31:
    li a0, TY_MAP
    mv a1, s1
    mv a2, s2
    li a3, 0
    call ty_make
    j 90f
4:
    ld a0, N_B(s0)
    call resolve_type
    call ty_result
    j 90f
5:
    ld s1, N_B(s0)              # name node
    beqz s1, 80f
    ld a0, N_A(s1)
    call scope_find
    beqz a0, 51f
    mv s2, a0
    call is_placeholder
    bnez a0, 80f
    mv a0, s2                   # spec 006 reference log
    mv a1, s1
    li a2, RR_TYPE
    call q_note_sym
    mv a0, s2
    ld t0, SY_KIND(a0)
    li t1, SYM_RECORD
    li t2, TY_RECORD
    beq t0, t1, 52f
    li t1, SYM_VARIANT
    li t2, TY_VARIANT
    beq t0, t1, 52f
    li t1, SYM_TPARAM
    bne t0, t1, 53f
    ld t0, N_C(s0)              # a type parameter takes no arguments
    beqz t0, 54f
    mv a0, s0
    ld a1, N_A(s1)
    li a2, 0
    ld a3, VEC_N(t0)
    call ty_argn_error
    j 90f
54:
    ld a0, SY_TYPE(s2)
    j 90f
53:
    la a0, e0201
    mv a1, s1
    la a2, m_not_type
    ld a3, N_A(s1)
    call ck_err1
    j 80f
51:
    ld a0, N_A(s1)
    mv a1, s1
    call report_undefined
    j 80f
52:
    sd t2, 0(sp)
    sd a0, 8(sp)
    mv a1, s1
    call rename_use
    ld a0, 8(sp)
    ld t2, 0(sp)
    ld a1, SY_DECL(a0)
    mv a2, t2
    mv a0, s0
    ld a3, N_C(s0)              # Name or Name[args]
    call resolve_instance
    j 90f
80:
    la t0, t_error
    ld a0, 0(t0)
90:
    beqz s0, 91f
    sd a0, N_TYPE(s0)
91:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    addi sp, sp, 48
    ret

# void *decl_type(void *decl)
# The type a record or variant declaration defines.
    .globl decl_type
decl_type:
    lhu t0, N_KIND(a0)
    mv a3, a0
    li a0, TY_RECORD
    li t1, N_RECORD
    beq t0, t1, 1f
    li a0, TY_VARIANT
1:
    li a1, 0
    li a2, 0
    tail ty_make

# void *new_local(void *name_node, void *type, int mutable)
# Defines a local in the current scope (E0203 when the name is already visible).
    .globl new_local
new_local:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    mv s1, a1
    mv s2, a2
    li a1, 0
    call check_naming
    ld a0, N_A(s0)
    call scope_find
    beqz a0, 1f
    mv s3, a0
    call is_placeholder
    bnez a0, 1f
    la a0, e0203
    mv a1, s0
    la a2, m_shadow
    ld a3, N_A(s0)
    call ck_err1
    ld t0, SY_NODE(s3)
    beqz t0, 1f
    sd a0, 0(sp)
    la a0, m_shadow_rel
    ld a1, N_A(s0)
    li a2, 0
    li a3, 0
    call msg
    mv a4, a0
    ld t0, SY_NODE(s3)
    ld a0, 0(sp)
    lwu a1, N_FILE(t0)
    lwu a2, N_START(t0)
    lwu a3, N_END(t0)
    call diag_add_related
1:
    ld a0, N_A(s0)
    li a1, SYM_LOCAL
    li a2, 0
    mv a3, s1
    mv a4, s0
    call sym_new
    mv s3, a0
    li t0, 0
    beqz s2, 2f
    li t0, SYF_MUT
2:
    sd t0, SY_FLAGS(s3)
    mv a0, s0
    li a1, 0
    call naming_rename
    sd a0, SY_RENAME(s3)
    la t0, ck_locals
    ld t1, 0(t0)
    sd t1, SY_SLOT(s3)
    addi t1, t1, 1
    sd t1, 0(t0)
    la t0, ck_depth
    ld t0, 0(t0)
    sd t0, SY_DEPTH(s3)
    mv a0, s3
    call scope_add
    mv a0, s3
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# int expect_type(void *node, void *expected, void *found)
# Reports E0301 when found does not match expected (errors match anything). 1 = fine.
    .globl expect_type
expect_type:
    beq a1, a2, 1f
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    sd s2, 0(sp)
    mv s0, a0
    mv s1, a1
    mv s2, a2
    mv a0, s1
    call ty_bad
    bnez a0, 2f
    mv a0, s2
    call ty_bad
    bnez a0, 2f
    mv a0, s2                   # a function value: wider effects than allowed is E0502
    mv a1, s1
    call ty_func_fits
    beqz a0, 2f
    li t0, 2
    bne a0, t0, 11f
    mv a0, s0
    mv a1, s1
    mv a2, s2
    call ty_effects_error
    li a0, 0
    j 3f
11:
    mv a0, s0
    mv a1, s1
    mv a2, s2
    call ck_mismatch
    mv a1, s1
    mv a2, s2
    mv a3, s0
    call suggest_conversion
    li a0, 0
    j 3f
2:
    li a0, 1
3:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    ld s2, 0(sp)
    addi sp, sp, 32
    ret
1:
    li a0, 1
    ret

# void check_block(void *block)
# Checks the statements of a block in a new scope; sets NF_TERMINATES on the block.
    .globl check_block
check_block:
    addi sp, sp, -64
    sd ra, 56(sp)
    sd s0, 48(sp)
    sd s1, 40(sp)
    sd s2, 32(sp)
    sd s3, 24(sp)
    sd s4, 16(sp)
    mv s0, a0
    la t0, ck_scope
    ld s4, 0(t0)
    ld s1, N_A(s0)
    li s2, 0                    # index
    li s3, 0                    # terminated so far (1 = later statements are unreachable)
1:
    ld t0, VEC_N(s1)
    bge s2, t0, 8f
    mv a0, s1
    mv a1, s2
    call vec_get
    addi s2, s2, 1
    lhu t0, N_KIND(a0)
    li t1, N_COMMENT
    beq t0, t1, 1b
    li t1, 1
    bne s3, t1, 2f
    sd a0, 0(sp)                # first unreachable statement: W0401 once
    la a0, m_unreachable
    call str_from_cstr
    mv a2, a0
    la a0, w0401
    ld a1, 0(sp)
    call ck_warn
    li s3, 2
    ld a0, 0(sp)
2:
    sd a0, 0(sp)
    la t0, ck_blk               # where the statement is (E0310 fixes look at the ones after it)
    sd s1, 0(t0)
    la t0, ck_blk_idx
    sd s2, 0(t0)
    call check_stmt
    ld a0, 0(sp)
    lhu t0, N_FLAGS(a0)
    andi t0, t0, NF_TERMINATES
    beqz t0, 1b
    bnez s3, 1b
    li s3, 1
    j 1b
8:
    beqz s3, 9f
    lhu t0, N_FLAGS(s0)
    ori t0, t0, NF_TERMINATES
    sh t0, N_FLAGS(s0)
9:
    mv a0, s4
    call scope_leave
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    addi sp, sp, 64
    ret

# void mark_terminates(void *node)
    .globl mark_terminates
mark_terminates:
    lhu t0, N_FLAGS(a0)
    ori t0, t0, NF_TERMINATES
    sh t0, N_FLAGS(a0)
    ret

# int terminates(void *node)
    .globl terminates
terminates:
    beqz a0, 1f
    lhu t0, N_FLAGS(a0)
    andi a0, t0, NF_TERMINATES
    snez a0, a0
1:
    ret

# void check_stmt(void *stmt)
    .globl check_stmt
check_stmt:
    addi sp, sp, -64
    sd ra, 56(sp)
    sd s0, 48(sp)
    sd s1, 40(sp)
    sd s2, 32(sp)
    sd s3, 24(sp)
    sd s4, 16(sp)
    mv s0, a0
    la t0, ck_cur_stmt
    sd s0, 0(t0)
    lhu t0, N_KIND(s0)
    li t1, N_LET
    beq t0, t1, 10f
    li t1, N_SET
    beq t0, t1, 20f
    li t1, N_IF
    beq t0, t1, 30f
    li t1, N_WHILE
    beq t0, t1, 40f
    li t1, N_FOR
    beq t0, t1, 50f
    li t1, N_MATCH
    beq t0, t1, 60f
    li t1, N_RETURN
    beq t0, t1, 70f
    li t1, N_BREAK
    beq t0, t1, 75f
    li t1, N_CONTINUE
    beq t0, t1, 76f
    li t1, N_EXPR_STMT
    beq t0, t1, 80f
    j 99f                       # N_ERROR / comments

10:                             # let / var
    li s1, 0                    # declared type
    ld a0, N_B(s0)
    beqz a0, 11f
    call resolve_type
    mv s1, a0
11:
    ld a0, N_C(s0)
    mv a1, s1
    call check_expr
    mv s2, a0
    beqz s1, 12f
    ld a0, N_C(s0)
    mv a1, s1
    mv a2, s2
    call expect_type
    mv s2, s1
12:
    mv a0, s2
    li a1, TY_VOID
    call ty_is
    beqz a0, 13f
    la a0, m_void_value
    call str_from_cstr
    mv a2, a0
    la a0, e0301
    ld a1, N_C(s0)
    call ck_err
    la t0, t_error
    ld s2, 0(t0)
13:
    sd s2, N_TYPE(s0)
    lhu t0, N_FLAGS(s0)
    andi t0, t0, NF_DISCARD
    beqz t0, 14f
    li t0, -1                   # `let _ = e;`: a result may not be discarded
    sd t0, N_E(s0)
    mv a0, s2
    li a1, TY_RESULT
    call ty_is
    beqz a0, 99f
    mv a0, s2
    call ty_name
    mv a3, a0
    la a0, e0404
    ld a1, N_C(s0)
    la a2, m_unhandled
    call ck_err1
    j 99f
14:
    ld a0, N_A(s0)
    beqz a0, 99f
    mv a1, s2
    lhu a2, N_FLAGS(s0)
    andi a2, a2, NF_MUT
    call new_local
    ld t0, SY_SLOT(a0)
    sd t0, N_E(s0)
    sd s0, SY_DECL(a0)
    j 99f

20:                             # set place = expr;
    ld s1, N_A(s0)
    lhu t0, N_KIND(s1)
    li t1, N_IDENT
    bne t0, t1, 25f
    ld a0, N_A(s1)
    call scope_find
    mv s3, a0
    beqz s3, 25f
    ld t0, SY_KIND(s3)
    li t1, SYM_LOCAL
    bne t0, t1, 25f
    ld t0, SY_FLAGS(s3)
    andi t0, t0, SYF_MUT
    bnez t0, 25f
    la a0, e0208                # assignment to a `let` (or parameter)
    mv a1, s1
    la a2, m_let_set
    ld t0, SY_DECL(s3)
    beqz t0, 21f
    lhu t1, N_KIND(t0)
    li t2, N_LET
    beq t1, t2, 22f
21:
    la a2, m_param_set
22:
    ld a3, N_A(s1)
    call ck_err1
    ld t0, SY_DECL(s3)
    beqz t0, 25f
    lhu t1, N_KIND(t0)
    li t2, N_LET
    bne t1, t2, 25f
    sd a0, 0(sp)
    la a0, s_var
    call str_from_cstr
    mv a4, a0
    ld t0, SY_DECL(s3)
    lwu a1, N_FILE(t0)
    lwu a2, N_START(t0)
    addi a3, a2, 3
    ld a0, 0(sp)
    call diag_add_fix
25:
    mv a0, s1
    li a1, 0
    call check_place
    mv s2, a0
    ld a0, N_B(s0)
    mv a1, s2
    call check_expr
    mv a2, a0
    ld a0, N_B(s0)
    mv a1, s2
    call expect_type
    j 99f

30:                             # if
    ld a0, N_A(s0)
    call check_cond
    ld a0, N_B(s0)
    call check_block
    ld a0, N_C(s0)
    beqz a0, 99f
    lhu t0, N_KIND(a0)
    li t1, N_IF
    bne t0, t1, 31f
    call check_stmt
    j 32f
31:
    call check_block
32:
    ld a0, N_B(s0)
    call terminates
    beqz a0, 99f
    ld a0, N_C(s0)
    call terminates
    beqz a0, 99f
    mv a0, s0
    call mark_terminates
    j 99f

40:                             # while
    ld a0, N_A(s0)
    call check_cond
    la t0, ck_broke
    ld s4, 0(t0)
    sd zero, 0(t0)
    la t0, ck_loops
    ld t1, 0(t0)
    addi t1, t1, 1
    sd t1, 0(t0)
    ld a0, N_B(s0)
    call check_block
    la t0, ck_loops
    ld t1, 0(t0)
    addi t1, t1, -1
    sd t1, 0(t0)
    la t0, ck_broke             # `while true` without break never falls through
    ld t1, 0(t0)
    sd s4, 0(t0)
    bnez t1, 99f
    ld t0, N_A(s0)
    lhu t1, N_KIND(t0)
    li t2, N_BOOL
    bne t1, t2, 99f
    ld t1, N_A(t0)
    beqz t1, 99f
    mv a0, s0
    call mark_terminates
    j 99f

50:                             # for
    la t0, ck_scope
    ld s4, 0(t0)
    ld a0, N_B(s0)
    li a1, 0
    call check_expr
    mv s1, a0
    ld t0, N_C(s0)
    beqz t0, 52f
    ld a0, N_B(s0)              # range: both bounds int
    la t0, t_int
    ld a1, 0(t0)
    mv a2, s1
    call expect_type
    ld a0, N_C(s0)
    la t0, t_int
    ld a1, 0(t0)
    call check_expr
    mv a2, a0
    ld a0, N_C(s0)
    la t0, t_int
    ld a1, 0(t0)
    call expect_type
    la t0, t_int
    ld s2, 0(t0)
    j 54f
52:
    mv a0, s1
    li a1, TY_LIST
    call ty_is
    beqz a0, 53f
    ld s2, TY_A(s1)
    j 54f
53:
    la t0, t_error
    ld s2, 0(t0)
    mv a0, s1
    call ty_bad
    bnez a0, 54f
    mv a0, s1
    call ty_name
    mv a3, a0
    la a0, e0301
    ld a1, N_B(s0)
    la a2, m_for_list
    call ck_err1
54:
    ld a0, N_A(s0)
    beqz a0, 55f
    mv a1, s2
    li a2, 0
    call new_local
    ld t0, SY_SLOT(a0)
    sd t0, N_E(s0)
    j 56f
55:
    la t0, ck_locals            # `for _ in`: a slot for the counter, no name
    ld t1, 0(t0)
    sd t1, N_E(s0)
    addi t1, t1, 1
    sd t1, 0(t0)
56:
    la t0, ck_loops
    ld t1, 0(t0)
    addi t1, t1, 1
    sd t1, 0(t0)
    la t0, ck_broke
    ld s3, 0(t0)
    ld a0, N_D(s0)
    call check_block
    la t0, ck_broke
    sd s3, 0(t0)
    la t0, ck_loops
    ld t1, 0(t0)
    addi t1, t1, -1
    sd t1, 0(t0)
    mv a0, s4
    call scope_leave
    j 99f

60:                             # match
    mv a0, s0
    call check_match
    j 99f

70:                             # return
    mv a0, s0
    call mark_terminates
    la t0, ck_ret
    ld s1, 0(t0)                # declared return type (0 in tests: like void)
    bnez s1, 71f
    la t0, t_void
    ld s1, 0(t0)
71:
    ld a0, N_A(s0)
    beqz a0, 73f
    mv a1, s1
    call check_expr
    mv s2, a0
    mv a0, s1
    li a1, TY_VOID
    call ty_is
    beqz a0, 72f
    mv a0, s2
    call ty_bad
    bnez a0, 99f
    la t0, ck_fn
    ld t0, 0(t0)
    beqz t0, 99f
    ld t0, N_A(t0)
    ld a3, N_A(t0)
    la a0, e0301
    ld a1, N_A(s0)
    la a2, m_ret_void
    call ck_err1
    j 99f
72:
    ld a0, N_A(s0)
    mv a1, s1
    mv a2, s2
    call expect_type
    j 99f
73:
    mv a0, s1                   # `return;` in a function that must return a value
    li a1, TY_VOID
    call ty_is
    bnez a0, 99f
    mv a0, s1
    call ty_bad
    bnez a0, 99f
    la t0, ck_fn
    ld t0, 0(t0)
    ld t0, N_A(t0)
    ld a0, N_A(t0)
    sd a0, 0(sp)
    mv a0, s1
    call ty_name
    mv a2, a0
    ld a1, 0(sp)
    la a0, m_ret_value
    li a3, 0
    call msg
    mv a2, a0
    la a0, e0301
    mv a1, s0
    call ck_err
    j 99f

75:                             # break
    la t0, ck_broke
    li t1, 1
    sd t1, 0(t0)
    la s1, m_break_kw
    j 77f
76:
    la s1, m_continue_kw
77:
    mv a0, s0
    call mark_terminates
    la t0, ck_loops
    ld t0, 0(t0)
    bnez t0, 99f
    mv a0, s1
    call str_from_cstr
    mv a3, a0
    la a0, e0407
    mv a1, s0
    la a2, m_break
    call ck_err1
    j 99f

80:                             # call or try statement
    ld s1, N_A(s0)
    la t0, ck_stmt_call
    sd s1, 0(t0)
    mv a0, s1
    li a1, 0
    call check_expr
    mv s2, a0
    la t0, ck_stmt_call
    sd zero, 0(t0)
    call ty_bad
    bnez a0, 99f
    lhu t0, N_KIND(s1)
    li t1, N_TRY
    beq t0, t1, 82f
    li t1, N_CALL               # exit() and todo() never return
    bne t0, t1, 81f
    ld t0, N_D(s1)
    li t1, B_TODO
    beq t0, t1, 83f
    li t1, 9
    bne t0, t1, 81f
    mv a0, s0
    call mark_terminates
81:
    mv a0, s2
    li a1, TY_VOID
    call ty_is
    bnez a0, 99f
    mv a0, s2
    li a1, TY_RESULT
    call ty_is
    beqz a0, 82f
    mv a0, s2                   # an ignored result: E0404 (fix: try)
    call ty_name
    mv a3, a0
    la a0, e0404
    mv a1, s1
    la a2, m_unhandled
    call ck_err1
    la t0, ck_ret
    ld a1, 0(t0)
    beqz a1, 99f
    mv s3, a0
    mv a0, a1
    li a1, TY_RESULT
    call ty_is
    beqz a0, 99f
    la a0, s_try
    call str_from_cstr
    mv a4, a0
    mv a0, s3
    lwu a1, N_FILE(s1)
    lwu a2, N_START(s1)
    mv a3, a2
    call diag_add_fix
    j 99f
82:
    mv a0, s2                   # an ignored value: E0405 (fix: let _ = )
    li a1, TY_VOID
    call ty_is
    bnez a0, 99f
    mv a0, s2
    call ty_name
    mv a3, a0
    la a0, e0405
    mv a1, s1
    la a2, m_unused_val
    call ck_err1
    mv s3, a0
    la a0, s_let_
    call str_from_cstr
    mv a4, a0
    mv a0, s3
    lwu a1, N_FILE(s1)
    lwu a2, N_START(s1)
    mv a3, a2
    call diag_add_fix
    j 99f
83:
    mv a0, s0
    call mark_terminates
99:
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    addi sp, sp, 64
    ret

# void check_cond(void *expr)
    .globl check_cond
check_cond:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    la t0, t_bool
    ld a1, 0(t0)
    call check_expr
    mv s1, a0
    li a1, TY_BOOL
    call ty_is
    bnez a0, 9f
    mv a0, s1
    call ty_bad
    bnez a0, 9f
    mv a0, s1
    call ty_name
    mv a3, a0
    la a0, e0301
    mv a1, s0
    la a2, m_cond
    call ck_err1
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void check_match(void *match)
    .globl check_match
check_match:
    addi sp, sp, -96
    sd ra, 88(sp)
    sd s0, 80(sp)
    sd s1, 72(sp)
    sd s2, 64(sp)
    sd s3, 56(sp)
    sd s4, 48(sp)
    sd s5, 40(sp)
    sd s6, 32(sp)
    mv s0, a0
    ld a0, N_A(s0)
    li a1, 0
    call check_expr
    mv s1, a0                   # scrutinee type
    li s2, 0                    # number of cases
    mv a0, s1
    li a1, TY_RESULT
    call ty_is
    beqz a0, 1f
    li s2, 2
    j 3f
1:
    mv a0, s1
    li a1, TY_VARIANT
    call ty_is
    beqz a0, 2f
    ld t0, TY_DECL(s1)
    ld a0, N_B(t0)
    call count_members
    mv s2, a0
    j 3f
2:
    mv a0, s1                   # not matchable: check arm bodies only
    call ty_bad
    bnez a0, 21f
    mv a0, s1
    call ty_name
    mv a3, a0
    la a0, e0301
    ld a1, N_A(s0)
    la a2, m_match_type
    call ck_err1
21:
    la t0, t_error
    ld s1, 0(t0)
3:
    addi a0, s2, 1              # seen[] flags
    slli a0, a0, 3
    call alloc
    mv s3, a0
    ld s4, N_B(s0)              # arms
    li s5, 0
    li s6, 1                    # all arms terminate
4:
    ld t0, VEC_N(s4)
    bge s5, t0, 8f
    mv a0, s4
    mv a1, s5
    call vec_get
    addi s5, s5, 1
    lhu t0, N_KIND(a0)
    li t1, N_ARM
    bne t0, t1, 4b
    sd a0, 0(sp)                # arm
    mv a1, s1
    mv a2, s3
    call check_arm
    ld a0, 0(sp)
    ld a0, N_C(a0)
    call terminates
    bnez a0, 4b
    li s6, 0
    j 4b
8:
    mv a0, s1
    call ty_bad
    bnez a0, 9f
    mv a0, s0                   # an arm with an unknown case name: its fix comes first, no E0401
    call has_unknown_arm
    bnez a0, 9f
    mv a0, s0                   # exhaustiveness
    mv a1, s1
    mv a2, s3
    mv a3, s2
    call report_missing_cases
    bnez a0, 9f
    beqz s6, 9f
    mv a0, s0
    call mark_terminates
9:
    ld ra, 88(sp)
    ld s0, 80(sp)
    ld s1, 72(sp)
    ld s2, 64(sp)
    ld s3, 56(sp)
    ld s4, 48(sp)
    ld s5, 40(sp)
    ld s6, 32(sp)
    addi sp, sp, 96
    ret

# long count_members(void *vec)
# Number of non-comment members of a record or variant.
    .globl count_members
count_members:
    ld t0, VEC_N(a0)
    ld t1, VEC_DATA(a0)
    li a0, 0
1:
    beqz t0, 2f
    ld t2, 0(t1)
    addi t1, t1, 8
    addi t0, t0, -1
    lhu t3, N_KIND(t2)
    li t4, N_COMMENT
    beq t3, t4, 1b
    addi a0, a0, 1
    j 1b
2:
    ret

# void *member_at(void *vec, long index)
# The index-th non-comment member.
    .globl member_at
member_at:
    ld t0, VEC_N(a0)
    ld t1, VEC_DATA(a0)
1:
    beqz t0, 3f
    ld t2, 0(t1)
    addi t1, t1, 8
    addi t0, t0, -1
    lhu t3, N_KIND(t2)
    li t4, N_COMMENT
    beq t3, t4, 1b
    beqz a1, 2f
    addi a1, a1, -1
    j 1b
2:
    mv a0, t2
    ret
3:
    li a0, 0
    ret

# long member_index(void *vec, void *name)
# Index of the member with this name, or -1.
    .globl member_index
member_index:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    mv s1, a1
    li s2, 0
1:
    mv a0, s0
    mv a1, s2
    call member_at
    beqz a0, 8f
    ld t0, N_A(a0)
    ld a0, N_A(t0)
    mv a1, s1
    call str_eq
    bnez a0, 9f
    addi s2, s2, 1
    j 1b
8:
    li s2, -1
9:
    mv a0, s2
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

    .section .rodata
s_ok: .string "ok"
s_err: .string "err"
    .text

# void *case_name(void *scrutinee_type, long index)
    .globl case_name
case_name:
    addi sp, sp, -16
    sd ra, 8(sp)
    ld t0, TY_KIND(a0)
    li t1, TY_RESULT
    bne t0, t1, 2f
    la a0, s_ok
    beqz a1, 1f
    la a0, s_err
1:
    call str_from_cstr
    j 9f
2:
    ld t0, TY_DECL(a0)
    ld a0, N_B(t0)
    call member_at
    ld t0, N_A(a0)
    ld a0, N_A(t0)
9:
    ld ra, 8(sp)
    addi sp, sp, 16
    ret

# void *case_payload(void *scrutinee_type, long index)
# Payload type of a case, or 0 when it has none.
    .globl case_payload
case_payload:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    mv s0, a0
    ld t0, TY_KIND(a0)
    li t1, TY_RESULT
    bne t0, t1, 3f
    beqz a1, 1f
    la t0, t_str
    ld a0, 0(t0)
    j 9f
1:
    ld a0, TY_A(a0)
    ld t0, TY_KIND(a0)
    li t1, TY_VOID
    bne t0, t1, 9f
    li a0, 0
    j 9f
3:
    ld t0, TY_DECL(a0)
    ld a0, N_B(t0)
    call member_at
    ld a0, N_B(a0)              # payload type node
    beqz a0, 9f
    ld a1, N_TYPE(a0)
    mv a0, s0                   # in an instance, with its type arguments
    call field_type_in
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    addi sp, sp, 32
    ret

# long case_index(void *scrutinee_type, void *name)
    .globl case_index
case_index:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    mv s1, a1
    ld t0, TY_KIND(s0)
    li t1, TY_RESULT
    bne t0, t1, 3f
    la a1, s_ok
    mv a0, s1
    call str_eq_cstr
    bnez a0, 1f
    la a1, s_err
    mv a0, s1
    call str_eq_cstr
    bnez a0, 2f
    li a0, -1
    j 9f
1:
    li a0, 0
    j 9f
2:
    li a0, 1
    j 9f
3:
    ld t0, TY_DECL(s0)
    ld a0, N_B(t0)
    mv a1, s1
    call member_index
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# int str_eq_cstr(void *str, char *s)
    .globl str_eq_cstr
str_eq_cstr:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    mv s1, a1
    mv a0, a1
    call cstr_len
    mv a2, a0
    mv a0, s0
    mv a1, s1
    call str_eq_bytes
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void check_arm(void *arm, void *scrutinee_type, long *seen)
    .globl check_arm
check_arm:
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
    la t0, ck_scope
    ld s4, 0(t0)
    li t0, -1
    sd t0, N_D(s0)
    mv a0, s1
    call ty_bad
    bnez a0, 5f
    ld t0, N_A(s0)
    beqz t0, 7f
    mv a0, s1
    ld a1, N_A(t0)
    call case_index
    mv s3, a0
    bgez s3, 1f
    ld t0, N_A(s0)              # unknown case
    ld a0, N_A(t0)
    sd a0, 0(sp)
    mv a0, s1
    call ty_name
    mv a2, a0
    ld a1, 0(sp)
    la a0, m_no_case
    li a3, 0
    call msg
    mv a2, a0
    la a0, e0201
    ld a1, N_A(s0)
    call ck_err
    mv a1, s1
    ld a2, N_A(s0)
    call suggest_case
    j 5f                        # still bind the arm's name (silently), so it does not cascade
1:
    sd s3, N_D(s0)
    ld t0, TY_KIND(s1)          # spec 006 reference log: (variant, case); not ok/err
    li t1, TY_VARIANT
    bne t0, t1, 1001f
    ld a0, TY_DECL(s1)
    mv a1, s3
    ld a2, N_A(s0)
    li a3, RR_USE
    call q_note
1001:
    slli t0, s3, 3
    add t0, s2, t0
    ld t1, 0(t0)
    beqz t1, 2f
    ld t0, N_A(s0)              # duplicate arm: E0402, fix removes it
    la a0, e0402
    mv a1, t0
    la a2, m_dup_case
    ld a3, N_A(t0)
    call ck_err1
    li a4, 0
    sd a0, 0(sp)
    li a0, 0
    li a1, 0
    call str_new
    mv a4, a0
    ld a0, 0(sp)
    lwu a1, N_FILE(s0)
    lwu a2, N_START(s0)
    lwu a3, N_END(s0)
    call diag_add_fix
    j 3f
2:
    li t1, 1
    sd t1, 0(t0)
3:
    ld t0, N_B(s0)              # payload binding
    beqz t0, 7f
    mv a0, s1
    mv a1, s3
    call case_payload
    bnez a0, 4f
    ld t0, N_A(s0)
    la a0, e0301
    ld a1, N_B(s0)
    la a2, m_no_payload
    ld a3, N_A(t0)
    call ck_err1
    la t0, t_error
    ld a0, 0(t0)
4:
    sd a0, N_TYPE(s0)           # spec 006: the binding's type (values in runtime errors)
    mv a1, a0
    ld a0, N_B(s0)
    li a2, 0
    call new_local
    ld t0, SY_SLOT(a0)
    sd t0, N_E(s0)
    j 7f
5:
    ld a0, N_B(s0)              # unknown scrutinee type: bind the name silently (no cascade)
    beqz a0, 7f
    la t0, t_error
    ld a1, 0(t0)
    li a2, 0
    call new_local
    li t0, SYF_USED
    sd t0, SY_FLAGS(a0)
7:
    ld a0, N_C(s0)
    call check_block
    mv a0, s4
    call scope_leave
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    addi sp, sp, 64
    ret

# int report_missing_cases(void *match, void *type, long *seen, long ncases)
# E0401 listing every unhandled case (with a fix inserting their arms). 1 = reported.
    .globl report_missing_cases
report_missing_cases:
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
    call sb_new
    mv s4, a0                   # list for the message
    call sb_new
    mv s5, a0                   # arms for the fix
    li s6, 0
    sd zero, 0(sp)              # missing count
1:
    bge s6, s3, 5f
    slli t0, s6, 3
    add t0, s2, t0
    ld t0, 0(t0)
    bnez t0, 4f
    ld t0, 0(sp)
    beqz t0, 2f
    mv a0, s4
    la a1, s_comma
    call sb_cstr
2:
    ld t0, 0(sp)
    addi t0, t0, 1
    sd t0, 0(sp)
    mv a0, s1
    mv a1, s6
    call case_name
    sd a0, 8(sp)
    mv a0, s4
    li a1, '`'
    call sb_byte
    mv a0, s4
    ld a1, 8(sp)
    call sb_str
    mv a0, s4
    li a1, '`'
    call sb_byte
    mv a0, s5                   # "case name(_) { }\n" (payload) or "case name { }\n"
    la a1, s_case_kw
    call sb_cstr
    mv a0, s5
    ld a1, 8(sp)
    call sb_str
    mv a0, s1
    mv a1, s6
    call case_payload
    beqz a0, 3f
    mv a0, s5
    la a1, s_paren_under
    call sb_cstr
3:
    mv a0, s5
    la a1, s_empty_block
    call sb_cstr
4:
    addi s6, s6, 1
    j 1b
5:
    ld t0, 0(sp)
    beqz t0, 8f
    mv a0, s4
    call sb_finish
    mv a1, a0
    la a0, m_missing_cases
    li a2, 0
    li a3, 0
    call msg
    mv a2, a0
    la a0, e0401                # span: `match` through the scrutinee
    lwu a3, N_START(s0)
    ld t0, N_A(s0)
    lwu a4, N_END(t0)
    lwu t1, N_FILE(s0)
    mv a5, a2
    mv a2, t1
    li a1, SEV_ERROR
    call diag_add
    mv s6, a0
    lwu t0, N_END(s0)           # insert the arms before the closing brace
    addi s2, t0, -1
    mv a0, s5
    call sb_finish
    mv a4, a0
    mv a0, s6
    lwu a1, N_FILE(s0)
    mv a2, s2
    mv a3, s2
    call diag_add_fix
    li a0, 1
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

    .section .rodata
s_case_kw: .string "case "
s_paren_under: .string "(_)"
s_empty_block: .string " {\n}\n"
    .text

# void define_item(void *item)
# Adds a top-level fn, record or variant to the file scope (E0202 on duplicates).
    .globl define_item
define_item:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    lhu t0, N_KIND(s0)
    li s2, SYM_FN
    li a1, 0
    li t1, N_FN
    beq t0, t1, 1f
    li s2, SYM_RECORD
    li a1, 1
    li t1, N_RECORD
    beq t0, t1, 1f
    li s2, SYM_VARIANT
    li t1, N_VARIANT
    beq t0, t1, 1f
    j 9f
1:
    ld s1, N_A(s0)              # name node
    beqz s1, 9f
    mv a0, s1
    call check_naming
    ld a0, N_A(s1)
    call builtin_id
    beqz a0, 2f
    la a0, e0202
    mv a1, s1
    la a2, m_dup_builtin
    ld a3, N_A(s1)
    call ck_err1
    j 9f
2:
    ld a0, N_A(s1)
    call scope_find
    beqz a0, 3f
    mv s3, a0
    la a0, e0202
    mv a1, s1
    la a2, m_dup
    ld a3, N_A(s1)
    call ck_err1
    sd a0, 0(sp)
    la a0, m_dup_first
    ld a1, N_A(s1)
    li a2, 0
    li a3, 0
    call msg
    mv a4, a0
    ld t0, SY_NODE(s3)
    ld a0, 0(sp)
    lwu a1, N_FILE(t0)
    lwu a2, N_START(t0)
    lwu a3, N_END(t0)
    call diag_add_related
    j 9f
3:
    ld a0, N_A(s1)
    mv a1, s2
    mv a2, s0
    li a3, 0
    mv a4, s1
    call sym_new
    mv s3, a0
    mv a0, s1                   # a declared name that breaks the convention: uses get renamed too
    li a1, 0
    li t0, SYM_FN
    beq s2, t0, 4f
    li a1, 1
4:
    call naming_rename
    sd a0, SY_RENAME(s3)
    mv a0, s3
    call scope_add
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# void resolve_item(void *item)
# Resolves the types in a declaration's signature, fields or cases (its type parameters in scope).
    .globl resolve_item
resolve_item:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    la t0, ck_scope
    ld s1, 0(t0)
    call ty_tparams_of
    mv a1, s0
    call tparams_push
    mv a0, s0
    call resolve_item_types
    mv a0, s1
    call scope_leave
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void resolve_item_types(void *item)
    .globl resolve_item_types
resolve_item_types:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    lhu t0, N_KIND(s0)
    li t1, N_FN
    beq t0, t1, 1f
    li t1, N_RECORD
    beq t0, t1, 5f
    li t1, N_VARIANT
    beq t0, t1, 5f
    j 9f
1:
    mv a0, s0
    call fx_declare
    ld s1, N_B(s0)              # params
    li s2, 0
2:
    ld t0, VEC_N(s1)
    bge s2, t0, 3f
    mv a0, s1
    mv a1, s2
    call vec_get
    mv s3, a0
    ld a0, N_B(s3)
    call resolve_type
    sd a0, N_TYPE(s3)
    addi s2, s2, 1
    j 2b
3:
    ld a0, N_C(s0)
    beqz a0, 4f
    call resolve_type
    j 41f
4:
    la t0, t_void
    ld a0, 0(t0)
41:
    sd a0, N_TYPE(s0)
    j 9f
5:
    lhu s3, N_KIND(s0)
    ld s1, N_B(s0)              # fields or cases
    li s2, 0
6:
    ld t0, VEC_N(s1)
    bge s2, t0, 9f
    mv a0, s1
    mv a1, s2
    call vec_get
    addi s2, s2, 1
    lhu t0, N_KIND(a0)
    li t1, N_COMMENT
    beq t0, t1, 6b
    sd a0, 0(sp)
    ld a0, N_A(a0)
    li a1, 0
    call check_naming
    ld t0, 0(sp)
    ld a0, N_B(t0)
    beqz a0, 6b
    call resolve_type
    j 6b
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# void check_fn(void *fn)
    .globl check_fn
check_fn:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    sd s4, 0(sp)
    mv s0, a0
    call fx_begin
    la t0, ck_fn
    sd s0, 0(t0)
    ld t0, N_TYPE(s0)
    la t1, ck_ret
    sd t0, 0(t1)
    la t0, ck_in_test
    sd zero, 0(t0)
    la t0, ck_loops
    sd zero, 0(t0)
    la t0, ck_locals
    sd zero, 0(t0)
    la t0, ck_scope
    ld s4, 0(t0)
    mv a0, s0                   # type parameters, then parameters
    call ty_tparams_of
    mv a1, s0
    call tparams_push
    ld s1, N_B(s0)              # parameters become the first locals
    li s2, 0
1:
    ld t0, VEC_N(s1)
    bge s2, t0, 2f
    mv a0, s1
    mv a1, s2
    call vec_get
    mv s3, a0
    ld a0, N_A(s3)
    ld a1, N_TYPE(s3)
    li a2, 0
    call new_local
    ld t0, SY_SLOT(a0)
    sd t0, N_E(s3)
    li t1, SYF_USED             # parameters are never reported as unused
    sd t1, SY_FLAGS(a0)
    addi s2, s2, 1
    j 1b
2:
    mv a0, s0
    call check_clauses
    ld a0, N_D(s0)
    beqz a0, 8f
    call check_block
    ld t0, N_TYPE(s0)           # E0403: a value-returning function that can fall off its end
    ld t1, TY_KIND(t0)
    li t2, TY_VOID
    beq t1, t2, 8f
    li t2, TY_ERROR
    beq t1, t2, 8f
    ld a0, N_D(s0)
    call terminates
    bnez a0, 8f
    la a0, e0403
    ld a1, N_A(s0)
    la a2, m_missing_ret
    ld t0, N_A(s0)
    ld a3, N_A(t0)
    call ck_err1
8:
    call fx_end
    mv a0, s4
    call scope_leave
    la t0, ck_locals
    ld t0, 0(t0)
    sd t0, N_E(s0)
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    ld s4, 0(sp)
    addi sp, sp, 48
    ret

# void check_clauses(void *fn)
# Contract clauses (spec 005 research R14, R15), after the parameters are bound: each a pure
# `bool`. `ensures` also sees `ret` (slot FX_RETSLOT), in a scope popped before the body.
check_clauses:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s2, a0
    ld t0, N_A(s2)
    beqz t0, 9f
    ld s0, N_B(t0)
    beqz s0, 9f
    ld a0, FX_REQUIRES(s0)
    mv a1, s0
    call check_clause_vec
    ld s1, FX_ENSURES(s0)
    beqz s1, 9f
    la t0, ck_scope
    ld s3, 0(t0)
    ld a0, N_TYPE(s2)
    li a1, TY_VOID
    call ty_is
    bnez a0, 2f
    li a0, NODE_SIZE            # `ret`: a name at the first `ensures` condition
    call alloc
    mv s2, a0
    li t0, N_NAME
    sh t0, N_KIND(s2)
    ld t0, VEC_DATA(s1)
    ld t0, 0(t0)
    lwu t1, N_FILE(t0)
    sw t1, N_FILE(s2)
    lwu t1, N_START(t0)
    sw t1, N_START(s2)
    lwu t1, N_END(t0)
    sw t1, N_END(s2)
    la a0, s_ret
    call str_from_cstr
    sd a0, N_A(s2)
    ld t0, N_A(s0)              # (s0 is the FX record; the return type is the function's)
    mv a0, s2
    la t0, ck_ret
    ld a1, 0(t0)
    li a2, 0
    call new_local
    li t1, SYF_USED
    sd t1, SY_FLAGS(a0)
    ld t0, SY_SLOT(a0)
    sd t0, FX_RETSLOT(s0)
2:
    mv a0, s1
    mv a1, s0
    call check_clause_vec
    mv a0, s3
    call scope_leave
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# void check_clause_vec(void *conds, void *fx)
# Each condition is checked as `bool` with its effects going to a scratch record (as for an
# anonymous function), so the function's own requirement and vo.effects do not change. Any
# effect is E0603. A hole in a condition counts for the function (FX_HOLES).
check_clause_vec:
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
    li s2, 0
    beqz s0, 9f
1:
    ld t0, VEC_N(s0)
    bge s2, t0, 9f
    mv a0, s0
    mv a1, s2
    call vec_get
    mv s3, a0
    addi s2, s2, 1
    li a0, FX_SIZE
    call alloc
    mv s4, a0
    li a0, 0
    call es_new
    sd a0, FX_DIRECT(s4)
    mv a0, s4
    call fx_swap
    mv s5, a0
    mv a0, s3
    call check_cond
    mv a0, s5
    call fx_swap
    ld t0, FX_HOLES(s4)
    beqz t0, 2f
    sd t0, FX_HOLES(s1)
2:
    ld a0, FX_DIRECT(s4)
    call es_is_empty
    bnez a0, 1b
    ld a0, FX_DIRECT(s4)        # name the effects (builtin needs are kept apart in ES_ANY)
    ld t0, ES_MASK(a0)
    ld t1, ES_ANY(a0)
    or t0, t0, t1
    beqz t0, 3f
    mv a0, t0
    call es_new
3:
    call fx_render
    mv a3, a0
    la a0, e0603
    mv a1, s3
    la a2, m_impure
    call ck_err1
    j 1b
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

# void check_test(void *test)
# A test, or a property (ck_in_test 2): its parameters are the first locals (E0602 when a
# parameter's type cannot be generated).
    .globl check_test
check_test:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    call fx_declare
    mv a0, s0
    call fx_begin
    la t0, ck_fn
    sd zero, 0(t0)
    la t0, ck_ret
    sd zero, 0(t0)
    la t0, ck_in_test
    li t1, 1
    sd t1, 0(t0)
    la t0, ck_loops
    sd zero, 0(t0)
    la t0, ck_locals
    sd zero, 0(t0)
    la t0, ck_scope
    ld s3, 0(t0)
    lhu t0, N_FLAGS(s0)
    andi t0, t0, NF_PROP
    beqz t0, 3f
    la t0, ck_in_test
    li t1, 2
    sd t1, 0(t0)
    li s2, 0
2:
    ld t0, N_D(s0)
    ld t0, FX_PARAMS(t0)
    ld t1, VEC_N(t0)
    bge s2, t1, 3f
    mv a0, t0
    mv a1, s2
    call vec_get
    mv s1, a0
    addi s2, s2, 1
    ld a0, N_B(s1)
    call resolve_type
    sd a0, N_TYPE(s1)
    call prop_generatable
    beqz a0, 21f
    la a2, m_ungen_fn
    li t0, 1
    beq a0, t0, 22f
    la a2, m_ungen_inf
22:
    sd a2, 0(sp)
    ld a0, N_TYPE(s1)
    call ty_name
    mv a1, a0
    ld t0, N_A(s1)
    ld a2, N_A(t0)
    ld a0, 0(sp)
    li a3, 0
    call msg
    mv a2, a0
    la a0, e0602
    mv a1, s1
    call ck_err
21:
    ld a0, N_A(s1)
    ld a1, N_TYPE(s1)
    li a2, 0
    call new_local
    ld t0, SY_SLOT(a0)
    sd t0, N_E(s1)
    li t1, SYF_USED             # parameters are never reported as unused
    sd t1, SY_FLAGS(a0)
    j 2b
3:
    ld a0, N_B(s0)
    beqz a0, 1f
    call check_block
1:
    call fx_end
    lhu t0, N_FLAGS(s0)
    andi t0, t0, NF_PROP
    beqz t0, 4f
    mv a0, s3
    call scope_leave
4:
    la t0, ck_locals
    ld t0, 0(t0)
    sd t0, N_E(s0)
    la t0, ck_in_test
    sd zero, 0(t0)
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# void check_tests_unique(void *items)
# E0202 when two tests in a file share a name.
    .globl check_tests_unique
check_tests_unique:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    li s1, 0
1:
    ld t0, VEC_N(s0)
    bge s1, t0, 9f
    mv a0, s0
    mv a1, s1
    call vec_get
    mv s2, a0
    addi s1, s1, 1
    lhu t0, N_KIND(s2)
    li t1, N_TEST
    bne t0, t1, 1b
    li s3, 0
2:
    addi t0, s1, -1
    bge s3, t0, 1b
    mv a0, s0
    mv a1, s3
    call vec_get
    addi s3, s3, 1
    lhu t0, N_KIND(a0)
    li t1, N_TEST
    bne t0, t1, 2b
    ld a0, N_A(a0)
    ld a1, N_A(s2)
    beqz a0, 2b
    beqz a1, 2b
    call str_eq
    beqz a0, 2b
    la a0, e0202
    mv a1, s2
    la a2, m_dup_test
    ld a3, N_A(s2)
    call ck_err1
    j 1b
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# void *find_main(void *items)
    .globl find_main
find_main:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    li s1, 0
1:
    ld t0, VEC_N(s0)
    bge s1, t0, 8f
    mv a0, s0
    mv a1, s1
    call vec_get
    addi s1, s1, 1
    lhu t0, N_KIND(a0)
    li t1, N_FN
    bne t0, t1, 1b
    sd a0, 0(sp)
    ld t0, N_A(a0)
    beqz t0, 1b
    ld a0, N_A(t0)
    la a1, s_main
    call str_eq_cstr
    beqz a0, 1b
    ld a0, 0(sp)
    j 9f
8:
    li a0, 0
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void check_main(int file, void *fn)
# For `run`: E0408 without main, E0409 for a wrong signature.
    .globl check_main
check_main:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    mv s1, a1
    bnez s1, 2f
    la a0, m_no_main
    call str_from_cstr
    mv a5, a0
    la a0, e0408
    li a1, SEV_ERROR
    mv a2, s0
    li a3, 0
    li a4, 0
    call diag_add
    j 9f
2:
    ld t0, N_B(s1)
    ld t0, VEC_N(t0)
    bnez t0, 3f
    ld t0, N_TYPE(s1)
    ld t1, TY_KIND(t0)
    li t2, TY_VOID
    beq t1, t2, 9f
    li t2, TY_ERROR
    beq t1, t2, 9f
    li t2, TY_RESULT
    bne t1, t2, 3f
    ld t0, TY_A(t0)
    ld t1, TY_KIND(t0)
    li t2, TY_VOID
    beq t1, t2, 9f
3:
    la a0, m_bad_main
    call str_from_cstr
    mv a2, a0
    la a0, e0409
    ld a1, N_A(s1)
    call ck_err
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void check_items(int file, int mode)
# Declarations, signatures, then bodies of one parsed file.
    .globl check_items
check_items:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s3, a0
    la t0, ck_mode
    sd a1, 0(t0)
    call file_get
    mv s2, a0
    ld t0, FILE_SCOPE(s2)
    la t1, ck_scope
    sd t0, 0(t1)
    mv a0, s3                   # imported items first (dependencies are already checked)
    call bind_imports
    ld t0, FILE_AST(s2)
    ld s0, N_A(t0)              # items
    li s1, 0
1:
    ld t0, VEC_N(s0)
    bge s1, t0, 2f
    mv a0, s0
    mv a1, s1
    call vec_get
    call define_item
    addi s1, s1, 1
    j 1b
2:
    la t0, ck_scope
    ld t0, 0(t0)
    sd t0, FILE_SCOPE(s2)
    li s1, 0
3:
    ld t0, VEC_N(s0)
    bge s1, t0, 4f
    mv a0, s0
    mv a1, s1
    call vec_get
    call ck_item_set
    call resolve_item
    addi s1, s1, 1
    j 3b
4:
    mv a0, s0
    call check_tests_unique
    call fx_file_begin
    li s1, 0
5:
    ld t0, VEC_N(s0)
    bge s1, t0, 7f
    mv a0, s0
    mv a1, s1
    call vec_get
    call ck_item_set
    addi s1, s1, 1
    lhu t0, N_KIND(a0)
    li t1, N_FN
    beq t0, t1, 6f
    li t1, N_TEST
    bne t0, t1, 5b
    call check_test
    j 5b
6:
    call check_fn
    j 5b
7:
    li a0, 0
    call ck_item_set
    mv a0, s0
    call fx_file_end
    la t0, ck_mode
    ld t0, 0(t0)
    li t1, 1
    bne t0, t1, 9f
    mv a0, s0
    call find_main
    mv a1, a0
    mv a0, s3
    call check_main
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# int has_unknown_arm(void *match)
    .globl has_unknown_arm
has_unknown_arm:
    ld t0, N_B(a0)
    ld t1, VEC_N(t0)
    ld t2, VEC_DATA(t0)
1:
    beqz t1, 8f
    ld t3, 0(t2)
    addi t2, t2, 8
    addi t1, t1, -1
    lhu t4, N_KIND(t3)
    li t5, N_ARM
    bne t4, t5, 1b
    ld t4, N_D(t3)
    bgez t4, 1b
    li a0, 1
    ret
8:
    li a0, 0
    ret

# void suggest_case(void *diag, void *type, void *name_node)
# A replacement fix with the case of the type whose name matches ignoring case (Ok -> ok) or is
# within edit distance 2.
    .globl suggest_case
suggest_case:
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
    li s4, 2                    # number of cases
    ld t0, TY_KIND(s1)
    li t1, TY_RESULT
    beq t0, t1, 1f
    ld t0, TY_DECL(s1)
    ld a0, N_B(t0)
    call count_members
    mv s4, a0
1:
    ld a0, N_A(s2)
    li a1, 0
    call rt_case
    sd a0, 0(sp)                # the written name, lower-cased
    li s3, 0
2:
    bge s3, s4, 9f
    mv a0, s1
    mv a1, s3
    call case_name
    sd a0, 8(sp)
    addi s3, s3, 1
    ld a1, 0(sp)
    call str_eq
    bnez a0, 3f
    ld a0, N_A(s2)
    ld a1, 8(sp)
    call name_distance
    li t0, 2
    bgt a0, t0, 2b
3:
    mv a0, s0
    lwu a1, N_FILE(s2)
    lwu a2, N_START(s2)
    lwu a3, N_END(s2)
    ld a4, 8(sp)
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

# void *naming_rename(void *name_node, int upper)
# The convention-conforming name when the node's name breaks the convention, else 0.
    .globl naming_rename
naming_rename:
    beqz a0, 9f
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    ld s0, N_A(a0)
    mv a0, s0
    call convert_name
    mv s1, a0
    mv a0, s0
    mv a1, s1
    call str_eq
    mv t0, a0
    mv a0, s1
    beqz t0, 1f
    li a0, 0
1:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
9:
    ret

# void rename_use(void *sym, void *node)
# At a use of a symbol whose declaration breaks the naming convention: E0207 with the rename.
    .globl rename_use
rename_use:
    ld t0, SY_RENAME(a0)
    beqz t0, 9f
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    sd s2, 0(sp)
    mv s0, a0
    mv s1, a1
    ld a1, SY_NAME(s0)
    ld a2, SY_RENAME(s0)
    la a0, m_rename_use
    li a3, 0
    call msg
    mv a2, a0
    la a0, e0207
    mv a1, s1
    call ck_err
    lwu a1, N_FILE(s1)
    lwu a2, N_START(s1)
    lwu a3, N_END(s1)
    ld a4, SY_RENAME(s0)
    call diag_add_fix
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    ld s2, 0(sp)
    addi sp, sp, 32
9:
    ret

    .section .rodata
m_rename_use: .string "`$1` breaks the naming convention: rename it to `$2` here too."
    .text
