# Checker, expressions: typing, operator selection, calls and builtins (contracts/grammar.md).
    .include "defs.inc"

    .equ B_PRINT, 1
    .equ B_EXIT, 9
    .equ B_STR, 11
    .equ B_LEN, 16
    .equ B_SLICE, 17
    .equ B_PUSH, 23
    .equ B_POP, 24
    .equ B_SORT, 25
    .equ B_HAS, 26
    .equ B_REMOVE, 27
    .equ B_KEYS, 28
    .equ B_EXPECT, 29
    .equ B_EXPECT_EQ, 30
    .equ B_OK, 31
    .equ B_ERR, 32
    .equ B_ABS, 33
    .equ B_MIN, 34
    .equ B_MAX, 35
    .equ B_COUNT, 49

    .section .rodata
    .align 3
builtin_names:
    .dword 0, b1, b2, b3, b4, b5, b6, b7, b8, b9, b10, b11, b12, b13, b14, b15, b16
    .dword b17, b18, b19, b20, b21, b22, b23, b24, b25, b26, b27, b28, b29, b30, b31, b32
    .dword b33, b34, b35, b36, b37, b38, b39, b40, b41, b42, b43, b44, b45, b46, b47, b48, b49
builtin_sigs:
    .dword 0, g_s_v, g_s_v, g_s_v, g__r, g__s, g_s_r, g_ss_u, g__S, g_i_v, g__i, 0, g_f_i, g_i_f
    .dword g_s_n, g_s_x, 0, 0, g_ss_i, g_ss_S, g_Ss_s, g_s_s, g_s_S, 0, 0, 0, 0, 0, 0, g_b_v, 0, 0, 0
    .dword 0, 0, 0, g_s_s, g_s_s, g_s_i, g_i_s, g_s_r, g_ii_i, 0, 0, 0, 0, 0, 0, 0, g_b_v
b1: .string "print"
b2: .string "write"
b3: .string "eprint"
b4: .string "read_line"
b5: .string "read_all"
b6: .string "read_file"
b7: .string "write_file"
b8: .string "args"
b9: .string "exit"
b10: .string "now_ms"
b11: .string "str"
b12: .string "int"
b13: .string "float"
b14: .string "parse_int"
b15: .string "parse_float"
b16: .string "len"
b17: .string "slice"
b18: .string "find"
b19: .string "split"
b20: .string "join"
b21: .string "trim"
b22: .string "chars"
b23: .string "push"
b24: .string "pop"
b25: .string "sort"
b26: .string "has"
b27: .string "remove"
b28: .string "keys"
b29: .string "expect"
b30: .string "expect_eq"
b31: .string "ok"
b32: .string "err"
b33: .string "abs"
b34: .string "min"
b35: .string "max"
b36: .string "upper"
b37: .string "lower"
b38: .string "ord"
b39: .string "chr"
b40: .string "env"
b41: .string "random_int"
b42: .string "sort_by"
b43: .string "map"
b44: .string "filter"
b45: .string "fold"
b46: .string "any"
b47: .string "all"
b48: .string "todo"
b49: .string "assume"
g_s_i: .string "s>i"
g_i_s: .string "i>s"
# signatures: argument codes, '>', result code. i int, f float, b bool, s str, v void,
# S list[str], r result[str], u result[void], n result[int], x result[float]
g_s_v: .string "s>v"
g__r: .string ">r"
g__s: .string ">s"
g_s_r: .string "s>r"
g_ss_u: .string "ss>u"
g__S: .string ">S"
g_i_v: .string "i>v"
g__i: .string ">i"
g_f_i: .string "f>i"
g_i_f: .string "i>f"
g_s_n: .string "s>n"
g_s_x: .string "s>x"
g_ss_i: .string "ss>i"
g_ii_i: .string "ii>i"
g_ss_S: .string "ss>S"
g_Ss_s: .string "Ss>s"
g_s_s: .string "s>s"
g_s_S: .string "s>S"
g_b_v: .string "b>v"
e0201: .string "E0201"
e0301: .string "E0301"
e0302: .string "E0302"
e0303: .string "E0303"
e0304: .string "E0304"
e0305: .string "E0305"
e0306: .string "E0306"
e0307: .string "E0307"
e0601: .string "E0601"
e0604: .string "E0604"
m_assume: .string "`$1` can only be used inside a `prop` block."
w0601: .string "W0601"
m_hole: .string "Hole: `$1` needs a value of type `$2`."
m_hole_unknown: .string "Cannot tell what type this hole must have; annotate the binding: `let x: T = todo(...)`."
e0308: .string "E0308"
e0406: .string "E0406"
e0410: .string "E0410"
m_undefined:    .string "`$1` is not defined."
m_not_value:    .string "`$1` is not a value."
m_builtin_value: .string "`$1` is a builtin and cannot be used as a value: wrap it in an anonymous function."
m_not_fn:       .string "`$1` is not a function."
m_argc:         .string "`$1` takes $2 argument(s), got $3."
m_no_field:     .string "`$1` has no field `$2`."
m_missing_fields: .string "Missing fields in `$1`: $2."
m_empty_list:   .string "An empty list needs a type: write `new list[T]`."
m_infer:        .string "Cannot infer the type of `$1(...)` here. Add a type annotation."
m_operands:     .string "Operator `$1` cannot be applied to `$2` and `$3`."
m_operand1:     .string "Operator `$1` cannot be applied to `$2`."
m_index:        .string "`$1` cannot be indexed."
m_index_str:    .string "Strings cannot be indexed; use `slice` or `chars`."
m_try_fn:       .string "`try` can only be used in a function that returns a `result`."
m_try_type:     .string "`try` needs a `result`, found `$1`."
m_one_of:       .string "Expected $1, found `$2`."
m_not_record:   .string "`$1` is not a record."
m_not_variant:  .string "`$1` is not a variant."
m_no_case:      .string "`$1` is not a case of `$2`."
m_needs_payload: .string "`$1.$2` needs a payload of type `$3`."
m_no_payload:   .string "`$1.$2` has no payload."
m_expect:       .string "`$1` can only be used inside a `test` block."
m_defined_here: .string "`$1` is defined here."
m_ok_value:     .string "`ok` is called like a function: `ok()` or `ok(value)`."
m_parse_hint:   .string "`$1` converts numbers; to read a number from text use `parse_$1` (it returns a `result`)."
s_ok_name:      .string "ok"
s_ok_call:      .string "ok()"
m_not_imported: .string "`$1` is defined in another file but not imported: add `import $2;`."
e0204:          .string "E0204"
s_import_kw:    .string "import "
s_semi_nl:      .string ";\n"
s_new_kw:       .string "new "
o_str_conv:     .string "`int`, `float` or `bool`"
o_len:          .string "a `str`, list or map"
o_slice:        .string "a `str` or list"
o_list:         .string "a list"
o_map:          .string "a map"
o_sortable:     .string "a list of `int`, `float` or `str`"
o_number:       .string "`int` or `float`"
o_ordered:      .string "`int`, `float` or `str`"
s_tick:         .string "`"
s_comma:        .string ", "
s_lparen:       .string "("
s_rparen:       .string ")"
s_str_fn:       .string "str("
s_float_fn:     .string "float("
s_int_fn:       .string "int("

    .text
# int builtin_id(void *name)
    .globl builtin_id
builtin_id:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    li s1, 1
1:
    li t0, B_COUNT
    bgt s1, t0, 8f
    la t0, builtin_names
    slli t1, s1, 3
    add t0, t0, t1
    ld a1, 0(t0)
    mv a0, s0
    call str_eq_cstr
    bnez a0, 9f
    addi s1, s1, 1
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

# void *code_type(int c)
# The type for a builtin signature code.
    .globl code_type
code_type:
    addi sp, sp, -16
    sd ra, 8(sp)
    li t0, 'i'
    la t1, t_int
    beq a0, t0, 1f
    li t0, 'f'
    la t1, t_float
    beq a0, t0, 1f
    li t0, 'b'
    la t1, t_bool
    beq a0, t0, 1f
    li t0, 's'
    la t1, t_str
    beq a0, t0, 1f
    li t0, 'v'
    la t1, t_void
    beq a0, t0, 1f
    li t0, 'S'
    beq a0, t0, 2f
    li t0, 'r'
    la t1, t_str
    beq a0, t0, 3f
    li t0, 'u'
    la t1, t_void
    beq a0, t0, 3f
    li t0, 'n'
    la t1, t_int
    beq a0, t0, 3f
    la t1, t_float
3:
    ld a0, 0(t1)
    call ty_result
    j 9f
2:
    la t1, t_str
    ld a0, 0(t1)
    call ty_list
    j 9f
1:
    ld a0, 0(t1)
9:
    ld ra, 8(sp)
    addi sp, sp, 16
    ret

# void *src_text(void *node)
# The source text covered by a node.
    .globl src_text
src_text:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    mv s0, a0
    lwu a0, N_FILE(s0)
    call file_get
    ld t0, FILE_TEXT(a0)
    lwu t1, N_START(s0)
    lwu t2, N_END(s0)
    add a0, t0, t1
    sub a1, t2, t1
    call str_new
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret

# void suggest_conversion(void *diag, void *expected, void *found, void *node)
# Adds a fix wrapping the expression in str(...), float(...) or int(...) when that converts it.
    .globl suggest_conversion
suggest_conversion:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    mv s3, a3
    ld t0, TY_KIND(a1)
    ld t1, TY_KIND(a2)
    li t2, TY_STR
    bne t0, t2, 2f
    la s1, s_str_fn
    li t2, TY_INT
    beq t1, t2, 5f
    li t2, TY_FLOAT
    beq t1, t2, 5f
    li t2, TY_BOOL
    beq t1, t2, 5f
    j 9f
2:
    li t2, TY_FLOAT
    bne t0, t2, 3f
    la s1, s_float_fn
    li t2, TY_INT
    beq t1, t2, 5f
    j 9f
3:
    li t2, TY_INT
    bne t0, t2, 9f
    la s1, s_int_fn
    li t2, TY_FLOAT
    beq t1, t2, 5f
    j 9f
5:
    call sb_new
    mv s2, a0
    mv a1, s1
    call sb_cstr
    mv a0, s3
    call src_text
    mv a1, a0
    mv a0, s2
    call sb_str
    mv a0, s2
    li a1, ')'
    call sb_byte
    mv a0, s2
    call sb_finish
    mv a4, a0
    mv a0, s0
    lwu a1, N_FILE(s3)
    lwu a2, N_START(s3)
    lwu a3, N_END(s3)
    call diag_add_fix
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# void *bad_type(void)
    .globl bad_type
bad_type:
    la t0, t_error
    ld a0, 0(t0)
    ret

# void *err_one_of(void *node, char *expected_text, void *found)
# E0301 "Expected <one of>, found `T`." Returns the error type.
    .globl err_one_of
err_one_of:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    sd s2, 0(sp)
    mv s0, a0
    mv s1, a1
    mv s2, a2
    mv a0, s2
    call ty_bad
    bnez a0, 9f
    mv a0, s2
    call ty_name
    mv s2, a0
    mv a0, s1
    call str_from_cstr
    mv a1, a0
    la a0, m_one_of
    mv a2, s2
    li a3, 0
    call msg
    mv a2, a0
    la a0, e0301
    mv a1, s0
    call ck_err
9:
    call bad_type
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    ld s2, 0(sp)
    addi sp, sp, 32
    ret

# void *check_arg(void *call, long i, void *expected)
# Checks argument i against an expected type (0 = none); returns its type.
    .globl check_arg
check_arg:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    mv s1, a2
    ld a0, N_B(a0)
    call vec_get
    mv s0, a0
    mv a1, s1
    call check_expr
    mv s2, a0
    beqz s1, 9f
    mv a0, s0
    mv a1, s1
    mv a2, s2
    call expect_type
    mv s2, s1
9:
    mv a0, s2
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    addi sp, sp, 48
    ret

# int check_argc(void *call, long want)
# E0302 when the call has a different number of arguments (then checks them all, untyped). 1 = ok.
    .globl check_argc
check_argc:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    mv s1, a1
    ld t0, N_B(s0)
    ld s2, VEC_N(t0)
    beq s2, s1, 8f
    mv a0, s0
    call call_name
    mv s3, a0                   # name
    mv a0, s1
    call int_to_str
    sd a0, 0(sp)
    mv a0, s2
    call int_to_str
    mv a3, a0
    ld a2, 0(sp)
    mv a1, s3
    la a0, m_argc
    call msg
    mv a2, a0
    la a0, e0302
    mv a1, s0
    call ck_err
    li s1, 0
1:
    bge s1, s2, 2f
    mv a0, s0
    mv a1, s1
    li a2, 0
    call check_arg
    addi s1, s1, 1
    j 1b
2:
    li a0, 0
    j 9f
8:
    li a0, 1
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# void *check_sig_builtin(void *call, char *sig)
# A builtin with a fixed signature string.
    .globl check_sig_builtin
check_sig_builtin:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    mv s1, a1
    li s2, 0                    # count the argument codes
1:
    add t0, s1, s2
    lbu t0, 0(t0)
    li t1, '>'
    beq t0, t1, 2f
    addi s2, s2, 1
    j 1b
2:
    mv a0, s0
    mv a1, s2
    call check_argc
    beqz a0, 5f
    li s3, 0
3:
    bge s3, s2, 5f
    add t0, s1, s3
    lbu a0, 0(t0)
    call code_type
    mv a2, a0
    mv a0, s0
    mv a1, s3
    call check_arg
    addi s3, s3, 1
    j 3b
5:
    add t0, s1, s2
    lbu a0, 1(t0)
    call code_type
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

    .section .bss
    .align 3
    .globl ck_stmt_call
ck_stmt_call: .space 8              # the call of the expression statement being checked (hole statements)
    .text

# void *check_todo(void *call, void *expected)
# todo(desc) has the expected type (research R1-R4): W0601 with that type and the locals in scope,
# or E0601 when there is no expected type. A statement hole needs the function's return type.
check_todo:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    mv s1, a1
    mv a0, s0
    li a1, 1
    call check_argc
    beqz a0, 51f
    mv a0, s0
    li a1, 0
    la t0, t_str
    ld a2, 0(t0)
    call check_arg
    la t0, ck_fx
    ld t0, 0(t0)
    beqz t0, 2f
    li t1, 1
    sd t1, FX_HOLES(t0)
2:
    la t0, ck_stmt_call         # `todo(...);`: the function's return type, or void
    ld t0, 0(t0)
    bne t0, s0, 3f
    la t0, ck_ret
    ld s1, 0(t0)
    bnez s1, 3f
    la t0, t_void
    ld s1, 0(t0)
3:
    mv a0, s1
    call ty_bad
    bnez a0, 50f
    ld a0, N_B(s0)              # W0601: Hole: `<desc>` needs a value of type `T`.
    li a1, 0
    call vec_get
    call src_text
    mv s2, a0
    mv a0, s1
    call ty_name
    mv s3, a0
    la a0, m_hole
    mv a1, s2
    mv a2, s3
    li a3, 0
    call msg
    mv a2, a0
    la a0, w0601
    mv a1, s0
    call ck_warn
    mv s2, a0
    mv a1, s3
    li a2, 0
    call diag_set_ef
    call vec_new                # the locals in scope, innermost first
    mv s3, a0
    mv a0, s2
    mv a1, s3
    call diag_set_scope
    la t0, ck_scope
    ld s2, 0(t0)
4:
    beqz s2, 9f
    ld t0, SY_KIND(s2)
    li t1, SYM_LOCAL
    bne t0, t1, 5f
    li a0, 16
    call alloc
    sd a0, 0(sp)
    ld t0, SY_NAME(s2)
    sd t0, 0(a0)
    ld a0, SY_TYPE(s2)
    call ty_name
    ld a1, 0(sp)
    sd a0, 8(a1)
    mv a0, s3
    call vec_push
5:
    ld s2, SY_NEXT(s2)
    j 4b
50:
    la a0, m_hole_unknown
    call str_from_cstr
    mv a2, a0
    la a0, e0601
    mv a1, s0
    call ck_err
51:
    call bad_type
    j 99f
9:
    mv a0, s1
99:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# void *check_builtin(void *call, int id, void *expected)
    .globl check_builtin
check_builtin:
    addi sp, sp, -64
    sd ra, 56(sp)
    sd s0, 48(sp)
    sd s1, 40(sp)
    sd s2, 32(sp)
    sd s3, 24(sp)
    sd s4, 16(sp)
    mv s0, a0
    mv s1, a1
    mv s4, a2
    sd s1, N_D(s0)
    mv a0, s1
    call fx_builtin
    li t0, B_SORT_BY            # sort_by, map, filter, fold, any, all
    blt s1, t0, 0f
    li t0, B_ALL
    bgt s1, t0, 0f
    mv a0, s0
    mv a1, s1
    call check_hof
    j 99f
0:
    li t0, B_EXPECT             # expect / expect_eq only in tests
    blt s1, t0, 1f
    li t0, B_EXPECT_EQ
    bgt s1, t0, 1f
    la t0, ck_in_test
    ld t0, 0(t0)
    bnez t0, 1f
    ld t0, N_A(s0)
    la a0, e0410
    mv a1, s0
    la a2, m_expect
    ld a3, N_A(t0)
    call ck_err1
1:
    li t0, B_ASSUME             # assume only in properties
    bne s1, t0, 16f
    la t0, ck_in_test
    ld t0, 0(t0)
    li t1, 2
    beq t0, t1, 16f
    ld t0, N_A(s0)
    la a0, e0604
    mv a1, s0
    la a2, m_assume
    ld a3, N_A(t0)
    call ck_err1
16:
    li t0, 12                   # int(...) / float(...) of a str: point to parse_int / parse_float
    beq s1, t0, 15f
    li t0, 13
    beq s1, t0, 15f
14:
    la t0, builtin_sigs
    slli t1, s1, 3
    add t0, t0, t1
    ld a1, 0(t0)
    beqz a1, 2f
    mv a0, s0
    call check_sig_builtin
    j 99f
15:
    ld t0, N_B(s0)              # one str argument: a parse hint instead of the plain mismatch
    ld t1, VEC_N(t0)
    li t2, 1
    bne t1, t2, 14b
    ld a0, VEC_DATA(t0)
    ld a0, 0(a0)
    li a1, 0
    call check_expr
    mv s2, a0
    li a1, TY_STR
    call ty_is
    bnez a0, 152f
    ld t0, N_B(s0)              # not a str: the usual argument check, without checking it twice
    ld t0, VEC_DATA(t0)
    ld s3, 0(t0)
    li a0, 'f'
    li t1, 12
    beq s1, t1, 153f
    li a0, 'i'
153:
    call code_type
    mv a1, a0
    mv a0, s3
    mv a2, s2
    call expect_type
    j 154f
152:
    ld t0, N_A(s0)
    ld a1, N_A(t0)
    la a0, m_parse_hint
    li a2, 0
    li a3, 0
    call msg
    mv a2, a0
    la a0, e0301
    mv a1, s0
    call ck_err
154:
    la t0, t_int
    li t1, 12
    beq s1, t1, 151f
    la t0, t_float
151:
    ld a0, 0(t0)
    j 99f
2:
    li t0, B_TODO
    bne s1, t0, 3f
    mv a0, s0
    mv a1, s4
    call check_todo
    j 99f
3:
    li t0, B_STR
    beq s1, t0, 10f
    li t0, B_LEN
    beq s1, t0, 11f
    li t0, B_SLICE
    beq s1, t0, 12f
    li t0, B_PUSH
    beq s1, t0, 13f
    li t0, B_POP
    beq s1, t0, 14f
    li t0, B_SORT
    beq s1, t0, 15f
    li t0, B_HAS
    beq s1, t0, 16f
    li t0, B_REMOVE
    beq s1, t0, 16f
    li t0, B_KEYS
    beq s1, t0, 17f
    li t0, B_EXPECT_EQ
    beq s1, t0, 18f
    li t0, B_OK
    beq s1, t0, 19f
    li t0, B_ABS
    beq s1, t0, 21f
    li t0, B_MIN
    beq s1, t0, 22f
    li t0, B_MAX
    beq s1, t0, 22f
    j 20f                       # err
21:                             # abs(int|float) -> same type
    mv a0, s0
    li a1, 1
    call check_argc
    beqz a0, 98f
    mv a0, s0
    li a1, 0
    li a2, 0
    call check_arg
    mv s2, a0
    call ty_bad
    bnez a0, 98f
    ld t0, TY_KIND(s2)
    li t1, TY_INT
    beq t0, t1, 211f
    li t1, TY_FLOAT
    beq t0, t1, 211f
    ld a0, N_B(s0)
    li a1, 0
    call vec_get
    la a1, o_number
    mv a2, s2
    call err_one_of
    j 99f
211:
    mv a0, s2
    j 99f
22:                             # min/max(T, T) for int, float or str -> T
    mv a0, s0
    li a1, 2
    call check_argc
    beqz a0, 98f
    mv a0, s0
    li a1, 0
    li a2, 0
    call check_arg
    mv s2, a0
    call ty_bad
    bnez a0, 98f
    ld t0, TY_KIND(s2)
    li t1, TY_INT
    beq t0, t1, 221f
    li t1, TY_FLOAT
    beq t0, t1, 221f
    li t1, TY_STR
    beq t0, t1, 221f
    ld a0, N_B(s0)              # an `ordered` type parameter (E0311 otherwise)
    ld a0, VEC_DATA(a0)
    ld a1, 0(a0)
    mv a0, s2
    call ck_param_order
    bnez a0, 221f
    ld a0, N_B(s0)
    li a1, 0
    call vec_get
    la a1, o_ordered
    mv a2, s2
    call err_one_of
    j 99f
221:
    mv a0, s0
    li a1, 1
    mv a2, s2
    call check_arg
    mv a0, s2
    j 99f

10:                             # str(int|float|bool)
    mv a0, s0
    li a1, 1
    call check_argc
    beqz a0, 101f
    mv a0, s0
    li a1, 0
    li a2, 0
    call check_arg
    mv s2, a0
    ld t0, TY_KIND(s2)
    li t1, TY_INT
    beq t0, t1, 101f
    li t1, TY_FLOAT
    beq t0, t1, 101f
    li t1, TY_BOOL
    beq t0, t1, 101f
    ld a0, N_B(s0)
    li a1, 0
    call vec_get
    la a1, o_str_conv
    mv a2, s2
    call err_one_of
101:
    la t0, t_str
    ld a0, 0(t0)
    j 99f
11:                             # len(str|list|map)
    mv a0, s0
    li a1, 1
    call check_argc
    beqz a0, 111f
    mv a0, s0
    li a1, 0
    li a2, 0
    call check_arg
    mv s2, a0
    ld t0, TY_KIND(s2)
    li t1, TY_STR
    beq t0, t1, 111f
    li t1, TY_LIST
    beq t0, t1, 111f
    li t1, TY_MAP
    beq t0, t1, 111f
    ld a0, N_B(s0)
    li a1, 0
    call vec_get
    la a1, o_len
    mv a2, s2
    call err_one_of
111:
    la t0, t_int
    ld a0, 0(t0)
    j 99f
12:                             # slice(str|list, int, int)
    mv a0, s0
    li a1, 3
    call check_argc
    beqz a0, 98f
    mv a0, s0
    li a1, 0
    li a2, 0
    call check_arg
    mv s2, a0
    mv a0, s0
    li a1, 1
    la t0, t_int
    ld a2, 0(t0)
    call check_arg
    mv a0, s0
    li a1, 2
    la t0, t_int
    ld a2, 0(t0)
    call check_arg
    ld t0, TY_KIND(s2)
    li t1, TY_STR
    beq t0, t1, 121f
    li t1, TY_LIST
    beq t0, t1, 121f
    ld a0, N_B(s0)
    li a1, 0
    call vec_get
    la a1, o_slice
    mv a2, s2
    call err_one_of
    j 99f
121:
    mv a0, s2
    j 99f
13:                             # push(list[T], T)
    mv a0, s0
    li a1, 2
    call check_argc
    beqz a0, 131f
    mv a0, s0
    call list_arg
    mv s2, a0
    li a2, 0
    beqz s2, 132f
    ld a2, TY_A(s2)
132:
    mv a0, s0
    li a1, 1
    call check_arg
131:
    la t0, t_void
    ld a0, 0(t0)
    j 99f
14:                             # pop(list[T]) -> result[T]
    mv a0, s0
    li a1, 1
    call check_argc
    beqz a0, 98f
    mv a0, s0
    call list_arg
    beqz a0, 98f
    ld a0, TY_A(a0)
    call ty_result
    j 99f
15:                             # sort(list[int|float|str])
    mv a0, s0
    li a1, 1
    call check_argc
    beqz a0, 151f
    mv a0, s0
    li a1, 0
    li a2, 0
    call check_arg
    mv s2, a0
    call ty_bad
    bnez a0, 151f
    ld t0, TY_KIND(s2)
    li t1, TY_LIST
    bne t0, t1, 152f
    ld t0, TY_A(s2)
    ld t0, TY_KIND(t0)
    li t1, TY_INT
    beq t0, t1, 151f
    li t1, TY_FLOAT
    beq t0, t1, 151f
    li t1, TY_STR
    beq t0, t1, 151f
    ld a0, N_B(s0)              # a list of an `ordered` type parameter (E0311 otherwise)
    ld a0, VEC_DATA(a0)
    ld a1, 0(a0)
    ld a0, TY_A(s2)
    call ck_param_order
    bnez a0, 151f
152:
    ld a0, N_B(s0)
    li a1, 0
    call vec_get
    la a1, o_sortable
    mv a2, s2
    call err_one_of
151:
    la t0, t_void
    ld a0, 0(t0)
    j 99f
16:                             # has(map, key) -> bool / remove(map, key)
    mv a0, s0
    li a1, 2
    call check_argc
    beqz a0, 161f
    mv a0, s0
    call map_arg
    li a2, 0
    beqz a0, 162f
    ld a2, TY_A(a0)
162:
    mv a0, s0
    li a1, 1
    call check_arg
161:
    la t0, t_bool
    li t1, B_HAS
    beq s1, t1, 163f
    la t0, t_void
163:
    ld a0, 0(t0)
    j 99f
17:                             # keys(map[K, V]) -> list[K]
    mv a0, s0
    li a1, 1
    call check_argc
    beqz a0, 98f
    mv a0, s0
    call map_arg
    beqz a0, 98f
    ld a0, TY_A(a0)
    call ty_list
    j 99f
18:                             # expect_eq(T, T)
    mv a0, s0
    li a1, 2
    call check_argc
    beqz a0, 181f
    mv a0, s0
    li a1, 0
    li a2, 0
    call check_arg
    mv s2, a0                   # the first argument's type is the expected type of the second
    call ty_bad
    beqz a0, 182f
    li s2, 0
182:
    mv a0, s0
    li a1, 1
    mv a2, s2
    call check_arg
181:
    la t0, t_void
    ld a0, 0(t0)
    j 99f
19:                             # ok(v) / ok()
    mv a0, s4
    li a1, TY_RESULT
    call ty_is
    beqz a0, 30f
    ld s2, TY_A(s4)             # inner
    mv a0, s2
    li a1, TY_VOID
    call ty_is
    beqz a0, 191f
    mv a0, s0
    li a1, 0
    call check_argc
    mv a0, s4
    j 99f
191:
    mv a0, s0
    li a1, 1
    call check_argc
    beqz a0, 192f
    mv a0, s0
    li a1, 0
    mv a2, s2
    call check_arg
192:
    mv a0, s4
    j 99f
20:                             # err(msg)
    mv a0, s4
    li a1, TY_RESULT
    call ty_is
    beqz a0, 30f
    mv a0, s0
    li a1, 1
    call check_argc
    beqz a0, 201f
    mv a0, s0
    li a1, 0
    la t0, t_str
    ld a2, 0(t0)
    call check_arg
201:
    mv a0, s4
    j 99f
30:                             # ok/err without a known result type: E0307
    ld t0, N_B(s0)
    ld s2, VEC_N(t0)
    li s3, 0
31:
    bge s3, s2, 32f
    mv a0, s0
    mv a1, s3
    li a2, 0
    call check_arg
    addi s3, s3, 1
    j 31b
32:
    mv a0, s4
    call ty_bad
    beqz s4, 33f
    bnez a0, 98f
33:
    ld t0, N_A(s0)
    la a0, e0307
    mv a1, s0
    la a2, m_infer
    ld a3, N_A(t0)
    call ck_err1
98:
    call bad_type
99:
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    addi sp, sp, 64
    ret

# void *list_arg(void *call)
# Checks argument 0 as a list; returns its type, or 0 (after E0301) when it is not one.
    .globl list_arg
list_arg:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    li a1, 0
    li a2, 0
    call check_arg
    mv s1, a0
    li a1, TY_LIST
    call ty_is
    bnez a0, 8f
    ld a0, N_B(s0)
    li a1, 0
    call vec_get
    la a1, o_list
    mv a2, s1
    call err_one_of
    li s1, 0
8:
    mv a0, s1
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void *map_arg(void *call)
    .globl map_arg
map_arg:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    li a1, 0
    li a2, 0
    call check_arg
    mv s1, a0
    li a1, TY_MAP
    call ty_is
    bnez a0, 8f
    ld a0, N_B(s0)
    li a1, 0
    call vec_get
    la a1, o_map
    mv a2, s1
    call err_one_of
    li s1, 0
8:
    mv a0, s1
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void *check_call(void *call, void *expected)
    .globl check_call
check_call:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    mv s3, a1
    ld t0, N_A(s0)
    ld s1, N_A(t0)              # name
    mv a0, s1
    call scope_find
    mv s2, a0
    beqz s2, 5f
    call is_placeholder
    bnez a0, 7f                 # already reported as undefined
    ld t0, SY_KIND(s2)
    li t1, SYM_FN
    bne t0, t1, 4f
    mv a0, s2
    ld a1, N_A(s0)
    call rename_use
    mv a0, s2                   # spec 006 reference log
    ld a1, N_A(s0)
    li a2, RR_CALL
    call q_note_sym
    ld t0, SY_DECL(s2)          # user function
    sd t0, N_C(s0)
    mv a1, t0
    mv a0, s0
    call fx_call
    ld a0, N_C(s0)              # generic: inference
    call ty_tparams_of
    beqz a0, 0f
    mv a0, s0
    ld a1, N_C(s0)
    mv a2, s3
    call check_generic_call
    j 9f
0:
    ld t0, N_C(s0)
    ld t1, N_B(t0)              # params
    mv a0, s0
    ld a1, VEC_N(t1)
    call check_argc
    beqz a0, 3f
    li s3, 0
1:
    ld t0, N_C(s0)
    ld t1, N_B(t0)
    ld t2, VEC_N(t1)
    bge s3, t2, 3f
    mv a0, t1
    mv a1, s3
    call vec_get
    ld a2, N_TYPE(a0)
    ld t0, N_B(a0)              # the parameter's type as written (E0502 fixes)
    la t1, ck_exp_tnode
    sd t0, 0(t1)
    mv a0, s0
    mv a1, s3
    call check_arg
    la t0, ck_exp_tnode
    sd zero, 0(t0)
    addi s3, s3, 1
    j 1b
3:
    ld t0, N_C(s0)
    ld a0, N_TYPE(t0)
    j 9f
4:
    li t1, SYM_LOCAL            # a local holding a function value: call through the value
    bne t0, t1, 42f
    ld t0, SY_TYPE(s2)
    beqz t0, 42f
    ld t1, TY_KIND(t0)
    li t2, TY_FUNC
    beq t1, t2, 41f
    li t2, TY_ERROR
    bne t1, t2, 42f
41:
    ld t0, N_A(s0)              # f(args) becomes a call of the identifier f
    li a0, N_IDENT
    lwu a1, N_START(t0)
    lwu a2, N_END(t0)
    call node_new
    ld t0, N_A(s0)
    ld t1, N_A(t0)
    sd t1, N_A(a0)
    lwu t1, N_FILE(t0)
    sw t1, N_FILE(a0)
    sd a0, N_A(s0)
    li t0, N_CALL_VALUE
    sh t0, N_KIND(s0)
    mv a0, s0
    mv a1, s3
    call check_call_value
    j 9f
42:
    ld t0, SY_FLAGS(s2)         # calling a local still uses it
    ori t0, t0, SYF_USED
    sd t0, SY_FLAGS(s2)
    la a0, e0303                # a local, record or variant is not callable
    ld a1, N_A(s0)
    la a2, m_not_fn
    mv a3, s1
    call ck_err1
    j 7f
5:
    mv a0, s1
    call builtin_id
    beqz a0, 6f
    sd a0, 0(sp)
    mv a1, a0                   # spec 006 reference log: (0, builtin id)
    li a0, 0
    ld a2, N_A(s0)
    li a3, RR_CALL
    call q_note
    ld a1, 0(sp)
    mv a0, s0
    mv a2, s3
    call check_builtin
    j 9f
6:
    li a0, 0                    # spec 006 reference log: (0, name) for vo impact's removed functions
    mv a1, s1
    ld a2, N_A(s0)
    li a3, RR_CALL
    call q_note
    mv a0, s1                   # undefined function name
    ld a1, N_A(s0)
    call report_undefined
7:
    ld t0, N_B(s0)              # still check the arguments
    ld s2, VEC_N(t0)
    li s3, 0
71:
    bge s3, s2, 72f
    mv a0, s0
    mv a1, s3
    li a2, 0
    call check_arg
    addi s3, s3, 1
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

# void report_undefined(void *name, void *node)
# E0201 for a name that resolves to nothing, with a typo suggestion; then binds a silent
# placeholder (type error) so later uses of the name are not reported again.
    .globl report_undefined
report_undefined:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    mv s1, a1
    mv a3, a0
    la a0, e0201
    la a2, m_undefined
    call ck_err1
    sd a0, 0(sp)
    mv a0, s0                   # `return ok;`: ok and err are called like functions
    la a1, s_ok_name
    call str_eq_cstr
    beqz a0, 0f
    la a0, m_ok_value
    call str_from_cstr
    ld t0, 0(sp)
    sd a0, D_MSG(t0)
    la a0, s_ok_call
    call str_from_cstr
    mv a4, a0
    ld a0, 0(sp)
    lwu a1, N_FILE(s1)
    lwu a2, N_START(s1)
    lwu a3, N_END(s1)
    call diag_add_fix
    j 1f
0:
    ld a0, 0(sp)
    mv a1, s0
    mv a2, s1
    call suggest_name
    bnez a0, 1f
    ld a0, 0(sp)                # nothing similar: maybe another file defines it (E0204)
    mv a1, s0
    mv a2, s1
    call suggest_import
1:
    mv a0, s0
    li a1, SYM_LOCAL
    li a2, 0
    la t0, t_error
    ld a3, 0(t0)
    li a4, 0                    # no name node: marks the placeholder
    call sym_new
    li t0, SYF_USED
    sd t0, SY_FLAGS(a0)
    li t0, -1
    sd t0, SY_SLOT(a0)
    call scope_add
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# int is_placeholder(void *sym)
    .globl is_placeholder
is_placeholder:
    ld t0, SY_NODE(a0)
    seqz a0, t0
    ret

# int suggest_name(void *diag, void *name, void *node)
# A replacement fix with the closest visible name or builtin (edit distance <= 2 and shorter
# than the name), plus a note where that name is defined.
    .globl suggest_name
suggest_name:
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
    li s3, 0                    # best symbol
    li s4, 3                    # best distance
    li s6, 0                    # best name (str)
    la t0, ck_scope
    ld s5, 0(t0)
1:
    beqz s5, 3f
    mv a0, s5
    call is_placeholder
    bnez a0, 2f
    mv a0, s5
    mv a1, s2
    call sym_fits
    beqz a0, 2f
    mv a0, s1
    ld a1, SY_NAME(s5)
    call name_distance
    bge a0, s4, 2f
    mv s4, a0
    mv s3, s5
    ld s6, SY_NAME(s5)
2:
    ld s5, SY_NEXT(s5)
    j 1b
3:
    lhu t0, N_KIND(s2)          # builtins are suggested only where a call names them
    li t1, N_NAME
    bne t0, t1, 6f
    li s5, 1                    # builtins
4:
    li t0, B_COUNT
    bgt s5, t0, 6f
    la t0, builtin_names
    slli t1, s5, 3
    add t0, t0, t1
    ld a0, 0(t0)
    call str_from_cstr
    sd a0, 0(sp)
    mv a1, a0
    mv a0, s1
    call name_distance
    bge a0, s4, 5f
    mv s4, a0
    li s3, 0
    ld s6, 0(sp)
5:
    addi s5, s5, 1
    j 4b
6:
    beqz s6, 98f
    ld t0, STR_LEN(s1)          # short names (<= 4 characters) only at distance 1
    li t1, 4
    bgt t0, t1, 61f
    li t1, 1
    bgt s4, t1, 98f
61:
    bge s4, t0, 98f
    mv a0, s0
    lwu a1, N_FILE(s2)
    lwu a2, N_START(s2)
    lwu a3, N_END(s2)
    mv a4, s6
    call diag_add_fix
    li a0, 1
    beqz s3, 99f
    ld t0, SY_FLAGS(s3)         # the suggested name counts as used
    ori t0, t0, SYF_USED
    sd t0, SY_FLAGS(s3)
    la a0, m_defined_here
    mv a1, s6
    li a2, 0
    li a3, 0
    call msg
    mv a4, a0
    ld t0, SY_NODE(s3)
    mv a0, s0
    lwu a1, N_FILE(t0)
    lwu a2, N_START(t0)
    lwu a3, N_END(t0)
    call diag_add_related
    li a0, 1
    j 99f
98:
    li a0, 0
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

# void *check_ident(void *node)
    .globl check_ident
check_ident:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    ld a0, N_A(s0)
    call scope_find
    mv s1, a0
    beqz s1, 5f
    sd s1, N_B(s0)
    mv a0, s1
    mv a1, s0
    call rename_use
    ld t0, SY_KIND(s1)          # spec 006 reference log: a function used as a value, or a use
    li a2, RR_USE
    li t1, SYM_FN
    bne t0, t1, 1001f
    li a2, RR_VALUE
1001:
    mv a0, s1
    mv a1, s0
    call q_note_sym
    ld t0, SY_KIND(s1)
    li t1, SYM_LOCAL
    bne t0, t1, 4f
    ld t0, SY_FLAGS(s1)
    ori t0, t0, SYF_USED
    sd t0, SY_FLAGS(s1)
    mv a0, s1                   # its slot here (a capture inside anonymous functions)
    mv a1, s0
    call ck_capture
    sd a0, N_C(s0)
    ld a0, SY_TYPE(s1)
    bnez a0, 9f
    call bad_type
    j 9f
4:
    li t1, SYM_FN               # a named function as a value
    bne t0, t1, 41f
    lhu t0, N_FLAGS(s0)
    ori t0, t0, NF_FNVAL
    sh t0, N_FLAGS(s0)
    ld a0, SY_DECL(s1)
    sd a0, N_D(s0)
    call ty_func_of
    j 9f
41:
    la a0, e0301
    mv a1, s0
    la a2, m_not_value
    ld a3, N_A(s0)
    call ck_err1
    call bad_type
    j 9f
5:
    ld a0, N_A(s0)              # a builtin used as a value (`ok`/`err` have their own hint)
    call builtin_id
    beqz a0, 51f
    li t0, B_OK
    beq a0, t0, 51f
    li t0, B_ERR
    beq a0, t0, 51f
    la a0, e0201
    mv a1, s0
    la a2, m_builtin_value
    ld a3, N_A(s0)
    call ck_err1
    call bad_type
    j 9f
51:
    ld a0, N_A(s0)
    mv a1, s0
    call report_undefined
    call bad_type
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void *check_place(void *node, void *unused)
# Type of an assignment target (variable, field or element).
    .globl check_place
check_place:
    li a1, 0
    tail check_expr

# void *check_field(void *node)
    .globl check_field
check_field:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    mv s0, a0
    ld a0, N_A(s0)
    li a1, 0
    call check_expr
    mv s1, a0
    call ty_bad
    bnez a0, 8f
    ld s2, N_B(s0)              # field name node
    beqz s2, 8f
    mv a0, s1
    li a1, TY_RECORD
    call ty_is
    beqz a0, 5f
    ld t0, TY_DECL(s1)
    ld a0, N_B(t0)
    ld a1, N_A(s2)
    call member_index
    bltz a0, 5f
    sd a0, N_C(s0)
    ld a0, TY_DECL(s1)          # spec 006 reference log: (record, field)
    ld a1, N_C(s0)
    mv a2, s2
    li a3, RR_USE
    call q_note
    ld a1, N_C(s0)
    ld t0, TY_DECL(s1)
    ld a0, N_B(t0)
    call member_at
    ld t0, N_B(a0)
    ld a1, N_TYPE(t0)
    mv a0, s1
    call field_type_in
    j 9f
5:
    mv a0, s1
    call ty_name
    mv a1, a0
    la a0, m_no_field
    ld a2, N_A(s2)
    li a3, 0
    call msg
    mv a2, a0
    la a0, e0304
    mv a1, s2
    call ck_err
    mv a1, s1
    mv a2, s2
    call suggest_field
8:
    call bad_type
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    addi sp, sp, 48
    ret

# void suggest_field(void *diag, void *record_type, void *name_node)
# A replacement fix with the closest field name (edit distance <= 2 and shorter than the name).
    .globl suggest_field
suggest_field:
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
    li a1, TY_RECORD
    call ty_is
    beqz a0, 9f
    li s3, 0                    # index
    li s4, 3                    # best distance
    li s5, 0                    # best name
1:
    ld t0, TY_DECL(s1)
    ld a0, N_B(t0)
    mv a1, s3
    call member_at
    beqz a0, 3f
    addi s3, s3, 1
    ld t0, N_A(a0)
    ld t0, N_A(t0)
    sd t0, 0(sp)
    ld a0, N_A(s2)
    mv a1, t0
    call name_distance
    bge a0, s4, 1b
    mv s4, a0
    ld s5, 0(sp)
    j 1b
3:
    beqz s5, 9f
    ld t0, N_A(s2)
    ld t0, STR_LEN(t0)
    bge s4, t0, 9f
    mv a0, s0
    lwu a1, N_FILE(s2)
    lwu a2, N_START(s2)
    lwu a3, N_END(s2)
    mv a4, s5
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

# void *check_index(void *node)
    .globl check_index
check_index:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    mv s0, a0
    ld a0, N_A(s0)
    li a1, 0
    call check_expr
    mv s1, a0
    call ty_bad
    bnez a0, 7f
    ld t0, TY_KIND(s1)
    li t1, TY_LIST
    beq t0, t1, 1f
    li t1, TY_MAP
    beq t0, t1, 2f
    li t1, TY_STR
    beq t0, t1, 3f
    mv a0, s1
    call ty_name
    mv a3, a0
    la a0, e0308
    mv a1, s0
    la a2, m_index
    call ck_err1
    j 7f
3:
    la a0, m_index_str
    call str_from_cstr
    mv a2, a0
    la a0, e0308
    mv a1, s0
    call ck_err
7:
    ld a0, N_B(s0)
    li a1, 0
    call check_expr
    call bad_type
    j 9f
1:
    ld a0, N_B(s0)
    la t0, t_int
    ld a1, 0(t0)
    call check_expr
    mv a2, a0
    ld a0, N_B(s0)
    la t0, t_int
    ld a1, 0(t0)
    call expect_type
    ld a0, TY_A(s1)
    j 9f
2:
    lhu t0, N_FLAGS(s0)
    ori t0, t0, NF_MAP
    sh t0, N_FLAGS(s0)
    ld s2, TY_A(s1)
    ld a0, N_B(s0)
    mv a1, s2
    call check_expr
    mv a2, a0
    ld a0, N_B(s0)
    mv a1, s2
    call expect_type
    ld a0, TY_B(s1)
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    addi sp, sp, 48
    ret

# void *op_error(void *node, void *left, void *right)
# E0308 for an operator its operand types do not support; right = 0 for unary operators.
    .globl op_error
op_error:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    mv s1, a1
    mv s2, a2
    ld a0, N_C(s0)              # operator token kind
    call tok_raw
    mv s3, a0
    mv a0, s1
    call ty_name
    mv s1, a0
    la a0, m_operand1
    beqz s2, 1f
    mv a0, s2
    call ty_name
    mv a3, a0
    la a0, m_operands
    mv a1, s3
    mv a2, s1
    call msg
    j 2f
1:
    mv a1, s3
    mv a2, s1
    li a3, 0
    call msg
2:
    mv a2, a0
    la a0, e0308
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

# void *check_binary(void *node)
    .globl check_binary
check_binary:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    ld s3, N_C(s0)              # operator token
    li t0, T_AND
    beq s3, t0, 60f
    li t0, T_OR
    beq s3, t0, 60f
    ld a0, N_A(s0)
    li a1, 0
    call check_expr
    mv s1, a0
    ld a0, N_B(s0)
    mv a1, s1
    call check_expr
    mv s2, a0
    mv a0, s1
    call ty_bad
    bnez a0, 98f
    mv a0, s2
    call ty_bad
    bnez a0, 98f
    beq s1, s2, 1f
    ld t0, TY_KIND(s1)          # operand types differ: E0301 when a conversion helps, else E0308
    ld t1, TY_KIND(s2)
    li t2, T_PLUS
    bne s3, t2, 11f
    li t2, TY_STR
    beq t0, t2, 12f             # "a" + 1: str(1)
11:
    li t2, T_EQ
    beq s3, t2, 12f
    li t2, T_NE
    beq s3, t2, 12f
    li t2, TY_INT
    beq t0, t2, 13f
    li t2, TY_FLOAT
    bne t0, t2, 97f
13:
    li t2, TY_INT
    beq t1, t2, 14f
    li t2, TY_FLOAT
    bne t1, t2, 97f
    li t2, TY_INT               # int (left) with float (right): convert the left side to float
    bne t0, t2, 12f
    ld a0, N_A(s0)
    mv a1, s2
    mv a2, s1
    call expect_type
    j 98f
14:
12:
    ld a0, N_B(s0)
    mv a1, s1
    mv a2, s2
    call expect_type
    j 98f
1:
    ld t0, TY_KIND(s1)
    li t1, T_EQ
    beq s3, t1, 50f
    li t1, T_NE
    beq s3, t1, 51f
    li t1, T_LT
    blt s3, t1, 2f
    li t1, T_GE
    ble s3, t1, 40f
2:                              # arithmetic
    li t1, TY_INT
    beq t0, t1, 20f
    li t1, TY_FLOAT
    beq t0, t1, 30f
    li t1, TY_STR
    bne t0, t1, 97f
    li t1, T_PLUS
    bne s3, t1, 97f
    li t2, OP_CONCAT
    j 90f
20:
    li t1, T_PLUS
    li t2, OP_ADD_INT
    beq s3, t1, 90f
    li t1, T_MINUS
    li t2, OP_SUB_INT
    beq s3, t1, 90f
    li t1, T_STAR
    li t2, OP_MUL_INT
    beq s3, t1, 90f
    li t1, T_SLASH
    li t2, OP_DIV_INT
    beq s3, t1, 90f
    li t2, OP_REM_INT
    j 90f
30:
    li t1, T_PLUS
    li t2, OP_ADD_FLOAT
    beq s3, t1, 90f
    li t1, T_MINUS
    li t2, OP_SUB_FLOAT
    beq s3, t1, 90f
    li t1, T_STAR
    li t2, OP_MUL_FLOAT
    beq s3, t1, 90f
    li t1, T_SLASH
    li t2, OP_DIV_FLOAT
    beq s3, t1, 90f
    j 97f                       # % on floats
40:                             # ordering: int, float, str
    sub t3, s3, t1              # 0..3 for < <= > >= (t1 = T_GE here; recompute)
    li t1, T_LT
    sub t3, s3, t1
    li t1, TY_INT
    li t2, OP_LT_INT
    beq t0, t1, 41f
    li t1, TY_FLOAT
    li t2, OP_LT_FLOAT
    beq t0, t1, 41f
    li t1, TY_STR
    li t2, OP_LT_STR
    beq t0, t1, 41f
    li t1, TY_PARAM
    bne t0, t1, 97f
    mv a0, s1                   # an `ordered` type parameter (E0311 otherwise)
    mv a1, s0
    call ck_param_order
    li t1, T_LT
    sub t3, s3, t1
    li t2, OP_LT_PARAM
41:
    add t2, t2, t3
    sd t2, N_D(s0)
    la t0, t_bool
    ld a0, 0(t0)
    j 99f
50:
    li t2, OP_EQ
    j 52f
51:
    li t2, OP_NE
52:
    sd t2, N_D(s0)
    ld t0, TY_KIND(s1)
    li t1, TY_VOID
    beq t0, t1, 97f
    li t1, TY_FUNC              # functions cannot be compared
    beq t0, t1, 97f
    la t0, t_bool
    ld a0, 0(t0)
    j 99f
60:                             # and / or
    ld a0, N_A(s0)
    call check_cond
    ld a0, N_B(s0)
    call check_cond
    li t2, OP_AND
    li t1, T_AND
    beq s3, t1, 61f
    li t2, OP_OR
61:
    sd t2, N_D(s0)
    la t0, t_bool
    ld a0, 0(t0)
    j 99f
90:
    sd t2, N_D(s0)
    mv a0, s1
    j 99f
97:
    mv a0, s0
    mv a1, s1
    mv a2, s2
    call op_error
    j 99f
98:
    call bad_type
99:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# void *check_unary(void *node)
    .globl check_unary
check_unary:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    ld t0, N_C(s0)
    li t1, T_NOT
    bne t0, t1, 1f
    ld a0, N_A(s0)
    call check_cond
    li t0, OP_NOT
    sd t0, N_D(s0)
    la t0, t_bool
    ld a0, 0(t0)
    j 9f
1:
    ld a0, N_A(s0)
    li a1, 0
    call check_expr
    mv s1, a0
    call ty_bad
    bnez a0, 8f
    ld t0, TY_KIND(s1)
    li t1, TY_INT
    li t2, OP_NEG_INT
    beq t0, t1, 2f
    li t1, TY_FLOAT
    li t2, OP_NEG_FLOAT
    beq t0, t1, 2f
    mv a0, s0
    mv a1, s1
    li a2, 0
    call op_error
    j 9f
2:
    sd t2, N_D(s0)
    mv a0, s1
    j 9f
8:
    call bad_type
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void *check_try(void *node)
    .globl check_try
check_try:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    la t0, ck_ret
    ld a0, 0(t0)
    li a1, TY_RESULT
    call ty_is
    bnez a0, 1f
    la a0, m_try_fn
    call str_from_cstr
    mv a2, a0
    la a0, e0406
    mv a1, s0
    call ck_err
1:
    ld a0, N_A(s0)
    li a1, 0
    call check_expr
    mv s1, a0
    call ty_bad
    bnez a0, 8f
    mv a0, s1
    li a1, TY_RESULT
    call ty_is
    beqz a0, 2f
    ld a0, TY_A(s1)
    j 9f
2:
    mv a0, s1
    call ty_name
    mv a3, a0
    la a0, e0301
    ld a1, N_A(s0)
    la a2, m_try_type
    call ck_err1
8:
    call bad_type
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void *check_list(void *node, void *expected)
    .globl check_list
check_list:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    mv s2, a1                   # expected type
    li s1, 0                    # expected element type
    mv a0, a1
    li a1, TY_LIST
    call ty_is
    beqz a0, 1f
    ld s1, TY_A(s2)
1:
    ld t0, N_A(s0)
    ld t1, VEC_N(t0)
    bnez t1, 3f
    la a0, m_empty_list         # [] is never allowed; with a known type the fix is `new list[T]`
    call str_from_cstr
    mv a2, a0
    la a0, e0306
    mv a1, s0
    call ck_err
    beqz s1, 2f
    mv s3, a0
    call sb_new
    sd a0, 0(sp)
    la a1, s_new_kw
    call sb_cstr
    ld a0, 0(sp)
    mv a1, s2
    call ty_name_into
    ld a0, 0(sp)
    call sb_finish
    mv a4, a0
    mv a0, s3
    lwu a1, N_FILE(s0)
    lwu a2, N_START(s0)
    lwu a3, N_END(s0)
    call diag_add_fix
    mv a0, s2                   # the annotation still gives the list its type
    j 9f
2:
    call bad_type
    j 9f
3:
    ld a0, N_A(s0)
    li a1, 0
    call vec_get
    mv a1, s1
    call check_expr
    mv s2, a0                   # element type
    li s3, 1
4:
    ld t0, N_A(s0)
    ld t1, VEC_N(t0)
    bge s3, t1, 5f
    mv a0, t0
    mv a1, s3
    call vec_get
    sd a0, 0(sp)
    mv a1, s2
    call check_expr
    mv a2, a0
    ld a0, 0(sp)
    mv a1, s2
    call expect_type
    addi s3, s3, 1
    j 4b
5:
    mv a0, s2
    call ty_bad
    bnez a0, 6f
    mv a0, s2
    call ty_list
    j 9f
6:
    call bad_type
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# void *lookup_decl(void *name_node, int kind, char *not_kind_msg)
# The declaration a type name refers to (record or variant), or 0 after an error.
    .globl lookup_decl
lookup_decl:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    mv s1, a1
    mv s2, a2
    beqz s0, 8f
    ld a0, N_A(s0)
    call scope_find
    beqz a0, 7f
    mv s3, a0
    call is_placeholder
    bnez a0, 8f
    mv a0, s3
    mv a1, s0
    call rename_use
    mv a0, s3                   # spec 006 reference log
    mv a1, s0
    li a2, RR_USE
    call q_note_sym
    ld t0, SY_KIND(s3)
    bne t0, s1, 6f
    ld a0, SY_DECL(s3)
    j 9f
6:
    la a0, e0301
    mv a1, s0
    mv a2, s2
    ld a3, N_A(s0)
    call ck_err1
    j 8f
7:
    ld a0, N_A(s0)
    mv a1, s0
    call report_undefined
8:
    li a0, 0
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# void *check_new_record(void *node)
    .globl check_new_record
check_new_record:
    addi sp, sp, -80
    sd ra, 72(sp)
    sd s0, 64(sp)
    sd s1, 56(sp)
    sd s2, 48(sp)
    sd s3, 40(sp)
    sd s4, 32(sp)
    sd s5, 24(sp)
    mv s0, a0
    ld a0, N_A(s0)
    li a1, SYM_RECORD
    la a2, m_not_record
    call lookup_decl
    mv s1, a0
    sd s1, N_C(s0)
    ld s2, N_B(s0)              # inits
    beqz s1, 70f
    mv a0, s1                   # spec 006 reference log: (record, -2) marks a literal (vo edit add-field)
    li a1, -2
    mv a2, s0
    li a3, RR_USE
    call q_note
    mv a0, s0                   # the record type (an instance when generic)
    mv a1, s1
    li a2, TY_RECORD
    ld a3, N_D(s0)
    call resolve_instance
    sd a0, 16(sp)
    ld a0, N_B(s1)
    call count_members
    addi a0, a0, 1
    slli a0, a0, 3
    call alloc
    mv s5, a0                   # seen fields
    li s3, 0
1:
    ld t0, VEC_N(s2)
    bge s3, t0, 4f
    mv a0, s2
    mv a1, s3
    call vec_get
    mv s4, a0                   # N_FINIT
    addi s3, s3, 1
    ld a0, N_B(s1)
    ld t0, N_A(s4)
    ld a1, N_A(t0)
    call member_index
    bltz a0, 3f
    sd a0, N_C(s4)
    mv a1, a0                   # spec 006 reference log: (record, field)
    mv a0, s1
    ld a2, N_A(s4)
    li a3, RR_USE
    call q_note
    ld a0, N_C(s4)
    slli t0, a0, 3
    add t0, s5, t0
    li t1, 1
    sd t1, 0(t0)
    mv a1, a0
    ld a0, N_B(s1)
    call member_at
    mv a0, s2
    addi a1, s3, -1
    call vec_get
    mv s4, a0
    ld a0, N_B(s1)
    ld a1, N_C(s4)
    call member_at
    ld t0, N_B(a0)
    ld a1, N_TYPE(t0)
    ld a0, 16(sp)
    call field_type_in
    mv a1, a0
    sd a1, 0(sp)
    ld a0, N_B(s4)
    call check_expr
    mv a2, a0
    ld a0, N_B(s4)
    ld a1, 0(sp)
    call expect_type
    j 1b
3:
    ld a0, N_B(s4)              # unknown field: check the value anyway
    li a1, 0
    call check_expr
    li t0, -1
    sd t0, N_C(s4)
    ld t0, N_A(s1)
    ld a1, N_A(t0)              # record name
    la a0, m_no_field
    ld t0, N_A(s4)
    ld a2, N_A(t0)
    li a3, 0
    call msg
    mv a2, a0
    la a0, e0304
    ld a1, N_A(s4)
    call ck_err
    j 1b
4:
    call sb_new                 # missing fields
    mv s4, a0
    li s3, 0
    sd zero, 8(sp)
5:
    ld a0, N_B(s1)
    mv a1, s3
    call member_at
    beqz a0, 6f
    slli t0, s3, 3
    add t0, s5, t0
    ld t1, 0(t0)
    addi s3, s3, 1
    bnez t1, 5b
    sd a0, 0(sp)
    ld t0, 8(sp)
    beqz t0, 51f
    mv a0, s4
    la a1, s_comma
    call sb_cstr
51:
    li t0, 1
    sd t0, 8(sp)
    mv a0, s4
    li a1, '`'
    call sb_byte
    ld t0, 0(sp)
    ld t0, N_A(t0)
    ld a1, N_A(t0)
    mv a0, s4
    call sb_str
    mv a0, s4
    li a1, '`'
    call sb_byte
    j 5b
6:
    ld t0, 8(sp)
    beqz t0, 69f
    mv a0, s4
    call sb_finish
    mv a2, a0
    ld t0, N_A(s1)
    ld a1, N_A(t0)
    la a0, m_missing_fields
    li a3, 0
    call msg
    mv a2, a0
    la a0, e0305
    mv a1, s0
    call ck_err
69:
    ld a0, 16(sp)
    j 9f
70:
    li s3, 0                    # unknown record: check the values anyway
71:
    ld t0, VEC_N(s2)
    bge s3, t0, 72f
    mv a0, s2
    mv a1, s3
    call vec_get
    ld a0, N_B(a0)
    li a1, 0
    call check_expr
    addi s3, s3, 1
    j 71b
72:
    call bad_type
9:
    ld ra, 72(sp)
    ld s0, 64(sp)
    ld s1, 56(sp)
    ld s2, 48(sp)
    ld s3, 40(sp)
    ld s4, 32(sp)
    ld s5, 24(sp)
    addi sp, sp, 80
    ret

# void *str_new_copy(void *str)
    .globl str_new_copy
str_new_copy:
    ld a1, STR_LEN(a0)
    addi a0, a0, STR_DATA
    tail str_new

# void *check_vcase(void *node, void *expected)
    .globl check_vcase
check_vcase:
    addi sp, sp, -64
    sd ra, 56(sp)
    sd s0, 48(sp)
    sd s1, 40(sp)
    sd s2, 32(sp)
    sd s3, 24(sp)
    sd a1, 16(sp)               # the expected type
    mv s0, a0
    ld a0, N_A(s0)
    li a1, SYM_VARIANT
    la a2, m_not_variant
    call lookup_decl
    mv s1, a0
    sd s1, N_D(s0)
    beqz s1, 7f
    ld s2, N_B(s0)              # case name node
    beqz s2, 7f
    ld a0, N_B(s1)
    ld a1, N_A(s2)
    call member_index
    mv s3, a0
    sd s3, N_E(s0)
    bltz s3, 1001f
    mv a0, s1                   # spec 006 reference log: (variant, case)
    mv a1, s3
    mv a2, s2
    li a3, RR_USE
    call q_note
1001:
    bgez s3, 1f
    ld t0, N_A(s1)
    ld a2, N_A(t0)
    ld a1, N_A(s2)
    la a0, m_no_case
    li a3, 0
    call msg
    mv a2, a0
    la a0, e0201
    mv a1, s2
    call ck_err
    j 7f
1:
    ld a0, N_B(s1)
    mv a1, s3
    call member_at
    ld s3, N_B(a0)              # payload type node, or 0
    mv a0, s1
    call ty_tparams_of
    beqz a0, 0f
    ld t0, N_C(s0)              # a generic variant (a missing or extra payload is reported below)
    snez t0, t0
    snez t1, s3
    bne t0, t1, 0f
    mv a0, s0
    mv a1, s1
    mv a2, s3
    ld a3, 16(sp)
    call vcase_generic
    j 9f
0:
    ld t0, N_C(s0)
    beqz s3, 3f
    beqz t0, 2f
    ld a0, N_C(s0)              # payload given and expected
    ld a1, N_TYPE(s3)
    call check_expr
    mv a2, a0
    ld a0, N_C(s0)
    ld a1, N_TYPE(s3)
    call expect_type
    j 6f
2:
    ld t0, N_A(s1)              # payload missing
    ld a1, N_A(t0)
    ld a2, N_A(s2)
    ld a0, N_TYPE(s3)
    sd a1, 0(sp)
    sd a2, 8(sp)
    call ty_name
    mv a3, a0
    ld a1, 0(sp)
    ld a2, 8(sp)
    la a0, m_needs_payload
    call msg
    mv a2, a0
    la a0, e0302
    mv a1, s0
    call ck_err
    j 6f
3:
    beqz t0, 6f
    ld a0, N_C(s0)              # payload given but the case has none
    li a1, 0
    call check_expr
    ld t0, N_A(s1)
    ld a1, N_A(t0)
    ld a2, N_A(s2)
    la a0, m_no_payload
    li a3, 0
    call msg
    mv a2, a0
    la a0, e0302
    mv a1, s0
    call ck_err
6:
    mv a0, s1
    call decl_type
    j 9f
7:
    ld a0, N_C(s0)
    beqz a0, 8f
    li a1, 0
    call check_expr
8:
    call bad_type
9:
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    addi sp, sp, 64
    ret

# void *check_expr(void *node, void *expected)
# Types an expression (expected guides ok/err and empty lists); stores and returns the type.
    .globl check_expr
check_expr:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    mv s1, a1
    beqz s0, 98f
    la t0, stack_floor          # deeply nested expressions: give up quietly (the parser reported it)
    ld t0, 0(t0)
    li t1, 0x20000
    add t0, t0, t1
    bltu sp, t0, 98f
    lhu t0, N_KIND(s0)
    li t1, N_INT
    la t2, t_int
    beq t0, t1, 1f
    li t1, N_FLOAT
    la t2, t_float
    beq t0, t1, 1f
    li t1, N_STR
    la t2, t_str
    beq t0, t1, 1f
    li t1, N_BOOL
    la t2, t_bool
    beq t0, t1, 1f
    li t1, N_IDENT
    beq t0, t1, 10f
    li t1, N_CALL
    beq t0, t1, 11f
    li t1, N_FIELD
    beq t0, t1, 12f
    li t1, N_INDEX
    beq t0, t1, 13f
    li t1, N_BINARY
    beq t0, t1, 14f
    li t1, N_UNARY
    beq t0, t1, 15f
    li t1, N_TRY
    beq t0, t1, 16f
    li t1, N_LIST
    beq t0, t1, 17f
    li t1, N_NEW_RECORD
    beq t0, t1, 18f
    li t1, N_NEW_LIST
    beq t0, t1, 19f
    li t1, N_NEW_MAP
    beq t0, t1, 20f
    li t1, N_VCASE
    beq t0, t1, 21f
    li t1, N_LAMBDA
    beq t0, t1, 22f
    li t1, N_CALL_VALUE
    beq t0, t1, 23f
    j 98f
22:
    mv a0, s0
    call check_lambda
    j 99f
23:
    mv a0, s0
    mv a1, s1
    call check_call_value
    j 99f
1:
    ld a0, 0(t2)
    j 99f
10:
    mv a0, s0
    call check_ident
    j 99f
11:
    mv a0, s0
    mv a1, s1
    call check_call
    j 99f
12:
    mv a0, s0
    call check_field
    j 99f
13:
    mv a0, s0
    call check_index
    j 99f
14:
    mv a0, s0
    call check_binary
    j 99f
15:
    mv a0, s0
    call check_unary
    j 99f
16:
    mv a0, s0
    call check_try
    j 99f
17:
    mv a0, s0
    mv a1, s1
    call check_list
    j 99f
18:
    mv a0, s0
    call check_new_record
    j 99f
19:
    ld a0, N_A(s0)
    call resolve_type
    call ty_list
    j 99f
20:
    li a0, TY_MAP               # resolve both, reusing the map rules of resolve_type
    mv a1, s0
    call new_map_type
    j 99f
21:
    mv a0, s0
    mv a1, s1
    call check_vcase
    j 99f
98:
    call bad_type
99:
    beqz s0, 991f
    sd a0, N_TYPE(s0)
    la t0, q_collect            # spec 006: vo query type-at
    ld t0, 0(t0)
    beqz t0, 991f
    mv s1, a0
    mv a0, s0
    call q_note_expr
    mv a0, s1
991:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void *new_map_type(int unused, void *node)
# The type of `new map[K, V]` (E0309 for an invalid key type).
    .globl new_map_type
new_map_type:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    mv s0, a1
    li a0, N_TYPE_NODE          # build a temporary map type node and resolve it
    lwu a1, N_START(s0)
    lwu a2, N_END(s0)
    call node_new
    li t0, TY_MAP
    sd t0, N_A(a0)
    ld t0, N_A(s0)
    sd t0, N_B(a0)
    ld t0, N_B(s0)
    sd t0, N_C(a0)
    lwu t0, N_FILE(s0)
    sw t0, N_FILE(a0)
    call resolve_type
    ld ra, 24(sp)
    ld s0, 16(sp)
    addi sp, sp, 32
    ret

# void suggest_import(void *diag, void *name, void *node)
# When a .vo file under this file's directory defines `name`, turns the E0201 into E0204 with a
# fix inserting the import line.
    .globl suggest_import
suggest_import:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    mv s1, a1
    mv s2, a2
    lwu a0, N_FILE(s2)
    mv a1, s1
    call find_import
    beqz a0, 9f
    mv s3, a0                   # "a.b.name"
    la t0, e0204
    sd t0, D_CODE(s0)
    la a0, m_not_imported
    mv a1, s1
    mv a2, s3
    li a3, 0
    call msg
    sd a0, D_MSG(s0)
    call sb_new
    sd a0, 0(sp)
    la a1, s_import_kw
    call sb_cstr
    ld a0, 0(sp)
    mv a1, s3
    call sb_str
    ld a0, 0(sp)
    la a1, s_semi_nl
    call sb_cstr
    ld a0, 0(sp)
    call sb_finish
    sd a0, 0(sp)
    lwu a0, N_FILE(s2)
    call import_insert_at
    mv a2, a0
    mv a3, a0
    ld a4, 0(sp)
    mv a0, s0
    lwu a1, N_FILE(s2)
    call diag_add_fix
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# int sym_fits(void *sym, void *node)
# Whether a symbol is a sensible suggestion for this use: a record or variant for a capitalized
# name, a function (not main) at a call site, otherwise a local value.
    .globl sym_fits
sym_fits:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    mv s1, a1
    ld t0, SY_KIND(s0)
    ld t1, N_A(s1)              # the name being looked up (N_NAME or N_IDENT both keep it in A)
    lbu t2, STR_DATA(t1)
    li t3, 'A'
    blt t2, t3, 1f
    li t3, 'Z'
    bgt t2, t3, 1f
    li t3, SYM_RECORD           # a type name
    beq t0, t3, 7f
    li t3, SYM_VARIANT
    beq t0, t3, 7f
    j 8f
1:
    lhu t2, N_KIND(s1)
    li t3, N_NAME
    bne t2, t3, 3f
    li t3, SYM_FN               # a call: functions other than main
    bne t0, t3, 8f
    ld a0, SY_NAME(s0)
    la a1, s_main_name
    call str_eq_cstr
    seqz a0, a0
    j 9f
3:
    li t3, SYM_LOCAL
    bne t0, t3, 8f
7:
    li a0, 1
    j 9f
8:
    li a0, 0
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

    .section .rodata
s_main_name: .string "main"
    .text
