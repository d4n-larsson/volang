# Formatter: AST -> canonical text (4-space indent, imports sorted first, minimal parentheses).
    .include "defs.inc"

    .section .bss
    .align 3
fm_sb:      .space 8                # the output builder
fm_text:    .space 8                # source text of the file being formatted
fm_level:   .space 8                # indentation level of the statement being printed

    .section .rodata
s_indent:   .string "    "
s_let:      .string "let "
s_var:      .string "var "
s_set:      .string "set "
s_if:       .string "if "
s_else:     .string " else "
s_while:    .string "while "
s_for:      .string "for "
s_in:       .string " in "
s_dotdot:   .string ".."
s_match:    .string "match "
s_case:     .string "case "
s_return:   .string "return"
s_break:    .string "break;"
s_continue: .string "continue;"
s_fn:       .string "fn "
s_arrow:    .string " -> "
s_record:   .string "record "
s_variant:  .string "variant "
s_test:     .string "test "
s_uses_kw:  .string " uses "
s_fn_open:  .string "fn("
s_ordered_w: .string "ordered"
s_effects_w: .string "effects"
s_import:   .string "import "
s_colon:    .string ": "
s_comma:    .string ", "
s_eq:       .string " = "
s_under:    .string "_"
s_open:     .string " {\n"
s_close:    .string "}"
s_not:      .string "not "
s_try:      .string "try "
s_new:      .string "new "
s_rec_open: .string " { "
s_rec_close: .string " }"
s_list:     .string "list["
s_map:      .string "map["
s_result:   .string "result["
s_int:      .string "int"
s_float:    .string "float"
s_bool:     .string "bool"
s_str:      .string "str"
s_void:     .string "void"
s_true:     .string "true"
s_false:    .string "false"

    .text
# void fm_c(char *s)
    .globl fm_c
fm_c:
    mv a1, a0
    la t0, fm_sb
    ld a0, 0(t0)
    tail sb_cstr

# void fm_fn_type(void *type_node)
# `fn(int, str) -> bool uses console`.
    .globl fm_fn_type
fm_fn_type:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    la a0, s_fn_open
    call fm_c
    li s1, 0
1:
    ld t0, N_B(s0)
    ld t1, VEC_N(t0)
    bge s1, t1, 2f
    beqz s1, 11f
    la a0, s_comma
    call fm_c
11:
    ld a0, N_B(s0)
    mv a1, s1
    call vec_get
    call fm_type
    addi s1, s1, 1
    j 1b
2:
    li a0, ')'
    call fm_b
    ld t0, N_C(s0)
    beqz t0, 3f
    la a0, s_arrow
    call fm_c
    ld a0, N_C(s0)
    call fm_type
3:
    ld a0, N_D(s0)
    call fm_uses
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void fm_lambda(void *lambda)
# `fn(x: int) -> int {` + the body one level deeper than the statement it is in + `}`.
    .globl fm_lambda
fm_lambda:
    addi sp, sp, -48            # 0..15: scratch
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    mv s0, a0
    la t0, fm_level
    ld s2, 0(t0)
    la a0, s_fn_open
    call fm_c
    li s1, 0
1:
    ld t0, N_B(s0)
    ld t1, VEC_N(t0)
    bge s1, t1, 2f
    beqz s1, 11f
    la a0, s_comma
    call fm_c
11:
    ld a0, N_B(s0)
    mv a1, s1
    call vec_get
    ld t0, N_A(a0)
    sd a0, 0(sp)
    mv a0, t0
    call fm_name
    la a0, s_colon
    call fm_c
    ld t0, 0(sp)
    ld a0, N_B(t0)
    call fm_type
    addi s1, s1, 1
    j 1b
2:
    li a0, ')'
    call fm_b
    ld t0, N_C(s0)
    beqz t0, 3f
    la a0, s_arrow
    call fm_c
    ld a0, N_C(s0)
    call fm_type
3:
    ld t0, N_A(s0)              # a `uses` clause (E0507) is kept, so nothing is lost
    ld t0, N_B(t0)
    ld a0, FX_USES(t0)
    call fm_uses
    li a0, ' '
    call fm_b
    ld a0, N_D(s0)
    mv a1, s2
    call fm_block
    la t0, fm_level             # a later anonymous function in the same statement
    sd s2, 0(t0)
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    addi sp, sp, 48
    ret

# void fm_type_args(void *type_nodes)
# `[int, str]` (nothing for 0).
    .globl fm_type_args
fm_type_args:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    beqz s0, 9f
    li a0, '['
    call fm_b
    li s1, 0
1:
    ld t0, VEC_N(s0)
    bge s1, t0, 2f
    beqz s1, 11f
    la a0, s_comma
    call fm_c
11:
    mv a0, s0
    mv a1, s1
    call vec_get
    call fm_type
    addi s1, s1, 1
    j 1b
2:
    li a0, ']'
    call fm_b
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void fm_tparams(void *tparams)
# `[T, K: ordered, E: effects]` (nothing for 0).
    .globl fm_tparams
fm_tparams:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    sd s2, 0(sp)
    mv s0, a0
    beqz s0, 9f
    li a0, '['
    call fm_b
    li s1, 0
1:
    ld t0, VEC_N(s0)
    bge s1, t0, 2f
    beqz s1, 11f
    la a0, s_comma
    call fm_c
11:
    mv a0, s0
    mv a1, s1
    call vec_get
    mv s2, a0
    ld a0, N_A(s2)
    call fm_name
    ld t0, N_B(s2)
    beqz t0, 12f
    la a0, s_colon
    call fm_c
    ld t0, N_B(s2)
    la a0, s_ordered_w
    li t1, TPC_ORDERED
    beq t0, t1, 13f
    la a0, s_effects_w
13:
    call fm_c
12:
    addi s1, s1, 1
    j 1b
2:
    li a0, ']'
    call fm_b
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    ld s2, 0(sp)
    addi sp, sp, 32
    ret

# void fm_uses(void *uses)
# ` uses ...` in canonical order (effects by kind, scopes sorted and normalized). A clause with an
# unknown name or invalid scope is printed as written, so formatting never loses text.
    .globl fm_uses
fm_uses:
    addi sp, sp, -64
    sd ra, 56(sp)
    sd s0, 48(sp)
    sd s1, 40(sp)
    sd s2, 32(sp)
    sd s3, 24(sp)
    sd s4, 16(sp)
    sd s5, 8(sp)
    mv s0, a0
    beqz s0, 9f
    la a0, s_uses_kw
    call fm_c
    li a0, 0
    call es_new
    mv s1, a0
    ld s2, N_A(s0)
    li s3, 0
1:
    ld t0, VEC_N(s2)
    bge s3, t0, 5f
    mv a0, s2
    mv a1, s3
    call vec_get
    addi s3, s3, 1
    mv s4, a0
    ld s5, N_A(s4)
    beqz s5, 6f                 # unknown name or effect parameter: as written
    ld t0, N_B(s4)
    bnez t0, 2f
    ld t0, ES_MASK(s1)
    or t0, t0, s5
    sd t0, ES_MASK(s1)
    j 1b
2:
    sd zero, 0(sp)
3:
    ld t0, N_B(s4)
    ld t1, VEC_N(t0)
    ld t2, 0(sp)
    bge t2, t1, 1b
    mv a0, t0
    mv a1, t2
    call vec_get
    ld a2, N_A(a0)
    mv a0, s1
    mv a1, s5
    call es_add_scope
    beqz a0, 6f
    ld t2, 0(sp)
    addi t2, t2, 1
    sd t2, 0(sp)
    j 3b
5:
    mv a0, s1
    la t0, fm_sb
    ld a1, 0(t0)
    call es_render
    j 9f
6:
    li s3, 0                    # as written
61:
    ld t0, VEC_N(s2)
    bge s3, t0, 9f
    beqz s3, 62f
    la a0, s_comma
    call fm_c
62:
    mv a0, s2
    mv a1, s3
    call vec_get
    addi s3, s3, 1
    mv s4, a0
    ld a0, N_D(s4)
    call fm_s
    ld t0, N_B(s4)
    beqz t0, 61b
    li a0, '('
    call fm_b
    li s5, 0
63:
    ld t0, N_B(s4)
    ld t1, VEC_N(t0)
    bge s5, t1, 64f
    beqz s5, 631f
    la a0, s_comma
    call fm_c
631:
    ld a0, N_B(s4)
    mv a1, s5
    call vec_get
    ld a1, N_A(a0)
    la t0, fm_sb
    ld a0, 0(t0)
    call render_str_lit
    addi s5, s5, 1
    j 63b
64:
    li a0, ')'
    call fm_b
    j 61b
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

# void fm_s(void *str)
    .globl fm_s
fm_s:
    mv a1, a0
    la t0, fm_sb
    ld a0, 0(t0)
    tail sb_str

# void fm_b(int byte)
    .globl fm_b
fm_b:
    mv a1, a0
    la t0, fm_sb
    ld a0, 0(t0)
    tail sb_byte

# void fm_indent(long level)
    .globl fm_indent
fm_indent:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    mv s0, a0
1:
    blez s0, 2f
    la a0, s_indent
    call fm_c
    addi s0, s0, -1
    j 1b
2:
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret

# void fm_name(void *name_node)
    .globl fm_name
fm_name:
    beqz a0, 1f
    ld a0, N_A(a0)
    tail fm_s
1:
    ret

# void fm_source(void *node)
# The node's source text (comments).
    .globl fm_source
fm_source:
    lwu t0, N_START(a0)
    lwu t1, N_END(a0)
    la t2, fm_text
    ld t2, 0(t2)
    add a1, t2, t0
    sub a2, t1, t0
    la t0, fm_sb
    ld a0, 0(t0)
    tail sb_bytes

# void fm_type(void *type_node)
    .globl fm_type
fm_type:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    mv s0, a0
    beqz s0, 9f
    ld t0, N_A(s0)
    li t1, TY_INT
    la a0, s_int
    beq t0, t1, 1f
    li t1, TY_FLOAT
    la a0, s_float
    beq t0, t1, 1f
    li t1, TY_BOOL
    la a0, s_bool
    beq t0, t1, 1f
    li t1, TY_STR
    la a0, s_str
    beq t0, t1, 1f
    li t1, TY_VOID
    la a0, s_void
    beq t0, t1, 1f
    li t1, TY_NAMED
    beq t0, t1, 2f
    li t1, TY_LIST
    la a0, s_list
    beq t0, t1, 3f
    li t1, TY_RESULT
    la a0, s_result
    beq t0, t1, 3f
    li t1, TY_FUNC
    bne t0, t1, 5f
    mv a0, s0
    call fm_fn_type
    j 9f
5:
    la a0, s_map                # map[K, V]
    call fm_c
    ld a0, N_B(s0)
    call fm_type
    la a0, s_comma
    call fm_c
    ld a0, N_C(s0)
    call fm_type
    j 4f
1:
    call fm_c
    j 9f
2:
    ld a0, N_B(s0)
    call fm_name
    ld a0, N_C(s0)              # Name[args]
    call fm_type_args
    j 9f
3:
    call fm_c
    ld a0, N_B(s0)
    call fm_type
4:
    li a0, ']'
    call fm_b
9:
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret

# long expr_prec(void *node)
# or 1, and 2, not 3, comparison 4, + - 5, * / % 6, unary - and try 7, postfix 8, primary 9.
    .globl expr_prec
expr_prec:
    lhu t0, N_KIND(a0)
    li t1, N_BINARY
    beq t0, t1, 1f
    li t1, N_UNARY
    beq t0, t1, 5f
    li t1, N_TRY
    li a1, 7
    beq t0, t1, 9f
    li t1, N_FIELD
    li a1, 8
    beq t0, t1, 9f
    li t1, N_INDEX
    beq t0, t1, 9f
    li t1, N_CALL_VALUE
    beq t0, t1, 9f
    li a1, 9
    j 9f
1:
    ld t0, N_C(a0)
    li a1, 1
    li t1, T_OR
    beq t0, t1, 9f
    li a1, 2
    li t1, T_AND
    beq t0, t1, 9f
    li a1, 5
    li t1, T_PLUS
    beq t0, t1, 9f
    li t1, T_MINUS
    beq t0, t1, 9f
    li a1, 6
    li t1, T_STAR
    beq t0, t1, 9f
    li t1, T_SLASH
    beq t0, t1, 9f
    li t1, T_PERCENT
    beq t0, t1, 9f
    li a1, 4
    j 9f
5:
    ld t0, N_C(a0)
    li a1, 3
    li t1, T_NOT
    beq t0, t1, 9f
    li a1, 7
9:
    mv a0, a1
    ret

# void fm_list(void *vec, long ctx)
# Comma-separated expressions.
    .globl fm_list
fm_list:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    li s1, 0
    beqz s0, 9f
1:
    ld t0, VEC_N(s0)
    bge s1, t0, 9f
    beqz s1, 2f
    la a0, s_comma
    call fm_c
2:
    mv a0, s0
    mv a1, s1
    call vec_get
    li a1, 0
    call fm_expr
    addi s1, s1, 1
    j 1b
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void fm_expr(void *node, long ctx)
# Prints an expression, in parentheses when its precedence is below ctx.
    .globl fm_expr
fm_expr:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    beqz s0, 99f
    mv s1, a1
    call expr_prec
    mv s2, a0                   # own precedence
    slt s3, s2, s1              # needs parentheses
    beqz s3, 1f
    li a0, '('
    call fm_b
1:
    lhu t0, N_KIND(s0)
    li t1, N_INT
    beq t0, t1, 10f
    li t1, N_FLOAT
    beq t0, t1, 11f
    li t1, N_STR
    beq t0, t1, 12f
    li t1, N_BOOL
    beq t0, t1, 13f
    li t1, N_IDENT
    beq t0, t1, 14f
    li t1, N_CALL
    beq t0, t1, 15f
    li t1, N_BINARY
    beq t0, t1, 16f
    li t1, N_UNARY
    beq t0, t1, 17f
    li t1, N_TRY
    beq t0, t1, 18f
    li t1, N_FIELD
    beq t0, t1, 19f
    li t1, N_INDEX
    beq t0, t1, 20f
    li t1, N_LIST
    beq t0, t1, 21f
    li t1, N_NEW_RECORD
    beq t0, t1, 22f
    li t1, N_NEW_LIST
    beq t0, t1, 23f
    li t1, N_NEW_MAP
    beq t0, t1, 24f
    li t1, N_VCASE
    beq t0, t1, 25f
    li t1, N_LAMBDA
    beq t0, t1, 26f
    li t1, N_CALL_VALUE
    beq t0, t1, 27f
    j 90f
26:
    mv a0, s0
    call fm_lambda
    j 90f
27:                             # callee(args)
    ld a0, N_A(s0)
    li a1, 8
    call fm_expr
    li a0, '('
    call fm_b
    ld a0, N_B(s0)
    li a1, 0
    call fm_list
    li a0, ')'
    call fm_b
    j 90f
10:
    la t0, fm_sb
    ld a0, 0(t0)
    ld a1, N_A(s0)
    call int_to_dec
    j 90f
11:
    la t0, fm_sb
    ld a0, 0(t0)
    ld a1, N_A(s0)
    call float_to_text
    j 90f
12:
    la t0, fm_sb
    ld a0, 0(t0)
    ld a1, N_A(s0)
    call render_str_lit
    j 90f
13:
    la a0, s_false
    ld t0, N_A(s0)
    beqz t0, 131f
    la a0, s_true
131:
    call fm_c
    j 90f
14:
    ld a0, N_A(s0)
    call fm_s
    j 90f
15:
    ld a0, N_A(s0)
    call fm_name
    li a0, '('
    call fm_b
    ld a0, N_B(s0)
    li a1, 0
    call fm_list
    li a0, ')'
    call fm_b
    j 90f
16:                             # binary: left at own level, right one higher (comparisons: both)
    mv a1, s2
    li t0, 4
    bne s2, t0, 161f
    addi a1, s2, 1
161:
    ld a0, N_A(s0)
    call fm_expr
    li a0, ' '
    call fm_b
    ld a0, N_C(s0)
    call tok_raw
    call fm_s
    li a0, ' '
    call fm_b
    ld a0, N_B(s0)
    addi a1, s2, 1
    call fm_expr
    j 90f
17:
    ld t0, N_C(s0)
    li t1, T_NOT
    bne t0, t1, 171f
    la a0, s_not
    call fm_c
    j 172f
171:
    li a0, '-'
    call fm_b
172:
    ld a0, N_A(s0)
    mv a1, s2
    call fm_expr
    j 90f
18:
    la a0, s_try
    call fm_c
    ld a0, N_A(s0)
    li a1, 7
    call fm_expr
    j 90f
19:
    ld a0, N_A(s0)
    li a1, 8
    call fm_expr
    li a0, '.'
    call fm_b
    ld a0, N_B(s0)
    call fm_name
    j 90f
20:
    ld a0, N_A(s0)
    li a1, 8
    call fm_expr
    li a0, '['
    call fm_b
    ld a0, N_B(s0)
    li a1, 0
    call fm_expr
    li a0, ']'
    call fm_b
    j 90f
21:
    li a0, '['
    call fm_b
    ld a0, N_A(s0)
    li a1, 0
    call fm_list
    li a0, ']'
    call fm_b
    j 90f
22:                             # new T { f: e, ... }
    la a0, s_new
    call fm_c
    ld a0, N_A(s0)
    call fm_name
    ld a0, N_D(s0)
    call fm_type_args
    la a0, s_rec_open
    call fm_c
    li s3, 0
221:
    ld t0, N_B(s0)
    ld t1, VEC_N(t0)
    bge s3, t1, 222f
    beqz s3, 223f
    la a0, s_comma
    call fm_c
223:
    ld a0, N_B(s0)
    mv a1, s3
    call vec_get
    sd a0, 0(sp)
    ld a0, N_A(a0)
    call fm_name
    la a0, s_colon
    call fm_c
    ld t0, 0(sp)
    ld a0, N_B(t0)
    li a1, 0
    call fm_expr
    addi s3, s3, 1
    j 221b
222:
    la a0, s_rec_close
    call fm_c
    slt s3, s2, s1              # (s3 was reused as a counter)
    j 90f
23:
    la a0, s_new
    call fm_c
    la a0, s_list
    call fm_c
    ld a0, N_A(s0)
    call fm_type
    li a0, ']'
    call fm_b
    j 90f
24:
    la a0, s_new
    call fm_c
    la a0, s_map
    call fm_c
    ld a0, N_A(s0)
    call fm_type
    la a0, s_comma
    call fm_c
    ld a0, N_B(s0)
    call fm_type
    li a0, ']'
    call fm_b
    j 90f
25:
    ld a0, N_A(s0)
    call fm_name
    li a0, '.'
    call fm_b
    ld a0, N_B(s0)
    call fm_name
    ld t0, N_C(s0)
    beqz t0, 90f
    li a0, '('
    call fm_b
    ld a0, N_C(s0)
    li a1, 0
    call fm_expr
    li a0, ')'
    call fm_b
90:
    beqz s3, 99f
    li a0, ')'
    call fm_b
99:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# void fm_block(void *block, long level)
# "{", the statements one level deeper, then "}" at this level (no newline after it).
    .globl fm_block
fm_block:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    sd s2, 0(sp)
    mv s0, a0
    mv s1, a1
    li a0, '{'
    call fm_b
    li a0, 10
    call fm_b
    li s2, 0
1:
    ld t0, N_A(s0)
    ld t1, VEC_N(t0)
    bge s2, t1, 2f
    mv a0, t0
    mv a1, s2
    call vec_get
    addi a1, s1, 1
    call fm_stmt
    addi s2, s2, 1
    j 1b
2:
    mv a0, s1
    call fm_indent
    la a0, s_close
    call fm_c
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    ld s2, 0(sp)
    addi sp, sp, 32
    ret

# void fm_if(void *node, long level)
# if ... { } [else if ... | else { }] without the leading indent or trailing newline.
    .globl fm_if
fm_if:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    mv s1, a1
    la a0, s_if
    call fm_c
    ld a0, N_A(s0)
    li a1, 0
    call fm_expr
    li a0, ' '
    call fm_b
    ld a0, N_B(s0)
    mv a1, s1
    call fm_block
    ld t0, N_C(s0)
    beqz t0, 9f
    la a0, s_else
    call fm_c
    ld a0, N_C(s0)
    lhu t0, N_KIND(a0)
    li t1, N_IF
    mv a1, s1
    bne t0, t1, 1f
    call fm_if
    j 9f
1:
    call fm_block
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void fm_stmt(void *stmt, long level)
# One statement (with its indentation and newline).
    .globl fm_stmt
fm_stmt:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    mv s0, a0
    mv s1, a1
    la t0, fm_level
    sd s1, 0(t0)
    mv a0, s1
    call fm_indent
    lhu t0, N_KIND(s0)
    li t1, N_COMMENT
    beq t0, t1, 10f
    li t1, N_LET
    beq t0, t1, 11f
    li t1, N_SET
    beq t0, t1, 12f
    li t1, N_IF
    beq t0, t1, 13f
    li t1, N_WHILE
    beq t0, t1, 14f
    li t1, N_FOR
    beq t0, t1, 15f
    li t1, N_MATCH
    beq t0, t1, 16f
    li t1, N_RETURN
    beq t0, t1, 17f
    li t1, N_BREAK
    la a0, s_break
    beq t0, t1, 18f
    li t1, N_CONTINUE
    la a0, s_continue
    beq t0, t1, 18f
    li t1, N_EXPR_STMT
    beq t0, t1, 19f
    j 98f
10:
    mv a0, s0
    call fm_source
    j 98f
11:
    la a0, s_let
    lhu t0, N_FLAGS(s0)
    andi t0, t0, NF_MUT
    beqz t0, 111f
    la a0, s_var
111:
    call fm_c
    lhu t0, N_FLAGS(s0)
    andi t0, t0, NF_DISCARD
    beqz t0, 112f
    la a0, s_under
    call fm_c
    j 113f
112:
    ld a0, N_A(s0)
    call fm_name
113:
    ld t0, N_B(s0)
    beqz t0, 114f
    la a0, s_colon
    call fm_c
    ld a0, N_B(s0)
    call fm_type
114:
    la a0, s_eq
    call fm_c
    ld a0, N_C(s0)
    li a1, 0
    call fm_expr
    j 97f
12:
    la a0, s_set
    call fm_c
    ld a0, N_A(s0)
    li a1, 0
    call fm_expr
    la a0, s_eq
    call fm_c
    ld a0, N_B(s0)
    li a1, 0
    call fm_expr
    j 97f
13:
    mv a0, s0
    mv a1, s1
    call fm_if
    j 98f
14:
    la a0, s_while
    call fm_c
    ld a0, N_A(s0)
    li a1, 0
    call fm_expr
    li a0, ' '
    call fm_b
    ld a0, N_B(s0)
    mv a1, s1
    call fm_block
    j 98f
15:
    la a0, s_for
    call fm_c
    ld a0, N_A(s0)
    bnez a0, 150f
    la a0, s_under
    call fm_c
    j 1501f
150:
    call fm_name
1501:
    la a0, s_in
    call fm_c
    ld a0, N_B(s0)
    li a1, 0
    call fm_expr
    ld t0, N_C(s0)
    beqz t0, 151f
    la a0, s_dotdot
    call fm_c
    ld a0, N_C(s0)
    li a1, 0
    call fm_expr
151:
    li a0, ' '
    call fm_b
    ld a0, N_D(s0)
    mv a1, s1
    call fm_block
    j 98f
16:                             # match expr { case ... }
    la a0, s_match
    call fm_c
    ld a0, N_A(s0)
    li a1, 0
    call fm_expr
    la a0, s_open
    call fm_c
    li s2, 0
161:
    ld t0, N_B(s0)
    ld t1, VEC_N(t0)
    bge s2, t1, 169f
    mv a0, t0
    mv a1, s2
    call vec_get
    addi s2, s2, 1
    sd a0, 0(sp)
    addi a0, s1, 1
    call fm_indent
    ld a0, 0(sp)
    lhu t0, N_KIND(a0)
    li t1, N_COMMENT
    bne t0, t1, 162f
    call fm_source
    li a0, 10
    call fm_b
    j 161b
162:
    la a0, s_case
    call fm_c
    ld t0, 0(sp)
    ld a0, N_A(t0)
    call fm_name
    ld t0, 0(sp)
    lhu t1, N_FLAGS(t0)
    andi t1, t1, NF_DISCARD
    bnez t1, 163f
    ld t1, N_B(t0)
    beqz t1, 164f
    li a0, '('
    call fm_b
    ld t0, 0(sp)
    ld a0, N_B(t0)
    call fm_name
    li a0, ')'
    call fm_b
    j 164f
163:
    li a0, '('
    call fm_b
    la a0, s_under
    call fm_c
    li a0, ')'
    call fm_b
164:
    li a0, ' '
    call fm_b
    ld t0, 0(sp)
    ld a0, N_C(t0)
    addi a1, s1, 1
    call fm_block
    li a0, 10
    call fm_b
    j 161b
169:
    mv a0, s1
    call fm_indent
    la a0, s_close
    call fm_c
    j 98f
17:
    la a0, s_return
    call fm_c
    ld t0, N_A(s0)
    beqz t0, 97f
    li a0, ' '
    call fm_b
    ld a0, N_A(s0)
    li a1, 0
    call fm_expr
    j 97f
18:
    call fm_c
    j 98f
19:
    ld a0, N_A(s0)
    li a1, 0
    call fm_expr
97:
    li a0, 59                   # ';'
    call fm_b
98:
    li a0, 10
    call fm_b
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    addi sp, sp, 48
    ret

# void fm_members(void *vec, int is_record)
# The body lines of a record (name: type;) or variant (name; / name(type);).
    .globl fm_members
fm_members:
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
    ld t0, VEC_N(s0)
    bge s2, t0, 9f
    mv a0, s0
    mv a1, s2
    call vec_get
    mv s3, a0
    addi s2, s2, 1
    la a0, s_indent
    call fm_c
    lhu t0, N_KIND(s3)
    li t1, N_COMMENT
    bne t0, t1, 2f
    mv a0, s3
    call fm_source
    j 5f
2:
    ld a0, N_A(s3)
    call fm_name
    ld t0, N_B(s3)
    beqz s1, 3f
    la a0, s_colon
    call fm_c
    ld a0, N_B(s3)
    call fm_type
    j 4f
3:
    beqz t0, 4f
    li a0, '('
    call fm_b
    ld a0, N_B(s3)
    call fm_type
    li a0, ')'
    call fm_b
4:
    li a0, 59
    call fm_b
5:
    li a0, 10
    call fm_b
    j 1b
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# void fm_item(void *item)
    .globl fm_item
fm_item:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    lhu t0, N_KIND(s0)
    li t1, N_COMMENT
    beq t0, t1, 10f
    li t1, N_IMPORT
    beq t0, t1, 11f
    li t1, N_FN
    beq t0, t1, 12f
    li t1, N_RECORD
    beq t0, t1, 13f
    li t1, N_VARIANT
    beq t0, t1, 14f
    li t1, N_TEST
    beq t0, t1, 15f
    j 98f
10:
    mv a0, s0
    call fm_source
    j 98f
11:
    la a0, s_import
    call fm_c
    mv a0, s0
    call import_text
    call fm_s
    li a0, 59
    call fm_b
    j 98f
12:
    la a0, s_fn
    call fm_c
    ld a0, N_A(s0)
    call fm_name
    ld t0, N_A(s0)              # [T, ...]
    beqz t0, 0f
    ld t0, N_B(t0)
    beqz t0, 0f
    ld a0, FX_TPARAMS(t0)
    call fm_tparams
0:
    li a0, '('
    call fm_b
    li s1, 0
121:
    ld t0, N_B(s0)
    ld t1, VEC_N(t0)
    bge s1, t1, 122f
    beqz s1, 123f
    la a0, s_comma
    call fm_c
123:
    ld a0, N_B(s0)
    mv a1, s1
    call vec_get
    sd a0, 0(sp)
    ld a0, N_A(a0)
    call fm_name
    la a0, s_colon
    call fm_c
    ld t0, 0(sp)
    ld a0, N_B(t0)
    call fm_type
    addi s1, s1, 1
    j 121b
122:
    li a0, ')'
    call fm_b
    ld t0, N_C(s0)
    beqz t0, 124f
    la a0, s_arrow
    call fm_c
    ld a0, N_C(s0)
    call fm_type
124:
    ld t0, N_A(s0)
    beqz t0, 125f
    ld t0, N_B(t0)
    beqz t0, 125f
    ld a0, FX_USES(t0)
    call fm_uses
125:
    li a0, ' '
    call fm_b
    ld a0, N_D(s0)
    li a1, 0
    call fm_block
    j 98f
13:
    la a0, s_record
    li s1, 1
    j 16f
14:
    la a0, s_variant
    li s1, 0
16:
    call fm_c
    ld a0, N_A(s0)
    call fm_name
    ld a0, N_C(s0)              # [T, ...]
    call fm_tparams
    la a0, s_open
    call fm_c
    ld a0, N_B(s0)
    mv a1, s1
    call fm_members
    la a0, s_close
    call fm_c
    j 98f
15:
    la a0, s_test
    call fm_c
    la t0, fm_sb
    ld a0, 0(t0)
    ld a1, N_A(s0)
    call render_str_lit
    ld a0, N_C(s0)
    call fm_uses
    li a0, ' '
    call fm_b
    ld a0, N_B(s0)
    li a1, 0
    call fm_block
98:
    li a0, 10
    call fm_b
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void *import_text(void *import)
# The dotted path of an import (a.b.name) as a str.
    .globl import_text
import_text:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    sd s2, 0(sp)
    mv s0, a0
    call sb_new
    mv s1, a0
    li s2, 0
1:
    ld t0, N_A(s0)
    ld t1, VEC_N(t0)
    bge s2, t1, 2f
    beqz s2, 3f
    mv a0, s1
    li a1, '.'
    call sb_byte
3:
    ld a0, N_A(s0)
    mv a1, s2
    call vec_get
    mv a1, a0
    mv a0, s1
    call sb_str
    addi s2, s2, 1
    j 1b
2:
    mv a0, s1
    call sb_finish
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    ld s2, 0(sp)
    addi sp, sp, 32
    ret

# void *fmt_file(int file)
# The canonical text of a parsed file: leading comments, sorted imports, then the other items
# in order; one blank line between items, none after a comment or between imports.
    .globl fmt_file
fmt_file:
    addi sp, sp, -80
    sd ra, 72(sp)
    sd s0, 64(sp)
    sd s1, 56(sp)
    sd s2, 48(sp)
    sd s3, 40(sp)
    sd s4, 32(sp)
    sd s5, 24(sp)
    call file_get
    ld t0, FILE_TEXT(a0)
    la t1, fm_text
    sd t0, 0(t1)
    ld t0, FILE_AST(a0)
    ld s0, N_A(t0)              # items
    call sb_new
    la t0, fm_sb
    sd a0, 0(t0)
    call vec_new
    mv s1, a0                   # output order
    li s2, 0                    # leading comments
1:
    ld t0, VEC_N(s0)
    bge s2, t0, 2f
    mv a0, s0
    mv a1, s2
    call vec_get
    lhu t0, N_KIND(a0)
    li t1, N_COMMENT
    bne t0, t1, 2f
    mv a1, a0
    mv a0, s1
    call vec_push
    addi s2, s2, 1
    j 1b
2:
    ld s3, VEC_N(s1)            # imports start here
    li s4, 0
3:
    ld t0, VEC_N(s0)
    bge s4, t0, 4f
    mv a0, s0
    mv a1, s4
    call vec_get
    addi s4, s4, 1
    lhu t0, N_KIND(a0)
    li t1, N_IMPORT
    bne t0, t1, 3b
    mv a1, a0
    mv a0, s1
    call vec_push
    j 3b
4:
    mv a0, s1                   # sort the imports by their dotted text
    mv a1, s3
    call sort_imports
5:
    bge s2, s4, 6f              # everything else, in order (s4 = item count)
    mv a0, s0
    mv a1, s2
    call vec_get
    addi s2, s2, 1
    lhu t0, N_KIND(a0)
    li t1, N_IMPORT
    beq t0, t1, 5b
    mv a1, a0
    mv a0, s1
    call vec_push
    j 5b
6:
    li s2, 0
    li s5, 0                    # kind of the previous element (0 = none)
7:
    ld t0, VEC_N(s1)
    bge s2, t0, 9f
    mv a0, s1
    mv a1, s2
    call vec_get
    mv s3, a0
    addi s2, s2, 1
    lhu s4, N_KIND(s3)
    beqz s5, 8f                 # blank line rules
    li t0, N_COMMENT
    beq s5, t0, 8f
    li t0, N_IMPORT
    bne s5, t0, 71f
    beq s4, t0, 8f
71:
    li a0, 10
    call fm_b
8:
    mv a0, s3
    call fm_item
    mv s5, s4
    j 7b
9:
    la t0, fm_sb
    ld a0, 0(t0)
    call sb_finish
    ld ra, 72(sp)
    ld s0, 64(sp)
    ld s1, 56(sp)
    ld s2, 48(sp)
    ld s3, 40(sp)
    ld s4, 32(sp)
    ld s5, 24(sp)
    addi sp, sp, 80
    ret

# void sort_imports(void *vec, long from)
# Insertion sort of vec[from..] (import nodes) by their dotted text.
    .globl sort_imports
sort_imports:
    addi sp, sp, -64
    sd ra, 56(sp)
    sd s0, 48(sp)
    sd s1, 40(sp)
    sd s2, 32(sp)
    sd s3, 24(sp)
    sd s4, 16(sp)
    mv s0, a0
    mv s1, a1
    addi s2, s1, 1
1:
    ld t0, VEC_N(s0)
    bge s2, t0, 9f
    ld t0, VEC_DATA(s0)
    slli t1, s2, 3
    add t0, t0, t1
    ld s3, 0(t0)                # element to insert
    mv a0, s3
    call import_text
    mv s4, a0
    addi t0, s2, -1
    sd t0, 0(sp)
2:
    ld t0, 0(sp)
    blt t0, s1, 3f
    ld t1, VEC_DATA(s0)
    slli t2, t0, 3
    add t1, t1, t2
    ld a0, 0(t1)
    call import_text
    mv a1, s4
    call str_cmp
    blez a0, 3f
    ld t0, 0(sp)
    ld t1, VEC_DATA(s0)
    slli t2, t0, 3
    add t1, t1, t2
    ld t3, 0(t1)
    sd t3, 8(t1)
    addi t0, t0, -1
    sd t0, 0(sp)
    j 2b
3:
    ld t0, 0(sp)
    addi t0, t0, 1
    ld t1, VEC_DATA(s0)
    slli t2, t0, 3
    add t1, t1, t2
    sd s3, 0(t1)
    addi s2, s2, 1
    j 1b
9:
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    addi sp, sp, 64
    ret
