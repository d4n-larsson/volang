# Queries on a checked program (spec 006 US1): `vo query callers|refs|type-at` and `vo impact`.
# The checker logs every resolved name while q_collect is 1 (research R1); this file resolves
# targets (R2), answers from the log (R3, R4) and reads unified diffs for impact (R5).
    .include "defs.inc"

    .section .data
    .align 3
    .globl q_collect
q_collect:  .dword 0            # 1 while query/impact/edit check a program
    .globl q_log
q_log:      .dword 0            # vec of RL_* entries
    .globl ck_item
ck_item:    .dword 0            # the function/test/property being checked, or 0
    .globl q_exprs
q_exprs:    .dword 0            # vec of every expression node checked while q_collect is 1 (type-at)
df_plus:    .dword 0            # vo impact: keys file << 32 | line of added lines
df_minus:   .dword 0            # places (file << 32 | new-side line) where lines were removed
df_removed: .dword 0            # {file, name str} of removed `fn NAME(` lines
df_ignored: .dword 0            # diff paths that are not program files (strs)
im_list:    .dword 0            # impact entries {item or name str, role cstr, via}

    .section .rodata
    .align 3
role_names: .dword r_decl, r_call, r_value, r_type, r_import, r_use
r_decl:     .string "declaration"
r_call:     .string "call"
r_value:    .string "value"
r_type:     .string "type"
r_import:   .string "import"
r_use:      .string "use"
code_t4:    .string "T0004"
code_t5:    .string "T0005"
n_callers:  .string "callers"
n_refs:     .string "refs"
n_type_at:  .string "type-at"
m_query_none: .string "`query` needs a question: `vo query callers|refs|type-at ENTRY TARGET`."
m_query_sub: .string "Unknown query `$1`. Queries: callers, refs, type-at."
m_query_args: .string "`query $1` takes the entry file of the program and one target: `vo query $1 main.vo TARGET`."
m_type_at_pos: .string "`type-at` needs a position FILE:LINE:COL, not `$1`."
m_no_file:  .string "No file `$1` in this program (a program is its entry file and the files it imports)."
m_outside:  .string "`$1` is outside its file."
m_nothing_at: .string "Nothing to look up at `$1`: point at a name."
m_no_type:  .string "No record or variant named `$1` in this program."
m_no_member: .string "No field or case `$1` in this program."
m_no_name:  .string "Nothing named `$1` in this program."
m_did_you_mean: .string "$1 Did you mean `$2`?"
m_ambiguous: .string "`$1` is declared in several files; name one with FILE:LINE:COL."
m_not_function: .string "`$1` is not a function; `callers` takes a function or builtin name."
j_ref_head: .string "{\"v\":1,\"kind\":\"ref\",\"target\":"
j_role:     .string ",\"role\":\""
j_quote_comma: .string "\",\"file\":"
j_text:     .string ",\"text\":"
j_in:       .string ",\"in\":"
j_close:    .string "}\n"
j_results:  .string ",\"results\":"
h_in:       .string " in "
h_colon:    .string ": "
h_reference: .string " reference\n"
h_references: .string " references\n"
h_note_errors: .string "note: the program has "
h_errors_tail: .string " errors; answers cover the parts that check\n"
h_error_tail: .string " error; answers cover the parts that check\n"
h_declared_at: .string " (declared at "
code_t7:    .string "T0007"
code_t2q:   .string "T0002"
s_dev_null: .string "/dev/null"
s_dev_stdin: .string "/dev/stdin"
s_plus3:    .string "+++ "
s_at2:      .string "@@ "
m_impact_args: .string "`impact` takes the entry file of the program: `vo impact main.vo --diff FILE`."
m_cannot_read_diff: .string "Cannot read the diff `$1`."
m_df_mismatch: .string "The diff does not match `$1` as it is now (line $2): make it again from the current files."
m_df_hunk:  .string "The diff is not a valid unified diff (a hunk is malformed or cut short)."
m_df_not:   .string "This is not a unified diff: write one with `git diff` or `diff -u`."
r_changed:  .string "changed"
r_added:    .string "added"
r_removed:  .string "removed"
r_affected: .string "affected"
r_test:     .string "test"
r_ignored:  .string "ignored"
j_impact_head: .string "{\"v\":1,\"kind\":\"impact\",\"name\":"
j_file:     .string ",\"file\":"
j_start:    .string ",\"start\":"
j_via:      .string ",\"via\":"
j_im_changed: .string ",\"changed\":"
j_im_added: .string ",\"added\":"
j_im_removed: .string ",\"removed\":"
j_im_affected: .string ",\"affected\":"
j_im_tests: .string ",\"tests\":"
h_via:      .string " via "
h_open:     .string " ("
h_empty:    .string ""
h_changed_sep: .string " changed, "
h_added_sep: .string " added, "
h_removed_sep: .string " removed, "
h_affected_sep: .string " affected, "
h_test_end: .string " test\n"
h_tests_end: .string " tests\n"
m_nothing_here: .string "Nothing to look up at this position: point at a name or an expression."
j_type_at_head: .string "{\"v\":1,\"kind\":\"type_at\",\"file\":"
j_type:     .string ",\"type\":"
j_name:     .string ",\"name\":"
j_declaration: .string ",\"declaration\":{\"file\":"
s_unknown_type: .string "?"

    .section .data
    .align 3

    .text
# void *ck_item_set(void *item)
# Sets ck_item to item when it is a function, test or property (else 0); returns item.
    .globl ck_item_set
ck_item_set:
    la t2, ck_item
    sd zero, 0(t2)
    beqz a0, 1f
    lhu t0, N_KIND(a0)
    li t1, N_FN
    beq t0, t1, 2f
    li t1, N_TEST
    bne t0, t1, 1f
2:
    sd a0, 0(t2)
1:
    ret

# void *q_note(long key_a, long key_b, void *node, long role)
# Appends a reference to the log while q_collect is 1 (research R1); the entry, or 0. Nodes without a span
# (synthetic ones, such as `ret` in contracts) are not references.
    .globl q_note
q_note:
    la t0, q_collect
    ld t0, 0(t0)
    beqz t0, 9f
    beqz a2, 9f
    lwu t0, N_START(a2)
    lwu t1, N_END(a2)
    bgeu t0, t1, 9f
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd a0, 0(sp)
    sd a1, 8(sp)
    sd a2, 16(sp)
    sd a3, 24(sp)
    li a0, RL_SIZE
    call alloc
    mv s0, a0
    ld t0, 0(sp)
    sd t0, RL_KEY_A(s0)
    ld t0, 8(sp)
    sd t0, RL_KEY_B(s0)
    ld t0, 16(sp)
    sd t0, RL_NODE(s0)
    ld t0, 24(sp)
    sd t0, RL_ROLE(s0)
    la t0, ck_item
    ld t0, 0(t0)
    sd t0, RL_ITEM(s0)
    la t0, q_log
    ld a0, 0(t0)
    bnez a0, 1f
    call vec_new
    la t0, q_log
    sd a0, 0(t0)
1:
    mv a1, s0
    call vec_push
    mv a0, s0
    ld ra, 40(sp)
    ld s0, 32(sp)
    addi sp, sp, 48
    ret
9:
    li a0, 0
    ret

# void q_note_sym(void *sym, void *node, long role)
# A reference to a symbol: items by their declaration, locals and type parameters by their
# declaring name node. Placeholders (undefined names) are skipped.
    .globl q_note_sym
q_note_sym:
    la t0, q_collect
    ld t0, 0(t0)
    beqz t0, 9f
    ld t0, SY_NODE(a0)
    beqz t0, 9f
    ld t1, SY_KIND(a0)
    li t2, SYM_LOCAL
    beq t1, t2, 1f
    li t2, SYM_TPARAM
    beq t1, t2, 1f
    ld t0, SY_DECL(a0)
    beqz t0, 9f
1:
    mv a3, a2
    mv a2, a1
    li a1, -1
    mv a0, t0
    tail q_note
9:
    ret

# void q_note_decl(void *sym)
# The declaration of a new symbol: an item's own name, an import's copy (role import), or a
# parameter, local or type parameter.
    .globl q_note_decl
q_note_decl:
    la t0, q_collect
    ld t0, 0(t0)
    beqz t0, 9f
    ld a2, SY_NODE(a0)
    beqz a2, 9f
    li a3, RR_DECL
    ld t1, SY_KIND(a0)
    li t2, SYM_LOCAL
    beq t1, t2, 1f
    li t2, SYM_TPARAM
    beq t1, t2, 1f
    ld t0, SY_DECL(a0)
    beqz t0, 9f
    ld t1, N_A(t0)
    beq t1, a2, 2f
    li a3, RR_IMPORT
2:
    mv t3, t0
    j 3f
1:
    mv t3, a2
3:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    mv s0, a0
    mv a0, t3
    li a1, -1
    call q_note
    beqz a0, 4f
    ld t0, SY_TYPE(s0)
    sd t0, RL_TYPE(a0)
4:
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
9:
    ret

# ---- loading a program for queries ----

# long q_load(char *entry)
# Loads and checks the program with the reference log on, adds the declarations of record fields
# and variant cases, and returns the entry's file id. A syntax error that stops checking prints
# the diagnostics and exits 65 (research R6).
    .globl q_load
q_load:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    call str_from_cstr
    call load_source
    mv s0, a0
    la t0, q_collect
    li t1, 1
    sd t1, 0(t0)
    mv a0, s0
    li a1, 0
    call check_program
    la t0, ld_hard
    ld t0, 0(t0)
    beqz t0, 1f
    li a0, 1
    call finish_check
    call vo_exit
1:
    call q_members_all
6:
    la t0, q_collect
    sd zero, 0(t0)
    mv a0, s0
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# void q_members_all(void)
# Declaration entries for the fields and cases of every record and variant of the program.
    .globl q_members_all
q_members_all:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    la t0, ck_item
    sd zero, 0(t0)
    call ld_files
    mv s1, a0
    li s2, 0
2:
    ld t0, VEC_N(s1)
    bge s2, t0, 9f
    mv a0, s1
    mv a1, s2
    call vec_get
    call file_get
    ld t0, FILE_AST(a0)
    ld s3, N_A(t0)              # items
    addi s2, s2, 1
    li s0, 0
3:
    ld t0, VEC_N(s3)
    bge s0, t0, 2b
    mv a0, s3
    mv a1, s0
    call vec_get
    addi s0, s0, 1
    lhu t0, N_KIND(a0)
    li t1, N_RECORD
    beq t0, t1, 4f
    li t1, N_VARIANT
    bne t0, t1, 3b
4:
    call q_members
    j 3b
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# void q_members(void *decl)
# Declaration entries (decl, index) for each field or case, with its type (0 for a case
# without a value).
    .globl q_members
q_members:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    sd s2, 0(sp)
    mv s0, a0
    li s1, 0
1:
    ld a0, N_B(s0)
    mv a1, s1
    call member_at
    beqz a0, 9f
    mv s2, a0
    mv a0, s0
    mv a1, s1
    ld a2, N_A(s2)
    li a3, RR_DECL
    call q_note
    beqz a0, 2f
    ld t0, N_B(s2)
    beqz t0, 2f
    ld t0, N_TYPE(t0)
    sd t0, RL_TYPE(a0)
2:
    addi s1, s1, 1
    j 1b
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    ld s2, 0(sp)
    addi sp, sp, 32
    ret

# ---- targets (research R2) ----

# void q_fail(void *message)
# T0004 unknown_target, exit 64.
    .globl q_fail
q_fail:
    la a1, code_t4
    tail q_fail_code

# void q_fail_code(void *message, char *code)
    .globl q_fail_code
q_fail_code:
    addi sp, sp, -16
    sd ra, 8(sp)
    mv a3, a0
    mv a2, a1
    la t0, opt_json
    ld a0, 0(t0)
    la t0, report_fd
    ld a1, 0(t0)
    li a4, 0
    call toolchain_error
    li a0, EXIT_USAGE
    call vo_exit
    ld ra, 8(sp)
    addi sp, sp, 16
    ret

# long q_program_file(void *path)
# The id of the program file with this path: equal, or one path a suffix of the other at a `/`.
# -1 when none matches.
    .globl q_program_file
q_program_file:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    call ld_files
    mv s1, a0
    li s2, 0
1:
    ld t0, VEC_N(s1)
    bge s2, t0, 8f
    mv a0, s1
    mv a1, s2
    call vec_get
    mv s3, a0
    call file_get
    ld a0, FILE_PATH(a0)
    mv a1, s0
    call path_suffix_match
    addi s2, s2, 1
    beqz a0, 1b
    mv a0, s3
    j 9f
8:
    li a0, -1
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# int path_suffix_match(void *a, void *b)
# 1 when the strs are equal, or the shorter is a suffix of the longer that starts after a `/`.
# ponytail: paths are matched by text (no realpath); `./a.vo` and `a.vo` match through the suffix rule.
    .globl path_suffix_match
path_suffix_match:
    ld t0, STR_LEN(a0)
    ld t1, STR_LEN(a1)
    bge t0, t1, 1f
    mv t2, a0                   # make a the longer one
    mv a0, a1
    mv a1, t2
    mv t2, t0
    mv t0, t1
    mv t1, t2
1:
    sub t2, t0, t1              # where b would start in a
    addi t3, a0, STR_DATA
    add t3, t3, t2
    addi t4, a1, STR_DATA
    mv t5, t1
2:
    beqz t5, 3f
    lbu t6, 0(t3)
    lbu a2, 0(t4)
    bne t6, a2, 8f
    addi t3, t3, 1
    addi t4, t4, 1
    addi t5, t5, -1
    j 2b
3:
    beqz t2, 7f                 # equal lengths: equal
    beqz t1, 8f
    addi t3, a0, STR_DATA
    add t3, t3, t2
    lbu t6, -1(t3)
    li a2, '/'
    bne t6, a2, 8f
7:
    li a0, 1
    ret
8:
    li a0, 0
    ret

# long q_parse_pos(char *text, long *file_out)
# For FILE:LINE:COL, the byte offset (and the file id in *file_out); -1 when text has another
# form. A file outside the program, or a position outside its file, is T0004.
    .globl q_parse_pos
q_parse_pos:
    addi sp, sp, -64
    sd ra, 56(sp)
    sd s0, 48(sp)
    sd s1, 40(sp)
    sd s2, 32(sp)
    sd s3, 24(sp)
    sd s4, 16(sp)
    mv s0, a0
    mv s4, a1
    call cstr_len
    mv s1, a0
    mv a0, s0                   # COL: digits after the last ':'
    mv a1, s1
    addi a2, sp, 8
    call q_tail_number
    bltz a0, 8f
    mv s3, a0                   # column
    ld s1, 8(sp)                # length before ':'
    mv a0, s0
    mv a1, s1
    addi a2, sp, 8
    call q_tail_number
    bltz a0, 8f
    mv s2, a0                   # line
    ld s1, 8(sp)
    beqz s1, 8f
    mv a0, s0
    mv a1, s1
    call str_new
    sd a0, 0(sp)
    call q_program_file
    bgez a0, 1f
    ld a1, 0(sp)
    la a0, m_no_file
    li a2, 0
    li a3, 0
    call msg
    call q_fail
1:
    sd a0, 0(s4)
    mv a1, s2
    mv a2, s3
    call pos_offset
    bgez a0, 9f
    mv a0, s0
    call str_from_cstr
    mv a1, a0
    la a0, m_outside
    li a2, 0
    li a3, 0
    call msg
    call q_fail
8:
    li a0, -1
9:
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    addi sp, sp, 64
    ret

# long q_tail_number(char *text, long n, long *before)
# The decimal number after the last ':' in text[0..n); -1 when there is none. *before gets the
# length of the text before that ':'.
    .globl q_tail_number
q_tail_number:
    mv t0, a1                   # scan back over digits
1:
    beqz t0, 8f
    add t1, a0, t0
    lbu t1, -1(t1)
    li t2, '0'
    blt t1, t2, 2f
    li t2, '9'
    bgt t1, t2, 2f
    addi t0, t0, -1
    j 1b
2:
    beq t0, a1, 8f              # no digits
    li t2, ':'
    bne t1, t2, 8f
    li t3, 0                    # value of text[t0..n)
    mv t4, t0
3:
    bge t4, a1, 4f
    add t5, a0, t4
    lbu t5, 0(t5)
    addi t5, t5, -48
    li t6, 10
    mul t3, t3, t6
    add t3, t3, t5
    li t6, 1000000000
    bgt t3, t6, 8f
    addi t4, t4, 1
    j 3b
4:
    addi t0, t0, -1
    sd t0, 0(a2)
    mv a0, t3
    ret
8:
    li a0, -1
    ret

# void *q_entry_at(long file, long off)
# The innermost logged reference or declaration whose span contains off, or 0.
    .globl q_entry_at
q_entry_at:
    la t0, q_log
    ld t0, 0(t0)
    li a2, 0                    # best
    li a3, -1                   # its length (unsigned: anything is smaller)
    beqz t0, 9f
    ld t1, VEC_N(t0)
    ld t2, VEC_DATA(t0)
1:
    beqz t1, 9f
    ld t3, 0(t2)
    addi t2, t2, 8
    addi t1, t1, -1
    ld t4, RL_NODE(t3)
    lwu t5, N_FILE(t4)
    bne t5, a0, 1b
    lwu t5, N_START(t4)
    lwu t6, N_END(t4)
    bgt t5, a1, 1b
    bge a1, t6, 1b
    sub t6, t6, t5
    bgeu t6, a3, 1b
    mv a3, t6
    mv a2, t3
    j 1b
9:
    mv a0, a2
    ret

# void *q_item_named(void *name, long kinds) -> declaration node, or 0
# Every top-level item declaration in the log with this name, of a kind in kinds (bit 1 << N_KIND).
# Exits with T0005 when several match.
    .globl q_item_named
q_item_named:
    addi sp, sp, -64
    sd ra, 56(sp)
    sd s0, 48(sp)
    sd s1, 40(sp)
    sd s2, 32(sp)
    sd s3, 24(sp)
    sd s4, 16(sp)
    mv s0, a0
    mv s4, a1
    li s1, 0                    # first match
    li s2, 0                    # vec of all matches (when more than one)
    li s3, 0
1:
    la t0, q_log
    ld t0, 0(t0)
    beqz t0, 8f
    ld t1, VEC_N(t0)
    bge s3, t1, 8f
    mv a0, t0
    mv a1, s3
    call vec_get
    addi s3, s3, 1
    ld t0, RL_ROLE(a0)
    bnez t0, 1b                 # RR_DECL
    ld t0, RL_KEY_B(a0)
    li t1, -1
    bne t0, t1, 1b
    ld t2, RL_KEY_A(a0)
    ld t3, RL_NODE(a0)
    ld t4, N_A(t2)              # an item: its own name node
    bne t4, t3, 1b
    lhu t4, N_KIND(t2)
    li t5, 1
    sll t5, t5, t4
    and t5, t5, s4
    beqz t5, 1b
    sd t2, 0(sp)
    ld a0, N_A(t3)
    mv a1, s0
    call str_eq
    beqz a0, 1b
    ld t2, 0(sp)
    beq t2, s1, 1b
    bnez s1, 2f
    mv s1, t2
    j 1b
2:
    bnez s2, 3f
    call vec_new
    mv s2, a0
    mv a1, s1
    call vec_push
3:
    mv a0, s2
    ld a1, 0(sp)
    call vec_push
    j 1b
8:
    beqz s2, 9f
    mv a0, s0                   # several: T0005 with each declaration in related
    mv a1, s2
    call q_ambiguous
9:
    mv a0, s1
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    addi sp, sp, 64
    ret

# void q_ambiguous(void *name, void *decls)
    .globl q_ambiguous
q_ambiguous:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    mv s0, a1
    mv a1, a0
    la a0, m_ambiguous
    li a2, 0
    li a3, 0
    call msg
    la t0, te_related
    sd s0, 0(t0)
    la a1, code_t5
    call q_fail_code
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret

# void *q_suggest(void *name, long kinds, void *members)
# The closest item name (members == 0) or member name to name within edit distance 2, or 0.
    .globl q_suggest
q_suggest:
    addi sp, sp, -64
    sd ra, 56(sp)
    sd s0, 48(sp)
    sd s1, 40(sp)
    sd s2, 32(sp)
    sd s3, 24(sp)
    sd s4, 16(sp)
    sd s5, 8(sp)
    mv s0, a0
    mv s4, a1
    mv s5, a2
    li s1, 0                    # best name
    li s2, 3                    # its distance
    li s3, 0
    beqz s5, 1f
11:                             # members
    mv a0, s5
    mv a1, s3
    call member_at
    beqz a0, 8f
    addi s3, s3, 1
    ld t0, N_A(a0)
    ld a0, N_A(t0)
    sd a0, 0(sp)
    mv a1, s0
    call name_distance
    beqz a0, 11b                # the same name is no suggestion
    bge a0, s2, 11b
    mv s2, a0
    ld s1, 0(sp)
    j 11b
1:
    la t0, q_log
    ld t0, 0(t0)
    beqz t0, 8f
    ld t1, VEC_N(t0)
    bge s3, t1, 8f
    mv a0, t0
    mv a1, s3
    call vec_get
    addi s3, s3, 1
    ld t0, RL_ROLE(a0)
    bnez t0, 1b
    ld t0, RL_KEY_B(a0)
    li t1, -1
    bne t0, t1, 1b
    ld t2, RL_KEY_A(a0)
    ld t3, RL_NODE(a0)
    ld t4, N_A(t2)
    bne t4, t3, 1b
    lhu t4, N_KIND(t2)
    li t5, 1
    sll t5, t5, t4
    and t5, t5, s4
    beqz t5, 1b
    ld a0, N_A(t3)
    sd a0, 0(sp)
    mv a1, s0
    call name_distance
    beqz a0, 1b
    bge a0, s2, 1b
    mv s2, a0
    ld s1, 0(sp)
    j 1b
8:
    mv a0, s1
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    ld s5, 8(sp)
    addi sp, sp, 64
    ret
# void q_unknown(void *msg_tmpl, void *name, void *suggestion)
# T0004 with "; did you mean `x`?" when there is a suggestion.
    .globl q_unknown
q_unknown:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    mv s0, a2
    li a2, 0
    li a3, 0
    call msg
    beqz s0, 1f
    mv a1, a0
    mv a2, s0
    la a0, m_did_you_mean
    li a3, 0
    call msg
1:
    call q_fail
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret

    .equ QK_ITEMS, (1 << N_FN) | (1 << N_RECORD) | (1 << N_VARIANT)
    .equ QK_TYPES, (1 << N_RECORD) | (1 << N_VARIANT)

# void q_resolve(char *target, long *keys)
# The target of a query or edit (research R2): name, Record.field, Variant.case or FILE:LINE:COL.
# keys[0], keys[1] get its (key_a, key_b).
    .globl q_resolve
q_resolve:
    addi sp, sp, -64
    sd ra, 56(sp)
    sd s0, 48(sp)
    sd s1, 40(sp)
    sd s2, 32(sp)
    sd s3, 24(sp)
    sd s4, 16(sp)
    mv s0, a0
    mv s4, a1
    addi a1, sp, 0
    call q_parse_pos
    bltz a0, 2f
    mv a1, a0                   # a position: the innermost entry there
    ld a0, 0(sp)
    call q_entry_at
    bnez a0, 1f
    mv a0, s0
    call str_from_cstr
    mv a1, a0
    la a0, m_nothing_at
    li a2, 0
    li a3, 0
    call msg
    call q_fail
1:
    ld a1, RL_KEY_B(a0)
    ld a0, RL_KEY_A(a0)
    j 9f
2:
    mv a0, s0                   # Name.member?
    li a1, '.'
    call cstr_index
    bltz a0, 5f
    mv s1, a0
    mv a0, s0
    mv a1, s1
    call str_new
    mv s2, a0                   # type name
    add a0, s0, s1
    addi a0, a0, 1
    call str_from_cstr
    mv s3, a0                   # member name
    mv a0, s2
    li a1, QK_TYPES
    call q_item_named
    bnez a0, 3f
    mv a0, s2
    li a1, QK_TYPES
    li a2, 0
    call q_suggest
    mv a2, a0
    mv a1, s2
    la a0, m_no_type
    call q_unknown
3:
    mv s1, a0                   # the record or variant
    ld a0, N_B(a0)
    mv a1, s3
    call member_index
    bgez a0, 4f
    li a1, 0
    ld a2, N_B(s1)
    mv a0, s3
    call q_suggest
    sd a0, 8(sp)
    mv a0, s0
    call str_from_cstr
    mv a1, a0
    ld a2, 8(sp)
    la a0, m_no_member
    call q_unknown
4:
    mv a1, a0
    mv a0, s1
    j 9f
5:
    mv a0, s0                   # a top-level name, or a builtin
    call str_from_cstr
    mv s2, a0
    li a1, QK_ITEMS
    call q_item_named
    beqz a0, 6f
    li a1, -1
    j 9f
6:
    mv a0, s2
    call builtin_id
    beqz a0, 7f
    mv a1, a0
    li a0, 0
    j 9f
7:
    mv a0, s2
    li a1, QK_ITEMS
    li a2, 0
    call q_suggest
    mv a2, a0
    mv a1, s2
    la a0, m_no_name
    call q_unknown
    li a0, 0                    # (q_unknown does not return)
    li a1, 0
9:
    sd a0, 0(s4)
    sd a1, 8(s4)
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    addi sp, sp, 64
    ret

# long cstr_index(char *s, int c)
# Index of the first c in s, or -1.
    .globl cstr_index
cstr_index:
    mv t0, a0
1:
    lbu t1, 0(t0)
    beqz t1, 8f
    beq t1, a1, 2f
    addi t0, t0, 1
    j 1b
2:
    sub a0, t0, a0
    ret
8:
    li a0, -1
    ret

# ---- answers ----

# void *q_matches(long key_a, long key_b, long calls_only)
# The log entries of a target, without duplicates (same span), sorted by file and position,
# the declaration first.
    .globl q_matches
q_matches:
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
    call vec_new
    mv s2, a0
    li s3, 0
1:
    la t0, q_log
    ld t0, 0(t0)
    beqz t0, 5f
    ld t1, VEC_N(t0)
    bge s3, t1, 5f
    mv a0, t0
    mv a1, s3
    call vec_get
    addi s3, s3, 1
    sd a0, 0(sp)
    ld a2, RL_KEY_A(a0)
    ld a3, RL_KEY_B(a0)
    mv a0, s0
    mv a1, s1
    call q_same_key
    beqz a0, 1b
    beqz s4, 2f
    ld t0, 0(sp)
    ld t1, RL_ROLE(t0)
    li t2, RR_CALL
    beq t1, t2, 2f
    li t2, RR_VALUE
    bne t1, t2, 1b
2:
    mv a0, s2
    ld a1, 0(sp)
    call q_insert_sorted
    j 1b
5:
    mv a0, s2
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    addi sp, sp, 64
    ret

# int q_same_key(long a1, long a2, long b1, long b2)
# (key_a, key_b) pairs equal; an unresolved name (key_a 0, key_b a str) compares by text.
    .globl q_same_key
q_same_key:
    bne a0, a2, 8f
    beq a1, a3, 7f
    bnez a0, 8f
    li t0, 4096                 # builtin ids are small; names are strs
    bltu a1, t0, 8f
    bltu a3, t0, 8f
    mv a0, a1
    mv a1, a3
    tail str_eq
7:
    li a0, 1
    ret
8:
    li a0, 0
    ret

# void q_insert_sorted(void *vec, void *entry)
# Inserts by (file, start), declarations before other roles at the same place; drops an entry
# whose span is already there.
    .globl q_insert_sorted
q_insert_sorted:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    mv s0, a0
    mv s1, a1
    ld t0, RL_NODE(s1)
    lwu t1, N_FILE(t0)
    slli t1, t1, 32
    lwu t2, N_START(t0)
    or s2, t1, t2               # sort key
    ld t3, VEC_N(s0)
    ld t4, VEC_DATA(s0)
    li t5, 0                    # insertion index
1:
    bge t5, t3, 3f
    slli t6, t5, 3
    add t6, t4, t6
    ld t6, 0(t6)
    ld a2, RL_NODE(t6)
    lwu a3, N_FILE(a2)
    slli a3, a3, 32
    lwu a4, N_START(a2)
    or a3, a3, a4
    bltu s2, a3, 3f
    bne s2, a3, 2f
    lwu a4, N_END(a2)           # same start: same span is a duplicate
    lwu a5, N_END(t0)
    beq a4, a5, 9f
2:
    addi t5, t5, 1
    j 1b
3:
    sd t5, 0(sp)
    mv a0, s0                   # append, then shift into place
    mv a1, s1
    call vec_push
    ld t5, 0(sp)
    ld t3, VEC_N(s0)
    ld t4, VEC_DATA(s0)
    addi t3, t3, -1
4:
    ble t3, t5, 5f
    slli t6, t3, 3
    add t6, t4, t6
    ld a2, -8(t6)
    sd a2, 0(t6)
    addi t3, t3, -1
    j 4b
5:
    slli t6, t5, 3
    add t6, t4, t6
    sd s1, 0(t6)
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    addi sp, sp, 48
    ret

# void q_decl_first(void *vec)
# Moves the first declaration entry to the front.
    .globl q_decl_first
q_decl_first:
    ld t0, VEC_N(a0)
    ld t1, VEC_DATA(a0)
    li t2, 0
1:
    bge t2, t0, 9f
    slli t3, t2, 3
    add t3, t1, t3
    ld t4, 0(t3)
    ld t5, RL_ROLE(t4)
    beqz t5, 2f
    addi t2, t2, 1
    j 1b
2:
    beqz t2, 3f                 # shift the ones before it up, then put it first
    ld a1, -8(t3)
    sd a1, 0(t3)
    addi t3, t3, -8
    addi t2, t2, -1
    j 2b
3:
    sd t4, 0(t1)
9:
    ret

# void q_line_text(void *sb, long file, long off)
# The source line containing off, without leading and trailing spaces.
    .globl q_line_text
q_line_text:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    mv s1, a1
    mv a0, s1
    mv a1, a2
    call pos_line
    mv s3, a0
    mv a0, s1
    mv a1, s3
    call line_start
    mv s2, a0
    mv a0, s1
    mv a1, s3
    call line_end
    mv s3, a0
    mv a0, s1
    call file_get
    ld t0, FILE_TEXT(a0)
    add s2, t0, s2
    add s3, t0, s3
1:
    bge s2, s3, 2f
    lbu t1, 0(s2)
    li t2, ' '
    bne t1, t2, 2f
    addi s2, s2, 1
    j 1b
2:
    ble s3, s2, 3f
    lbu t1, -1(s3)
    li t2, ' '
    beq t1, t2, 21f
    li t2, 13
    bne t1, t2, 3f
21:
    addi s3, s3, -1
    j 2b
3:
    mv a0, s0
    mv a1, s2
    sub a2, s3, s2
    call sb_bytes
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# void q_out(void *sb)
# Writes the builder to the report fd.
    .globl q_out
q_out:
    mv t0, a0
    la t1, report_fd
    ld a0, 0(t1)
    ld a1, SB_PTR(t0)
    ld a2, SB_LEN(t0)
    tail out_bytes

# void q_ref(void *entry, char *target)
# One reference: a `ref` record, or `FILE:LINE:COL ROLE in ITEM: LINE`.
    .globl q_ref
q_ref:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    mv s1, a1
    ld s3, RL_NODE(s0)
    call sb_new
    mv s2, a0
    la t0, opt_json
    ld t0, 0(t0)
    beqz t0, 5f
    la a1, j_ref_head
    call sb_cstr
    mv a0, s1
    call str_from_cstr
    mv a1, a0
    mv a0, s2
    call json_str_into
    mv a0, s2
    la a1, j_role
    call sb_cstr
    ld a0, RL_ROLE(s0)
    call q_role_name
    mv a1, a0
    mv a0, s2
    call sb_cstr
    mv a0, s2
    la a1, j_quote_comma
    call sb_cstr
    mv a0, s2
    lwu a1, N_FILE(s3)
    lwu a2, N_START(s3)
    lwu a3, N_END(s3)
    call json_span
    mv a0, s2
    la a1, j_text
    call sb_cstr
    mv a0, s2
    lwu a1, N_FILE(s3)
    lwu a2, N_START(s3)
    lwu a3, N_END(s3)
    call json_src_text
    ld t0, RL_ITEM(s0)
    beqz t0, 1f
    mv a0, s2
    la a1, j_in
    call sb_cstr
    ld a0, RL_ITEM(s0)
    call fx_item_name
    mv a1, a0
    mv a0, s2
    call json_str_into
1:
    mv a0, s2
    la a1, j_close
    call sb_cstr
    j 8f
5:
    lwu a1, N_FILE(s3)
    lwu a2, N_START(s3)
    call human_loc
    mv a0, s2
    li a1, ' '
    call sb_byte
    ld a0, RL_ROLE(s0)
    call q_role_name
    mv a1, a0
    mv a0, s2
    call sb_cstr
    ld t0, RL_ITEM(s0)
    beqz t0, 6f
    mv a0, s2
    la a1, h_in
    call sb_cstr
    ld a0, RL_ITEM(s0)
    call fx_item_name
    mv a1, a0
    mv a0, s2
    call sb_str
6:
    mv a0, s2
    la a1, h_colon
    call sb_cstr
    mv a0, s2
    lwu a1, N_FILE(s3)
    lwu a2, N_START(s3)
    call q_line_text
    mv a0, s2
    li a1, 10
    call sb_byte
8:
    mv a0, s2
    call q_out
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# char *q_role_name(long role)
    .globl q_role_name
q_role_name:
    la t0, role_names
    slli a0, a0, 3
    add t0, t0, a0
    ld a0, 0(t0)
    ret

# int q_finish(long results, char *noun1, char *noun_n)
# The closing lines: `N noun` and the note about check errors, or the JSON summary. Returns 0.
    .globl q_finish
q_finish:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    mv s1, a1
    mv s2, a2
    call sb_new
    mv s3, a0
    la t0, opt_json
    ld t0, 0(t0)
    beqz t0, 5f
    la a1, j_results            # summary extra: ,"results":N
    call sb_cstr
    mv a0, s3
    mv a1, s0
    call int_to_dec
    mv a0, s3
    li a1, 0
    call sb_byte
    ld a0, SB_PTR(s3)
    call str_from_cstr
    mv a4, a0
    call ld_files
    ld a3, VEC_N(a0)
    call cmd_name
    mv a2, a0
    li a0, 1
    la t0, report_fd
    ld a1, 0(t0)
    call summary_emit
    j 8f
5:
    beqz s1, 6f                 # type-at prints no count
    mv a0, s3
    mv a1, s0
    call int_to_dec
    mv a1, s2
    li t0, 1
    bne s0, t0, 51f
    mv a1, s1
51:
    mv a0, s3
    call sb_cstr
6:
    la t0, diag_errors
    ld a1, 0(t0)
    beqz a1, 7f
    mv a0, s3
    la a1, h_note_errors
    call sb_cstr
    la t0, diag_errors
    ld a1, 0(t0)
    mv a0, s3
    call int_to_dec
    la a1, h_errors_tail
    la t0, diag_errors
    ld t0, 0(t0)
    li t1, 1
    bne t0, t1, 61f
    la a1, h_error_tail
61:
    mv a0, s3
    call sb_cstr
7:
    mv a0, s3
    call q_out
8:
    li a0, 0
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# int cmd_query(void)
# `vo query callers|refs|type-at ENTRY TARGET` (contracts/cli.md).
    .globl cmd_query
cmd_query:
    addi sp, sp, -64
    sd ra, 56(sp)
    sd s0, 48(sp)
    sd s1, 40(sp)
    sd s2, 32(sp)
    sd s3, 24(sp)
    sd s4, 16(sp)
    la t0, pos_n
    ld t0, 0(t0)
    la t1, pos_args
    ld s0, 0(t1)
    beqz t0, 90f
    ld a0, 0(s0)                # the subcommand
    la a1, n_callers
    call cstr_eq
    li s4, 1
    bnez a0, 1f
    ld a0, 0(s0)
    la a1, n_refs
    call cstr_eq
    li s4, 2
    bnez a0, 1f
    ld a0, 0(s0)
    la a1, n_type_at
    call cstr_eq
    li s4, 3
    bnez a0, 1f
    la a0, m_query_sub
    ld a1, 0(s0)
    call usage_error_arg
1:
    la t0, pos_n
    ld t0, 0(t0)
    li t1, 3
    beq t0, t1, 2f
    la a0, m_query_args
    ld a1, 0(s0)
    call usage_error_arg
2:
    li t1, 3
    bne s4, t1, 3f
    ld a0, 8(s0)                # type-at
    call q_load
    ld a0, 16(s0)
    addi a1, sp, 0
    call q_parse_pos
    bgez a0, 21f
    la a0, m_type_at_pos
    ld a1, 16(s0)
    call usage_error_arg
21:
    mv a1, a0
    ld a0, 0(sp)
    call q_type_at
    j 9f
3:
    ld a0, 8(s0)
    call q_load
    ld a0, 16(s0)
    addi a1, sp, 0
    call q_resolve
    ld s1, 0(sp)
    ld s2, 8(sp)
    li t1, 1
    bne s4, t1, 4f
    beqz s1, 4f                 # callers: a function or builtin
    li t0, -1
    bne s2, t0, 31f
    lhu t0, N_KIND(s1)
    li t1, N_FN
    beq t0, t1, 4f
31:
    ld a0, 16(s0)
    call str_from_cstr
    mv a1, a0
    la a0, m_not_function
    li a2, 0
    li a3, 0
    call msg
    call q_fail
4:
    li a2, 0
    li t1, 1
    bne s4, t1, 41f
    li a2, 1
41:
    mv a0, s1
    mv a1, s2
    call q_matches
    mv s3, a0
    li t1, 2
    bne s4, t1, 42f
    mv a0, s3
    call q_decl_first
42:
    li s1, 0
5:
    ld t0, VEC_N(s3)
    bge s1, t0, 6f
    mv a0, s3
    mv a1, s1
    call vec_get
    ld a1, 16(s0)
    call q_ref
    addi s1, s1, 1
    j 5b
6:
    ld a0, VEC_N(s3)
    la a1, h_reference
    la a2, h_references
    call q_finish
    j 9f
90:
    la a0, m_query_none
    call str_from_cstr
    call usage_error
9:
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    addi sp, sp, 64
    ret

# ---- vo impact (research R5) ----
# ponytail: impact follows calls and uses as values only; a change to a record's declaration does
# not reach the functions that only use the record. Add type-level impact if reviews need it.

# void df_fail(void *message)
# T0007 diff_mismatch, exit 64.
    .globl df_fail
df_fail:
    la a1, code_t7
    tail q_fail_code

# void df_mismatch(long file, long line)
# T0007: the diff's line does not match the file as it is now.
    .globl df_mismatch
df_mismatch:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a1
    call file_get
    ld s1, FILE_PATH(a0)
    call sb_new                 # the line number as a str
    sd a0, 0(sp)
    mv a1, s0
    call int_to_dec
    ld a0, 0(sp)
    call sb_finish
    mv a2, a0
    mv a1, s1
    la a0, m_df_mismatch
    li a3, 0
    call msg
    call df_fail
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void df_verify(long file, long line, char *p, long n)
# The diff says line `line` of the file is p[0..n): T0007 when it is not.
    .globl df_verify
df_verify:
    bltz a0, 9f
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
    call file_get
    ld t0, FILE_NLINES(a0)
    bgt s1, t0, 8f
    mv a0, s0
    mv a1, s1
    call line_end
    sd a0, 0(sp)
    mv a0, s0
    mv a1, s1
    call line_start
    ld t1, 0(sp)
    sub t2, t1, a0              # the line's length
    bne t2, s3, 8f
    mv t3, a0
    mv a0, s0
    sd t3, 0(sp)
    call file_get
    ld t4, FILE_TEXT(a0)
    ld t3, 0(sp)
    add t4, t4, t3
1:
    beqz s3, 7f
    lbu t5, 0(t4)
    lbu t6, 0(s2)
    bne t5, t6, 8f
    addi t4, t4, 1
    addi s2, s2, 1
    addi s3, s3, -1
    j 1b
8:
    mv a0, s0
    mv a1, s1
    call df_mismatch
7:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
9:
    ret

# void df_push_key(void *vec_global, long file, long line)
# Appends file << 32 | line to the vec held in *vec_global (created on first use).
    .globl df_push_key
df_push_key:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    slli a1, a1, 32
    or s1, a1, a2
    ld a0, 0(s0)
    bnez a0, 1f
    call vec_new
    sd a0, 0(s0)
1:
    mv a1, s1
    call vec_push
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void df_removed_scan(long file, char *p, long n)
# A removed line `fn NAME(` names a function the diff may have removed (canonical layout).
    .globl df_removed_scan
df_removed_scan:
    bltz a0, 9f
    li t0, 4
    blt a2, t0, 9f
    lbu t0, 0(a1)
    li t1, 'f'
    bne t0, t1, 9f
    lbu t0, 1(a1)
    li t1, 'n'
    bne t0, t1, 9f
    lbu t0, 2(a1)
    li t1, ' '
    bne t0, t1, 9f
    addi t2, a1, 3              # the name: up to `(` or `[`
    add t3, a1, a2
    mv t4, t2
1:
    bge t4, t3, 9f
    lbu t0, 0(t4)
    li t1, '('
    beq t0, t1, 2f
    li t1, '['
    beq t0, t1, 2f
    addi t4, t4, 1
    j 1b
2:
    beq t4, t2, 9f
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    mv a0, t2
    sub a1, t4, t2
    call str_new
    mv s1, a0
    li a0, 16
    call alloc
    sd s0, 0(a0)
    sd s1, 8(a0)
    mv s1, a0
    la t0, df_removed
    ld a0, 0(t0)
    bnez a0, 3f
    call vec_new
    la t0, df_removed
    sd a0, 0(t0)
3:
    mv a1, s1
    call vec_push
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
9:
    ret

# long df_number(char **pp, char *end, long *out)
# Reads decimal digits at *pp (advancing it); 1 when there was at least one digit.
    .globl df_number
df_number:
    ld t0, 0(a0)
    li t1, 0
    li t2, 0
1:
    bge t0, a1, 2f
    lbu t3, 0(t0)
    addi t3, t3, -48
    li t4, 9
    bgtu t3, t4, 2f
    li t4, 10
    mul t1, t1, t4
    add t1, t1, t3
    addi t0, t0, 1
    li t2, 1
    j 1b
2:
    sd t0, 0(a0)
    sd t1, 0(a2)
    mv a0, t2
    ret

# void df_range(char **pp, char *end, long *start, long *count)
# `S[,C]` at *pp; C defaults to 1. T0007 when there is no number.
    .globl df_range
df_range:
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
    call df_number
    beqz a0, 8f
    li t0, 1
    sd t0, 0(s3)
    ld t0, 0(s0)
    bge t0, s1, 9f
    lbu t1, 0(t0)
    li t2, ','
    bne t1, t2, 9f
    addi t0, t0, 1
    sd t0, 0(s0)
    mv a0, s0
    mv a1, s1
    mv a2, s3
    call df_number
    bnez a0, 9f
8:
    la a0, m_df_hunk
    call str_from_cstr
    call df_fail
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# long df_starts(char *p, long n, char *prefix)
# 1 when p[0..n) starts with the C string prefix.
    .globl df_starts
df_starts:
1:
    lbu t0, 0(a2)
    beqz t0, 2f
    beqz a1, 3f
    lbu t1, 0(a0)
    bne t0, t1, 3f
    addi a0, a0, 1
    addi a1, a1, -1
    addi a2, a2, 1
    j 1b
2:
    li a0, 1
    ret
3:
    li a0, 0
    ret

# long df_file(char *p, long n)
# The program file of a `+++ PATH` line's path (p after "+++ "), or -1 (recorded as ignored).
    .globl df_file
df_file:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    mv s1, a1
    mv t0, s0                   # the path ends at a tab (diff -u's timestamp)
    add t1, s0, s1
1:
    bge t0, t1, 2f
    lbu t2, 0(t0)
    li t3, 9
    beq t2, t3, 2f
    addi t0, t0, 1
    j 1b
2:
    sub s1, t0, s0
    mv a0, s0
    mv a1, s1
    la a2, s_dev_null
    call df_starts
    beqz a0, 3f
    li a0, -1                   # a deleted file
    j 9f
3:
    li t0, 2                    # drop one a/ or b/
    blt s1, t0, 4f
    lbu t1, 1(s0)
    li t2, '/'
    bne t1, t2, 4f
    lbu t1, 0(s0)
    li t2, 'a'
    beq t1, t2, 31f
    li t2, 'b'
    bne t1, t2, 4f
31:
    addi s0, s0, 2
    addi s1, s1, -2
4:
    mv a0, s0
    mv a1, s1
    call str_new
    sd a0, 0(sp)
    call q_program_file
    bgez a0, 9f
    la t0, df_ignored           # not part of the program: reported as ignored
    ld a0, 0(t0)
    bnez a0, 5f
    call vec_new
    la t0, df_ignored
    sd a0, 0(t0)
5:
    ld a1, 0(sp)
    call vec_push
    li a0, -1
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void df_parse(char *text, long len)
# Reads a unified diff: the changed new-side lines of each program file (df_plus), the places
# where lines were removed (df_minus) and the functions a removed line declared (df_removed).
# Context and added lines must match the current files (T0007).
    .globl df_parse
df_parse:
    addi sp, sp, -112
    sd ra, 104(sp)
    sd s0, 96(sp)
    sd s1, 88(sp)
    sd s2, 80(sp)
    sd s3, 72(sp)
    sd s4, 64(sp)
    sd s5, 56(sp)
    sd s6, 48(sp)
    sd s7, 40(sp)
    mv s0, a0                   # p
    add s1, a0, a1              # end
    li s2, -1                   # current program file, or -1
    li s3, 0                    # new-side line number
    li s4, 0                    # old lines left in the hunk
    li s5, 0                    # new lines left
    li s7, 0                    # saw a +++ line
1:
    bge s0, s1, 80f
    mv s6, s0                   # find the end of the line
2:
    bge s6, s1, 3f
    lbu t0, 0(s6)
    li t1, 10
    beq t0, t1, 3f
    addi s6, s6, 1
    j 2b
3:
    sub t0, s6, s0              # line = [s0, s6); drop a trailing \r
    beqz t0, 31f
    lbu t1, -1(s6)
    li t2, 13
    bne t1, t2, 31f
    addi t0, t0, -1
31:
    sd t0, 0(sp)                # its length
    or t1, s4, s5
    beqz t1, 50f
    beqz t0, 41f                # in a hunk; an empty line is an empty context line
    lbu t1, 0(s0)
    li t2, ' '
    beq t1, t2, 41f
    li t2, '+'
    beq t1, t2, 42f
    li t2, '-'
    beq t1, t2, 43f
    li t2, 92                   # backslash: "\ No newline at end of file"
    beq t1, t2, 70f
    la a0, m_df_hunk
    call str_from_cstr
    call df_fail
41:
    mv a0, s2
    mv a1, s3
    addi a2, s0, 1
    ld a3, 0(sp)
    addi a3, a3, -1
    bgez a3, 411f
    li a3, 0
411:
    call df_verify
    addi s3, s3, 1
    addi s4, s4, -1
    addi s5, s5, -1
    j 70f
42:
    mv a0, s2
    mv a1, s3
    addi a2, s0, 1
    ld a3, 0(sp)
    addi a3, a3, -1
    call df_verify
    bltz s2, 421f
    la a0, df_plus
    mv a1, s2
    mv a2, s3
    call df_push_key
421:
    addi s3, s3, 1
    addi s5, s5, -1
    j 70f
43:
    bltz s2, 431f
    la a0, df_minus
    mv a1, s2
    mv a2, s3
    call df_push_key
    mv a0, s2
    addi a1, s0, 1
    ld a2, 0(sp)
    addi a2, a2, -1
    call df_removed_scan
431:
    addi s4, s4, -1
    j 70f
50:
    mv a0, s0                   # outside a hunk
    ld a1, 0(sp)
    la a2, s_plus3
    call df_starts
    beqz a0, 51f
    addi a0, s0, 4
    ld a1, 0(sp)
    addi a1, a1, -4
    call df_file
    mv s2, a0
    li s7, 1
    j 70f
51:
    mv a0, s0
    ld a1, 0(sp)
    la a2, s_at2
    call df_starts
    beqz a0, 70f
    bnez s7, 52f
    la a0, m_df_hunk
    call str_from_cstr
    call df_fail
52:
    addi t0, s0, 3              # @@ -a,b +c,d @@
    sd t0, 8(sp)
    ld t1, 0(sp)
    add t1, s0, t1
    sd t1, 16(sp)
    lbu t2, 0(t0)
    li t3, '-'
    bne t2, t3, 59f
    addi t0, t0, 1
    sd t0, 8(sp)
    addi a0, sp, 8
    ld a1, 16(sp)
    addi a2, sp, 24
    addi a3, sp, 32
    call df_range
    ld s4, 32(sp)
    ld t0, 8(sp)
    lbu t2, 0(t0)
    li t3, ' '
    bne t2, t3, 59f
    lbu t2, 1(t0)
    li t3, '+'
    bne t2, t3, 59f
    addi t0, t0, 2
    sd t0, 8(sp)
    addi a0, sp, 8
    ld a1, 16(sp)
    addi a2, sp, 24
    addi a3, sp, 32
    call df_range
    ld s3, 24(sp)
    ld s5, 32(sp)
    bnez s5, 70f                # no new lines: the change sits after line c
    addi s3, s3, 1
    j 70f
59:
    la a0, m_df_hunk
    call str_from_cstr
    call df_fail
70:
    addi s0, s6, 1
    j 1b
80:
    bnez s7, 81f
    la a0, m_df_not
    call str_from_cstr
    call df_fail
81:
    or t0, s4, s5
    beqz t0, 9f
    la a0, m_df_hunk
    call str_from_cstr
    call df_fail
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

# long df_count_in(void *vec, long file, long l1, long l2, long open_start)
# How many keys of vec are in file and in lines [l1, l2] ((l1, l2] when open_start).
    .globl df_count_in
df_count_in:
    li t6, 0
    beqz a0, 9f
    beqz a4, 1f
    addi a2, a2, 1
1:
    ld t0, VEC_N(a0)
    ld t1, VEC_DATA(a0)
2:
    beqz t0, 9f
    ld t2, 0(t1)
    addi t1, t1, 8
    addi t0, t0, -1
    srli t3, t2, 32
    bne t3, a1, 2b
    slli t3, t2, 32
    srli t3, t3, 32
    blt t3, a2, 2b
    bgt t3, a3, 2b
    addi t6, t6, 1
    j 2b
9:
    mv a0, t6
    ret

# void im_add(void *item, char *role, void *via)
# Appends an impact record {item or name str, role, via} to im_list.
    .globl im_add
im_add:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    sd s2, 0(sp)
    mv s0, a0
    mv s1, a1
    mv s2, a2
    li a0, 24
    call alloc
    sd s0, 0(a0)
    sd s1, 8(a0)
    sd s2, 16(a0)
    mv s0, a0
    la t0, im_list
    ld a0, 0(t0)
    bnez a0, 1f
    call vec_new
    la t0, im_list
    sd a0, 0(t0)
1:
    mv a1, s0
    call vec_push
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    ld s2, 0(sp)
    addi sp, sp, 32
    ret

# long im_has(void *item)
# 1 when item is already in im_list.
    .globl im_has
im_has:
    la t0, im_list
    ld t0, 0(t0)
    beqz t0, 8f
    ld t1, VEC_N(t0)
    ld t2, VEC_DATA(t0)
1:
    beqz t1, 8f
    ld t3, 0(t2)
    ld t3, 0(t3)
    beq t3, a0, 2f
    addi t2, t2, 8
    addi t1, t1, -1
    j 1b
2:
    li a0, 1
    ret
8:
    li a0, 0
    ret

# void im_items(void)
# changed and added functions, tests and properties, in file and position order.
    .globl im_items
im_items:
    addi sp, sp, -80
    sd ra, 72(sp)
    sd s0, 64(sp)
    sd s1, 56(sp)
    sd s2, 48(sp)
    sd s3, 40(sp)
    sd s4, 32(sp)
    sd s5, 24(sp)
    call ld_files
    mv s0, a0
    li s1, 0
1:
    ld t0, VEC_N(s0)
    bge s1, t0, 9f
    mv a0, s0
    mv a1, s1
    call vec_get
    mv s2, a0                   # file id
    addi s1, s1, 1
    call file_get
    ld t0, FILE_AST(a0)
    ld s3, N_A(t0)
    li s4, 0
2:
    ld t0, VEC_N(s3)
    bge s4, t0, 1b
    mv a0, s3
    mv a1, s4
    call vec_get
    addi s4, s4, 1
    mv s5, a0
    lhu t0, N_KIND(s5)
    li t1, N_FN
    beq t0, t1, 3f
    li t1, N_TEST
    bne t0, t1, 2b
3:
    mv a0, s2                   # its lines
    lwu a1, N_START(s5)
    call pos_line
    sd a0, 0(sp)
    lwu a1, N_END(s5)
    addi a1, a1, -1
    mv a0, s2
    call pos_line
    sd a0, 8(sp)
    la t0, df_plus
    ld a0, 0(t0)
    mv a1, s2
    ld a2, 0(sp)
    ld a3, 8(sp)
    li a4, 0
    call df_count_in
    sd a0, 16(sp)               # added lines inside
    ld t0, 8(sp)
    ld t1, 0(sp)
    sub t0, t0, t1
    addi t0, t0, 1
    bne a0, t0, 4f
    mv a0, s5                   # every line is new: added
    la a1, r_added
    li a2, 0
    call im_add
    j 2b
4:
    la t0, df_minus
    ld a0, 0(t0)
    mv a1, s2
    ld a2, 0(sp)
    ld a3, 8(sp)
    li a4, 1
    call df_count_in
    ld t0, 16(sp)
    add a0, a0, t0
    beqz a0, 2b
    mv a0, s5
    la a1, r_changed
    li a2, 0
    call im_add
    j 2b
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

# void im_removed(void)
# Removed functions: names from removed `fn NAME(` lines that are no longer items of their file.
    .globl im_removed
im_removed:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    li s0, 0
1:
    la t0, df_removed
    ld t0, 0(t0)
    beqz t0, 9f
    ld t1, VEC_N(t0)
    bge s0, t1, 9f
    mv a0, t0
    mv a1, s0
    call vec_get
    addi s0, s0, 1
    mv s1, a0                   # {file, name}
    ld a0, 0(s1)
    call file_get
    ld t0, FILE_AST(a0)
    ld s2, N_A(t0)
    li s3, 0
2:
    ld t0, VEC_N(s2)
    bge s3, t0, 3f
    mv a0, s2
    mv a1, s3
    call vec_get
    addi s3, s3, 1
    lhu t0, N_KIND(a0)
    li t1, N_FN
    bne t0, t1, 2b
    ld t0, N_A(a0)
    ld a0, N_A(t0)
    ld a1, 8(s1)
    call str_eq
    beqz a0, 2b
    j 1b                        # still there: not removed
3:
    ld a0, 8(s1)
    la a1, r_removed
    li a2, 0
    call im_add
    j 1b
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# void im_callers(void)
# Breadth first from the changed, added and removed functions: every function, test and
# property that calls one of them, directly or not (calls and uses as values).
    .globl im_callers
im_callers:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    li s0, 0                    # queue position in im_list
1:
    la t0, im_list
    ld t0, 0(t0)
    beqz t0, 9f
    ld t1, VEC_N(t0)
    bge s0, t1, 9f
    mv a0, t0
    mv a1, s0
    call vec_get
    addi s0, s0, 1
    ld s1, 0(a0)                # an item, or a removed name (a str)
    ld t0, 8(a0)
    la t1, r_test
    beq t0, t1, 1b              # nothing calls a test
    li s2, 0
2:
    la t0, q_log
    ld t0, 0(t0)
    beqz t0, 1b
    ld t1, VEC_N(t0)
    bge s2, t1, 1b
    mv a0, t0
    mv a1, s2
    call vec_get
    addi s2, s2, 1
    mv s3, a0
    ld t0, RL_ROLE(s3)
    li t1, RR_CALL
    beq t0, t1, 3f
    li t1, RR_VALUE
    bne t0, t1, 2b
3:
    ld t0, RL_ITEM(s3)
    beqz t0, 2b
    ld a0, RL_KEY_A(s3)         # a call of s1? (by declaration, or by name when removed)
    beqz a0, 4f
    bne a0, s1, 2b
    ld t0, RL_KEY_B(s3)
    li t1, -1
    bne t0, t1, 2b
    j 5f
4:
    ld a1, RL_KEY_B(s3)
    li t0, 4096
    bltu a1, t0, 2b
    lhu t0, N_KIND(s1)          # s1 is a str only for removed names
    li t1, N_FN
    beq t0, t1, 2b
    li t1, N_TEST
    beq t0, t1, 2b
    mv a0, s1
    call str_eq
    beqz a0, 2b
5:
    ld a0, RL_ITEM(s3)
    call im_has
    bnez a0, 2b
    ld a0, RL_ITEM(s3)
    lhu t0, N_KIND(a0)
    la a1, r_affected
    li t1, N_TEST
    bne t0, t1, 6f
    la a1, r_test
6:
    mv a2, s1
    call im_add
    j 2b
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# void *im_name(void *x)
# The name of an impact entry: an item's name (`test "x"` for tests), or the str itself.
    .globl im_name
im_name:
    lhu t0, N_KIND(a0)
    li t1, N_FN
    beq t0, t1, 1f
    li t1, N_TEST
    beq t0, t1, 1f
    ret
1:
    tail fx_item_name

# void im_print(void *entry)
# One impact line or record (contracts/reports.md).
    .globl im_print
im_print:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    ld s1, 0(s0)                # item or str
    call sb_new
    mv s2, a0
    ld t0, 8(s0)                # removed and ignored entries are names, not items
    la t1, r_removed
    li s3, 0
    beq t0, t1, 1f
    la t1, r_ignored
    beq t0, t1, 1f
    li s3, 1
1:
    la t0, opt_json
    ld t0, 0(t0)
    beqz t0, 5f
    la a1, j_impact_head
    call sb_cstr
    mv a0, s1
    call im_name
    mv a1, a0
    mv a0, s2
    call json_str_into
    mv a0, s2
    la a1, j_role
    call sb_cstr
    ld a1, 8(s0)
    mv a0, s2
    call sb_cstr
    mv a0, s2
    li a1, '"'
    call sb_byte
    beqz s3, 2f
    mv a0, s2
    la a1, j_file
    call sb_cstr
    lwu a0, N_FILE(s1)
    call file_get
    ld a1, FILE_PATH(a0)
    mv a0, s2
    call json_str_into
    mv a0, s2
    la a1, j_start
    call sb_cstr
    mv a0, s2
    lwu a1, N_FILE(s1)
    lwu a2, N_START(s1)
    call json_pos
2:
    ld t0, 16(s0)
    beqz t0, 3f
    mv a0, s2
    la a1, j_via
    call sb_cstr
    ld a0, 16(s0)
    call im_name
    mv a1, a0
    mv a0, s2
    call json_str_into
3:
    mv a0, s2
    la a1, j_close
    call sb_cstr
    j 8f
5:
    ld a1, 8(s0)                # ROLE NAME [via X] [(FILE:LINE)]; a test's name says what it is
    la t1, r_test
    beq a1, t1, 51f
    call sb_cstr
    mv a0, s2
    li a1, ' '
    call sb_byte
51:
    mv a0, s1
    call im_name
    mv a1, a0
    mv a0, s2
    call sb_str
    ld t0, 16(s0)
    beqz t0, 6f
    mv a0, s2
    la a1, h_via
    call sb_cstr
    ld a0, 16(s0)
    call im_name
    mv a1, a0
    mv a0, s2
    call sb_str
6:
    beqz s3, 7f
    mv a0, s2
    la a1, h_open
    call sb_cstr
    lwu a0, N_FILE(s1)
    call file_get
    ld a1, FILE_PATH(a0)
    mv a0, s2
    call sb_str
    mv a0, s2
    li a1, ':'
    call sb_byte
    lwu a0, N_FILE(s1)
    lwu a1, N_START(s1)
    call pos_line
    mv a1, a0
    mv a0, s2
    call int_to_dec
    mv a0, s2
    li a1, ')'
    call sb_byte
7:
    mv a0, s2
    li a1, 10
    call sb_byte
8:
    mv a0, s2
    call q_out
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# long im_count(char *role)
    .globl im_count
im_count:
    la t0, im_list
    ld t0, 0(t0)
    li t6, 0
    beqz t0, 9f
    ld t1, VEC_N(t0)
    ld t2, VEC_DATA(t0)
1:
    beqz t1, 9f
    ld t3, 0(t2)
    ld t3, 8(t3)
    addi t2, t2, 8
    addi t1, t1, -1
    bne t3, a0, 1b
    addi t6, t6, 1
    j 1b
9:
    mv a0, t6
    ret

# void im_count_into(void *sb, char *key, char *role)
# Appends `key` and the number of entries with this role.
    .globl im_count_into
im_count_into:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    mv s1, a1
    mv a0, a2
    call im_count
    sd a0, 0(sp)
    mv a0, s0
    mv a1, s1
    call sb_cstr
    mv a0, s0
    ld a1, 0(sp)
    call int_to_dec
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# int cmd_impact(void)
# `vo impact ENTRY --diff FILE` (contracts/cli.md, research R5).
    .globl cmd_impact
cmd_impact:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    la t0, pos_n
    ld t0, 0(t0)
    li t1, 1
    beq t0, t1, 1f
    la a0, m_impact_args
    call str_from_cstr
    call usage_error
1:
    la t0, pos_args
    ld t0, 0(t0)
    ld a0, 0(t0)
    call q_load
    la t0, opt_diff             # the diff (- is stdin)
    ld s0, 0(t0)
    lbu t0, 0(s0)
    li t1, '-'
    bne t0, t1, 2f
    lbu t0, 1(s0)
    bnez t0, 2f
    la s0, s_dev_stdin
2:
    mv a0, s0
    addi a1, sp, 0
    call read_whole_file
    bgez a0, 3f
    mv a0, s0
    call str_from_cstr
    mv s1, a0
    mv a1, a0
    la a0, m_cannot_read_diff
    li a2, 0
    li a3, 0
    call msg
    mv a3, a0
    la t0, opt_json
    ld a0, 0(t0)
    la t0, report_fd
    ld a1, 0(t0)
    la a2, code_t2q
    mv a4, s1
    call toolchain_error
    li a0, EXIT_NOINPUT
    call vo_exit
3:
    mv a1, a0
    ld a0, 0(sp)
    call df_parse
    call im_items
    call im_removed
    call im_callers
    la t0, df_ignored           # ignored diff files come last
    ld s1, 0(t0)
    li s2, 0
4:
    beqz s1, 5f
    ld t0, VEC_N(s1)
    bge s2, t0, 5f
    mv a0, s1
    mv a1, s2
    call vec_get
    la a1, r_ignored
    li a2, 0
    call im_add
    addi s2, s2, 1
    j 4b
5:
    li s1, 0                    # print in the order: changed, added, removed, then the rest
    la s2, r_changed
    call im_print_role
    la s2, r_added
    call im_print_role
    la s2, r_removed
    call im_print_role
    li s2, 0
    call im_print_role
    call sb_new                 # the counts
    mv s1, a0
    la t0, opt_json
    ld t0, 0(t0)
    beqz t0, 7f
    mv a0, s1
    la a1, j_im_changed
    la a2, r_changed
    call im_count_into
    mv a0, s1
    la a1, j_im_added
    la a2, r_added
    call im_count_into
    mv a0, s1
    la a1, j_im_removed
    la a2, r_removed
    call im_count_into
    mv a0, s1
    la a1, j_im_affected
    la a2, r_affected
    call im_count_into
    mv a0, s1
    la a1, j_im_tests
    la a2, r_test
    call im_count_into
    mv a0, s1
    li a1, 0
    call sb_byte
    ld a0, SB_PTR(s1)
    call str_from_cstr
    mv a4, a0
    call ld_files
    ld a3, VEC_N(a0)
    call cmd_name
    mv a2, a0
    li a0, 1
    la t0, report_fd
    ld a1, 0(t0)
    call summary_emit
    j 9f
7:
    mv a0, s1
    la a1, h_empty
    la a2, r_changed
    call im_count_into
    mv a0, s1
    la a1, h_changed_sep
    la a2, r_added
    call im_count_into
    mv a0, s1
    la a1, h_added_sep
    la a2, r_removed
    call im_count_into
    mv a0, s1
    la a1, h_removed_sep
    la a2, r_affected
    call im_count_into
    mv a0, s1
    la a1, h_affected_sep
    la a2, r_test
    call im_count_into
    la a0, r_test
    call im_count
    la a1, h_test_end
    li t0, 1
    beq a0, t0, 71f
    la a1, h_tests_end
71:
    mv a0, s1
    call sb_cstr
    mv a0, s1
    call q_out
    li a0, 0
    li a1, 0
    li a2, 0
    call q_finish
9:
    li a0, 0
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    addi sp, sp, 48
    ret

# (cmd_impact) void im_print_role(void): prints im_list entries whose role is s2, or (s2 = 0)
# every entry that is not changed, added or removed. Uses the caller's s1, s2 (a local helper).
    .globl im_print_role
im_print_role:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s3, 8(sp)
    li s0, 0
1:
    la t0, im_list
    ld t0, 0(t0)
    beqz t0, 9f
    ld t1, VEC_N(t0)
    bge s0, t1, 9f
    mv a0, t0
    mv a1, s0
    call vec_get
    addi s0, s0, 1
    ld t0, 8(a0)
    beqz s2, 2f
    bne t0, s2, 1b
    j 3f
2:
    la t1, r_changed
    beq t0, t1, 1b
    la t1, r_added
    beq t0, t1, 1b
    la t1, r_removed
    beq t0, t1, 1b
3:
    call im_print
    j 1b
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 32
    ret

# void q_note_expr(void *node)
# Remembers a checked expression (vo query type-at), while q_collect is 1.
    .globl q_note_expr
q_note_expr:
    lwu t0, N_START(a0)
    lwu t1, N_END(a0)
    bgeu t0, t1, 9f
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    mv s0, a0
    la t0, q_exprs
    ld a0, 0(t0)
    bnez a0, 1f
    call vec_new
    la t0, q_exprs
    sd a0, 0(t0)
1:
    mv a1, s0
    call vec_push
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
9:
    ret

# void *q_expr_at(long file, long off)
# The innermost checked expression whose span contains off, or 0.
    .globl q_expr_at
q_expr_at:
    la t0, q_exprs
    ld t0, 0(t0)
    li a2, 0
    li a3, -1
    beqz t0, 9f
    ld t1, VEC_N(t0)
    ld t2, VEC_DATA(t0)
1:
    beqz t1, 9f
    ld t4, 0(t2)
    addi t2, t2, 8
    addi t1, t1, -1
    lwu t5, N_FILE(t4)
    bne t5, a0, 1b
    lwu t5, N_START(t4)
    lwu t6, N_END(t4)
    bgt t5, a1, 1b
    bge a1, t6, 1b
    sub t6, t6, t5
    bgeu t6, a3, 1b
    mv a3, t6
    mv a2, t4
    j 1b
9:
    mv a0, a2
    ret

# void *q_decl_of(long key_a, long key_b)
# The declaration entry of a target, or 0.
    .globl q_decl_of
q_decl_of:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    mv s0, a0
    mv s1, a1
    li s2, 0
1:
    la t0, q_log
    ld t0, 0(t0)
    li a0, 0
    beqz t0, 9f
    ld t1, VEC_N(t0)
    bge s2, t1, 9f
    mv a0, t0
    mv a1, s2
    call vec_get
    addi s2, s2, 1
    ld t0, RL_ROLE(a0)
    bnez t0, 1b
    ld t0, RL_KEY_A(a0)
    bne t0, s0, 1b
    ld t0, RL_KEY_B(a0)
    bne t0, s1, 1b
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    addi sp, sp, 48
    ret

# int q_type_at(long file, long off)
# `vo query type-at`: the innermost name or expression at the position and its type (research R4).
    .globl q_type_at
q_type_at:
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
    call q_entry_at
    mv s2, a0                   # a name there, or 0
    mv a0, s0
    mv a1, s1
    call q_expr_at
    mv s3, a0                   # an expression there, or 0
    li s5, 0                    # the declaration entry
    bnez s2, 1f
    bnez s3, 5f
    la a0, m_nothing_here
    call str_from_cstr
    call q_fail
1:
    beqz s3, 2f                 # the smaller span wins; a name on a tie
    ld t0, RL_NODE(s2)
    lwu t1, N_START(t0)
    lwu t2, N_END(t0)
    sub t1, t2, t1
    lwu t3, N_START(s3)
    lwu t4, N_END(s3)
    sub t3, t4, t3
    bgt t1, t3, 5f
2:
    ld a0, RL_KEY_A(s2)
    ld a1, RL_KEY_B(s2)
    call q_decl_of
    mv s5, a0
    ld s4, RL_TYPE(s2)          # the type: declared here, the checked expression, or the declaration's
    bnez s4, 3f
    ld t0, RL_NODE(s2)
    beqz s3, 21f
    bne t0, s3, 21f
    ld s4, N_TYPE(s3)
    bnez s4, 3f
21:
    beqz s5, 22f
    ld s4, RL_TYPE(s5)
    bnez s4, 3f
22:
    beqz s3, 3f
    ld s4, N_TYPE(s3)
3:
    ld s3, RL_NODE(s2)          # the node reported
    j 6f
5:
    ld s4, N_TYPE(s3)
    li s2, 0
6:
    call sb_new
    mv s1, a0
    la t0, opt_json
    ld t0, 0(t0)
    beqz t0, 7f
    la a1, j_type_at_head
    call sb_cstr
    mv a0, s1
    lwu a1, N_FILE(s3)
    lwu a2, N_START(s3)
    lwu a3, N_END(s3)
    call json_span
    mv a0, s1
    la a1, j_text
    call sb_cstr
    mv a0, s1
    lwu a1, N_FILE(s3)
    lwu a2, N_START(s3)
    lwu a3, N_END(s3)
    call json_src_text
    mv a0, s1
    la a1, j_type
    call sb_cstr
    mv a0, s4
    call q_type_name
    mv a1, a0
    mv a0, s1
    call json_str_into
    beqz s2, 69f
    mv a0, s1
    la a1, j_name
    call sb_cstr
    mv a0, s1
    lwu a1, N_FILE(s3)
    lwu a2, N_START(s3)
    lwu a3, N_END(s3)
    call json_src_text
    beqz s5, 69f
    mv a0, s1
    la a1, j_declaration
    call sb_cstr
    ld t0, RL_NODE(s5)
    mv a0, s1
    lwu a1, N_FILE(t0)
    lwu a2, N_START(t0)
    lwu a3, N_END(t0)
    call json_span
    mv a0, s1
    li a1, '}'
    call sb_byte
69:
    mv a0, s1
    la a1, j_close
    call sb_cstr
    j 8f
7:
    lwu a1, N_FILE(s3)          # FILE:LINE:COL TEXT: TYPE (declared at FILE:LINE:COL)
    lwu a2, N_START(s3)
    call human_loc
    mv a0, s1
    li a1, ' '
    call sb_byte
    lwu a0, N_FILE(s3)
    call file_get
    ld t0, FILE_TEXT(a0)
    lwu t1, N_START(s3)
    lwu t2, N_END(s3)
    add a1, t0, t1
    sub a2, t2, t1
    mv a0, s1
    call sb_bytes
    mv a0, s1
    la a1, h_colon
    call sb_cstr
    mv a0, s4
    call q_type_name
    mv a1, a0
    mv a0, s1
    call sb_str
    beqz s5, 71f
    mv a0, s1
    la a1, h_declared_at
    call sb_cstr
    ld t0, RL_NODE(s5)
    mv a0, s1
    lwu a1, N_FILE(t0)
    lwu a2, N_START(t0)
    call human_loc
    mv a0, s1
    li a1, ')'
    call sb_byte
71:
    mv a0, s1
    li a1, 10
    call sb_byte
8:
    mv a0, s1
    call q_out
    li a0, 1
    li a1, 0
    li a2, 0
    call q_finish
    ld ra, 72(sp)
    ld s0, 64(sp)
    ld s1, 56(sp)
    ld s2, 48(sp)
    ld s3, 40(sp)
    ld s4, 32(sp)
    ld s5, 24(sp)
    addi sp, sp, 80
    ret

# void *q_type_name(void *type)
    .globl q_type_name
q_type_name:
    beqz a0, 1f
    tail ty_name
1:
    la a0, s_unknown_type
    tail str_from_cstr
