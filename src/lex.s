# Lexer: source bytes -> 16-byte tokens (contracts/grammar.md, lexical rules).
    .include "defs.inc"

    .section .bss
    .align 3
lx_text:    .space 8
lx_len:     .space 8
lx_file:    .space 8
lx_toks:    .space 8
lx_n:       .space 8
lx_nl:      .space 8                # a newline was seen since the last token
lx_code:    .space 8                # the current line has a token
lx_lstart:  .space 8                # offset where the current line starts
lx_skip_to: .space 8                # set by lx_number_suffix: resume lexing here

    .section .rodata
kw_table:
    .dword k_fn, k_record, k_variant, k_test, k_import, k_let, k_var, k_set, k_if, k_else
    .dword k_while, k_for, k_in, k_match, k_case, k_return, k_break, k_continue, k_try, k_new
    .dword k_and, k_or, k_not, k_true, k_false, k_int, k_float, k_bool, k_str, k_list
    .dword k_map, k_result, k_void, k_uses, k_prop, k_requires, k_ensures, 0
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
k_uses: .string "uses"
k_prop: .string "prop"
k_requires: .string "requires"
k_ensures: .string "ensures"
e0101: .string "E0101"
e0102: .string "E0102"
e0103: .string "E0103"
e0104: .string "E0104"
e0105: .string "E0105"
m_unexpected:   .string "Unexpected character `$1`."
m_trailing:     .string "A comment must be on its own line."
m_unterminated: .string "This string is not closed before the end of the line."
m_escape:       .string "Unknown escape sequence `$1`. Use \\n, \\t, \\\\ or \\\"."
m_utf8:         .string "Invalid UTF-8 byte 0x$1."
m_int_big:      .string "Integer literal is larger than 9223372036854775807."
m_float_long:   .string "Float literal has more than 15 significant digits."
m_carry:        .string "vo writes `$1`, not `$2`."
m_single:       .string "Strings use double quotes."
m_ternary:      .string "vo has no `?:` or `?` operator: use `if`/`else`, or `try` to pass an error on."
w_and:          .string "and"
w_or:           .string "or"
w_not:          .string "not"
s_dquote:       .string "\""
m_suffix:       .string "Number literals have no exponent or suffix: write the number in full, like 1000.0."
s_quote:        .string "\""
hexup:          .string "0123456789ABCDEF"

    .text
# void lx_push(int kind, long start, long end)
    .globl lx_push
lx_push:
    la t0, lx_toks
    ld t0, 0(t0)
    la t1, lx_n
    ld t2, 0(t1)
    slli t3, t2, 4
    add t0, t0, t3
    sh a0, TOK_KIND(t0)
    la t4, lx_nl
    ld t5, 0(t4)
    sh t5, TOK_FLAGS(t0)
    sd zero, 0(t4)
    sw a1, TOK_START(t0)
    sw a2, TOK_END(t0)
    addi t2, t2, 1
    sd t2, 0(t1)
    la t4, lx_code
    li t5, 1
    sd t5, 0(t4)
    ret

# void *lx_err(char *code, long start, long end, void *message)
# Adds an error diagnostic for the current file.
    .globl lx_err
lx_err:
    mv a5, a3
    mv a4, a2
    mv a3, a1
    la t0, lx_file
    ld a2, 0(t0)
    li a1, SEV_ERROR
    tail diag_add

# void *lx_slice(long start, long end)
    .globl lx_slice
lx_slice:
    la t0, lx_text
    ld t0, 0(t0)
    sub a1, a1, a0
    add a0, t0, a0
    tail str_new

# int is_word_char(int c)
    .globl is_word_char
is_word_char:
    li t0, '_'
    beq a0, t0, 1f
    li t0, '0'
    blt a0, t0, 2f
    li t0, '9'
    ble a0, t0, 1f
    li t0, 'A'
    blt a0, t0, 2f
    li t0, 'Z'
    ble a0, t0, 1f
    li t0, 'a'
    blt a0, t0, 2f
    li t0, 'z'
    ble a0, t0, 1f
2:
    li a0, 0
    ret
1:
    li a0, 1
    ret

# int keyword_kind(char *p, long n)
# The keyword token kind for the word, or T_IDENT.
    .globl keyword_kind
keyword_kind:
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
    la t0, kw_table
    slli t1, s2, 3
    add t0, t0, t1
    ld s3, 0(t0)
    beqz s3, 8f
    mv a0, s3
    call cstr_len
    bne a0, s1, 2f
    mv a0, s0
    mv a1, s3
    mv a2, s1
    call mem_eq
    bnez a0, 3f
2:
    addi s2, s2, 1
    j 1b
3:
    addi a0, s2, T_KW_FIRST
    j 9f
8:
    li a0, T_IDENT
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# void *lex(int file)
# Tokenizes a registered file; stores and returns the token array (ending with T_EOF).
    .globl lex
lex:
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
    la t0, lx_file
    sd a0, 0(t0)
    call file_get
    mv s7, a0                   # file record
    ld s0, FILE_TEXT(s7)
    ld s1, FILE_LEN(s7)
    la t0, lx_text
    sd s0, 0(t0)
    la t0, lx_len
    sd s1, 0(t0)
    addi a0, s1, 2
    slli a0, a0, 4
    call alloc
    la t0, lx_toks
    sd a0, 0(t0)
    sd a0, FILE_TOKENS(s7)
    la t0, lx_n
    sd zero, 0(t0)
    la t0, lx_nl
    li t1, 1
    sd t1, 0(t0)
    la t0, lx_code
    sd zero, 0(t0)
    la t0, lx_lstart
    sd zero, 0(t0)
    mv a0, s0                   # UTF-8 check first: report the first bad byte
    mv a1, s1
    call utf8_validate
    bltz a0, 1f
    mv s2, a0
    add t0, s0, s2
    lbu t0, 0(t0)
    sd t0, 0(sp)
    li a0, 2
    call alloc
    ld t0, 0(sp)
    la t1, hexup
    srli t2, t0, 4
    add t2, t1, t2
    lbu t2, 0(t2)
    andi t3, t0, 15
    add t3, t1, t3
    lbu t3, 0(t3)
    sb t2, 0(a0)
    sb t3, 1(a0)
    li a1, 2
    call str_new
    mv a1, a0
    la a0, m_utf8
    li a2, 0
    li a3, 0
    call msg
    mv a3, a0
    la a0, e0104
    mv a1, s2
    addi a2, s2, 1
    call lx_err
1:
    li s2, 0                    # pos
10:
    bge s2, s1, 900f
    add t0, s0, s2
    lbu s3, 0(t0)               # current byte
    li t1, 10
    beq s3, t1, 11f
    li t1, ' '
    beq s3, t1, 12f
    li t1, 9
    beq s3, t1, 12f
    li t1, 13
    beq s3, t1, 12f
    li t1, 35
    beq s3, t1, 20f
    li t1, '"'
    beq s3, t1, 40f
    li t1, '0'
    blt s3, t1, 13f
    li t1, '9'
    ble s3, t1, 30f
13:
    mv a0, s3
    call is_word_char
    bnez a0, 50f
    j 60f
11:                             # newline
    addi s2, s2, 1
    la t0, lx_nl
    li t1, 1
    sd t1, 0(t0)
    la t0, lx_code
    sd zero, 0(t0)
    la t0, lx_lstart
    sd s2, 0(t0)
    j 10b
12:
    addi s2, s2, 1
    j 10b

20:                             # comment
    mv s4, s2
21:
    bge s2, s1, 22f
    add t0, s0, s2
    lbu t0, 0(t0)
    li t1, 10
    beq t0, t1, 22f
    addi s2, s2, 1
    j 21b
22:
    la t0, lx_code
    ld t0, 0(t0)
    bnez t0, 23f
    li a0, T_COMMENT
    mv a1, s4
    mv a2, s2
    call lx_push
    la t0, lx_code              # a comment line does not make the line "code"
    sd zero, 0(t0)
    j 10b
23:
    la a0, m_trailing           # trailing comment: E0101, fix moves it to its own line
    call str_from_cstr
    mv a3, a0
    la a0, e0101
    mv a1, s4
    mv a2, s2
    call lx_err
    mv s5, a0
    call sb_new
    mv s6, a0
    li a1, 10
    call sb_byte
    la t0, lx_lstart
    ld t0, 0(t0)
    sd t0, 0(sp)
24:
    ld t0, 0(sp)
    add t1, s0, t0
    lbu a1, 0(t1)
    li t2, ' '
    beq a1, t2, 25f
    li t2, 9
    bne a1, t2, 26f
25:
    mv a0, s6
    call sb_byte
    ld t0, 0(sp)
    addi t0, t0, 1
    sd t0, 0(sp)
    j 24b
26:
    mv a0, s6
    call sb_finish
    mv a4, a0
    mv a0, s5
    la t0, lx_file
    ld a1, 0(t0)
    mv a2, s4
    mv a3, s4
    call diag_add_fix
    j 10b

30:                             # number
    mv s4, s2
31:
    bge s2, s1, 32f
    add t0, s0, s2
    lbu t0, 0(t0)
    li t1, '0'
    blt t0, t1, 32f
    li t1, '9'
    bgt t0, t1, 32f
    addi s2, s2, 1
    j 31b
32:
    addi t0, s2, 1              # '.' followed by a digit: float
    bge t0, s1, 35f
    add t1, s0, s2
    lbu t2, 0(t1)
    li t3, '.'
    bne t2, t3, 35f
    lbu t2, 1(t1)
    li t3, '0'
    blt t2, t3, 35f
    li t3, '9'
    bgt t2, t3, 35f
    addi s2, s2, 1
33:
    bge s2, s1, 34f
    add t0, s0, s2
    lbu t0, 0(t0)
    li t1, '0'
    blt t0, t1, 34f
    li t1, '9'
    bgt t0, t1, 34f
    addi s2, s2, 1
    j 33b
34:
    mv a0, s4
    mv a1, s2
    call lx_number_suffix
    li a0, T_FLOAT
    mv a1, s4
    mv a2, s2
    call lx_push
    mv t0, s4                   # significant digits: first through last non-zero digit
    li t2, 0                    # started?
    li t3, 0                    # count since the first non-zero digit
    li t5, 0                    # count through the last non-zero digit
341:
    bge t0, s2, 344f
    add t1, s0, t0
    lbu t1, 0(t1)
    addi t0, t0, 1
    li t4, '.'
    beq t1, t4, 341b
    bnez t2, 342f
    li t4, '0'
    beq t1, t4, 341b
    li t2, 1
342:
    addi t3, t3, 1
    li t4, '0'
    beq t1, t4, 341b
    mv t5, t3
    j 341b
344:
    mv t3, t5
    li t4, 15
    ble t3, t4, 39f
    la a0, m_float_long
    call str_from_cstr
    mv a3, a0
    la a0, e0105
    mv a1, s4
    mv a2, s2
    call lx_err
    j 39f
35:
    mv a0, s4
    mv a1, s2
    call lx_number_suffix
    li a0, T_INT
    mv a1, s4
    mv a2, s2
    call lx_push
    add a0, s0, s4
    sub a1, s2, s4
    addi a2, sp, 0
    call dec_to_int
    beqz a0, 39f
    la a0, m_int_big
    call str_from_cstr
    mv a3, a0
    la a0, e0105
    mv a1, s4
    mv a2, s2
    call lx_err
    j 39f

39:                             # after a number: resume after a skipped suffix (1e3)
    la t0, lx_skip_to
    ld t1, 0(t0)
    beqz t1, 10b
    mv s2, t1
    sd zero, 0(t0)
    j 10b

40:                             # string
    mv s4, s2
    addi s2, s2, 1
41:
    bge s2, s1, 48f
    add t0, s0, s2
    lbu t1, 0(t0)
    li t2, 10
    beq t1, t2, 48f
    li t2, '"'
    beq t1, t2, 47f
    li t2, '\\'
    beq t1, t2, 42f
    addi s2, s2, 1
    j 41b
42:
    addi t3, s2, 1
    bge t3, s1, 48f
    lbu t1, 1(t0)
    li t2, 10
    beq t1, t2, 48f
    li t2, 'n'
    beq t1, t2, 43f
    li t2, 't'
    beq t1, t2, 43f
    li t2, '\\'
    beq t1, t2, 43f
    li t2, '"'
    beq t1, t2, 43f
    mv a0, s2                   # unknown escape: report \x (whole character)
    addi a1, s2, 1
    add a0, s0, a1
    sub a1, s1, a1
    call utf8_seq_len
    bnez a0, 44f
    li a0, 1
44:
    addi s5, a0, 1
    add s5, s5, s2              # end of the escape
    mv a0, s2
    mv a1, s5
    call lx_slice
    mv a1, a0
    la a0, m_escape
    li a2, 0
    li a3, 0
    call msg
    mv a3, a0
    la a0, e0103
    mv a1, s2
    mv a2, s5
    call lx_err
    mv s2, s5
    j 41b
43:
    addi s2, s2, 2
    j 41b
47:
    addi s2, s2, 1
    li a0, T_STRING
    mv a1, s4
    mv a2, s2
    call lx_push
    j 10b
48:                             # unterminated: E0102, fix inserts the closing quote
    li a0, T_STRING
    mv a1, s4
    mv a2, s2
    call lx_push
    la a0, m_unterminated
    call str_from_cstr
    mv a3, a0
    la a0, e0102
    mv a1, s4
    mv a2, s2
    call lx_err
    mv s5, a0
    la a0, s_quote
    call str_from_cstr
    mv a4, a0
    mv a0, s5
    la t0, lx_file
    ld a1, 0(t0)
    mv a2, s2
    mv a3, s2
    call diag_add_fix
    j 10b

50:                             # word: identifier, type name, keyword or _
    mv s4, s2
51:
    bge s2, s1, 52f
    add t0, s0, s2
    lbu a0, 0(t0)
    call is_word_char
    beqz a0, 52f
    addi s2, s2, 1
    j 51b
52:
    sub t0, s2, s4
    li t1, 1
    bne t0, t1, 53f
    li t1, '_'
    bne s3, t1, 53f
    li a0, T_UNDERSCORE
    j 55f
53:
    li t1, 'A'
    blt s3, t1, 54f
    li t1, 'Z'
    bgt s3, t1, 54f
    li a0, T_TYPE_IDENT
    j 55f
54:
    add a0, s0, s4
    sub a1, s2, s4
    call keyword_kind
55:
    mv a1, s4
    mv a2, s2
    call lx_push
    j 10b

60:                             # punctuation
    addi t0, s2, 1
    li s5, 0                    # second byte (0 at end of file)
    bge t0, s1, 61f
    add t1, s0, s2
    lbu s5, 1(t1)
61:
    li s6, 1                    # token length
    li t1, '('
    li a0, T_LPAREN
    beq s3, t1, 69f
    li t1, ')'
    li a0, T_RPAREN
    beq s3, t1, 69f
    li t1, '{'
    li a0, T_LBRACE
    beq s3, t1, 69f
    li t1, '}'
    li a0, T_RBRACE
    beq s3, t1, 69f
    li t1, '['
    li a0, T_LBRACKET
    beq s3, t1, 69f
    li t1, ']'
    li a0, T_RBRACKET
    beq s3, t1, 69f
    li t1, ','
    li a0, T_COMMA
    beq s3, t1, 69f
    li t1, 59
    li a0, T_SEMI
    beq s3, t1, 69f
    li t1, ':'
    li a0, T_COLON
    beq s3, t1, 69f
    li t1, '+'
    li a0, T_PLUS
    beq s3, t1, 69f
    li t1, '*'
    li a0, T_STAR
    beq s3, t1, 69f
    li t1, '/'
    li a0, T_SLASH
    beq s3, t1, 69f
    li t1, '%'
    li a0, T_PERCENT
    beq s3, t1, 69f
    li t1, '.'
    bne s3, t1, 62f
    li a0, T_DOT
    li t1, '.'
    bne s5, t1, 69f
    li a0, T_DOTDOT
    li s6, 2
    j 69f
62:
    li t1, '-'
    bne s3, t1, 63f
    li a0, T_MINUS
    li t1, '>'
    bne s5, t1, 69f
    li a0, T_ARROW
    li s6, 2
    j 69f
63:
    li t1, '='
    bne s3, t1, 64f
    li a0, T_ASSIGN
    li t1, '='
    bne s5, t1, 69f
    li a0, T_EQ
    li s6, 2
    j 69f
64:
    li t1, '<'
    bne s3, t1, 65f
    li a0, T_LT
    li t1, '='
    bne s5, t1, 69f
    li a0, T_LE
    li s6, 2
    j 69f
65:
    li t1, '>'
    bne s3, t1, 66f
    li a0, T_GT
    li t1, '='
    bne s5, t1, 69f
    li a0, T_GE
    li s6, 2
    j 69f
66:
    li t1, '!'
    bne s3, t1, 67f
    li t1, '='
    bne s5, t1, 671f
    li a0, T_NE
    li s6, 2
    j 69f
671:
    mv a0, s2                   # `!x`: vo writes `not`
    li a1, 1
    la a2, w_not
    li a3, T_NOT
    call lx_carry
    mv s2, a0
    j 10b
67:
    li t1, '&'
    bne s3, t1, 68f
    bne s5, t1, 70f
    mv a0, s2                   # `&&`: vo writes `and`
    li a1, 2
    la a2, w_and
    li a3, T_AND
    call lx_carry
    mv s2, a0
    j 10b
68:
    li t1, '|'
    bne s3, t1, 681f
    bne s5, t1, 70f
    mv a0, s2                   # `||`: vo writes `or`
    li a1, 2
    la a2, w_or
    li a3, T_OR
    call lx_carry
    mv s2, a0
    j 10b
681:
    li t1, 39                   # single quote: strings use double quotes
    bne s3, t1, 682f
    mv a0, s2
    call lx_single_quote
    beqz a0, 70f
    mv s2, a0
    j 10b
682:
    li t1, '?'
    bne s3, t1, 70f
    la a0, m_ternary
    call str_from_cstr
    mv a3, a0
    la a0, e0101
    mv a1, s2
    addi a2, s2, 1
    call lx_err
    addi s2, s2, 1
    j 10b
69:
    mv a1, s2
    add s2, s2, s6
    mv a2, s2
    call lx_push
    j 10b
70:                             # anything else: E0101 with a removal fix
    add a0, s0, s2
    sub a1, s1, s2
    call utf8_seq_len
    bnez a0, 71f
    li a0, 1
71:
    add s5, s2, a0
    mv a0, s2
    mv a1, s5
    call lx_slice
    mv a1, a0
    la a0, m_unexpected
    li a2, 0
    li a3, 0
    call msg
    mv a3, a0
    la a0, e0101
    mv a1, s2
    mv a2, s5
    call lx_err
    mv s6, a0
    li a0, 0
    li a1, 0
    call str_new
    mv a4, a0
    mv a0, s6
    la t0, lx_file
    ld a1, 0(t0)
    mv a2, s2
    mv a3, s5
    call diag_add_fix
    mv s2, s5
    j 10b

900:
    li a0, T_EOF
    mv a1, s1
    mv a2, s1
    call lx_push
    la t0, lx_toks
    ld a0, 0(t0)
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

# void lx_number_suffix(long start, long end)
# Letters right after a number (1e3, 10px): E0105 over the whole run; the letters are skipped,
# so lexing goes on after them.
    .globl lx_number_suffix
lx_number_suffix:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    mv s1, a1
    la t0, lx_len
    ld t0, 0(t0)
    bge s1, t0, 9f
    la t1, lx_text
    ld t1, 0(t1)
    add t2, t1, s1
    lbu a0, 0(t2)
    call is_word_char
    beqz a0, 9f
1:
    la t0, lx_len               # find the end of the run
    ld t0, 0(t0)
    bge s1, t0, 2f
    la t1, lx_text
    ld t1, 0(t1)
    add t2, t1, s1
    lbu a0, 0(t2)
    call is_word_char
    beqz a0, 2f
    addi s1, s1, 1
    j 1b
2:
    la a0, m_suffix
    call str_from_cstr
    mv a3, a0
    la a0, e0105
    mv a1, s0
    mv a2, s1
    call lx_err
    la t0, lx_skip_to           # tell the caller where lexing resumes
    sd s1, 0(t0)
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# long lx_carry(long pos, long len, char *word, int kind)
# A habit from other languages (&&, ||, !): E0101 with a fix replacing it by the vo keyword
# (spaced so it stays a separate word); lexing goes on as if the keyword had been written.
# Returns the position after the operator.
    .globl lx_carry
lx_carry:
    addi sp, sp, -64
    sd ra, 56(sp)
    sd s0, 48(sp)
    sd s1, 40(sp)
    sd s2, 32(sp)
    sd s3, 24(sp)
    sd s4, 16(sp)
    mv s0, a0
    add s1, a0, a1              # end
    mv s2, a2
    mv s3, a3
    mv a0, s0
    mv a1, s1
    call lx_slice
    sd a0, 0(sp)                # the operator text
    mv a0, s2
    call str_from_cstr
    mv a1, a0
    ld a2, 0(sp)
    la a0, m_carry
    li a3, 0
    call msg
    mv a3, a0
    la a0, e0101
    mv a1, s0
    mv a2, s1
    call lx_err
    mv s4, a0
    call sb_new                 # replacement, with spaces where a word would touch
    sd a0, 8(sp)
    la t0, lx_text
    ld t0, 0(t0)
    beqz s0, 1f
    add t1, t0, s0
    lbu t1, -1(t1)
    li t2, ' '
    beq t1, t2, 1f
    li a1, ' '
    call sb_byte
1:
    ld a0, 8(sp)
    mv a1, s2
    call sb_cstr
    la t0, lx_len
    ld t0, 0(t0)
    bge s1, t0, 2f
    la t1, lx_text
    ld t1, 0(t1)
    add t1, t1, s1
    lbu t1, 0(t1)
    li t2, ' '
    beq t1, t2, 2f
    ld a0, 8(sp)
    li a1, ' '
    call sb_byte
2:
    ld a0, 8(sp)
    call sb_finish
    mv a4, a0
    mv a0, s4
    la t0, lx_file
    ld a1, 0(t0)
    mv a2, s0
    mv a3, s1
    call diag_add_fix
    mv a0, s3
    mv a1, s0
    mv a2, s1
    call lx_push
    mv a0, s1
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    addi sp, sp, 64
    ret

# long lx_single_quote(long pos)
# 'text' on one line: E0101 with a fix using double quotes, lexed as a string. Returns the
# position after the closing quote, or 0 when there is none on the line.
    .globl lx_single_quote
lx_single_quote:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    mv s0, a0
    la t0, lx_text
    ld t0, 0(t0)
    la t1, lx_len
    ld t1, 0(t1)
    addi s1, s0, 1
1:
    bge s1, t1, 8f
    add t2, t0, s1
    lbu t2, 0(t2)
    li t3, 10
    beq t2, t3, 8f
    li t3, 39
    beq t2, t3, 2f
    addi s1, s1, 1
    j 1b
2:
    addi s1, s1, 1              # end, after the closing quote
    la a0, m_single
    call str_from_cstr
    mv a3, a0
    la a0, e0101
    mv a1, s0
    mv a2, s1
    call lx_err
    mv s2, a0
    call sb_new
    sd a0, 0(sp)
    la a1, s_dquote
    call sb_cstr
    addi a0, s0, 1
    addi a1, s1, -1
    call lx_slice
    mv a1, a0
    ld a0, 0(sp)
    call sb_str
    ld a0, 0(sp)
    la a1, s_dquote
    call sb_cstr
    ld a0, 0(sp)
    call sb_finish
    mv a4, a0
    mv a0, s2
    la t0, lx_file
    ld a1, 0(t0)
    mv a2, s0
    mv a3, s1
    call diag_add_fix
    li a0, T_STRING
    mv a1, s0
    mv a2, s1
    call lx_push
    mv a0, s1
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
