# Structural edits (spec 006 US2): `vo edit rename` and `vo edit add-field`, verified by checking
# the edited text a second time in the same process (research R7-R10).
#
# An edit is a list of changes (CH_*: file, [start, end), old text, new text) over the files of the
# first check (generation 0). The edited texts are registered as generation 1 and the program is
# checked again from them; the edit is accepted only if the errors are unchanged and, for a rename,
# the target's references are exactly the renamed places. Then every changed file is written
# (temporary file, then rename), all or none.
# ponytail: the second check runs in the same process after resetting the per-program state
# (diag_reset, a new file generation); a fresh process would be cleaner but is not reliable under
# qemu-user. tests/cases/edit/recheck_identity guards the reset.
    .include "defs.inc"

    .section .data
    .align 3
ed_changes: .dword 0            # vec of CH_* records, sorted by file and start
ed_files:   .dword 0            # the program's files (generation 0 ids)
ed_new_ids: .dword 0            # their generation-1 ids, in the same order
ed_old_errs: .dword 0           # vec of {code cstr, path str} of the first check's errors
ed_sites:   .dword 0            # rename: the renamed references (log entries of generation 0)
ed_decl:    .dword 0            # rename: the target's declaration entry (generation 0)
ed_summary: .dword 0            # str: "renamed a to b" / "added field f to R"
ed_action:  .dword 0            # str: "Renaming `a` to `b`" (refusal messages)

    .section .rodata
code_t6:    .string "T0006"
w_rename:   .string "rename"
w_add_field: .string "add-field"
s_tmp_suffix: .string ".vo-edit.tmp"
s_arrow:    .string " -> "
s_plus:     .string " + "
s_main:     .string "main"
s_space:    .string " "
s_semi_nl:  .string ";\n"
s_comma_sp: .string ", "
s_colon_sp: .string ": "
s_indent:   .string "    "
s_record_head: .string "record "
m_edit_none: .string "`edit` needs an edit: `vo edit rename ENTRY TARGET NEW_NAME` or `vo edit add-field ENTRY RECORD FIELD: TYPE = DEFAULT`."
m_edit_sub: .string "Unknown edit `$1`. Edits: rename, add-field."
m_rename_args: .string "`edit rename` takes the entry file, the target and the new name: `vo edit rename main.vo old_name new_name`."
m_add_field_args: .string "`edit add-field` takes the entry file, the record and the field: `vo edit add-field main.vo Item discount: int = 0`."
m_bad_name: .string "`$1` cannot be a new name: it must be a name that is not a keyword or a builtin."
m_same_name: .string "`$1` already has that name."
m_no_rename_builtin: .string "`$1` is a builtin: builtins cannot be renamed."
m_no_rename_main: .string "`main` cannot be renamed: `vo run` starts the program at the function named `main`."
m_not_a_name: .string "`$1` is not a name of the program: point at a declaration or a use of one."
m_not_record: .string "`$1` is not a record."
m_field_exists: .string "`$1` already has a field `$2`."
m_bad_field: .string "`$1` is not a field: write `FIELD: TYPE = DEFAULT`, for example `discount: int = 0`."
m_would_not_check: .string "$1 would not check ($2): nothing was written."
m_changes_errors: .string "$1 would change the errors the program already has: nothing was written."
m_changes_meaning: .string "$1 would change what a name refers to at $2: nothing was written."
m_cannot_write: .string "$1: cannot write `$2`; nothing was changed."
m_renamed:  .string "renamed `$1` to `$2`"
m_renaming: .string "Renaming `$1` to `$2`"
m_adding:   .string "Adding field `$1` to `$2`"
m_renamed_h: .string "renamed $1 to $2"
m_added:    .string "added field `$1` to `$2`"
m_added_h:  .string "added field $1 to $2"
m_new_errors: .string "$1 new errors"
m_new_error: .string "1 new error"
h_changes:  .string ": "
h_changes_tail: .string " changes in "
h_change_tail: .string " change in "
h_files_tail: .string " files"
h_file_tail: .string " file"
h_dry:      .string " (dry run, nothing written)"
j_edit_head: .string "{\"v\":1,\"kind\":\"edit\",\"file\":"
j_start_key: .string ",\"start\":"
j_end_key:  .string ",\"end\":"
j_text:     .string ",\"text\":"
j_repl:     .string ",\"replacement\":"
j_close:    .string "}\n"
j_changes:  .string ",\"changes\":"
j_changed:  .string ",\"changed\":"
j_written:  .string ",\"written\":"
j_true:     .string "true"
j_false:    .string "false"

    .text
# ---- changes ----

# void ed_add(long file, long start, long end, void *new_text)
# Adds a change (the old text is taken from the file); one already at the same place is kept.
    .globl ed_add
ed_add:
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
    call file_get
    ld t0, FILE_TEXT(a0)
    add a0, t0, s1
    sub a1, s2, s1
    call str_new
    mv s4, a0
    li a0, CH_SIZE
    call alloc
    sd s0, CH_FILE(a0)
    sd s1, CH_START(a0)
    sd s2, CH_END(a0)
    sd s4, CH_OLD(a0)
    sd s3, CH_NEW(a0)
    mv s4, a0
    la t0, ed_changes
    ld a0, 0(t0)
    bnez a0, 1f
    call vec_new
    la t0, ed_changes
    sd a0, 0(t0)
1:
    mv s3, a0
    ld t0, VEC_N(s3)            # sorted insert by (file, start); same place: drop
    ld t1, VEC_DATA(s3)
    li t2, 0
2:
    bge t2, t0, 4f
    slli t3, t2, 3
    add t3, t1, t3
    ld t3, 0(t3)
    ld t4, CH_FILE(t3)
    blt s0, t4, 4f
    bgt s0, t4, 3f
    ld t5, CH_START(t3)
    blt s1, t5, 4f
    bne s1, t5, 3f
    ld t5, CH_END(t3)
    beq s2, t5, 9f
3:
    addi t2, t2, 1
    j 2b
4:
    sd t2, 0(sp)
    mv a0, s3
    mv a1, s4
    call vec_push
    ld t2, 0(sp)
    ld t0, VEC_N(s3)
    ld t1, VEC_DATA(s3)
    addi t0, t0, -1
5:
    ble t0, t2, 6f
    slli t3, t0, 3
    add t3, t1, t3
    ld t4, -8(t3)
    sd t4, 0(t3)
    addi t0, t0, -1
    j 5b
6:
    slli t3, t2, 3
    add t3, t1, t3
    sd s4, 0(t3)
9:
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    addi sp, sp, 64
    ret

# void *ed_text(long file)
# The file's text with its changes applied, as a NUL-terminated str.
    .globl ed_text
ed_text:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    call sb_new
    mv s1, a0
    li s2, 0                    # copied up to here
    li s3, 0
1:
    la t0, ed_changes
    ld t0, 0(t0)
    beqz t0, 3f
    ld t1, VEC_N(t0)
    bge s3, t1, 3f
    mv a0, t0
    mv a1, s3
    call vec_get
    addi s3, s3, 1
    ld t0, CH_FILE(a0)
    bne t0, s0, 1b
    sd a0, 0(sp)
    mv a0, s0
    call file_get
    ld t0, FILE_TEXT(a0)
    ld t1, 0(sp)
    ld t2, CH_START(t1)
    add a1, t0, s2
    sub a2, t2, s2
    mv a0, s1
    call sb_bytes
    ld t1, 0(sp)
    ld a1, CH_NEW(t1)
    mv a0, s1
    call sb_str
    ld t1, 0(sp)
    ld s2, CH_END(t1)
    j 1b
3:
    mv a0, s0
    call file_get
    ld t0, FILE_TEXT(a0)
    ld t1, FILE_LEN(a0)
    add a1, t0, s2
    sub a2, t1, s2
    mv a0, s1
    call sb_bytes
    mv a0, s1
    call sb_finish
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# long ed_map(long file, long off, long back)
# An offset across the changes of a file: old to new (back = 0), or new to old (back = 1; inside a
# replacement it maps to the change's start).
    .globl ed_map
ed_map:
    la t0, ed_changes
    ld t0, 0(t0)
    li t6, 0                    # delta new - old so far
    beqz t0, 8f
    ld t1, VEC_N(t0)
    ld t2, VEC_DATA(t0)
1:
    beqz t1, 8f
    ld t3, 0(t2)
    addi t2, t2, 8
    addi t1, t1, -1
    ld t4, CH_FILE(t3)
    bne t4, a0, 1b
    ld t4, CH_START(t3)
    ld t5, CH_END(t3)
    ld a3, CH_NEW(t3)
    ld a3, STR_LEN(a3)
    bnez a2, 3f
    bgt t5, a1, 8f              # old to new: changes that end at or before off
    beq t4, a1, 21f             # an insertion exactly at off moves it too, unless it starts there
21:
    blt a1, t5, 8f
    sub t5, t5, t4
    sub a3, a3, t5
    add t6, t6, a3
    j 1b
3:
    add a4, t4, t6              # the change's start in the new text
    blt a1, a4, 8f
    add a5, a4, a3
    bge a1, a5, 4f
    mv a0, t4                   # inside the replacement
    ret
4:
    sub t5, t5, t4
    sub a3, a3, t5
    add t6, t6, a3
    j 1b
8:
    bnez a2, 9f
    add a0, a1, t6
    ret
9:
    sub a0, a1, t6
    ret

# ---- checks of names ----

# long ed_valid_name(void *name)
# 1 when name is an identifier that is not a keyword or a builtin.
    .globl ed_valid_name
ed_valid_name:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    mv s0, a0
    ld t0, STR_LEN(s0)
    beqz t0, 8f
    addi t1, s0, STR_DATA
    lbu t2, 0(t1)
    li t3, '0'
    blt t2, t3, 1f
    li t3, '9'
    ble t2, t3, 8f              # a digit first
1:
    beqz t0, 2f
    lbu a0, 0(t1)
    addi t1, t1, 1
    li t3, '_'
    beq a0, t3, 11f
    li t3, 'a'
    blt a0, t3, 12f
    li t3, 'z'
    ble a0, t3, 11f
12:
    li t3, 'A'
    blt a0, t3, 13f
    li t3, 'Z'
    ble a0, t3, 11f
13:
    li t3, '0'
    blt a0, t3, 8f
    li t3, '9'
    bgt a0, t3, 8f
11:
    addi t0, t0, -1
    j 1b
2:
    addi a0, s0, STR_DATA       # not a keyword
    ld a1, STR_LEN(s0)
    call keyword_kind
    li t0, T_IDENT
    bne a0, t0, 8f
    mv a0, s0                   # not a builtin
    call builtin_id
    bnez a0, 8f
    li a0, 1
    j 9f
8:
    li a0, 0
9:
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret

# void ed_refuse(void *message)
# T0006 edit_refused, exit 65 (nothing is written).
    .globl ed_refuse
ed_refuse:
    addi sp, sp, -16
    sd ra, 8(sp)
    mv a3, a0
    la t0, opt_json
    ld a0, 0(t0)
    la t0, report_fd
    ld a1, 0(t0)
    la a2, code_t6
    li a4, 0
    call toolchain_error
    li a0, EXIT_CHECK
    call vo_exit
    ld ra, 8(sp)
    addi sp, sp, 16
    ret

# void ed_refuse_arg(char *tmpl, void *arg)
    .globl ed_refuse_arg
ed_refuse_arg:
    addi sp, sp, -16
    sd ra, 8(sp)
    li a2, 0
    li a3, 0
    call msg
    call ed_refuse
    ld ra, 8(sp)
    addi sp, sp, 16
    ret

# ---- rename (research R8) ----

# void ed_rename(char *target, char *new_name)
    .globl ed_rename
ed_rename:
    addi sp, sp, -80
    sd ra, 72(sp)
    sd s0, 64(sp)
    sd s1, 56(sp)
    sd s2, 48(sp)
    sd s3, 40(sp)
    sd s4, 32(sp)
    sd s5, 24(sp)
    mv s0, a0
    mv a0, a1
    call str_from_cstr
    mv s1, a0                   # the new name
    mv a0, s0
    addi a1, sp, 0
    call q_resolve
    ld s2, 0(sp)                # key
    ld s3, 8(sp)
    bnez s2, 1f
    mv a0, s0                   # (0, id): a builtin
    call str_from_cstr
    mv a1, a0
    la a0, m_no_rename_builtin
    call ed_refuse_arg
1:
    li a2, 0
    mv a0, s2
    mv a1, s3
    call q_matches
    mv s4, a0                   # every reference, sorted
    call q_decl_entry
    bnez a0, 2f
    mv a0, s0
    call str_from_cstr
    mv a1, a0
    la a0, m_not_a_name
    call ed_refuse_arg
2:
    mv s5, a0
    la t0, ed_decl
    sd s5, 0(t0)
    la t0, ed_sites
    sd s4, 0(t0)
    ld t0, RL_NODE(s5)          # the old name: the declaration's text
    lwu a0, N_FILE(t0)
    call file_get
    ld t1, FILE_TEXT(a0)
    ld t0, RL_NODE(s5)
    lwu t2, N_START(t0)
    lwu t3, N_END(t0)
    add a0, t1, t2
    sub a1, t3, t2
    call str_new
    sd a0, 16(sp)
    ld t0, RL_KEY_B(s5)         # main cannot be renamed
    li t1, -1
    bne t0, t1, 3f
    ld t0, RL_KEY_A(s5)
    ld t1, RL_NODE(s5)
    ld t2, N_A(t0)
    bne t1, t2, 3f
    lhu t2, N_KIND(t0)
    li t3, N_FN
    bne t2, t3, 3f
    ld a0, 16(sp)
    la a1, s_main
    call str_eq_cstr
    beqz a0, 3f
    la a0, m_no_rename_main
    call str_from_cstr
    call ed_refuse
3:
    ld a0, 16(sp)
    mv a1, s1
    call str_eq
    beqz a0, 4f
    la a0, m_same_name
    ld a1, 16(sp)
    call ed_refuse_arg
4:
    mv a0, s1
    call ed_valid_name
    bnez a0, 5f
    la a0, m_bad_name
    mv a1, s1
    call ed_refuse_arg
5:
    li s0, 0                    # a change at every reference (each node spans the name)
6:
    ld t0, VEC_N(s4)
    bge s0, t0, 7f
    mv a0, s4
    mv a1, s0
    call vec_get
    addi s0, s0, 1
    ld t0, RL_NODE(a0)
    lwu a0, N_FILE(t0)
    lwu a1, N_START(t0)
    lwu a2, N_END(t0)
    mv a3, s1
    call ed_add
    j 6b
7:
    ld t0, RL_KEY_B(s5)         # a function's line in vo.effects
    li t1, -1
    bne t0, t1, 8f
    ld t0, RL_KEY_A(s5)
    ld t1, RL_NODE(s5)
    ld t2, N_A(t0)
    bne t1, t2, 8f
    lhu t2, N_KIND(t0)
    li t3, N_FN
    bne t2, t3, 8f
    mv a0, t0
    ld a1, 16(sp)
    mv a2, s1
    call ed_effects_line
8:
    ld a1, 16(sp)
    mv a2, s1
    la a0, m_renamed
    li a3, 0
    call msg
    la t0, ed_summary
    sd a0, 0(t0)
    ld a1, 16(sp)
    mv a2, s1
    la a0, m_renaming
    li a3, 0
    call msg
    la t0, ed_action
    sd a0, 0(t0)
    ld ra, 72(sp)
    ld s0, 64(sp)
    ld s1, 56(sp)
    ld s2, 48(sp)
    ld s3, 40(sp)
    ld s4, 32(sp)
    ld s5, 24(sp)
    addi sp, sp, 80
    ret

# void *q_decl_entry(void *matches)
# The first declaration entry of a sorted match list (q_decl_first order), or 0.
    .globl q_decl_entry
q_decl_entry:
    ld t0, VEC_N(a0)
    ld t1, VEC_DATA(a0)
1:
    beqz t0, 8f
    ld t2, 0(t1)
    ld t3, RL_ROLE(t2)
    beqz t3, 2f
    addi t1, t1, 8
    addi t0, t0, -1
    j 1b
2:
    mv a0, t2
    ret
8:
    li a0, 0
    ret

# void ed_effects_line(void *fn_decl, void *old_name, void *new_name)
# Renames the function's `path:name` line in the program's vo.effects, if there is one.
    .globl ed_effects_line
ed_effects_line:
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
    la t0, ed_files             # the entry: the last file of the program
    ld t0, 0(t0)
    ld t1, VEC_N(t0)
    addi a1, t1, -1
    mv a0, t0
    call vec_get
    call rec_find
    bltz a0, 9f
    mv s3, a0                   # the record's file id
    lwu a0, N_FILE(s0)
    call rec_relpath
    mv s4, a0                   # "path" as the record writes it
    mv a0, s3
    call file_get
    ld s5, FILE_TEXT(a0)
    ld s6, FILE_LEN(a0)
    li t0, 0                    # line start
1:
    bge t0, s6, 9f
    sd t0, 0(sp)
    ld t1, STR_LEN(s4)          # path ':' name, then ' ' or the end of the line
    ld t2, STR_LEN(s1)
    add t3, t0, t1
    addi t3, t3, 1
    add t4, t3, t2
    bgt t4, s6, 5f
    add a0, s5, t0
    addi a1, s4, STR_DATA
    mv a2, t1
    sd t3, 8(sp)
    call mem_eq
    beqz a0, 5f
    ld t3, 8(sp)
    add t5, s5, t3
    lbu t5, -1(t5)
    li t6, ':'
    bne t5, t6, 5f
    add a0, s5, t3
    addi a1, s1, STR_DATA
    ld a2, STR_LEN(s1)
    call mem_eq
    beqz a0, 5f
    ld t3, 8(sp)
    ld t2, STR_LEN(s1)
    add t4, t3, t2
    beq t4, s6, 4f
    add t5, s5, t4
    lbu t5, 0(t5)
    li t6, ' '
    beq t5, t6, 4f
    li t6, 10
    bne t5, t6, 5f
4:
    mv a0, s3
    mv a1, t3
    mv a2, t4
    mv a3, s2
    call ed_add
    j 9f
5:
    ld t0, 0(sp)                # next line
6:
    bge t0, s6, 9f
    add t1, s5, t0
    lbu t1, 0(t1)
    addi t0, t0, 1
    li t2, 10
    bne t1, t2, 6b
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

# ---- add-field (research R9) ----

# void *ed_trim(char *p, long n)
# p[0..n) without surrounding spaces, as a str.
    .globl ed_trim
ed_trim:
1:
    beqz a1, 3f
    lbu t0, 0(a0)
    li t1, ' '
    bne t0, t1, 2f
    addi a0, a0, 1
    addi a1, a1, -1
    j 1b
2:
    add t2, a0, a1
    lbu t0, -1(t2)
    li t1, ' '
    bne t0, t1, 3f
    addi a1, a1, -1
    bnez a1, 2b
3:
    tail str_new

# long ed_line_indent(long file, long off, long *start)
# The number of spaces at the start of the line containing off; *start gets the line's start.
    .globl ed_line_indent
ed_line_indent:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    sd s2, 0(sp)
    mv s0, a0
    mv s2, a2
    call pos_line
    mv a1, a0
    mv a0, s0
    call line_start
    sd a0, 0(s2)
    mv s1, a0
    mv a0, s0
    call file_get
    ld t0, FILE_TEXT(a0)
    add t0, t0, s1
    li a0, 0
1:
    lbu t1, 0(t0)
    li t2, ' '
    bne t1, t2, 2f
    addi a0, a0, 1
    addi t0, t0, 1
    j 1b
2:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    ld s2, 0(sp)
    addi sp, sp, 32
    ret

# void ed_add_field(char *record, void *spec)
# spec is "FIELD: TYPE = DEFAULT" (the words after RECORD joined by spaces).
    .globl ed_add_field
ed_add_field:
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
    mv s1, a1
    addi s2, s1, STR_DATA       # split at the first ':' and the first '=' after it
    ld s3, STR_LEN(s1)
    li t0, 0
1:
    bge t0, s3, 90f
    add t1, s2, t0
    lbu t1, 0(t1)
    li t2, ':'
    beq t1, t2, 2f
    li t2, 59                   # ';'
    beq t1, t2, 90f
    addi t0, t0, 1
    j 1b
2:
    mv t3, t0                   # ':'
    addi t0, t0, 1
3:
    bge t0, s3, 90f
    add t1, s2, t0
    lbu t1, 0(t1)
    li t2, '='
    beq t1, t2, 4f
    li t2, 59
    beq t1, t2, 90f
    addi t0, t0, 1
    j 3b
4:
    mv t4, t0                   # '='
    add t0, t4, 1               # the default must not hold ';' or a newline either
5:
    bge t0, s3, 6f
    add t1, s2, t0
    lbu t1, 0(t1)
    li t2, 59
    beq t1, t2, 90f
    li t2, 10
    beq t1, t2, 90f
    addi t0, t0, 1
    j 5b
6:
    sd t3, 0(sp)
    sd t4, 8(sp)
    mv a0, s2
    mv a1, t3
    call ed_trim
    mv s4, a0                   # field name
    ld t3, 0(sp)
    ld t4, 8(sp)
    add a0, s2, t3
    addi a0, a0, 1
    sub a1, t4, t3
    addi a1, a1, -1
    call ed_trim
    mv s5, a0                   # type
    ld t4, 8(sp)
    add a0, s2, t4
    addi a0, a0, 1
    sub a1, s3, t4
    addi a1, a1, -1
    call ed_trim
    mv s6, a0                   # default
    ld t0, STR_LEN(s5)
    beqz t0, 90f
    ld t0, STR_LEN(s6)
    beqz t0, 90f
    mv a0, s4
    call ed_valid_name
    beqz a0, 90f
    mv a0, s0                   # the record
    call str_from_cstr
    sd a0, 16(sp)
    li a1, (1 << N_RECORD)
    call q_item_named
    bnez a0, 7f
    la a0, m_not_record
    ld a1, 16(sp)
    call ed_refuse_arg
7:
    mv s3, a0                   # the record's declaration
    ld a0, N_B(s3)
    mv a1, s4
    call member_index
    bltz a0, 8f
    ld a1, 16(sp)
    mv a2, s4
    la a0, m_field_exists
    li a3, 0
    call msg
    call ed_refuse
8:
    call ed_field_decl_change
    mv a0, s3                   # every literal of the record: key (record, -2)
    li a1, -2
    li a2, 0
    call q_matches
    mv s2, a0
    li s1, 0
81:
    ld t0, VEC_N(s2)
    bge s1, t0, 82f
    mv a0, s2
    mv a1, s1
    call vec_get
    addi s1, s1, 1
    ld a0, RL_NODE(a0)
    call ed_literal_change
    j 81b
82:
    mv a1, s4
    ld a2, 16(sp)
    la a0, m_added
    li a3, 0
    call msg
    la t0, ed_summary
    sd a0, 0(t0)
    mv a1, s4
    ld a2, 16(sp)
    la a0, m_adding
    li a3, 0
    call msg
    la t0, ed_action
    sd a0, 0(t0)
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
90:
    mv a0, s1
    mv a1, s1
    la a0, m_bad_field
    call ed_refuse_arg
    j 82b

# (ed_add_field) void ed_field_decl_change(void): the declaration's new line, from ed_add_field's
# s3 (record), s4 (name), s5 (type). A local helper that reads the caller's registers.
ed_field_decl_change:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    call sb_new
    mv s0, a0
    ld a0, N_B(s3)              # the last field
    call q_last_member
    beqz a0, 5f
    mv s2, a0
    lwu s1, N_FILE(s2)
    mv a0, s1
    lwu a1, N_START(s2)
    addi a2, sp, 0
    call ed_line_indent
    mv s2, a0                   # indentation
    ld a0, N_B(s3)
    call q_last_member
    lwu a1, N_END(a0)           # after the field's line: the next line's start
    mv a0, s1
    call pos_line
    addi a1, a0, 1
    mv a0, s1
    sd a1, 8(sp)
    call file_get
    ld t0, FILE_NLINES(a0)
    ld a1, 8(sp)
    bgt a1, t0, 5f
    mv a0, s1
    call line_start
    sd a0, 8(sp)
1:
    beqz s2, 2f
    mv a0, s0
    li a1, ' '
    call sb_byte
    addi s2, s2, -1
    j 1b
2:
    mv a0, s0
    mv a1, s4
    call sb_str
    mv a0, s0
    la a1, s_colon_sp
    call sb_cstr
    mv a0, s0
    mv a1, s5
    call sb_str
    mv a0, s0
    la a1, s_semi_nl
    call sb_cstr
    mv a0, s0
    call sb_finish
    mv a3, a0
    mv a0, s1
    ld a1, 8(sp)
    mv a2, a1
    call ed_add
    j 9f
5:
    lwu s1, N_FILE(s3)          # no field: before the closing `}`
    lwu t0, N_END(s3)
    addi t0, t0, -1
    sd t0, 8(sp)
    mv a0, s0
    la a1, s_indent
    call sb_cstr
    j 2b
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    addi sp, sp, 48
    ret

# void *q_last_member(void *members)
# The last non-comment member of a member vec, or 0.
    .globl q_last_member
q_last_member:
    ld t0, VEC_N(a0)
    ld t1, VEC_DATA(a0)
    li a0, 0
1:
    beqz t0, 9f
    addi t0, t0, -1
    slli t2, t0, 3
    add t2, t1, t2
    ld t2, 0(t2)
    lhu t3, N_KIND(t2)
    li t4, N_COMMENT
    beq t3, t4, 1b
    mv a0, t2
9:
    ret

# (ed_add_field) void ed_literal_change(void *literal): `, FIELD: DEFAULT` after its last field,
# from ed_add_field's s4 (name) and s6 (default). A local helper that reads the caller's registers.
ed_literal_change:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s3, a0
    lwu s1, N_FILE(s3)
    call sb_new
    mv s0, a0
    ld t0, N_B(s3)              # the initialisers
    ld t1, VEC_N(t0)
    beqz t1, 5f
    addi a1, t1, -1
    mv a0, t0
    call vec_get
    lwu s2, N_END(a0)           # after the last one
    mv a0, s1
    mv a1, s2
    call pos_line
    sd a0, 0(sp)
    lwu a1, N_END(s3)
    addi a1, a1, -1
    mv a0, s1
    call pos_line
    ld t0, 0(sp)
    beq t0, a0, 2f
    mv a0, s0                   # the closing brace on a later line: a new line, same indentation
    li a1, ','
    call sb_byte
    mv a0, s0
    li a1, 10
    call sb_byte
    ld t0, N_B(s3)
    ld t1, VEC_N(t0)
    addi a1, t1, -1
    mv a0, t0
    call vec_get
    lwu a1, N_START(a0)
    mv a0, s1
    addi a2, sp, 0
    call ed_line_indent
    mv s3, a0
3:
    beqz s3, 4f
    mv a0, s0
    li a1, ' '
    call sb_byte
    addi s3, s3, -1
    j 3b
2:
    mv a0, s0
    la a1, s_comma_sp
    call sb_cstr
4:
    mv a0, s0
    mv a1, s4
    call sb_str
    mv a0, s0
    la a1, s_colon_sp
    call sb_cstr
    mv a0, s0
    mv a1, s6
    call sb_str
    mv a0, s0
    call sb_finish
    mv a3, a0
    mv a0, s1
    mv a1, s2
    mv a2, s2
    call ed_add
    j 9f
5:
    lwu s2, N_END(s3)           # no initialiser (only reachable for records with no fields)
    addi s2, s2, -1
    mv a0, s0
    li a1, ' '
    call sb_byte
    j 4b
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# ---- verification (research R7) ----

# void ed_save_errors(void)
# Remembers the first check's errors as {code, path}.
    .globl ed_save_errors
ed_save_errors:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    call vec_new
    la t0, ed_old_errs
    sd a0, 0(t0)
    li s0, 0
1:
    la t0, diag_n
    ld t0, 0(t0)
    bge s0, t0, 9f
    la t0, diag_arr
    ld t0, 0(t0)
    slli t1, s0, 3
    add t0, t0, t1
    ld s1, 0(t0)
    addi s0, s0, 1
    ld t0, D_SEV(s1)
    li t1, SEV_ERROR
    bne t0, t1, 1b
    li a0, 16
    call alloc
    ld t0, D_CODE(s1)
    sd t0, 0(a0)
    sd a0, 0(sp)
    ld a0, D_FILE(s1)
    call file_get
    ld t0, FILE_PATH(a0)
    ld a1, 0(sp)
    sd t0, 8(a1)
    la t0, ed_old_errs
    ld a0, 0(t0)
    call vec_push
    j 1b
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# long ed_recheck(void)
# Checks the edited program (generation 1) with the reference log on; the new entry's file id.
    .globl ed_recheck
ed_recheck:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    call diag_reset
    la t0, q_log
    sd zero, 0(t0)
    la t0, q_exprs
    sd zero, 0(t0)
    la t0, ld_hard
    sd zero, 0(t0)
    la t0, p_hard
    sd zero, 0(t0)
    la t0, te_related
    sd zero, 0(t0)
    la t0, cur_gen
    li t1, 1
    sd t1, 0(t0)
    call vec_new
    la t0, ed_new_ids
    sd a0, 0(t0)
    li s0, 0
    li s3, -1                   # the new entry
1:
    la t0, ed_files
    ld s1, 0(t0)
    ld t0, VEC_N(s1)
    bge s0, t0, 2f
    mv a0, s1
    mv a1, s0
    call vec_get
    mv s2, a0
    call ed_text
    mv t1, a0
    mv a0, s2
    sd t1, 0(sp)
    call file_get
    ld a0, FILE_PATH(a0)
    ld t1, 0(sp)
    addi a1, t1, STR_DATA
    ld a2, STR_LEN(t1)
    call file_add
    mv s3, a0
    mv a1, a0
    la t0, ed_new_ids
    ld a0, 0(t0)
    call vec_push
    addi s0, s0, 1
    j 1b
2:
    la t0, q_collect
    li t1, 1
    sd t1, 0(t0)
    mv a0, s3                   # the entry is the last file of the program
    li a1, 0
    call check_program
    call q_members_all
    la t0, q_collect
    sd zero, 0(t0)
    mv a0, s3
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# long ed_old_file(long new_file)
# The generation-0 id of a generation-1 program file (or new_file itself).
    .globl ed_old_file
ed_old_file:
    la t0, ed_new_ids
    ld t0, 0(t0)
    beqz t0, 9f
    ld t1, VEC_N(t0)
    ld t2, VEC_DATA(t0)
    li t3, 0
1:
    bge t3, t1, 9f
    slli t4, t3, 3
    add t4, t2, t4
    ld t4, 0(t4)
    beq t4, a0, 2f
    addi t3, t3, 1
    j 1b
2:
    la t0, ed_files
    ld t0, 0(t0)
    ld t0, VEC_DATA(t0)
    slli t4, t3, 3
    add t0, t0, t4
    ld a0, 0(t0)
9:
    ret

# long ed_new_file(long old_file)
    .globl ed_new_file
ed_new_file:
    la t0, ed_files
    ld t0, 0(t0)
    ld t1, VEC_N(t0)
    ld t2, VEC_DATA(t0)
    li t3, 0
1:
    bge t3, t1, 9f
    slli t4, t3, 3
    add t4, t2, t4
    ld t4, 0(t4)
    beq t4, a0, 2f
    addi t3, t3, 1
    j 1b
2:
    la t0, ed_new_ids
    ld t0, 0(t0)
    ld t0, VEC_DATA(t0)
    slli t4, t3, 3
    add t0, t0, t4
    ld a0, 0(t0)
9:
    ret

# void ed_map_span_back(long *file, long *start, long *end)
# A span of the edited text (generation 1) as a span of the current text.
    .globl ed_map_span_back
ed_map_span_back:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    sd s2, 0(sp)
    mv s0, a0
    mv s1, a1
    mv s2, a2
    ld a0, 0(s0)
    call ed_old_file
    sd a0, 0(s0)
    ld a1, 0(s1)
    li a2, 1
    call ed_map
    sd a0, 0(s1)
    ld a0, 0(s0)
    ld a1, 0(s2)
    li a2, 1
    call ed_map
    sd a0, 0(s2)
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    ld s2, 0(sp)
    addi sp, sp, 32
    ret

# long ed_new_errors(void)
# Keeps only the second check's errors that the first check did not have (by code and file),
# with their spans mapped to the current text; returns their number, or -1 when an error of the
# first check disappeared.
    .globl ed_new_errors
ed_new_errors:
    addi sp, sp, -64
    sd ra, 56(sp)
    sd s0, 48(sp)
    sd s1, 40(sp)
    sd s2, 32(sp)
    sd s3, 24(sp)
    sd s4, 16(sp)
    la t0, ed_old_errs          # unmatched first-check errors
    ld a0, 0(t0)
    ld s4, VEC_N(a0)
    li s0, 0                    # read index
    li s1, 0                    # kept
1:
    la t0, diag_n
    ld t0, 0(t0)
    bge s0, t0, 7f
    la t0, diag_arr
    ld t0, 0(t0)
    slli t1, s0, 3
    add t0, t0, t1
    ld s2, 0(t0)
    addi s0, s0, 1
    ld t0, D_SEV(s2)
    li t1, SEV_ERROR
    bne t0, t1, 1b              # warnings are dropped
    mv a0, s2
    call ed_match_old
    beqz a0, 2f
    addi s4, s4, -1
    j 1b
2:
    sd zero, D_FIXES(s2)        # fixes would be for the edited text
    addi a0, s2, D_FILE
    addi a1, s2, D_START
    addi a2, s2, D_END
    call ed_map_span_back
    ld s3, D_RELATED(s2)
3:
    beqz s3, 4f
    addi a0, s3, FX_FILE
    addi a1, s3, FX_START
    addi a2, s3, FX_END
    call ed_map_span_back
    ld s3, FX_NEXT(s3)
    j 3b
4:
    la t0, diag_arr
    ld t0, 0(t0)
    slli t1, s1, 3
    add t0, t0, t1
    sd s2, 0(t0)
    addi s1, s1, 1
    j 1b
7:
    la t0, diag_n
    sd s1, 0(t0)
    la t0, diag_errors
    sd s1, 0(t0)
    la t0, diag_warnings
    sd zero, 0(t0)
    mv a0, s1
    beqz s4, 9f
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

# long ed_match_old(void *diag)
# 1 when an unmatched first-check error has the diagnostic's code and file path (it is then used up).
    .globl ed_match_old
ed_match_old:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    ld a0, D_FILE(s0)
    call file_get
    ld s1, FILE_PATH(a0)
    li s2, 0
1:
    la t0, ed_old_errs
    ld t0, 0(t0)
    ld t1, VEC_N(t0)
    li a0, 0
    bge s2, t1, 9f
    mv a0, t0
    mv a1, s2
    call vec_get
    mv s3, a0
    addi s2, s2, 1
    ld a0, 0(s3)
    beqz a0, 1b                 # used up
    ld a1, D_CODE(s0)
    call cstr_eq
    beqz a0, 1b
    ld a0, 8(s3)
    mv a1, s1
    call str_eq
    beqz a0, 1b
    sd zero, 0(s3)
    li a0, 1
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# void ed_check_refs(void)
# Rename: in the second check, the target (found by its declaration's new place) must have
# exactly the renamed references.
    .globl ed_check_refs
ed_check_refs:
    addi sp, sp, -64
    sd ra, 56(sp)
    sd s0, 48(sp)
    sd s1, 40(sp)
    sd s2, 32(sp)
    sd s3, 24(sp)
    sd s4, 16(sp)
    la t0, ed_decl
    ld t0, 0(t0)
    ld t0, RL_NODE(t0)
    lwu s0, N_FILE(t0)
    lwu a1, N_START(t0)
    mv a0, s0
    li a2, 0
    call ed_map
    mv s1, a0                   # the declaration's new start
    mv a0, s0
    call ed_new_file
    mv s0, a0
    li s2, 0                    # its entry in the new log
1:
    la t0, q_log
    ld t0, 0(t0)
    beqz t0, 80f
    ld t1, VEC_N(t0)
    bge s2, t1, 80f
    mv a0, t0
    mv a1, s2
    call vec_get
    addi s2, s2, 1
    ld t0, RL_ROLE(a0)
    bnez t0, 1b
    ld t0, RL_NODE(a0)
    lwu t1, N_FILE(t0)
    bne t1, s0, 1b
    lwu t1, N_START(t0)
    bne t1, s1, 1b
    mv s3, a0
    ld a0, RL_KEY_A(s3)
    ld a1, RL_KEY_B(s3)
    li a2, 0
    call q_matches
    mv s4, a0                   # its references now
    la t0, ed_sites
    ld t0, 0(t0)
    ld t1, VEC_N(t0)
    ld t2, VEC_N(s4)
    bne t1, t2, 80f
    li s2, 0                    # each one is a renamed place
2:
    ld t0, VEC_N(s4)
    bge s2, t0, 9f
    mv a0, s4
    mv a1, s2
    call vec_get
    addi s2, s2, 1
    ld t0, RL_NODE(a0)
    lwu a0, N_FILE(t0)
    lwu a1, N_START(t0)
    call ed_is_site
    beqz a0, 80f
    j 2b
80:
    la t0, ed_summary
    ld a1, 0(t0)
    la a0, m_changes_meaning
    la t0, ed_decl
    ld t0, 0(t0)
    ld t0, RL_NODE(t0)
    sd t0, 0(sp)
    call sb_new
    mv s0, a0
    ld t0, 0(sp)
    lwu a1, N_FILE(t0)
    lwu a2, N_START(t0)
    call human_loc
    mv a0, s0
    call sb_finish
    mv a2, a0
    la t0, ed_action
    ld a1, 0(t0)
    la a0, m_changes_meaning
    li a3, 0
    call msg
    call ed_refuse
9:
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    addi sp, sp, 64
    ret

# long ed_is_site(long new_file, long new_start)
# 1 when a renamed reference starts there (in the edited text).
    .globl ed_is_site
ed_is_site:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    mv s1, a1
    call ed_old_file
    mv s0, a0
    mv a1, s1
    li a2, 1
    call ed_map
    mv s1, a0                   # the start in the current text
    li s2, 0
1:
    la t0, ed_sites
    ld t0, 0(t0)
    ld t1, VEC_N(t0)
    li a0, 0
    bge s2, t1, 9f
    mv a0, t0
    mv a1, s2
    call vec_get
    addi s2, s2, 1
    ld t0, RL_NODE(a0)
    lwu t1, N_FILE(t0)
    bne t1, s0, 1b
    lwu t1, N_START(t0)
    bne t1, s1, 1b
    li a0, 1
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    addi sp, sp, 48
    ret

# ---- output and writing (research R10) ----

# void ed_print_change(void *change)
    .globl ed_print_change
ed_print_change:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    call sb_new
    mv s1, a0
    la t0, opt_json
    ld t0, 0(t0)
    beqz t0, 5f
    la a1, j_edit_head
    call sb_cstr
    ld a0, CH_FILE(s0)
    call file_get
    ld a1, FILE_PATH(a0)
    mv a0, s1
    call json_str_into
    mv a0, s1
    la a1, j_start_key
    call sb_cstr
    mv a0, s1
    ld a1, CH_FILE(s0)
    ld a2, CH_START(s0)
    call json_pos
    mv a0, s1
    la a1, j_end_key
    call sb_cstr
    mv a0, s1
    ld a1, CH_FILE(s0)
    ld a2, CH_END(s0)
    call json_pos
    mv a0, s1
    la a1, j_text
    call sb_cstr
    mv a0, s1
    ld a1, CH_OLD(s0)
    call json_str_into
    mv a0, s1
    la a1, j_repl
    call sb_cstr
    mv a0, s1
    ld a1, CH_NEW(s0)
    call json_str_into
    mv a0, s1
    la a1, j_close
    call sb_cstr
    j 8f
5:
    ld a1, CH_FILE(s0)          # FILE:LINE:COL OLD -> NEW, or FILE:LINE:COL + NEW
    ld a2, CH_START(s0)
    call human_loc
    ld t0, CH_OLD(s0)
    ld t0, STR_LEN(t0)
    beqz t0, 6f
    mv a0, s1
    la a1, s_space
    call sb_cstr
    mv a0, s1
    ld a1, CH_OLD(s0)
    call sb_visible
    mv a0, s1
    la a1, s_arrow
    call sb_cstr
    j 7f
6:
    mv a0, s1
    la a1, s_plus
    call sb_cstr
7:
    mv a0, s1
    ld a1, CH_NEW(s0)
    call sb_visible
    mv a0, s1
    li a1, 10
    call sb_byte
8:
    mv a0, s1
    call q_out
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# long ed_changed_files(void)
# The number of files with at least one change.
    .globl ed_changed_files
ed_changed_files:
    la t0, ed_changes
    ld t0, 0(t0)
    li a0, 0
    beqz t0, 9f
    ld t1, VEC_N(t0)
    ld t2, VEC_DATA(t0)
    li t3, -1
1:
    beqz t1, 9f
    ld t4, 0(t2)
    ld t4, CH_FILE(t4)
    addi t2, t2, 8
    addi t1, t1, -1
    beq t4, t3, 1b
    mv t3, t4
    addi a0, a0, 1
    j 1b
9:
    ret

# void ed_finish(long written)
# The last line: what was done, the counts, the dry-run note; or the JSON summary.
    .globl ed_finish
ed_finish:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    mv s0, a0
    call sb_new
    mv s1, a0
    la t0, ed_changes
    ld t0, 0(t0)
    li s2, 0
    beqz t0, 1f
    ld s2, VEC_N(t0)
1:
    la t0, opt_json
    ld t0, 0(t0)
    beqz t0, 5f
    mv a0, s1
    la a1, j_changes
    call sb_cstr
    mv a0, s1
    mv a1, s2
    call int_to_dec
    mv a0, s1
    la a1, j_changed
    call sb_cstr
    call ed_changed_files
    mv a1, a0
    mv a0, s1
    call int_to_dec
    mv a0, s1
    la a1, j_written
    call sb_cstr
    la a1, j_true
    bnez s0, 2f
    la a1, j_false
2:
    mv a0, s1
    call sb_cstr
    mv a0, s1
    li a1, 0
    call sb_byte
    ld a0, SB_PTR(s1)
    call str_from_cstr
    mv a4, a0
    la t0, ed_files
    ld t0, 0(t0)
    ld a3, VEC_N(t0)
    call cmd_name
    mv a2, a0
    li a0, 1
    la t0, report_fd
    ld a1, 0(t0)
    call summary_emit
    j 9f
5:
    mv a0, s1
    la t0, ed_summary
    ld a1, 0(t0)
    call sb_visible_plain
    mv a0, s1
    la a1, h_changes
    call sb_cstr
    mv a0, s1
    mv a1, s2
    call int_to_dec
    la a1, h_changes_tail
    li t0, 1
    bne s2, t0, 6f
    la a1, h_change_tail
6:
    mv a0, s1
    call sb_cstr
    call ed_changed_files
    mv s2, a0
    mv a1, a0
    mv a0, s1
    call int_to_dec
    la a1, h_files_tail
    li t0, 1
    bne s2, t0, 7f
    la a1, h_file_tail
7:
    mv a0, s1
    call sb_cstr
    bnez s0, 8f
    mv a0, s1
    la a1, h_dry
    call sb_cstr
8:
    mv a0, s1
    li a1, 10
    call sb_byte
    mv a0, s1
    call q_out
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    addi sp, sp, 48
    ret

# void sb_visible_plain(void *sb, void *str)
# The summary without its backquotes (text output reads "renamed a to b").
    .globl sb_visible_plain
sb_visible_plain:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    sd s2, 0(sp)
    mv s0, a0
    addi s1, a1, STR_DATA
    ld s2, STR_LEN(a1)
1:
    beqz s2, 9f
    lbu a1, 0(s1)
    addi s1, s1, 1
    addi s2, s2, -1
    li t0, 96                   # backquote
    beq a1, t0, 1b
    mv a0, s0
    call sb_byte
    j 1b
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    ld s2, 0(sp)
    addi sp, sp, 32
    ret

# void *ed_tmp_path(long file)
# "<path>.vo-edit.tmp", NUL-terminated.
    .globl ed_tmp_path
ed_tmp_path:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    call file_get
    ld s0, FILE_PATH(a0)
    call sb_new
    mv s1, a0
    mv a1, s0
    call sb_str
    mv a0, s1
    la a1, s_tmp_suffix
    call sb_cstr
    mv a0, s1
    call sb_finish
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# long ed_write_tmp(long file, void *text)
# Writes text to the file's temporary path; 0 on success.
    .globl ed_write_tmp
ed_write_tmp:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    sd s2, 0(sp)
    mv s1, a1
    call ed_tmp_path
    addi a0, a0, STR_DATA
    li a1, O_WRCREAT
    call sys_open
    bltz a0, 8f
    mv s0, a0
    addi a1, s1, STR_DATA
    ld a2, STR_LEN(s1)
    call write_checked
    mv s2, a0
    mv a0, s0
    call sys_close
    mv a0, s2
    j 9f
8:
    li a0, 1
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    ld s2, 0(sp)
    addi sp, sp, 32
    ret

# void ed_unlink_tmps(void)
# Removes the temporary files written so far (after a failure).
    .globl ed_unlink_tmps
ed_unlink_tmps:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    li s0, 0
    li s1, -1
1:
    la t0, ed_changes
    ld t0, 0(t0)
    ld t1, VEC_N(t0)
    bge s0, t1, 9f
    mv a0, t0
    mv a1, s0
    call vec_get
    addi s0, s0, 1
    ld a0, CH_FILE(a0)
    beq a0, s1, 1b
    mv s1, a0
    call ed_tmp_path
    addi a1, a0, STR_DATA
    li a0, AT_FDCWD
    li a2, 0
    li a7, SYS_unlinkat
    ecall
    j 1b
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void ed_write_all(void)
# Every changed file to its temporary path, then each renamed over its file. A failed temporary
# write removes the temporary files; a failed rename writes the files already renamed back with
# their original text. Either way T0006, and nothing is left changed (FR-012).
    .globl ed_write_all
ed_write_all:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    li s0, 0
    li s1, -1
1:
    la t0, ed_changes           # temporary files
    ld t0, 0(t0)
    ld t1, VEC_N(t0)
    bge s0, t1, 3f
    mv a0, t0
    mv a1, s0
    call vec_get
    addi s0, s0, 1
    ld s2, CH_FILE(a0)
    beq s2, s1, 1b
    mv s1, s2
    mv a0, s2
    call ed_text
    mv a1, a0
    mv a0, s2
    call ed_write_tmp
    beqz a0, 1b
    call ed_unlink_tmps
    mv a0, s2
    j 90f
3:
    li s0, 0                    # then the renames
    li s1, -1
4:
    la t0, ed_changes
    ld t0, 0(t0)
    ld t1, VEC_N(t0)
    bge s0, t1, 9f
    mv a0, t0
    mv a1, s0
    call vec_get
    addi s0, s0, 1
    ld s2, CH_FILE(a0)
    beq s2, s1, 4b
    mv s1, s2
    mv a0, s2
    call ed_tmp_path
    mv s3, a0
    mv a0, s2
    call file_get
    ld a1, FILE_PATH(a0)
    addi a1, a1, STR_DATA
    addi a0, s3, STR_DATA
    call sys_rename
    bgez a0, 4b
    mv a0, s2                   # undo: the files renamed so far get their old text back
    mv a1, s0
    call ed_undo
    call ed_unlink_tmps
    mv a0, s2
    j 90f
90:
    call file_get
    ld a1, FILE_PATH(a0)
    la t0, ed_action
    ld t1, 0(t0)
    mv a2, a1
    mv a1, t1
    la a0, m_cannot_write
    li a3, 0
    call msg
    call ed_refuse
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# void ed_undo(long failed_file, long upto)
# Writes the original text back to every changed file before failed_file (changes [0, upto)).
    .globl ed_undo
ed_undo:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s3, a0
    li s0, 0
    li s1, -1
1:
    la t0, ed_changes
    ld t0, 0(t0)
    ld t1, VEC_N(t0)
    bge s0, t1, 9f
    mv a0, t0
    mv a1, s0
    call vec_get
    addi s0, s0, 1
    ld s2, CH_FILE(a0)
    beq s2, s3, 9f
    beq s2, s1, 1b
    mv s1, s2
    mv a0, s2
    call file_get
    ld a1, FILE_TEXT(a0)
    ld a2, FILE_LEN(a0)
    ld a0, FILE_PATH(a0)
    sd a0, 0(sp)
    mv a0, a1
    mv a1, a2
    call str_new
    mv a1, a0
    ld a0, 0(sp)
    call write_file_safely
    j 1b
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# ---- the command ----

# int cmd_edit(void)
# `vo edit rename|add-field ENTRY ...` (contracts/cli.md).
    .globl cmd_edit
cmd_edit:
    addi sp, sp, -64
    sd ra, 56(sp)
    sd s0, 48(sp)
    sd s1, 40(sp)
    sd s2, 32(sp)
    sd s3, 24(sp)
    la t0, pos_args
    ld s0, 0(t0)
    la t0, pos_n
    ld s1, 0(t0)
    bnez s1, 1f
    la a0, m_edit_none
    call str_from_cstr
    call usage_error
1:
    ld a0, 0(s0)
    la a1, w_rename
    call cstr_eq
    li s2, 1
    bnez a0, 2f
    ld a0, 0(s0)
    la a1, w_add_field
    call cstr_eq
    li s2, 2
    bnez a0, 2f
    la a0, m_edit_sub
    ld a1, 0(s0)
    call usage_error_arg
2:
    li t0, 1                    # rename: exactly 4 words; add-field: at least 4
    bne s2, t0, 3f
    li t0, 4
    beq s1, t0, 4f
    la a0, m_rename_args
    call str_from_cstr
    call usage_error
3:
    li t0, 4
    bge s1, t0, 4f
    la a0, m_add_field_args
    call str_from_cstr
    call usage_error
4:
    ld a0, 8(s0)                # the first check, with the reference log
    call q_load
    call ld_files
    la t0, ed_files
    sd a0, 0(t0)
    call ed_save_errors
    li t0, 1
    bne s2, t0, 5f
    ld a0, 16(s0)
    ld a1, 24(s0)
    call ed_rename
    j 6f
5:
    call sb_new                 # add-field: the words after RECORD, joined by spaces
    mv s3, a0
    li t0, 3
    sd t0, 0(sp)
51:
    ld t0, 0(sp)
    bge t0, s1, 52f
    li t1, 3
    beq t0, t1, 53f
    mv a0, s3
    li a1, ' '
    call sb_byte
53:
    ld t0, 0(sp)
    slli t1, t0, 3
    add t1, s0, t1
    ld a1, 0(t1)
    mv a0, s3
    call sb_cstr
    ld t0, 0(sp)
    addi t0, t0, 1
    sd t0, 0(sp)
    j 51b
52:
    mv a0, s3
    call sb_finish
    mv a1, a0
    ld a0, 16(s0)
    call ed_add_field
6:
    call ed_recheck             # the second check, on the edited text
    la t0, ld_hard
    ld t0, 0(t0)
    bnez t0, 61f
    call ed_new_errors
    beqz a0, 7f
    bgtz a0, 61f
    la t0, ed_action
    ld a1, 0(t0)
    la a0, m_changes_errors
    li a2, 0
    li a3, 0
    call msg
    call ed_refuse
61:
    la t0, diag_n               # the new errors, then T0006
    ld s3, 0(t0)
    call sb_new
    sd a0, 8(sp)
    li t0, 1
    bne s3, t0, 62f
    la a0, m_new_error
    call str_from_cstr
    j 63f
62:
    ld a0, 8(sp)
    mv a1, s3
    call int_to_dec
    ld a0, 8(sp)
    call sb_finish
    mv a1, a0
    la a0, m_new_errors
    li a2, 0
    li a3, 0
    call msg
63:
    mv a2, a0
    la t0, ed_action
    ld a1, 0(t0)
    la a0, m_would_not_check
    li a3, 0
    call msg
    sd a0, 16(sp)
    la t0, opt_json
    ld a0, 0(t0)
    la t0, report_fd
    ld a1, 0(t0)
    call diag_emit_all
    ld a0, 16(sp)
    call ed_refuse
7:
    li t0, 1                    # rename: the references must be exactly the renamed places
    bne s2, t0, 8f
    call ed_check_refs
8:
    la t0, opt_dry_run          # accepted: write the files (unless --dry-run), then list the changes
    ld t0, 0(t0)
    bnez t0, 80f
    call ed_write_all
80:
    li s3, 0
81:
    la t0, ed_changes
    ld t0, 0(t0)
    beqz t0, 82f
    ld t1, VEC_N(t0)
    bge s3, t1, 82f
    mv a0, t0
    mv a1, s3
    call vec_get
    call ed_print_change
    addi s3, s3, 1
    j 81b
82:
    la t0, opt_dry_run
    ld s3, 0(t0)
    seqz s3, s3
    mv a0, s3
    call ed_finish
    li a0, 0
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    addi sp, sp, 64
    ret
