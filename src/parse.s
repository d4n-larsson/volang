# Recursive-descent parser: tokens -> AST (contracts/grammar.md).
# Every statement kind is chosen from its first token. After an error, p_err is set and every
# routine returns early; the block and item loops stop (US2 adds recovery).
    .include "defs.inc"

    .section .bss
    .align 3
p_toks:     .space 8
p_pos:      .space 8
p_text:     .space 8
    .globl p_err
p_err:      .space 8                # an error in the current statement: unwind to recovery
    .globl p_hard
p_hard:     .space 8
p_methods:  .space 8                # N_CALL_VALUE nodes in method form (their E0110 is pending)
p_depth:    .space 8                # current expression/block nesting                # a syntax error left the tree incomplete: skip checking

    .section .rodata
    .align 3
tok_spell:
    .dword w_eof, w_name, w_tname, w_number, w_number, w_string, w_comment, w_under, w_invalid, w_q
    .dword k_fn, k_record, k_variant, k_test, k_import, k_let, k_var, k_set, k_if, k_else
    .dword k_while, k_for, k_in, k_match, k_case, k_return, k_break, k_continue, k_try, k_new
    .dword k_and, k_or, k_not, k_true, k_false, k_int, k_float, k_bool, k_str, k_list
    .dword k_map, k_result, k_void, w_q, w_q, w_q, w_q, w_q, w_q, w_q
    .dword p_lparen, p_rparen, p_lbrace, p_rbrace, p_lbrack, p_rbrack, p_comma, p_semi, p_colon, p_dot
    .dword p_dotdot, p_arrow, p_assign, p_eq, p_ne, p_lt, p_le, p_gt, p_ge, p_plus
    .dword p_minus, p_star, p_slash, p_percent
tok_display_class:
    .dword d_eof, d_name, d_tname, d_number, d_number, d_string, d_comment, d_under, d_invalid
w_eof: .string "end of file"
w_name: .string "name"
w_tname: .string "type name"
w_number: .string "number"
w_string: .string "string"
w_comment: .string "comment"
w_under: .string "_"
w_invalid: .string "invalid token"
w_q: .string "?"
s_empty: .string ""
d_eof: .string "the end of the file"
d_name: .string "a name"
d_tname: .string "a type name"
d_number: .string "a number"
d_string: .string "a string"
d_comment: .string "a comment"
d_under: .string "`_`"
d_invalid: .string "an invalid token"
k_fn: .string "fn"
k_record: .string "record"
k_variant: .string "variant"
k_test: .string "test"
k_import: .string "import"
k_let: .string "let"
k_var: .string "var"
k_set: .string "set"
k_if: .string "if"
k_else: .string "else"
k_while: .string "while"
k_for: .string "for"
k_in: .string "in"
k_match: .string "match"
k_case: .string "case"
k_return: .string "return"
k_break: .string "break"
k_continue: .string "continue"
k_try: .string "try"
k_new: .string "new"
k_and: .string "and"
k_or: .string "or"
k_not: .string "not"
k_true: .string "true"
k_false: .string "false"
k_int: .string "int"
k_float: .string "float"
k_bool: .string "bool"
k_str: .string "str"
k_list: .string "list"
k_map: .string "map"
k_result: .string "result"
k_void: .string "void"
p_lparen: .string "("
p_rparen: .string ")"
p_lbrace: .string "{"
p_rbrace: .string "}"
p_lbrack: .string "["
p_rbrack: .string "]"
p_comma: .string ","
p_semi: .string ";"
p_colon: .string ":"
p_dot: .string "."
p_dotdot: .string ".."
p_arrow: .string "->"
p_assign: .string "="
p_eq: .string "=="
p_ne: .string "!="
p_lt: .string "<"
p_le: .string "<="
p_gt: .string ">"
p_ge: .string ">="
p_plus: .string "+"
p_minus: .string "-"
p_star: .string "*"
p_slash: .string "/"
p_percent: .string "%"
e0110: .string "E0110"
e0114: .string "E0114"
e0115: .string "E0115"
m_expected: .string "Expected $1, found $2."
m_chain: .string "Comparisons cannot be chained; combine them with `and`."
m_stmt_call: .string "A statement must start with a keyword or be a call."
m_deep: .string "This code is nested too deeply."
e0111: .string "E0111"
e0112: .string "E0112"
e0113: .string "E0113"
m_unclosed: .string "Expected `}` before the end of the file."
m_opened: .string "This block is never closed."
m_missing_semi: .string "Missing `;` at the end of this statement."
m_trailing_comma: .string "A list cannot end with a comma."
m_assign_set: .string "Assignments start with `set`: write `set $1 = ...;`."
m_compound: .string "vo has no compound assignment: write `set $1 = $1 $2 ...;`."
m_method: .string "vo has no methods: write `$1`."
m_elif: .string "vo writes `else if`, not `elif`."
m_new: .string "Records are built with `new`: `new $1 { ... }`."
m_map_literal: .string "vo has no map literals: create a map with `new map[str, int]`, then `set m[key] = value;`."
m_member_comma: .string "Fields and cases end with `;`, not `,`."
m_init: .string "A variable needs a starting value, like `var n: int = 0;`."
m_void_arrow: .string "A function that returns nothing has no `->`: remove `-> void`."
m_compound_stmt: .string "Assignments start with `set`, and vo has no compound assignment: write `set $1 = $1 $2 ...;`."
w_elif: .string "elif"
s_else_if: .string "else if"
s_new_sp: .string "new "
s_comma_sep: .string ", "
d_int: .string " = 0"
d_float: .string " = 0.0"
d_bool: .string " = false"
d_str: .string " = \"\""
d_new: .string " = new "
s_semi: .string ";"
s_close_nl: .string "}\n"
s_set_kw: .string "set "
s_eq_sp: .string " = "
x_item: .string "`fn`, `record`, `variant`, `test` or `import`"
x_item_raw: .string "item"
x_type: .string "a type"
x_type_raw: .string "type"
x_expr: .string "an expression"
x_expr_raw: .string "expression"
x_stmt: .string "a statement"
x_stmt_raw: .string "statement"
x_name: .string "a name"
x_name_raw: .string "name"
s_anon: .string "anonymous function"
e0507: .string "E0507"
m_uses_lambda: .string "Anonymous functions do not declare effects: they get them from their body. Remove the `uses` clause."
s_ordered: .string "ordered"
s_effects: .string "effects"
x_constraint: .string "`ordered` or `effects`"
x_constraint_raw: .string "constraint"
m_fn_stmt: .string "Functions are declared at the top level: write `let f = fn(...) { ... };` for a local function."

    .text
# void *p_tok(void)
    .globl p_tok
p_tok:
    la t0, p_toks
    ld t0, 0(t0)
    la t1, p_pos
    ld t1, 0(t1)
    slli t1, t1, 4
    add a0, t0, t1
    ret

# int p_kind(void)
    .globl p_kind
p_kind:
    la t0, p_toks
    ld t0, 0(t0)
    la t1, p_pos
    ld t1, 0(t1)
    slli t1, t1, 4
    add t0, t0, t1
    lhu a0, TOK_KIND(t0)
    ret

# int p_kind_at(long ahead)
# Kind of the token `ahead` positions after the current one (EOF does not advance).
    .globl p_kind_at
p_kind_at:
    la t0, p_toks
    ld t0, 0(t0)
    la t1, p_pos
    ld t1, 0(t1)
    slli t1, t1, 4
    add t0, t0, t1
1:
    lhu t2, TOK_KIND(t0)
    beqz a0, 2f
    beqz t2, 2f
    addi t0, t0, TOK_SIZE
    addi a0, a0, -1
    j 1b
2:
    mv a0, t2
    ret

# long p_start(void)
    .globl p_start
p_start:
    la t0, p_toks
    ld t0, 0(t0)
    la t1, p_pos
    ld t1, 0(t1)
    slli t1, t1, 4
    add t0, t0, t1
    lwu a0, TOK_START(t0)
    ret

# long p_prev_end(void)
    .globl p_prev_end
p_prev_end:
    la t0, p_toks
    ld t0, 0(t0)
    la t1, p_pos
    ld t1, 0(t1)
    beqz t1, 1f
    addi t1, t1, -1
    slli t1, t1, 4
    add t0, t0, t1
    lwu a0, TOK_END(t0)
    ret
1:
    li a0, 0
    ret

# void *p_next(void)
# Consumes the current token (never moves past EOF) and returns it.
    .globl p_next
p_next:
    la t0, p_toks
    ld t0, 0(t0)
    la t1, p_pos
    ld t2, 0(t1)
    slli t3, t2, 4
    add a0, t0, t3
    lhu t4, TOK_KIND(a0)
    beqz t4, 1f
    addi t2, t2, 1
    sd t2, 0(t1)
1:
    ret

# int p_accept(int kind)
    .globl p_accept
p_accept:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    mv s0, a0
    call p_kind
    bne a0, s0, 1f
    call p_next
    li a0, 1
    j 2f
1:
    li a0, 0
2:
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret

# void *tok_display(int kind)
# How a token kind is named in messages: `;`, a name, the end of the file.
    .globl tok_display
tok_display:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    li t0, T_KW_FIRST
    bge a0, t0, 1f
    la t0, tok_display_class
    slli t1, a0, 3
    add t0, t0, t1
    ld a0, 0(t0)
    call str_from_cstr
    j 2f
1:
    la t0, tok_spell
    slli t1, a0, 3
    add t0, t0, t1
    ld s0, 0(t0)
    call sb_new
    sd a0, 0(sp)
    li a1, '`'
    call sb_byte
    ld a0, 0(sp)
    mv a1, s0
    call sb_cstr
    ld a0, 0(sp)
    li a1, '`'
    call sb_byte
    ld a0, 0(sp)
    call sb_finish
2:
    ld ra, 24(sp)
    ld s0, 16(sp)
    addi sp, sp, 32
    ret

# void *tok_raw(int kind)
    .globl tok_raw
tok_raw:
    la t0, tok_spell
    slli t1, a0, 3
    add t0, t0, t1
    ld a0, 0(t0)
    tail str_from_cstr

# void p_fail(void *expected_display, void *expected_raw)
# E0110 at the current token (only the first error of a statement is reported).
    .globl p_fail
p_fail:
    la t0, p_err
    ld t1, 0(t0)
    bnez t1, 9f
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    mv s1, a1
    call p_kind
    mv s2, a0
    call tok_display
    mv a2, a0
    mv a1, s0
    la a0, m_expected
    li a3, 0
    call msg
    mv s3, a0
    call p_tok
    lwu a3, TOK_START(a0)
    lwu a4, TOK_END(a0)
    mv a5, s3
    la a0, e0110
    li a1, SEV_ERROR
    la t0, cur_file
    ld a2, 0(t0)
    call diag_add
    sd a0, 0(sp)
    mv a0, s2
    call tok_raw
    mv a2, a0
    ld a0, 0(sp)
    mv a1, s1
    call diag_set_ef
    call p_mark_hard
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
9:
    ret

# void p_mark_hard(void)
# Records a syntax error that leaves the tree incomplete.
    .globl p_mark_hard
p_mark_hard:
    la t0, p_err
    li t1, 1
    sd t1, 0(t0)
    la t0, p_hard
    sd t1, 0(t0)
    ret

# void *p_soft_err(char *code, long start, long end, char *message)
# A syntax error the parser can continue past (the tree stays complete).
    .globl p_soft_err
p_soft_err:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    sd s2, 0(sp)
    mv s0, a0
    mv s1, a1
    mv s2, a2
    mv a0, a3
    call str_from_cstr
    mv a5, a0
    mv a0, s0
    li a1, SEV_ERROR
    la t0, cur_file
    ld a2, 0(t0)
    mv a3, s1
    mv a4, s2
    call diag_add
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    ld s2, 0(sp)
    addi sp, sp, 32
    ret

# void p_fix(void *diag, long start, long end, char *replacement)
    .globl p_fix
p_fix:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    sd s2, 0(sp)
    mv s0, a0
    mv s1, a1
    mv s2, a2
    mv a0, a3
    call str_from_cstr
    mv a4, a0
    mv a0, s0
    la t0, cur_file
    ld a1, 0(t0)
    mv a2, s1
    mv a3, s2
    call diag_add_fix
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    ld s2, 0(sp)
    addi sp, sp, 32
    ret

# void p_end_stmt(void)
# Consumes the `;` ending a statement; when it is missing, E0112 with an insertion fix (parsing goes on).
    .globl p_end_stmt
p_end_stmt:
    la t0, p_err
    ld t0, 0(t0)
    bnez t0, 9f
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    call p_kind
    li t0, T_SEMI
    bne a0, t0, 1f
    call p_next
    j 8f
1:
    call p_prev_end
    mv s0, a0
    la a0, e0112
    mv a1, s0
    mv a2, s0
    la a3, m_missing_semi
    call p_soft_err
    mv a1, s0
    mv a2, s0
    la a3, s_semi
    call p_fix
8:
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
9:
    ret

# int p_comma_more(int close)
# After a list element: 1 when a comma and another element follow. A comma right before the
# closing token is E0113 with a removal fix (parsing goes on).
    .globl p_comma_more
p_comma_more:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    li a0, T_COMMA
    call p_accept
    beqz a0, 8f
    call p_kind
    bne a0, s0, 7f
    call p_prev_end             # the comma just consumed
    mv s1, a0
    la a0, e0113
    addi a1, s1, -1
    mv a2, s1
    la a3, m_trailing_comma
    call p_soft_err
    addi a1, s1, -1
    mv a2, s1
    la a3, s_empty
    call p_fix
    j 8f
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

# int is_item_kw(int kind)
    .globl is_item_kw
is_item_kw:
    li t0, T_FN
    blt a0, t0, 1f
    li t0, T_IMPORT
    bgt a0, t0, 1f
    li a0, 1
    ret
1:
    li a0, 0
    ret

# int p_sync_stmt(void)
# After an error in a statement: skips to the next `;` (consumed) or the enclosing `}` and clears
# the error (returns 0); stops at an item keyword or the end of the file (returns 1, error kept).
    .globl p_sync_stmt
p_sync_stmt:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    li s0, 0                    # depth
1:
    call p_kind
    mv s1, a0
    beqz s1, 8f
    bnez s0, 2f
    mv a0, s1
    call is_item_kw
    bnez a0, 8f
2:
    li t0, T_LBRACE
    bne s1, t0, 3f
    addi s0, s0, 1
    j 6f
3:
    li t0, T_RBRACE
    bne s1, t0, 4f
    beqz s0, 7f
    addi s0, s0, -1
    j 6f
4:
    li t0, T_SEMI
    bne s1, t0, 6f
    bnez s0, 6f
    call p_next
    j 7f
6:
    call p_next
    j 1b
7:
    la t0, p_err
    sd zero, 0(t0)
    li a0, 0
    j 9f
8:
    li a0, 1
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void p_sync_item(void)
# After an error in an item: skips to the next item keyword outside braces (or the end) and clears it.
    .globl p_sync_item
p_sync_item:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    li s0, 0
1:
    call p_kind
    mv s1, a0
    beqz s1, 8f
    bgtz s0, 2f
    mv a0, s1
    call is_item_kw
    bnez a0, 8f
2:
    li t0, T_LBRACE
    bne s1, t0, 3f
    addi s0, s0, 1
3:
    li t0, T_RBRACE
    bne s1, t0, 4f
    addi s0, s0, -1
4:
    call p_next
    j 1b
8:
    la t0, p_err
    sd zero, 0(t0)
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void p_fail_c(char *expected_display, char *expected_raw)
    .globl p_fail_c
p_fail_c:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    mv s0, a1
    call str_from_cstr
    sd a0, 0(sp)                # the display str; s0 still holds the raw cstr
    mv a0, s0
    call str_from_cstr
    mv a1, a0
    ld a0, 0(sp)
    call p_fail
    ld ra, 24(sp)
    ld s0, 16(sp)
    addi sp, sp, 32
    ret

# void *p_expect(int kind)
# Consumes a token of the kind, or reports E0110 and returns 0.
    .globl p_expect
p_expect:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    mv s0, a0
    la t0, p_err
    ld t0, 0(t0)
    bnez t0, 8f
    call p_kind
    bne a0, s0, 1f
    call p_next
    j 9f
1:
    mv a0, s0
    call tok_display
    sd a0, 0(sp)                # display (s0 no longer needed after raw)
    mv a0, s0
    call tok_raw
    mv a1, a0
    ld a0, 0(sp)
    call p_fail
    mv a0, s0                   # exactly one token fits: insert it only for ) or ] before a token
    call p_insert_ok            # that can follow them (otherwise the fix would not help)
    beqz a0, 8f
    call p_last_diag
    beqz a0, 8f
    sd a0, 0(sp)
    call p_prev_end
    sd a0, 8(sp)
    mv a0, s0
    call tok_raw
    mv a4, a0
    ld a0, 0(sp)
    la t0, cur_file
    ld a1, 0(t0)
    ld a2, 8(sp)
    mv a3, a2
    call diag_add_fix
8:
    li a0, 0
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    addi sp, sp, 32
    ret

# int p_deep(void)
# 1 (after reporting E0115 once) when the native stack is nearly exhausted.
    .globl p_deep
p_deep:
    la t0, stack_floor
    ld t0, 0(t0)
    li t1, 0x40000              # keep 256 KiB in reserve for the checker and output
    add t0, t0, t1
    bgeu sp, t0, 8f
    la t0, p_err
    ld t1, 0(t0)
    bnez t1, 7f
    addi sp, sp, -16
    sd ra, 8(sp)
    la a0, m_deep
    call str_from_cstr
    mv a5, a0
    call p_tok
    lwu a3, TOK_START(a0)
    lwu a4, TOK_END(a0)
    la a0, e0115
    li a1, SEV_ERROR
    la t0, cur_file
    ld a2, 0(t0)
    call diag_add
    call p_mark_hard
    ld ra, 8(sp)
    addi sp, sp, 16
7:
    li a0, 1
    ret
8:
    li a0, 0
    ret

# int p_enter(void)
# Enters one nesting level (expressions and blocks); 1 (after E0115) beyond 5,000 levels or when
# the native stack runs low. Every call is paired with p_leave.
    .globl p_enter
p_enter:
    la t0, p_depth
    ld t1, 0(t0)
    addi t1, t1, 1
    sd t1, 0(t0)
    li t2, 5000
    bgt t1, t2, 1f
    tail p_deep
1:
    la t0, stack_floor          # force p_deep to report: pretend the stack is exhausted
    ld t1, 0(t0)
    addi sp, sp, -16
    sd ra, 8(sp)
    sd t1, 0(sp)
    li t2, -1
    srli t2, t2, 1
    sd t2, 0(t0)
    call p_deep
    ld t1, 0(sp)
    la t0, stack_floor
    sd t1, 0(t0)
    ld ra, 8(sp)
    addi sp, sp, 16
    ret

# void p_leave(void)
    .globl p_leave
p_leave:
    la t0, p_depth
    ld t1, 0(t0)
    addi t1, t1, -1
    sd t1, 0(t0)
    ret

# int p_failed(void)
    .globl p_failed
p_failed:
    la t0, p_err
    ld a0, 0(t0)
    ret

# void *p_finish(void *node, long start)
# Sets the node's span from start to the end of the previous token.
    .globl p_finish
p_finish:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    mv s0, a0
    sw a1, N_START(s0)
    call p_prev_end
    sw a0, N_END(s0)
    mv a0, s0
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret

# void *p_node(int kind)
# A node starting at the current token.
    .globl p_node
p_node:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    mv s0, a0
    call p_tok
    lwu a1, TOK_START(a0)
    lwu a2, TOK_END(a0)
    mv a0, s0
    call node_new
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret

# void *p_tok_str(void *tok)
# The token's source text as a str.
    .globl p_tok_str
p_tok_str:
    lwu t0, TOK_START(a0)
    lwu t1, TOK_END(a0)
    la t2, p_text
    ld t2, 0(t2)
    add a0, t2, t0
    sub a1, t1, t0
    tail str_new

# void *p_name(void)
# A name (identifier or type name) as an N_NAME node, or 0 after E0110.
    .globl p_name
p_name:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    call p_kind
    li t0, T_IDENT
    beq a0, t0, 1f
    li t0, T_TYPE_IDENT
    beq a0, t0, 1f
    la a0, x_name
    la a1, x_name_raw
    call p_fail_c
    li a0, 0
    j 9f
1:
    li a0, N_NAME
    call p_node
    mv s0, a0
    call p_next
    call p_tok_str
    sd a0, N_A(s0)
    mv a0, s0
9:
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret

# void *decode_string(long start, long end)
# The value of a string literal token (quotes removed, escapes decoded).
    .globl decode_string
decode_string:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    la t0, p_text
    ld t0, 0(t0)
    add s1, t0, a0
    addi s1, s1, 1              # skip the opening quote
    add s2, t0, a1
    addi t1, s2, -1             # drop the closing quote (the same character as the opening one)
    bltu t1, s1, 1f
    lbu t2, 0(t1)
    lbu t3, -1(s1)
    bne t2, t3, 1f
    mv s2, t1
1:
    call sb_new
    mv s0, a0
2:
    bgeu s1, s2, 9f
    lbu s3, 0(s1)
    addi s1, s1, 1
    li t0, '\\'
    bne s3, t0, 5f
    bgeu s1, s2, 5f
    lbu s3, 0(s1)
    addi s1, s1, 1
    li t0, 'n'
    bne s3, t0, 3f
    li s3, 10
    j 5f
3:
    li t0, 't'
    bne s3, t0, 5f
    li s3, 9
5:
    mv a0, s0
    mv a1, s3
    call sb_byte
    j 2b
9:
    mv a0, s0
    call sb_finish
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# void *parse_type(void)
    .globl parse_type
parse_type:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    sd s2, 0(sp)
    call p_start
    mv s1, a0
    li a0, N_TYPE_NODE
    call p_node
    mv s0, a0
    call p_kind
    mv s2, a0
    li t0, T_KINT
    li t1, TY_INT
    beq s2, t0, 1f
    li t0, T_KFLOAT
    li t1, TY_FLOAT
    beq s2, t0, 1f
    li t0, T_KBOOL
    li t1, TY_BOOL
    beq s2, t0, 1f
    li t0, T_KSTR
    li t1, TY_STR
    beq s2, t0, 1f
    li t0, T_TYPE_IDENT
    beq s2, t0, 2f
    li t0, T_KLIST
    beq s2, t0, 3f
    li t0, T_KMAP
    beq s2, t0, 4f
    li t0, T_KRESULT
    beq s2, t0, 5f
    li t0, T_FN
    beq s2, t0, 60f
    la a0, x_type
    la a1, x_type_raw
    call p_fail_c
    j 9f
60:                             # fn(types) -> type uses ...
    li t0, TY_FUNC
    sd t0, N_A(s0)
    call p_next
    call vec_new
    sd a0, N_B(s0)
    li a0, T_LPAREN
    call p_expect
    li a0, T_RPAREN
    call p_accept
    bnez a0, 62f
61:
    call p_failed
    bnez a0, 9f
    call parse_type
    mv a1, a0
    ld a0, N_B(s0)
    call vec_push
    li a0, T_COMMA
    call p_accept
    bnez a0, 61b
    li a0, T_RPAREN
    call p_expect
62:
    li a0, T_ARROW
    call p_accept
    beqz a0, 63f
    call parse_type
    sd a0, N_C(s0)
63:
    call parse_uses
    sd a0, N_D(s0)
    j 8f
1:
    sd t1, N_A(s0)
    call p_next
    j 8f
2:
    li t0, TY_NAMED
    sd t0, N_A(s0)
    call p_name
    sd a0, N_B(s0)
    call parse_type_args
    sd a0, N_C(s0)
    j 8f
3:
    li t0, TY_LIST
    sd t0, N_A(s0)
    call p_next
    li a0, T_LBRACKET
    call p_expect
    call parse_type
    sd a0, N_B(s0)
    li a0, T_RBRACKET
    call p_expect
    j 8f
4:
    li t0, TY_MAP
    sd t0, N_A(s0)
    call p_next
    li a0, T_LBRACKET
    call p_expect
    call parse_type
    sd a0, N_B(s0)
    li a0, T_COMMA
    call p_expect
    call parse_type
    sd a0, N_C(s0)
    li a0, T_RBRACKET
    call p_expect
    j 8f
5:
    li t0, TY_RESULT
    sd t0, N_A(s0)
    call p_next
    li a0, T_LBRACKET
    call p_expect
    call p_kind
    li t0, T_KVOID
    bne a0, t0, 6f
    li a0, N_TYPE_NODE
    call p_node
    li t0, TY_VOID
    sd t0, N_A(a0)
    sd a0, N_B(s0)
    call p_next
    j 7f
6:
    call parse_type
    sd a0, N_B(s0)
7:
    li a0, T_RBRACKET
    call p_expect
8:
    mv a0, s0
    mv a1, s1
    call p_finish
9:
    mv a0, s0
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    ld s2, 0(sp)
    addi sp, sp, 32
    ret

# void *parse_args(void)
# "(" [expr {"," expr}] ")" -> vec of expressions.
    .globl parse_args
parse_args:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    call vec_new
    mv s0, a0
    li a0, T_LPAREN
    call p_expect
    li a0, T_RPAREN
    call p_accept
    bnez a0, 9f
1:
    call parse_expr
    mv a1, a0
    mv a0, s0
    call vec_push
    call p_failed
    bnez a0, 9f
    li a0, T_RPAREN
    call p_comma_more
    bnez a0, 1b
    li a0, T_RPAREN
    call p_expect
9:
    mv a0, s0
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret

# void *parse_primary(void)
    .globl parse_primary
parse_primary:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    call p_start
    mv s1, a0
    call p_kind
    mv s2, a0
    li t0, T_INT
    beq s2, t0, 10f
    li t0, T_FLOAT
    beq s2, t0, 11f
    li t0, T_STRING
    beq s2, t0, 12f
    li t0, T_TRUE
    beq s2, t0, 13f
    li t0, T_FALSE
    beq s2, t0, 13f
    li t0, T_IDENT
    beq s2, t0, 14f
    li t0, T_KSTR
    beq s2, t0, 15f
    li t0, T_KINT
    beq s2, t0, 15f
    li t0, T_KFLOAT
    beq s2, t0, 15f
    li t0, T_KMAP               # `map(xs, f)`: the builtin (`map[K, V]` is a type)
    bne s2, t0, 14001f
    li a0, 1
    call p_kind_at
    li t0, T_LPAREN
    beq a0, t0, 15f
14001:
    li t0, T_TYPE_IDENT
    beq s2, t0, 16f
    li t0, T_LPAREN
    beq s2, t0, 17f
    li t0, T_LBRACKET
    beq s2, t0, 18f
    li t0, T_NEW
    beq s2, t0, 19f
    li t0, T_FN
    bne s2, t0, 190f
    call parse_lambda
    mv s0, a0
    j 99f
190:
    li t0, T_LBRACE
    bne s2, t0, 191f
    la a0, m_map_literal        # `{"a": 1}`: there are no map literals
    call p_hard_msg
    j 192f
191:
    la a0, x_expr
    la a1, x_expr_raw
    call p_fail_c
192:
    li a0, N_ERROR
    call p_node
    mv s0, a0
    j 99f
10:                             # int literal (range errors were reported by the lexer)
    li a0, N_INT
    call p_node
    mv s0, a0
    call p_next
    lwu t0, TOK_START(a0)
    lwu t1, TOK_END(a0)
    la t2, p_text
    ld t2, 0(t2)
    add a0, t2, t0
    sub a1, t1, t0
    addi a2, s0, N_A
    call dec_to_int
    j 98f
11:
    li a0, N_FLOAT
    call p_node
    mv s0, a0
    call p_next
    lwu t0, TOK_START(a0)
    lwu t1, TOK_END(a0)
    la t2, p_text
    ld t2, 0(t2)
    add a0, t2, t0
    sub a1, t1, t0
    call text_to_float
    sd a0, N_A(s0)
    j 98f
12:
    li a0, N_STR
    call p_node
    mv s0, a0
    call p_next
    lwu a1, TOK_END(a0)
    lwu a0, TOK_START(a0)
    call decode_string
    sd a0, N_A(s0)
    j 98f
13:
    li a0, N_BOOL
    call p_node
    mv s0, a0
    li t0, T_TRUE
    sub t1, s2, t0
    seqz t1, t1
    sd t1, N_A(s0)
    call p_next
    j 98f
14:                             # name or call
    li a0, 1
    call p_kind_at
    li t0, T_LPAREN
    beq a0, t0, 15f
    li a0, N_IDENT
    call p_node
    mv s0, a0
    call p_next
    call p_tok_str
    sd a0, N_A(s0)
    j 98f
15:                             # call (conversion keywords str/int/float are calls too)
    li a0, N_CALL
    call p_node
    mv s0, a0
    li a0, N_NAME
    call p_node
    mv s3, a0
    call p_next
    call p_tok_str
    sd a0, N_A(s3)
    sd s3, N_A(s0)
    li t0, T_LPAREN
    call p_kind
    li t0, T_LPAREN
    beq a0, t0, 151f
    li a0, T_LPAREN             # a bare `str`/`int`/`float`: report the missing "("
    call p_expect
    j 98f
151:
    call parse_args
    sd a0, N_B(s0)
    j 98f
16:                             # Type.case or Type.case(expr)
    li a0, 1
    call p_kind_at
    li t0, T_LBRACE
    bne a0, t0, 160f
    call p_missing_new          # `Point { ... }`: records are built with `new`
    li a0, N_NEW_RECORD
    call p_node
    mv s0, a0
    j 193f
160:
    li a0, N_VCASE
    call p_node
    mv s0, a0
    call p_name
    sd a0, N_A(s0)
    li a0, T_DOT
    call p_expect
    call p_name
    sd a0, N_B(s0)
    call p_failed
    bnez a0, 98f
    li a0, T_LPAREN
    call p_accept
    beqz a0, 98f
    call parse_expr
    sd a0, N_C(s0)
    li a0, T_RPAREN
    call p_expect
    j 98f
17:                             # ( expr )
    call p_next
    call parse_expr
    mv s0, a0
    li a0, T_RPAREN
    call p_expect
    j 99f
18:                             # [a, b, ...]
    li a0, N_LIST
    call p_node
    mv s0, a0
    call vec_new
    sd a0, N_A(s0)
    call p_next
    li a0, T_RBRACKET
    call p_accept
    bnez a0, 98f
181:
    call parse_expr
    mv a1, a0
    ld a0, N_A(s0)
    call vec_push
    call p_failed
    bnez a0, 98f
    li a0, T_RBRACKET
    call p_comma_more
    bnez a0, 181b
    li a0, T_RBRACKET
    call p_expect
    j 98f
19:                             # new ...
    call p_next
    call p_kind
    li t0, T_KLIST
    beq a0, t0, 191f
    li t0, T_KMAP
    beq a0, t0, 192f
    li a0, N_NEW_RECORD
    call p_node
    mv s0, a0
    call p_kind
    li t0, T_TYPE_IDENT
    beq a0, t0, 193f
    li a0, T_TYPE_IDENT
    call p_expect
    j 98f
193:
    call p_name
    sd a0, N_A(s0)
    call parse_type_args
    sd a0, N_D(s0)
    call vec_new
    sd a0, N_B(s0)
    li a0, T_LBRACE
    call p_expect
    call p_failed
    bnez a0, 98f
194:
    call p_start
    mv s3, a0
    li a0, N_FINIT
    call p_node
    sd a0, 0(sp)
    call p_name
    ld t0, 0(sp)
    sd a0, N_A(t0)
    li a0, T_COLON
    call p_expect
    call parse_expr
    ld t0, 0(sp)
    sd a0, N_B(t0)
    mv a0, t0
    mv a1, s3
    call p_finish
    mv a1, a0
    ld a0, N_B(s0)
    call vec_push
    call p_failed
    bnez a0, 98f
    li a0, T_RBRACE
    call p_comma_more
    bnez a0, 194b
    li a0, T_RBRACE
    call p_expect
    j 98f
191:
    li a0, N_NEW_LIST
    call p_node
    mv s0, a0
    call p_next
    li a0, T_LBRACKET
    call p_expect
    call parse_type
    sd a0, N_A(s0)
    li a0, T_RBRACKET
    call p_expect
    j 98f
192:
    li a0, N_NEW_MAP
    call p_node
    mv s0, a0
    call p_next
    li a0, T_LBRACKET
    call p_expect
    call parse_type
    sd a0, N_A(s0)
    li a0, T_COMMA
    call p_expect
    call parse_type
    sd a0, N_B(s0)
    li a0, T_RBRACKET
    call p_expect
98:
    mv a0, s0
    mv a1, s1
    call p_finish
99:
    mv a0, s0
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# void *parse_postfix(void)
    .globl parse_postfix
parse_postfix:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    sd s2, 0(sp)
    call p_start
    mv s1, a0
    call parse_primary
    mv s0, a0
1:
    call p_failed
    bnez a0, 9f
    call p_kind
    li t0, T_DOT
    beq a0, t0, 2f
    li t0, T_LBRACKET
    beq a0, t0, 3f
    li t0, T_LPAREN
    beq a0, t0, 4f
    j 9f
4:                              # a call of whatever the expression evaluates to
    li a0, N_CALL_VALUE
    li a1, 0
    li a2, 0
    call node_new
    mv s2, a0
    sd s0, N_A(s2)
    call parse_args
    sd a0, N_B(s2)
    mv a0, s2
    mv a1, s1
    call p_finish
    mv s0, a0
    j 1b
2:
    call p_next
    li a0, N_FIELD
    li a1, 0
    li a2, 0
    call node_new
    mv s2, a0
    sd s0, N_A(s2)
    call p_name
    sd a0, N_B(s2)
    beqz a0, 21f                # no name after `.` (already reported)
    call p_kind                 # `x.f(...)`: a field call, or `xs.len()` (no methods): the checker decides
    li t0, T_LPAREN
    bne a0, t0, 21f
    mv a0, s2
    mv a1, s1
    call p_finish
    mv a0, s0
    ld a1, N_B(s2)
    mv a2, s1
    mv a3, s2
    call p_method_call
    mv s0, a0
    j 1b
21:
    mv a0, s2
    mv a1, s1
    call p_finish
    mv s0, a0
    j 1b
3:
    call p_next
    li a0, N_INDEX
    li a1, 0
    li a2, 0
    call node_new
    mv s2, a0
    sd s0, N_A(s2)
    call parse_expr
    sd a0, N_B(s2)
    li a0, T_RBRACKET
    call p_expect
    mv a0, s2
    mv a1, s1
    call p_finish
    mv s0, a0
    j 1b
9:
    mv a0, s0
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    ld s2, 0(sp)
    addi sp, sp, 32
    ret

# void *parse_unary(void)
    .globl parse_unary
parse_unary:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    sd s2, 0(sp)
    call p_enter
    bnez a0, 8f
    call p_start
    mv s1, a0
    call p_kind
    mv s2, a0
    li t0, T_MINUS
    beq s2, t0, 1f
    li t0, T_TRY
    beq s2, t0, 2f
    call parse_postfix
    j 9f
1:
    li a0, N_UNARY
    call p_node
    mv s0, a0
    sd s2, N_C(s0)
    call p_next
    call parse_unary
    sd a0, N_A(s0)
    j 3f
2:
    li a0, N_TRY
    call p_node
    mv s0, a0
    call p_next
    call parse_unary
    sd a0, N_A(s0)
3:
    mv a0, s0
    mv a1, s1
    call p_finish
    j 9f
8:
    li a0, N_ERROR
    call p_node
9:
    mv s0, a0
    call p_leave
    mv a0, s0
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    ld s2, 0(sp)
    addi sp, sp, 32
    ret

# void *p_binary(void *left, int op, long start, void *right)
    .globl p_binary
p_binary:
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
    li a0, N_BINARY
    li a1, 0
    li a2, 0
    call node_new
    sd s0, N_A(a0)
    sd s3, N_B(a0)
    sd s1, N_C(a0)
    mv a1, s2
    call p_finish
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# void *parse_prod(void)
    .globl parse_prod
parse_prod:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    sd s2, 0(sp)
    call p_start
    mv s1, a0
    call parse_unary
    mv s0, a0
1:
    call p_failed
    bnez a0, 9f
    call p_kind
    mv s2, a0
    li t0, T_STAR
    beq s2, t0, 2f
    li t0, T_SLASH
    beq s2, t0, 2f
    li t0, T_PERCENT
    beq s2, t0, 2f
    j 9f
2:
    call p_next
    call parse_unary
    mv a3, a0
    mv a0, s0
    mv a1, s2
    mv a2, s1
    call p_binary
    mv s0, a0
    j 1b
9:
    mv a0, s0
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    ld s2, 0(sp)
    addi sp, sp, 32
    ret

# void *parse_sum(void)
    .globl parse_sum
parse_sum:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    sd s2, 0(sp)
    call p_start
    mv s1, a0
    call parse_prod
    mv s0, a0
1:
    call p_failed
    bnez a0, 9f
    call p_kind
    mv s2, a0
    li t0, T_PLUS
    beq s2, t0, 2f
    li t0, T_MINUS
    beq s2, t0, 2f
    j 9f
2:
    call p_next
    call parse_prod
    mv a3, a0
    mv a0, s0
    mv a1, s2
    mv a2, s1
    call p_binary
    mv s0, a0
    j 1b
9:
    mv a0, s0
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    ld s2, 0(sp)
    addi sp, sp, 32
    ret

# int is_cmp(int kind)
    .globl is_cmp
is_cmp:
    li t0, T_EQ
    blt a0, t0, 1f
    li t0, T_GE
    bgt a0, t0, 1f
    li a0, 1
    ret
1:
    li a0, 0
    ret

# void *parse_cmp(void)
    .globl parse_cmp
parse_cmp:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    call p_start
    mv s1, a0
    call parse_sum
    mv s0, a0
    call p_failed
    bnez a0, 9f
    call p_kind
    mv s2, a0
    call is_cmp
    beqz a0, 9f
    call p_next
    call parse_sum
    mv a3, a0
    mv a0, s0
    mv a1, s2
    mv a2, s1
    call p_binary
    mv s0, a0
    call p_failed
    bnez a0, 9f
    call p_kind
    call is_cmp
    beqz a0, 9f
    la a0, m_chain              # a < b < c
    call str_from_cstr
    mv a5, a0
    call p_tok
    lwu a3, TOK_START(a0)
    lwu a4, TOK_END(a0)
    la a0, e0110
    li a1, SEV_ERROR
    la t0, cur_file
    ld a2, 0(t0)
    call diag_add
    sd a0, 0(sp)
    la a0, k_and
    call str_from_cstr
    mv s2, a0
    call p_kind
    call tok_raw
    mv a2, a0
    mv a1, s2
    ld a0, 0(sp)
    call diag_set_ef
    call p_mark_hard
9:
    mv a0, s0
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    addi sp, sp, 48
    ret

# void *parse_not(void)
    .globl parse_not
parse_not:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    call p_deep
    bnez a0, 8f
    call p_kind
    li t0, T_NOT
    beq a0, t0, 1f
    call parse_cmp
    j 9f
1:
    call p_start
    mv s1, a0
    li a0, N_UNARY
    call p_node
    mv s0, a0
    li t0, T_NOT
    sd t0, N_C(s0)
    call p_next
    call parse_not
    sd a0, N_A(s0)
    mv a0, s0
    mv a1, s1
    call p_finish
    j 9f
8:
    li a0, N_ERROR
    call p_node
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void *parse_and(void)
    .globl parse_and
parse_and:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    call p_start
    mv s1, a0
    call parse_not
    mv s0, a0
1:
    call p_failed
    bnez a0, 9f
    li a0, T_AND
    call p_accept
    beqz a0, 9f
    call parse_not
    mv a3, a0
    mv a0, s0
    li a1, T_AND
    mv a2, s1
    call p_binary
    mv s0, a0
    j 1b
9:
    mv a0, s0
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void *parse_expr(void)
    .globl parse_expr
parse_expr:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    call p_start
    mv s1, a0
    call parse_and
    mv s0, a0
1:
    call p_failed
    bnez a0, 9f
    li a0, T_OR
    call p_accept
    beqz a0, 9f
    call parse_and
    mv a3, a0
    mv a0, s0
    li a1, T_OR
    mv a2, s1
    call p_binary
    mv s0, a0
    j 1b
9:
    mv a0, s0
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void *parse_block(void)
    .globl parse_block
parse_block:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    call p_start
    mv s1, a0
    li a0, N_BLOCK
    call p_node
    mv s0, a0
    call vec_new
    sd a0, N_A(s0)
    li a0, T_LBRACE
    call p_expect
    call p_enter
    bnez a0, 8f
1:
    call p_failed
    bnez a0, 9f
    call p_kind
    li t0, T_RBRACE
    beq a0, t0, 2f
    beqz a0, 2f
    call parse_stmt
    mv a1, a0
    ld a0, N_A(s0)
    call vec_push
    call p_failed
    beqz a0, 1b
    call p_sync_stmt
    bnez a0, 9f
    j 1b
2:
    call p_failed
    bnez a0, 3f
    call p_kind
    bnez a0, 3f
    mv a0, s1                   # the end of the file inside this block: E0111
    call p_unclosed
    j 9f
3:
    li a0, T_RBRACE
    call p_expect
    mv a0, s0
    mv a1, s1
    call p_finish
9:
8:
    call p_leave                # paired with p_enter on every path
    mv a0, s0
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void *parse_if(void)
    .globl parse_if
parse_if:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    call p_start
    mv s1, a0
    li a0, N_IF
    call p_node
    mv s0, a0
    call p_next
    call parse_expr
    sd a0, N_A(s0)
    call parse_block
    sd a0, N_B(s0)
    call p_elif                 # `elif`: vo writes `else if`
    beqz a0, 0f
    call parse_if
    sd a0, N_C(s0)
    j 8f
0:
    li a0, T_ELSE
    call p_accept
    beqz a0, 8f
    call p_kind
    li t0, T_IF
    bne a0, t0, 1f
    call parse_if
    sd a0, N_C(s0)
    j 8f
1:
    call parse_block
    sd a0, N_C(s0)
8:
    mv a0, s0
    mv a1, s1
    call p_finish
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void *parse_stmt(void)
    .globl parse_stmt
parse_stmt:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    call p_start
    mv s1, a0
    call p_kind
    mv s2, a0
    li t0, T_COMMENT
    beq s2, t0, 10f
    li t0, T_LET
    beq s2, t0, 11f
    li t0, T_VAR
    beq s2, t0, 11f
    li t0, T_SET
    beq s2, t0, 12f
    li t0, T_IF
    beq s2, t0, 13f
    li t0, T_WHILE
    beq s2, t0, 14f
    li t0, T_FOR
    beq s2, t0, 15f
    li t0, T_MATCH
    beq s2, t0, 16f
    li t0, T_RETURN
    beq s2, t0, 17f
    li t0, T_BREAK
    li t1, N_BREAK
    beq s2, t0, 18f
    li t0, T_CONTINUE
    li t1, N_CONTINUE
    beq s2, t0, 18f
    li t0, T_TRY
    beq s2, t0, 19f
    li t0, T_IDENT
    beq s2, t0, 20f
    li t0, T_KSTR
    beq s2, t0, 20f
    li t0, T_KINT
    beq s2, t0, 20f
    li t0, T_KFLOAT
    beq s2, t0, 20f
    li t0, T_TYPE_IDENT
    beq s2, t0, 20f
    li t0, T_FN
    bne s2, t0, 191f
    la a0, m_fn_stmt            # `fn` inside a block: reported, then parsed and dropped
    call p_hard_msg
    li a0, N_ERROR
    call p_node
    mv s0, a0
    call parse_fn
    j 99f
191:
    la a0, x_stmt
    la a1, x_stmt_raw
    call p_fail_c
    li a0, N_ERROR
    call p_node
    mv s0, a0
    j 99f
10:
    li a0, N_COMMENT
    call p_node
    mv s0, a0
    call p_next
    j 99f
11:                             # let / var
    li a0, N_LET
    call p_node
    mv s0, a0
    li t0, T_VAR
    bne s2, t0, 111f
    li t0, NF_MUT
    sh t0, N_FLAGS(s0)
111:
    call p_next
    call p_kind
    li t0, T_UNDERSCORE
    bne a0, t0, 112f
    li t0, T_VAR
    beq s2, t0, 112f
    lhu t0, N_FLAGS(s0)
    ori t0, t0, NF_DISCARD
    sh t0, N_FLAGS(s0)
    call p_next
    j 113f
112:
    call p_name
    sd a0, N_A(s0)
113:
    li a0, T_COLON
    call p_accept
    beqz a0, 114f
    call parse_type
    sd a0, N_B(s0)
    call p_kind                 # `var x: int;`: a variable needs a starting value
    li t0, T_SEMI
    bne a0, t0, 114f
    ld a0, N_B(s0)
    call p_missing_init
114:
    li a0, T_ASSIGN
    call p_expect
    call parse_expr
    sd a0, N_C(s0)
    j 90f
12:                             # set place = expr;
    li a0, N_SET
    call p_node
    mv s0, a0
    call p_next
    call parse_postfix
    sd a0, N_A(s0)
    lhu t0, N_KIND(a0)
    li t1, N_IDENT
    beq t0, t1, 121f
    li t1, N_FIELD
    beq t0, t1, 121f
    li t1, N_INDEX
    beq t0, t1, 121f
    li t1, N_ERROR
    beq t0, t1, 121f
    la a0, x_name                # set needs a variable, field or index
    la a1, x_name_raw
    call p_fail_c
121:
    ld a0, N_A(s0)
    call p_compound_assign      # `set x += 1;` (no compound assignment in vo)
    li a0, T_ASSIGN
    call p_expect
    call parse_expr
    sd a0, N_B(s0)
    j 90f
13:
    call parse_if
    mv s0, a0
    j 99f
14:
    li a0, N_WHILE
    call p_node
    mv s0, a0
    call p_next
    call parse_expr
    sd a0, N_A(s0)
    call parse_block
    sd a0, N_B(s0)
    j 98f
15:                             # for x in expr [.. expr] block
    li a0, N_FOR
    call p_node
    mv s0, a0
    call p_next
    call p_kind
    li t0, T_UNDERSCORE         # `for _ in a..b` repeats without naming the counter
    bne a0, t0, 152f
    lhu t0, N_FLAGS(s0)
    ori t0, t0, NF_DISCARD
    sh t0, N_FLAGS(s0)
    call p_next
    j 153f
152:
    call p_name
    sd a0, N_A(s0)
153:
    li a0, T_IN
    call p_expect
    call parse_expr
    sd a0, N_B(s0)
    li a0, T_DOTDOT
    call p_accept
    beqz a0, 151f
    call parse_expr
    sd a0, N_C(s0)
151:
    call parse_block
    sd a0, N_D(s0)
    j 98f
16:                             # match expr { case name[(binding)] block ... }
    li a0, N_MATCH
    call p_node
    mv s0, a0
    call p_next
    call parse_expr
    sd a0, N_A(s0)
    call vec_new
    sd a0, N_B(s0)
    li a0, T_LBRACE
    call p_expect
161:
    call p_failed
    bnez a0, 169f
    call p_kind
    li t0, T_RBRACE
    beq a0, t0, 169f
    beqz a0, 169f
    li t0, T_COMMENT
    bne a0, t0, 162f
    li a0, N_COMMENT
    call p_node
    mv a1, a0
    ld a0, N_B(s0)
    call vec_push
    call p_next
    j 161b
162:
    call p_start
    mv s3, a0
    li a0, N_ARM
    call p_node
    mv s2, a0
    li t0, -1
    sd t0, N_E(s2)
    li a0, T_CASE
    call p_expect
    call p_name
    sd a0, N_A(s2)
    li a0, T_LPAREN
    call p_accept
    beqz a0, 164f
    call p_kind
    li t0, T_UNDERSCORE
    bne a0, t0, 163f
    lhu t0, N_FLAGS(s2)
    ori t0, t0, NF_DISCARD
    sh t0, N_FLAGS(s2)
    call p_next
    j 1631f
163:
    call p_name
    sd a0, N_B(s2)
1631:
    li a0, T_RPAREN
    call p_expect
164:
    call parse_block
    sd a0, N_C(s2)
    mv a0, s2
    mv a1, s3
    call p_finish
    mv a1, a0
    ld a0, N_B(s0)
    call vec_push
    j 161b
169:
    li a0, T_RBRACE
    call p_expect
    j 98f
17:                             # return [expr];
    li a0, N_RETURN
    call p_node
    mv s0, a0
    call p_next
    call p_kind
    li t0, T_SEMI
    beq a0, t0, 90f
    call parse_expr
    sd a0, N_A(s0)
    j 90f
18:                             # break; continue;
    mv a0, t1
    call p_node
    mv s0, a0
    call p_next
    j 90f
19:                             # try call;
    li a0, N_EXPR_STMT
    call p_node
    mv s0, a0
    call parse_unary
    sd a0, N_A(s0)
    j 90f
20:                             # call statement
    call p_compound_stmt        # `x += 1;`
    bnez a0, 98f
    li a0, N_EXPR_STMT
    call p_node
    mv s0, a0
    call parse_expr
    sd a0, N_A(s0)
    call p_failed
    bnez a0, 98f
    ld t0, N_A(s0)
    lhu t1, N_KIND(t0)
    li t2, N_CALL
    beq t1, t2, 90f
    li t2, N_CALL_VALUE
    beq t1, t2, 90f
    li t2, N_IDENT              # `x = e;`: E0114 with the fix `set x = e;`, parsed as a set
    beq t1, t2, 201f
    li t2, N_FIELD
    beq t1, t2, 201f
    li t2, N_INDEX
    bne t1, t2, 202f
201:
    call p_kind
    li t0, T_ASSIGN
    bne a0, t0, 202f
    ld a0, N_A(s0)
    call node_text
    mv a1, a0
    la a0, m_assign_set
    li a2, 0
    li a3, 0
    call msg
    mv a5, a0
    ld t0, N_A(s0)
    lwu a3, N_START(t0)
    lwu a4, N_END(t0)
    la a0, e0114
    li a1, SEV_ERROR
    la t0, cur_file
    ld a2, 0(t0)
    call diag_add
    mv a1, s1
    mv a2, s1
    la a3, s_set_kw
    call p_fix
    li t0, N_SET
    sh t0, N_KIND(s0)
    call p_next                 # the `=`
    call parse_expr
    sd a0, N_B(s0)
    j 90f
202:
    la a0, m_stmt_call          # E0114
    call str_from_cstr
    mv a5, a0
    ld t0, N_A(s0)
    lwu a3, N_START(t0)
    lwu a4, N_END(t0)
    la a0, e0114
    li a1, SEV_ERROR
    la t0, cur_file
    ld a2, 0(t0)
    call diag_add
    call p_mark_hard
    j 98f
90:
    call p_end_stmt
98:
    mv a0, s0
    mv a1, s1
    call p_finish
99:
    mv a0, s0
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# void *parse_fn(void)
    .globl parse_fn
parse_fn:
    li a0, N_FN
    tail parse_fn_kind

# void *parse_lambda(void)
# An anonymous function `fn(params) -> type { ... }`: the same layout as N_FN, with a made-up
# name node ("anonymous function") on the `fn` keyword.
    .globl parse_lambda
parse_lambda:
    li a0, N_LAMBDA
    tail parse_fn_kind

# void *parse_fn_kind(int kind)
    .globl parse_fn_kind
parse_fn_kind:
    addi sp, sp, -64            # 0..15: scratch
    sd ra, 56(sp)
    sd s0, 48(sp)
    sd s1, 40(sp)
    sd s2, 32(sp)
    sd s3, 24(sp)
    sd s4, 16(sp)
    sd zero, 0(sp)              # type parameters
    mv s4, a0
    call p_start
    mv s1, a0
    mv a0, s4
    call p_node
    mv s0, a0
    li t0, N_FN
    bne s4, t0, 71f
    call p_next
    call p_name
    sd a0, N_A(s0)
    call parse_tparams
    sd a0, 0(sp)
    j 72f
71:
    li a0, N_NAME
    call p_node
    mv s2, a0
    la a0, s_anon
    call str_from_cstr
    sd a0, N_A(s2)
    sd s2, N_A(s0)
    call p_next
72:
    call vec_new
    sd a0, N_B(s0)
    li a0, T_LPAREN
    call p_expect
    li a0, T_RPAREN
    call p_accept
    bnez a0, 3f
1:
    call p_failed
    bnez a0, 9f
    call p_start
    mv s3, a0
    li a0, N_PARAM
    call p_node
    mv s2, a0
    call p_name
    sd a0, N_A(s2)
    li a0, T_COLON
    call p_expect
    call parse_type
    sd a0, N_B(s2)
    mv a0, s2
    mv a1, s3
    call p_finish
    mv a1, a0
    ld a0, N_B(s0)
    call vec_push
    li a0, T_COMMA
    call p_accept
    bnez a0, 1b
    li a0, T_RPAREN
    call p_expect
3:
    call p_start
    mv s3, a0                   # where `->` starts
    li a0, T_ARROW
    call p_accept
    beqz a0, 4f
    call p_kind
    li t0, T_KVOID              # `-> void`: functions that return nothing have no `->`
    bne a0, t0, 31f
    mv a0, s3
    call p_arrow_void
    j 4f
31:
    call parse_type
    sd a0, N_C(s0)
4:
    ld s2, N_A(s0)              # the side record hangs off the name node
    beqz s2, 41f
    li a0, FX_SIZE
    call alloc
    sd a0, N_B(s2)
    mv s2, a0
    ld t0, 0(sp)
    sd t0, FX_TPARAMS(s2)
    call parse_uses
    sd a0, FX_USES(s2)
    li t0, N_LAMBDA             # anonymous functions infer their effects (E0507)
    bne s4, t0, 42f
    beqz a0, 42f
    call p_uses_on_lambda
    j 42f
41:
    call parse_uses
42:
    call parse_block
    sd a0, N_D(s0)
9:
    mv a0, s0
    mv a1, s1
    call p_finish
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    addi sp, sp, 64
    ret

# void *parse_tparams(void)
# An optional type-parameter list `[T, K: ordered, E: effects]` (vec of N_TPARAM), or 0.
    .globl parse_tparams
parse_tparams:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    sd s2, 0(sp)
    li s0, 0
    call p_kind
    li t0, T_LBRACKET
    bne a0, t0, 9f
    call p_next
    call vec_new
    mv s0, a0
1:
    call p_failed
    bnez a0, 9f
    call p_start
    mv s2, a0
    li a0, N_TPARAM
    call p_node
    mv s1, a0
    call p_name
    sd a0, N_A(s1)
    li a0, T_COLON
    call p_accept
    beqz a0, 3f
    call p_kind                 # the constraint: `ordered` or `effects`
    li t0, T_IDENT
    bne a0, t0, 2f
    call p_tok
    call p_tok_str
    mv a1, a0
    la a0, s_ordered
    call str_eq_cstr_rev
    li t0, TPC_ORDERED
    bnez a0, 21f
    call p_tok
    call p_tok_str
    mv a1, a0
    la a0, s_effects
    call str_eq_cstr_rev
    li t0, TPC_EFFECTS
    beqz a0, 2f
21:
    sd t0, N_B(s1)
    call p_next
    j 3f
2:
    la a0, x_constraint
    la a1, x_constraint_raw
    call p_fail_c
    j 9f
3:
    mv a0, s1
    mv a1, s2
    call p_finish
    mv a1, a0
    mv a0, s0
    call vec_push
    li a0, T_COMMA
    call p_accept
    bnez a0, 1b
    li a0, T_RBRACKET
    call p_expect
9:
    mv a0, s0
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    ld s2, 0(sp)
    addi sp, sp, 32
    ret

# int str_eq_cstr_rev(char *c, void *str)
    .globl str_eq_cstr_rev
str_eq_cstr_rev:
    mv t0, a0
    mv a0, a1
    mv a1, t0
    tail str_eq_cstr

# void *parse_type_args(void)
# Optional type arguments `[type, ...]` after a type name (vec of type nodes), or 0.
    .globl parse_type_args
parse_type_args:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    li s0, 0
    call p_kind
    li t0, T_LBRACKET
    bne a0, t0, 9f
    call p_next
    call vec_new
    mv s0, a0
1:
    call p_failed
    bnez a0, 9f
    call parse_type
    mv a1, a0
    mv a0, s0
    call vec_push
    li a0, T_COMMA
    call p_accept
    bnez a0, 1b
    li a0, T_RBRACKET
    call p_expect
9:
    mv a0, s0
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret

# void p_uses_on_lambda(void *uses)
# E0507: an anonymous function with a `uses` clause; the fix removes it (up to the body).
    .globl p_uses_on_lambda
p_uses_on_lambda:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    call p_start                # the body's `{`
    mv s1, a0
    la a0, m_uses_lambda
    call str_from_cstr
    mv a5, a0
    la a0, e0507
    li a1, SEV_ERROR
    la t0, cur_file
    ld a2, 0(t0)
    lwu a3, N_START(s0)
    lwu a4, N_END(s0)
    call diag_add
    lwu a1, N_START(s0)
    mv a2, s1
    la a3, s_empty
    call p_fix
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void *parse_uses(void)
# An optional effect clause `uses e, e(...), ...` as an N_USES node, or 0 when there is none.
    .globl parse_uses
parse_uses:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    call p_kind
    li t0, T_USES
    li s0, 0
    bne a0, t0, 9f
    call p_start
    mv s1, a0
    li a0, N_USES
    call p_node
    mv s0, a0
    call p_next
    call vec_new
    sd a0, N_A(s0)
1:
    call p_failed
    bnez a0, 8f
    call parse_effect
    beqz a0, 8f
    mv a1, a0
    ld a0, N_A(s0)
    call vec_push
    call p_kind
    li t0, T_COMMA
    bne a0, t0, 8f
    li a0, 2                    # `, name: type`: a function type's clause ended in a parameter list
    call p_kind_at
    li t0, T_COLON
    beq a0, t0, 8f
    call p_next
    j 1b
8:
    mv a0, s0
    mv a1, s1
    call p_finish
9:
    mv a0, s0
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void *parse_effect(void)
# One effect: a name (`file.read` is two names joined by `.`) with optional scope strings, or an
# effect parameter (a type name). N_EFFECT: A bit (0 = unknown), B scope N_STR nodes, C the effect
# parameter's name node, D the name text.
    .globl parse_effect
parse_effect:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    call p_start
    mv s1, a0
    li a0, N_EFFECT
    call p_node
    mv s0, a0
    call p_kind
    li t0, T_TYPE_IDENT
    bne a0, t0, 1f
    call p_name                 # effect parameter
    sd a0, N_C(s0)
    ld a0, N_A(a0)
    sd a0, N_D(s0)
    j 8f
1:
    li t0, T_IDENT
    beq a0, t0, 2f
    li a0, T_IDENT
    call p_expect
    li s0, 0
    j 9f
2:
    call p_next
    call p_tok_str
    mv s2, a0                   # the name text
    call p_kind
    li t0, T_DOT
    bne a0, t0, 3f
    li a0, 1
    call p_kind_at
    li t0, T_IDENT
    bne a0, t0, 3f
    call p_next                 # `.`
    call p_next
    call p_tok_str
    mv s3, a0
    call sb_new
    mv s1, a0
    mv a1, s2
    call sb_str
    mv a0, s1
    li a1, 46                   # '.'
    call sb_byte
    mv a0, s1
    mv a1, s3
    call sb_str
    mv a0, s1
    call sb_finish
    mv s2, a0
    lwu s1, N_START(s0)
3:
    sd s2, N_D(s0)
    addi a0, s2, STR_DATA
    ld a1, STR_LEN(s2)
    call es_bit_by_name
    sd a0, N_A(s0)
    li a0, T_LPAREN
    call p_accept
    beqz a0, 8f
    call vec_new
    sd a0, N_B(s0)
4:
    call p_kind
    li t0, T_STRING
    beq a0, t0, 5f
    li a0, T_STRING
    call p_expect
    j 8f
5:
    li a0, N_STR
    call p_node
    mv s3, a0
    call p_next
    lwu a1, TOK_END(a0)
    lwu a0, TOK_START(a0)
    call decode_string
    sd a0, N_A(s3)
    mv a1, s3
    ld a0, N_B(s0)
    call vec_push
    li a0, T_COMMA
    call p_accept
    bnez a0, 4b
    li a0, T_RPAREN
    call p_expect
8:
    mv a0, s0
    mv a1, s1
    call p_finish
    mv a0, s0
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# void *parse_members(int kind)
# The body of a record (kind N_FIELD_DECL: "name: type;") or variant (N_CASE_DECL: "name[(type)];").
    .globl parse_members
parse_members:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s1, a0
    call vec_new
    mv s0, a0
    li a0, T_LBRACE
    call p_expect
1:
    call p_failed
    bnez a0, 9f
    call p_kind
    li t0, T_RBRACE
    beq a0, t0, 8f
    beqz a0, 8f
    li t0, T_COMMENT
    bne a0, t0, 2f
    li a0, N_COMMENT
    call p_node
    mv a1, a0
    mv a0, s0
    call vec_push
    call p_next
    j 1b
2:
    call p_start
    mv s3, a0
    mv a0, s1
    call p_node
    mv s2, a0
    call p_name
    sd a0, N_A(s2)
    li t0, N_FIELD_DECL
    bne s1, t0, 3f
    li a0, T_COLON
    call p_expect
    call parse_type
    sd a0, N_B(s2)
    j 4f
3:
    li a0, T_LPAREN
    call p_accept
    beqz a0, 4f
    call parse_type
    sd a0, N_B(s2)
    li a0, T_RPAREN
    call p_expect
4:
    call p_member_comma         # `x: int,`: members end with `;`
    bnez a0, 41f
    call p_end_stmt
41:
    mv a0, s2
    mv a1, s3
    call p_finish
    mv a1, a0
    mv a0, s0
    call vec_push
    j 1b
8:
    li a0, T_RBRACE
    call p_expect
9:
    mv a0, s0
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# void *parse_item(void)
    .globl parse_item
parse_item:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    call p_start
    mv s1, a0
    call p_kind
    mv s2, a0
    li t0, T_COMMENT
    beq s2, t0, 1f
    li t0, T_FN
    beq s2, t0, 2f
    li t0, T_RECORD
    beq s2, t0, 3f
    li t0, T_VARIANT
    beq s2, t0, 4f
    li t0, T_TEST
    beq s2, t0, 5f
    li t0, T_IMPORT
    beq s2, t0, 6f
    la a0, x_item
    la a1, x_item_raw
    call p_fail_c
    li a0, N_ERROR
    call p_node
    mv s0, a0
    j 9f
1:
    li a0, N_COMMENT
    call p_node
    mv s0, a0
    call p_next
    j 9f
2:
    call parse_fn
    mv s0, a0
    j 9f
3:
    li a0, N_RECORD
    call p_node
    mv s0, a0
    call p_next
    call p_name
    sd a0, N_A(s0)
    call parse_tparams
    sd a0, N_C(s0)
    li a0, N_FIELD_DECL
    call parse_members
    sd a0, N_B(s0)
    j 8f
4:
    li a0, N_VARIANT
    call p_node
    mv s0, a0
    call p_next
    call p_name
    sd a0, N_A(s0)
    call parse_tparams
    sd a0, N_C(s0)
    li a0, N_CASE_DECL
    call parse_members
    sd a0, N_B(s0)
    j 8f
5:
    li a0, N_TEST
    call p_node
    mv s0, a0
    call p_next
    call p_kind
    li t0, T_STRING
    beq a0, t0, 51f
    li a0, T_STRING
    call p_expect
    j 8f
51:
    call p_next
    lwu a1, TOK_END(a0)
    lwu a0, TOK_START(a0)
    call decode_string
    sd a0, N_A(s0)
    call parse_uses
    sd a0, N_C(s0)
    call parse_block
    sd a0, N_B(s0)
    j 8f
6:                              # import a.b.name;
    li a0, N_IMPORT
    call p_node
    mv s0, a0
    call p_next
    call vec_new
    sd a0, N_A(s0)
61:
    call p_name
    beqz a0, 8f
    sd a0, N_B(s0)
    ld a1, N_A(a0)
    ld a0, N_A(s0)
    call vec_push
    li a0, T_DOT
    call p_accept
    bnez a0, 61b
    call p_end_stmt
8:
    mv a0, s0
    mv a1, s1
    call p_finish
9:
    mv a0, s0
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    addi sp, sp, 48
    ret

# void *parse_file(int file)
# Parses a lexed file; stores and returns its N_FILE_NODE.
    .globl parse_file
parse_file:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    sd s2, 0(sp)
    mv s2, a0
    la t0, cur_file
    sd s2, 0(t0)
    call file_get
    mv s1, a0
    ld t0, FILE_TOKENS(s1)
    la t1, p_toks
    sd t0, 0(t1)
    ld t0, FILE_TEXT(s1)
    la t1, p_text
    sd t0, 0(t1)
    la t0, p_pos
    sd zero, 0(t0)
    la t0, p_err
    sd zero, 0(t0)
    la t0, p_hard
    sd zero, 0(t0)
    li a0, N_FILE_NODE
    li a1, 0
    ld a2, FILE_LEN(s1)
    call node_new
    mv s0, a0
    sd s0, FILE_AST(s1)
    call vec_new
    sd a0, N_A(s0)
    call vec_new
    la t0, p_methods
    sd a0, 0(t0)
1:
    call p_kind
    beqz a0, 9f
    call parse_item
    mv a1, a0
    ld a0, N_A(s0)
    call vec_push
    call p_failed
    beqz a0, 1b
    call p_sync_item
    j 1b
9:
    la t0, p_hard               # the checker will not run: report `obj.name(...)` as v1 did
    ld t0, 0(t0)
    beqz t0, 92f
    li s1, 0
91:
    la t0, p_methods
    ld t0, 0(t0)
    ld t1, VEC_N(t0)
    bge s1, t1, 92f
    mv a0, t0
    mv a1, s1
    call vec_get
    addi s1, s1, 1
    call p_method_diag
    j 91b
92:
    mv a0, s0
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    ld s2, 0(sp)
    addi sp, sp, 32
    ret

# void p_method_diag(void *call)
# E0110 for a pending `obj.name(args)` with its rewrite fix (stored by p_method_call).
    .globl p_method_diag
p_method_diag:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    ld s0, N_E(a0)
    la a0, e0110
    li a1, SEV_ERROR
    ld a2, 32(s0)
    ld a3, 8(s0)
    ld a4, 16(s0)
    ld a5, 0(s0)
    call diag_add
    ld a4, 24(s0)
    ld a1, 32(s0)
    ld a2, 8(s0)
    ld a3, 16(s0)
    call diag_add_fix
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret

# void *node_text(void *node)
# The source text of a node in the file being parsed.
    .globl node_text
node_text:
    lwu t0, N_START(a0)
    lwu t1, N_END(a0)
    la t2, p_text
    ld t2, 0(t2)
    add a0, t2, t0
    sub a1, t1, t0
    tail str_new

# void p_unclosed(long open_start)
# E0111 at the end of the file for the block opened at open_start, with a fix inserting "}".
    .globl p_unclosed
p_unclosed:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    call p_start                # the EOF token
    mv s1, a0
    la a0, e0111
    mv a1, s1
    mv a2, s1
    la a3, m_unclosed
    call p_soft_err
    sd a0, 0(sp)
    mv a1, s1
    mv a2, s1
    la a3, s_close_nl
    call p_fix
    la a0, m_opened
    call str_from_cstr
    mv a4, a0
    ld a0, 0(sp)
    la t0, cur_file
    ld a1, 0(t0)
    mv a2, s0
    addi a3, s0, 1
    call diag_add_related
    call p_mark_hard
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void *p_last_diag(void)
# The most recent diagnostic when it is an E0110 at the current token, else 0.
    .globl p_last_diag
p_last_diag:
    addi sp, sp, -16
    sd ra, 8(sp)
    call diag_last
    beqz a0, 9f
    ld t0, D_CODE(a0)
    la t1, e0110
    bne t0, t1, 8f
    ld t2, D_START(a0)
    sd a0, 0(sp)
    call p_start
    mv t2, a0
    ld a0, 0(sp)
    ld t3, D_START(a0)
    beq t2, t3, 9f
8:
    li a0, 0
9:
    ld ra, 8(sp)
    addi sp, sp, 16
    ret

# void p_compound_assign(void *place)
# After `set place`: an operator followed by `=` (`+=`, `-=`, ...) is E0110 with a fix that
# rewrites it to `= place op`.
    .globl p_compound_assign
p_compound_assign:
    addi sp, sp, -64            # 0: diag, 8: place text, 16: builder; 24..63: saved registers
    sd ra, 56(sp)
    sd s0, 48(sp)
    sd s1, 40(sp)
    sd s2, 32(sp)
    sd s3, 24(sp)
    mv s3, a0
    call p_kind
    mv s0, a0                   # operator
    li t0, T_PLUS
    blt s0, t0, 9f
    li t0, T_PERCENT
    bgt s0, t0, 9f
    li a0, 1
    call p_kind_at
    li t0, T_ASSIGN
    bne a0, t0, 9f
    call p_tok
    lwu s1, TOK_START(a0)       # span: the operator and the `=`
    call p_next
    call p_tok
    lwu s2, TOK_END(a0)
    mv a0, s3
    call node_text
    sd a0, 8(sp)
    mv a0, s0
    call tok_raw
    mv a2, a0
    ld a1, 8(sp)
    la a0, m_compound
    li a3, 0
    call msg
    mv a5, a0
    la a0, e0110
    li a1, SEV_ERROR
    la t0, cur_file
    ld a2, 0(t0)
    mv a3, s1
    mv a4, s2
    call diag_add
    sd a0, 0(sp)
    call sb_new                 # replacement: "= <place> <op>"
    sd a0, 16(sp)
    li a1, '='
    call sb_byte
    ld a0, 16(sp)
    li a1, ' '
    call sb_byte
    ld a0, 16(sp)
    ld a1, 8(sp)
    call sb_str
    ld a0, 16(sp)
    li a1, ' '
    call sb_byte
    mv a0, s0
    call tok_raw
    mv a1, a0
    ld a0, 16(sp)
    call sb_str
    ld a0, 16(sp)
    call sb_finish
    mv a4, a0
    ld a0, 0(sp)
    la t0, cur_file
    ld a1, 0(t0)
    mv a2, s1
    mv a3, s2
    call diag_add_fix
    call p_mark_hard
9:
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    addi sp, sp, 64
    ret

# int p_insert_ok(int expected)
# Whether inserting the expected token before the current one is a plausible repair.
    .globl p_insert_ok
p_insert_ok:
    li t0, T_RPAREN
    beq a0, t0, 1f
    li t0, T_RBRACKET
    beq a0, t0, 1f
    li a0, 0
    ret
1:
    addi sp, sp, -16
    sd ra, 8(sp)
    call p_tok
    lhu t0, TOK_FLAGS(a0)
    lhu t1, TOK_KIND(a0)
    andi t0, t0, TF_NEWLINE_BEFORE
    bnez t0, 2f
    li t2, T_SEMI
    beq t1, t2, 2f
    li t2, T_LBRACE
    beq t1, t2, 2f
    li t2, T_RPAREN
    beq t1, t2, 2f
    li t2, T_RBRACKET
    beq t1, t2, 2f
    li t2, T_COMMA
    beq t1, t2, 2f
    li a0, 0
    j 3f
2:
    li a0, 1
3:
    ld ra, 8(sp)
    addi sp, sp, 16
    ret

# void p_hard_msg(char *message)
# A hard syntax error with a custom message at the current token.
    .globl p_hard_msg
p_hard_msg:
    la t0, p_err
    ld t0, 0(t0)
    bnez t0, 9f
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    mv s0, a0
    call p_tok
    lwu a1, TOK_START(a0)
    lwu a2, TOK_END(a0)
    la a0, e0110
    mv a3, s0
    call p_soft_err
    call p_mark_hard
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
9:
    ret

# void *p_method_call(void *object, void *name_node, long start, void *field)
# `obj.name(args)`: an N_CALL_VALUE of the field, flagged NF_METHOD, holding v1's rewrite `name(obj,
# args)` (N_D) and its E0110 message, span and fix (N_E: message, start, end, replacement, file).
# The checker reports E0110 unless the field has a function type; so does parse_file when the
# checker will not run.
    .globl p_method_call
p_method_call:
    addi sp, sp, -80            # 0..23: scratch; 32..79: saved registers
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
    mv s5, a3
    call p_tok
    lwu s3, TOK_END(a0)         # just after "("
    call parse_args
    mv s4, a0
    call p_prev_end             # the closing ")"
    sd a0, 0(sp)
    mv a0, s0                   # args: the object first
    call vec_new
    sd a0, 8(sp)
    mv a1, s0
    call vec_push
    li t0, 0
    sd t0, 16(sp)
1:
    ld t0, 16(sp)
    ld t1, VEC_N(s4)
    bge t0, t1, 2f
    mv a0, s4
    mv a1, t0
    call vec_get
    mv a1, a0
    ld a0, 8(sp)
    call vec_push
    ld t0, 16(sp)
    addi t0, t0, 1
    sd t0, 16(sp)
    j 1b
2:
    li a0, N_CALL
    mv a1, s2
    ld a2, 0(sp)
    call node_new
    sd s1, N_A(a0)
    ld t0, 8(sp)
    sd t0, N_B(a0)
    sd a0, 16(sp)               # the call node
    call sb_new                 # replacement text: name(obj[, args])
    mv s4, a0
    ld a1, N_A(s1)
    call sb_str
    mv a0, s4
    li a1, '('
    call sb_byte
    mv a0, s0
    call node_text
    mv a1, a0
    mv a0, s4
    call sb_str
    ld t0, 0(sp)                # the original argument text, if any
    addi t0, t0, -1
    ble t0, s3, 3f
    mv a0, s4
    la a1, s_comma_sep
    call sb_cstr
    la t0, p_text
    ld t0, 0(t0)
    add a1, t0, s3
    ld t1, 0(sp)
    addi t1, t1, -1
    sub a2, t1, s3
    mv a0, s4
    call sb_bytes
3:
    mv a0, s4
    li a1, ')'
    call sb_byte
    mv a0, s4
    call sb_finish
    sd a0, 8(sp)                # replacement
    mv a1, a0
    la a0, m_method
    li a2, 0
    li a3, 0
    call msg
    sd a0, 0(sp)                # message (0(sp) held the end; it is in the call node too)
    li a0, 40
    call alloc
    mv s4, a0                   # E0110 info
    ld t0, 0(sp)
    sd t0, 0(s4)
    sd s2, 8(s4)
    ld t0, 16(sp)
    lwu t1, N_END(t0)
    sd t1, 16(s4)
    ld t0, 8(sp)
    sd t0, 24(s4)
    la t0, cur_file
    ld t0, 0(t0)
    sd t0, 32(s4)
    li a0, N_CALL_VALUE         # the call as written
    mv a1, s2
    ld t0, 16(sp)
    lwu a2, N_END(t0)
    call node_new
    sd s5, N_A(a0)
    ld t0, 16(sp)
    ld t1, N_B(t0)
    ld t2, VEC_N(t1)            # its own arguments: the rewrite's without the object
    sd a0, 0(sp)
    call vec_new
    ld t0, 0(sp)
    sd a0, N_B(t0)
    li t3, 1
    sd t3, 8(sp)
1:
    ld t0, 16(sp)
    ld t1, N_B(t0)
    ld t2, VEC_N(t1)
    ld t3, 8(sp)
    bge t3, t2, 2f
    mv a0, t1
    mv a1, t3
    call vec_get
    mv a1, a0
    ld t0, 0(sp)
    ld a0, N_B(t0)
    call vec_push
    ld t3, 8(sp)
    addi t3, t3, 1
    sd t3, 8(sp)
    j 1b
2:
    ld a0, 0(sp)
    ld t0, 16(sp)
    sd t0, N_D(a0)
    sd s4, N_E(a0)
    li t0, NF_METHOD
    sh t0, N_FLAGS(a0)
    mv a1, a0
    la t0, p_methods
    ld a0, 0(t0)
    call vec_push
    ld a0, 0(sp)
    ld ra, 72(sp)
    ld s0, 64(sp)
    ld s1, 56(sp)
    ld s2, 48(sp)
    ld s3, 40(sp)
    ld s4, 32(sp)
    ld s5, 24(sp)
    addi sp, sp, 80
    ret

# int p_elif(void)
# When the current token is `elif`: E0110 with a fix to `else if` (1; the caller parses it as if).
    .globl p_elif
p_elif:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    call p_kind
    li t0, T_IDENT
    bne a0, t0, 8f
    call p_tok
    mv s0, a0
    call p_tok_str
    la a1, w_elif
    call str_eq_cstr
    beqz a0, 8f
    la a0, e0110
    lwu a1, TOK_START(s0)
    lwu a2, TOK_END(s0)
    la a3, m_elif
    call p_soft_err
    lwu a1, TOK_START(s0)
    lwu a2, TOK_END(s0)
    la a3, s_else_if
    call p_fix
    li a0, 1
    j 9f
8:
    li a0, 0
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    addi sp, sp, 32
    ret

# void p_missing_new(void)
# At `Type {`: E0110 with a fix inserting `new ` (the caller parses the record literal).
    .globl p_missing_new
p_missing_new:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    call p_tok
    mv s0, a0
    call p_tok_str
    mv a1, a0
    la a0, m_new
    li a2, 0
    li a3, 0
    call msg
    mv a5, a0
    la a0, e0110
    li a1, SEV_ERROR
    la t0, cur_file
    ld a2, 0(t0)
    lwu a3, TOK_START(s0)
    lwu a4, TOK_END(s0)
    call diag_add
    lwu a1, TOK_START(s0)
    mv a2, a1
    la a3, s_new_sp
    call p_fix
    ld ra, 24(sp)
    ld s0, 16(sp)
    addi sp, sp, 32
    ret

# int p_member_comma(void)
# A `,` where a field or case should end with `;`: E0112 with a fix replacing it (consumed, 1).
    .globl p_member_comma
p_member_comma:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    call p_kind
    li t0, T_COMMA
    bne a0, t0, 9f
    call p_tok
    mv s0, a0
    la a0, e0112
    lwu a1, TOK_START(s0)
    lwu a2, TOK_END(s0)
    la a3, m_member_comma
    call p_soft_err
    lwu a1, TOK_START(s0)
    lwu a2, TOK_END(s0)
    la a3, s_semi
    call p_fix
    call p_next
    li a0, 1
    j 10f
9:
    li a0, 0
10:
    ld ra, 24(sp)
    ld s0, 16(sp)
    addi sp, sp, 32
    ret

# void p_missing_init(void *type_node)
# `var x: T;`: a hard E0110 with a fix inserting a starting value for T when there is an obvious one.
    .globl p_missing_init
p_missing_init:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    call p_prev_end
    mv s1, a0
    la a0, e0110
    mv a1, s1
    mv a2, s1
    la a3, m_init
    call p_soft_err
    sd a0, 0(sp)
    ld t0, N_A(s0)
    li t1, TY_INT
    la a3, d_int
    beq t0, t1, 1f
    li t1, TY_FLOAT
    la a3, d_float
    beq t0, t1, 1f
    li t1, TY_BOOL
    la a3, d_bool
    beq t0, t1, 1f
    li t1, TY_STR
    la a3, d_str
    beq t0, t1, 1f
    li t1, TY_LIST
    beq t0, t1, 2f
    li t1, TY_MAP
    beq t0, t1, 2f
    j 8f
1:
    ld a0, 0(sp)
    mv a1, s1
    mv a2, s1
    call p_fix
    j 8f
2:
    call sb_new                 # " = new list[T]" / " = new map[K, V]"
    mv s1, a0
    la a1, d_new
    call sb_cstr
    mv a0, s0
    call node_text
    mv a1, a0
    mv a0, s1
    call sb_str
    mv a0, s1
    call sb_finish
    mv a4, a0
    call p_prev_end
    mv a2, a0
    mv a3, a0
    ld a0, 0(sp)
    la t0, cur_file
    ld a1, 0(t0)
    call diag_add_fix
8:
    call p_mark_hard
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void p_arrow_void(long arrow_start)
# `-> void`: E0110 with a fix removing it (soft: the function returns nothing).
    .globl p_arrow_void
p_arrow_void:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    call p_tok
    lwu s1, TOK_END(a0)         # end of `void`
    call p_next
    addi a1, s0, -1             # include the space before `->`
    la t0, p_text
    ld t0, 0(t0)
    add t1, t0, a1
    lbu t1, 0(t1)
    li t2, ' '
    beq t1, t2, 1f
    mv a1, s0
1:
    sd a1, 0(sp)
    la a0, e0110
    mv a1, s0
    mv a2, s1
    la a3, m_void_arrow
    call p_soft_err
    ld a1, 0(sp)
    mv a2, s1
    la a3, s_empty
    call p_fix
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# int p_compound_stmt(void)
# At a statement `x += e;`: hard E0114 with a fix to `set x = x + e;`. 1 when it applied.
    .globl p_compound_stmt
p_compound_stmt:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    li a0, 1
    call p_kind_at
    mv s0, a0                   # operator
    li t0, T_PLUS
    blt s0, t0, 8f
    li t0, T_PERCENT
    bgt s0, t0, 8f
    li a0, 2
    call p_kind_at
    li t0, T_ASSIGN
    bne a0, t0, 8f
    call p_tok
    mv s1, a0                   # the name token
    call p_tok_str
    mv s2, a0
    mv a0, s0
    call tok_raw
    mv a2, a0
    mv a1, s2
    la a0, m_compound_stmt
    li a3, 0
    call msg
    mv a5, a0
    la a0, e0114
    li a1, SEV_ERROR
    la t0, cur_file
    ld a2, 0(t0)
    lwu a3, TOK_START(s1)
    lwu a4, TOK_START(s1)
    addi t0, s1, 2*TOK_SIZE
    lwu a4, TOK_END(t0)         # through the `=`
    call diag_add
    sd a0, 0(sp)
    call sb_new                 # "set x = x +"
    sd a0, 8(sp)
    la a1, s_set_kw
    call sb_cstr
    ld a0, 8(sp)
    mv a1, s2
    call sb_str
    ld a0, 8(sp)
    la a1, s_eq_sp
    call sb_cstr
    ld a0, 8(sp)
    mv a1, s2
    call sb_str
    ld a0, 8(sp)
    li a1, ' '
    call sb_byte
    mv a0, s0
    call tok_raw
    mv a1, a0
    ld a0, 8(sp)
    call sb_str
    ld a0, 8(sp)
    call sb_finish
    mv a4, a0
    ld a0, 0(sp)
    la t0, cur_file
    ld a1, 0(t0)
    lwu a2, TOK_START(s1)
    addi t0, s1, 2*TOK_SIZE
    lwu a3, TOK_END(t0)
    call diag_add_fix
    call p_mark_hard
    li a0, 1
    j 9f
8:
    li a0, 0
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    addi sp, sp, 48
    ret
