# effects_record.s: the effects record `vo.effects` (contracts/effects-record.md).
#
# An entry is one top-level function or test: path (relative to the project root), name
# (`name` or `test "name"`), declared effect set, and where to point diagnostics (the function's
# name, or the record line for entries read from the file). The record is found in the nearest
# ancestor directory of the entry file and registered in the file table, so E0510 and W0502 can
# point at its lines. Comparing the record with the program yields changes: added, widened
# (growth: E0504 in `vo check`), removed, narrowed (W0502) and renamed (W0503).

    .include "defs.inc"

    .equ RE_PATH, 0
    .equ RE_NAME, 8
    .equ RE_SET, 16
    .equ RE_ITEM, 24                # the function or test node, 0 for record entries
    .equ RE_FILE, 32
    .equ RE_START, 40
    .equ RE_END, 48
    .equ RE_USED, 56                # matched by a change already
    .equ RE_SIZE, 64
    .equ RC_KIND, 0
    .equ RC_NEW, 8
    .equ RC_OLD, 16
    .equ RC_DELTA, 24
    .equ RC_SIZE, 32
    .equ CH_ADDED, 1
    .equ CH_WIDENED, 2
    .equ CH_REMOVED, 3
    .equ CH_NARROWED, 4
    .equ CH_RENAMED, 5

    .section .bss
    .align 3
    .globl rec_root
rec_root:   .space 8                # str: the project root (absolute, "" for /)
rec_tmp:    .space 8

    .section .rodata
e0504: .string "E0504"
e0510: .string "E0510"
w0502: .string "W0502"
w0503: .string "W0503"
w0504: .string "W0504"
s_record:   .string "/vo.effects"
s_pure:     .string " pure"
s_uses:     .string " uses "
s_header:   .string "# vo effects record: update with `vo effects --update`\n"
m_grew:     .string "`$1` gains $2; approve with `vo effects --update`."
m_shrank:   .string "`$1` no longer uses $2."
m_gone:     .string "`$1` was removed; it used $2."
m_renamed:  .string "`$1` was `$2`; its effects are unchanged ($3)."
m_via:      .string "`$1` calls `$2`, which uses $3."
m_bad_line: .string "This line of vo.effects cannot be read: write `<path>:<name> uses <effects>` or `<path>:<name> pure`."
m_no_rec:   .string "This program has no effects record: create vo.effects with `vo effects --update`."
change_words:
    .dword 0, w_added, w_widened, w_removed, w_narrowed, w_renamed
w_added:    .string "added"
w_widened:  .string "widened"
w_removed:  .string "removed"
w_narrowed: .string "narrowed"
w_renamed:  .string "renamed"
j_head:     .string "{\"v\":1,\"kind\":\"effect_change\",\"change\":\""
j_file:     .string "\",\"file\":"
j_function: .string ",\"function\":"
j_old_fn:   .string ",\"old_function\":"
j_effects:  .string ",\"effects\":"
j_before:   .string ",\"before\":"
j_after:    .string ",\"after\":"
j_via:      .string ",\"via\":["
j_calls:    .string ",\"calls\":"
j_start:    .string ",\"start\":"
j_fn_open:  .string "{\"function\":"
j_end:      .string "]}\n"
j_cap_head: .string "{\"v\":1,\"kind\":\"capabilities\",\"entry\":"
j_uses:     .string ",\"uses\":"
j_close:    .string "}\n"
s_pure_word: .string "pure"
s_uses_word: .string "uses "
h_plus:     .string "+ "
h_minus:    .string "- "
h_tilde:    .string "~ "
h_arrow:    .string " -> "
h_colon:    .string ": "
s_main:     .string "main"

    .text

# void *rec_relpath(int file)
# The file's path relative to the project root (absolute when it lies outside the root);
# "@NAME/path.vo" for a file of a listed package (spec 007).
    .globl rec_relpath
rec_relpath:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    call file_get
    ld a0, FILE_PATH(a0)
    call rt_resolve
    beqz a0, 9f
    mv s0, a0
    call pkg_relpath
    bnez a0, 9f
    la t0, rec_root
    ld s1, 0(t0)
    beqz s1, 8f
    mv a0, s0
    mv a1, s1
    call path_under
    beqz a0, 8f
    ld t0, STR_LEN(s1)          # strip "<root>/"
    addi t0, t0, 1
    addi a0, s0, STR_DATA
    add a0, a0, t0
    ld a1, STR_LEN(s0)
    sub a1, a1, t0
    call str_new
    j 9f
8:
    mv a0, s0
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# long rec_find(int entry)
# Registers the nearest vo.effects (entry's directory and up) and returns its file id, or -1.
# Sets rec_root to its directory, or to the entry's directory when there is none.
    .globl rec_find
rec_find:
    la a1, s_record
    la a2, rec_root
    tail find_up

# long find_up(int entry, char *slash_name, void **dir_out)
# Registers the nearest file named slash_name ("/vo.effects") in entry's directory or above and
# returns its file id, or -1. *dir_out: its directory, or the entry's directory when there is none.
    .globl find_up
find_up:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a1
    mv s1, a2
    call file_get
    ld a0, FILE_DIR(a0)
    call rt_resolve
    bnez a0, 1f
    call rt_start_cwd
1:
    mv a1, s0
    mv a2, s1
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    tail find_up_dir

# long find_up_dir(void *dir, char *slash_name, void **dir_out)
# find_up from an absolute directory ("" is /).
    .globl find_up_dir
find_up_dir:
    addi sp, sp, -64
    sd ra, 56(sp)
    sd s0, 48(sp)
    sd s1, 40(sp)
    sd s2, 32(sp)
    sd s3, 24(sp)
    sd s4, 16(sp)
    mv s3, a1
    mv s4, a2
    mv s0, a0                   # the directory being tried
    sd s0, 0(s4)
2:
    call sb_new
    mv s1, a0
    mv a1, s0
    call sb_str
    mv a0, s1
    mv a1, s3
    call sb_cstr
    mv a0, s1
    call sb_finish
    mv s1, a0                   # <dir>/NAME
    addi a0, s1, STR_DATA
    addi a1, sp, 0
    call read_whole_file
    bltz a0, 4f
    mv a2, a0
    ld a1, 0(sp)
    mv a0, s1
    call file_add
    mv s2, a0
    sd s0, 0(s4)
    mv a0, s2
    j 9f
4:
    ld t0, STR_LEN(s0)          # up one directory
    li a0, -1
    beqz t0, 9f
5:
    addi t0, t0, -1
    add t1, s0, t0
    lbu t1, STR_DATA(t1)
    li t2, 47
    beq t1, t2, 6f
    bnez t0, 5b
6:
    addi a0, s0, STR_DATA
    mv a1, t0
    call str_new
    mv s0, a0
    j 2b
9:
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    addi sp, sp, 64
    ret

# void *rec_entry(void *path, void *name, void *set)
    .globl rec_entry
rec_entry:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    sd s2, 0(sp)
    mv s0, a0
    mv s1, a1
    mv s2, a2
    li a0, RE_SIZE
    call alloc
    sd s0, RE_PATH(a0)
    sd s1, RE_NAME(a0)
    sd s2, RE_SET(a0)
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    ld s2, 0(sp)
    addi sp, sp, 32
    ret

# int rec_cmp(void *a, void *b)
# Order of entries: path, then name, byte-wise.
    .globl rec_cmp
rec_cmp:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    mv s1, a1
    ld a0, RE_PATH(s0)
    ld a1, RE_PATH(s1)
    call str_cmp
    bnez a0, 9f
    ld a0, RE_NAME(s0)
    ld a1, RE_NAME(s1)
    call str_cmp
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void rec_sort(void *vec)
# Insertion sort by rec_cmp (stable).
    .globl rec_sort
rec_sort:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    ld s0, VEC_DATA(a0)
    ld s1, VEC_N(a0)
    li s2, 1
1:
    bge s2, s1, 9f
    mv s3, s2
2:
    beqz s3, 3f
    slli t0, s3, 3
    add t0, s0, t0
    ld a0, -8(t0)
    ld a1, 0(t0)
    call rec_cmp
    blez a0, 3f
    slli t0, s3, 3
    add t0, s0, t0
    ld t1, -8(t0)
    ld t2, 0(t0)
    sd t2, -8(t0)
    sd t1, 0(t0)
    addi s3, s3, -1
    j 2b
3:
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

# void *rec_entries(void *files)
# Entries of every function and test in the program's files (a vec of file ids), sorted.
    .globl rec_entries
rec_entries:
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
    call vec_new
    mv s1, a0
    li s2, 0                    # file index
1:
    ld t0, VEC_N(s0)
    bge s2, t0, 8f
    mv a0, s0
    mv a1, s2
    call vec_get
    addi s2, s2, 1
    mv s3, a0                   # file id
    call file_get
    ld t0, FILE_STD(a0)         # spec 007: the standard library is not in the record
    bnez t0, 1b
    mv a0, s3
    call rec_relpath
    mv s6, a0
    mv a0, s3
    call file_get
    ld t0, FILE_AST(a0)
    beqz t0, 1b
    ld s4, N_A(t0)              # items
    li s5, 0
2:
    ld t0, VEC_N(s4)
    bge s5, t0, 1b
    mv a0, s4
    mv a1, s5
    call vec_get
    addi s5, s5, 1
    sd a0, 0(sp)
    call item_fx
    beqz a0, 2b
    ld t0, FX_DECLARED(a0)
    bnez t0, 3f
    li a0, 0
    call es_new
    mv t0, a0
3:
    sd t0, 8(sp)
    ld a0, 0(sp)
    call fx_item_name
    mv a1, a0
    mv a0, s6
    ld a2, 8(sp)
    call rec_entry
    ld t0, 0(sp)
    sd t0, RE_ITEM(a0)
    sd s3, RE_FILE(a0)
    lhu t1, N_KIND(t0)          # point at the name, or the `test` keyword
    lwu t2, N_START(t0)
    addi t3, t2, 4
    li t4, N_FN
    bne t1, t4, 4f
    ld t5, N_A(t0)
    lwu t2, N_START(t5)
    lwu t3, N_END(t5)
4:
    sd t2, RE_START(a0)
    sd t3, RE_END(a0)
    mv a1, a0
    mv a0, s1
    call vec_push
    j 2b
8:
    mv a0, s1
    call rec_sort
    mv a0, s1
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

# long rec_skip_spaces(char *p, long i, long n)
    .globl rec_skip_spaces
rec_skip_spaces:
1:
    bge a1, a2, 2f
    add t0, a0, a1
    lbu t0, 0(t0)
    li t1, 32
    bne t0, t1, 2f
    addi a1, a1, 1
    j 1b
2:
    mv a0, a1
    ret

# void *rec_parse_set(char *p, long n)
# Effects in `uses` syntax (`console, file.write("out/")`) as a set, or 0 when malformed.
    .globl rec_parse_set
rec_parse_set:
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
    li a0, 0
    call es_new
    mv s2, a0
    li s3, 0                    # i
1:
    mv a0, s0
    mv a1, s3
    mv a2, s1
    call rec_skip_spaces
    mv s3, a0
    bge s3, s1, 99f             # an effect is expected
    mv s4, s3                   # name [s4, s3)
2:
    bge s3, s1, 3f
    add t0, s0, s3
    lbu t0, 0(t0)
    li t1, 46                   # '.'
    beq t0, t1, 21f
    li t1, 95                   # '_'
    beq t0, t1, 21f
    addi t0, t0, -97            # 'a'..'z'
    li t1, 26
    bgeu t0, t1, 3f
21:
    addi s3, s3, 1
    j 2b
3:
    beq s3, s4, 99f
    add a0, s0, s4
    sub a1, s3, s4
    call es_bit_by_name
    beqz a0, 99f
    mv s5, a0                   # bit
    bge s3, s1, 4f
    add t0, s0, s3
    lbu t0, 0(t0)
    li t1, 40                   # '('
    beq t0, t1, 5f
4:
    ld t0, ES_MASK(s2)
    or t0, t0, s5
    sd t0, ES_MASK(s2)
    j 7f
5:
    addi s3, s3, 1
51:
    mv a0, s0
    mv a1, s3
    mv a2, s1
    call rec_skip_spaces
    mv s3, a0
    bge s3, s1, 99f
    add t0, s0, s3
    lbu t0, 0(t0)
    li t1, 34                   # '"'
    bne t0, t1, 99f
    addi s3, s3, 1
    call sb_new
    mv s6, a0
52:
    bge s3, s1, 99f
    add t0, s0, s3
    lbu a1, 0(t0)
    li t1, 34
    beq a1, t1, 54f
    li t1, 92                   # backslash: the next byte as it is
    bne a1, t1, 53f
    addi s3, s3, 1
    bge s3, s1, 99f
    add t0, s0, s3
    lbu a1, 0(t0)
53:
    mv a0, s6
    call sb_byte
    addi s3, s3, 1
    j 52b
54:
    addi s3, s3, 1
    mv a0, s6
    call sb_finish
    mv a2, a0
    ld t0, STR_LEN(a2)          # "./" normalizes to "" (the root directory itself)
    mv a0, s2
    mv a1, s5
    call es_add_scope
    beqz a0, 99f
    mv a0, s0
    mv a1, s3
    mv a2, s1
    call rec_skip_spaces
    mv s3, a0
    bge s3, s1, 99f
    add t0, s0, s3
    lbu t0, 0(t0)
    addi s3, s3, 1
    li t1, 44                   # ','
    beq t0, t1, 51b
    li t1, 41                   # ')'
    bne t0, t1, 99f
7:
    mv a0, s0
    mv a1, s3
    mv a2, s1
    call rec_skip_spaces
    mv s3, a0
    bge s3, s1, 8f
    add t0, s0, s3
    lbu t0, 0(t0)
    li t1, 44
    bne t0, t1, 99f
    addi s3, s3, 1
    j 1b
8:
    mv a0, s2
    j 100f
99:
    li a0, 0
100:
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

# void *rec_parse(int file)
# The entries of a registered vo.effects; E0510 for each line that cannot be read.
    .globl rec_parse
rec_parse:
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
    call file_get
    ld s1, FILE_TEXT(a0)
    ld s2, FILE_LEN(a0)
    call vec_new
    mv s3, a0
    li s4, 0                    # line start
1:
    bge s4, s2, 9f
    mv s5, s4                   # line end
2:
    bge s5, s2, 3f
    add t0, s1, s5
    lbu t0, 0(t0)
    li t1, 10
    beq t0, t1, 3f
    addi s5, s5, 1
    j 2b
3:
    beq s4, s5, 8f              # empty line
    add t0, s1, s4
    lbu t0, 0(t0)
    li t1, 35                   # comment
    beq t0, t1, 8f
    mv s6, s4                   # ':' ends the path
4:
    bge s6, s5, 7f
    add t0, s1, s6
    lbu t0, 0(t0)
    li t1, 58
    beq t0, t1, 41f
    addi s6, s6, 1
    j 4b
41:
    sub t0, s5, s4              # "... pure"?
    li t1, 5
    blt t0, t1, 5f
    add a0, s1, s5
    addi a0, a0, -5
    la a1, s_pure
    li a2, 5
    call mem_eq
    beqz a0, 5f
    addi t0, s5, -5
    sd t0, 8(sp)                # name end
    li a0, 0
    call es_new
    sd a0, 16(sp)
    j 6f
5:
    addi s7, s6, 1              # " uses " after the name
51:
    addi t0, s7, 6
    bgt t0, s5, 7f
    add a0, s1, s7
    la a1, s_uses
    li a2, 6
    call mem_eq
    bnez a0, 52f
    addi s7, s7, 1
    j 51b
52:
    sd s7, 8(sp)
    add a0, s1, s7
    addi a0, a0, 6
    addi t0, s7, 6
    sub a1, s5, t0
    call rec_parse_set
    beqz a0, 7f
    sd a0, 16(sp)
6:
    mv a0, s1                   # path [s4, s6), name (s6, name end)
    add a0, a0, s4
    sub a1, s6, s4
    call str_new
    sd a0, 0(sp)
    add a0, s1, s6
    addi a0, a0, 1
    ld t0, 8(sp)
    sub a1, t0, s6
    addi a1, a1, -1
    call str_new
    mv a1, a0
    ld a0, 0(sp)
    ld a2, 16(sp)
    call rec_entry
    sd s0, RE_FILE(a0)
    sd s4, RE_START(a0)
    sd s5, RE_END(a0)
    mv a1, a0
    mv a0, s3
    call vec_push
    j 8f
7:
    la a0, m_bad_line
    call str_from_cstr
    mv a5, a0
    la a0, e0510
    li a1, SEV_ERROR
    mv a2, s0
    mv a3, s4
    mv a4, s5
    call diag_add
8:
    addi s4, s5, 1
    j 1b
9:
    mv a0, s3
    call rec_sort
    mv a0, s3
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

# void *rec_change(long kind, void *new, void *old, void *delta)
    .globl rec_change
rec_change:
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
    li a0, RC_SIZE
    call alloc
    sd s0, RC_KIND(a0)
    sd s1, RC_NEW(a0)
    sd s2, RC_OLD(a0)
    sd s3, RC_DELTA(a0)
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# void *rec_lookup(void *entries, void *entry)
# The entry with the same path and name, or 0.
    .globl rec_lookup
rec_lookup:
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
    bge s2, t0, 8f
    mv a0, s0
    mv a1, s2
    call vec_get
    addi s2, s2, 1
    mv s3, a0
    mv a1, s1
    call rec_cmp
    bnez a0, 1b
    mv a0, s3
    j 9f
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

# void *rec_compare(void *old, void *new)
# The changes from the record to the program, in the order of the new entries, then removals.
    .globl rec_compare
rec_compare:
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
    mv s0, a0
    mv s1, a1
    call vec_new
    mv s2, a0
    li s3, 0
1:
    ld t0, VEC_N(s1)
    bge s3, t0, 5f
    mv a0, s1
    mv a1, s3
    call vec_get
    addi s3, s3, 1
    mv s4, a0                   # new entry
    mv a0, s0
    mv a1, s4
    call rec_lookup
    mv s5, a0                   # old entry
    bnez s5, 2f
    ld a0, RE_SET(s4)           # a new function: growth unless pure
    call es_is_empty
    bnez a0, 1b
    li a0, CH_ADDED
    mv a1, s4
    li a2, 0
    ld a3, RE_SET(s4)
    j 4f
2:
    li t0, 1
    sd t0, RE_USED(s5)
    ld a0, RE_SET(s5)
    ld a1, RE_SET(s4)
    call es_missing
    mv s6, a0                   # growth
    call es_is_empty
    bnez a0, 3f
    ld a0, RE_SET(s5)
    mv a1, s6
    call es_lacks_bits
    li t0, CH_WIDENED
    beqz a0, 21f
    li t0, CH_ADDED
21:
    mv a0, t0
    mv a1, s4
    mv a2, s5
    mv a3, s6
    call rec_change             # growth; a reduction of other effects is reported too
    mv a1, a0
    mv a0, s2
    call vec_push
3:
    ld a0, RE_SET(s4)
    ld a1, RE_SET(s5)
    call es_missing
    mv s6, a0                   # reduction
    call es_is_empty
    bnez a0, 1b
    ld a0, RE_SET(s4)
    mv a1, s6
    call es_lacks_bits
    li t0, CH_NARROWED
    beqz a0, 31f
    li t0, CH_REMOVED
31:
    mv a0, t0
    mv a1, s4
    mv a2, s5
    mv a3, s6
4:
    call rec_change
    mv a1, a0
    mv a0, s2
    call vec_push
    j 1b
5:
    li s3, 0                    # old entries no longer in the program
6:
    ld t0, VEC_N(s0)
    bge s3, t0, 9f
    mv a0, s0
    mv a1, s3
    call vec_get
    addi s3, s3, 1
    mv s4, a0
    ld t0, RE_USED(s4)
    bnez t0, 6b
    ld a0, RE_SET(s4)
    call es_is_empty
    bnez a0, 6b
    li s5, 0                    # a new function in the same file with the same effects: a rename
7:
    ld t0, VEC_N(s2)
    bge s5, t0, 8f
    mv a0, s2
    mv a1, s5
    call vec_get
    addi s5, s5, 1
    mv s6, a0
    ld t0, RC_KIND(s6)
    li t1, CH_ADDED
    bne t0, t1, 7b
    ld t0, RC_OLD(s6)
    bnez t0, 7b
    ld s7, RC_NEW(s6)
    ld a0, RE_PATH(s7)
    ld a1, RE_PATH(s4)
    call str_eq
    beqz a0, 7b
    ld a0, RE_SET(s7)
    ld a1, RE_SET(s4)
    call es_equal
    beqz a0, 7b
    li t0, CH_RENAMED           # ponytail: pairs by identical effects in file order; match bodies if mispairing shows up
    sd t0, RC_KIND(s6)
    sd s4, RC_OLD(s6)
    j 6b
8:
    li a0, CH_REMOVED
    li a1, 0
    mv a2, s4
    ld a3, RE_SET(s4)
    call rec_change
    mv a1, a0
    mv a0, s2
    call vec_push
    j 6b
9:
    mv a0, s2
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

# void rec_via(void *diag, void *sb, void *entry, void *old_set)
# For each call in the entry's function that brings in effects beyond old_set: a related note on
# diag (when diag != 0) and a JSON `via` element in sb (when sb != 0).
    .globl rec_via
rec_via:
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
    mv s2, a2
    mv s3, a3
    sd zero, 16(sp)             # JSON elements written
    ld a0, RE_ITEM(s2)
    beqz a0, 9f
    call item_fx
    beqz a0, 9f
    mv s4, a0                   # the caller's FX
    call fx_edges
    mv s5, a0
    beqz s5, 9f
    li s6, 0
1:
    ld t0, VEC_N(s5)
    bge s6, t0, 9f
    mv a0, s5
    mv a1, s6
    call vec_get
    addi s6, s6, 1
    mv s7, a0                   # edge {caller FX, callee decl, call}
    ld t0, 0(s7)
    bne t0, s4, 1b
    ld a0, 8(s7)
    call item_fx
    beqz a0, 1b
    ld a1, FX_DECLARED(a0)
    beqz a1, 1b
    sd a1, 8(sp)
    mv a0, s3
    call es_missing
    call es_is_empty
    bnez a0, 1b
    ld a0, 8(sp)
    call fx_render
    sd a0, 0(sp)                # the callee's effects
    beqz s0, 2f
    ld a0, 8(s7)
    call fx_item_name
    mv a2, a0
    ld a1, RE_NAME(s2)
    la a0, m_via
    ld a3, 0(sp)
    call msg
    mv a4, a0
    ld t0, 16(s7)
    mv a0, s0
    lwu a1, N_FILE(t0)
    lwu a2, N_START(t0)
    lwu a3, N_END(t0)
    call diag_add_related
2:
    beqz s1, 1b
    ld t0, 16(sp)
    beqz t0, 3f
    mv a0, s1
    li a1, 44
    call sb_byte
3:
    li t0, 1
    sd t0, 16(sp)
    mv a0, s1
    la a1, j_fn_open
    call sb_cstr
    mv a0, s1
    ld a1, RE_NAME(s2)
    call json_str_into
    mv a0, s1
    la a1, j_calls
    call sb_cstr
    ld a0, 8(s7)
    call fx_item_name
    mv a1, a0
    mv a0, s1
    call json_str_into
    mv a0, s1
    la a1, j_file
    addi a1, a1, 1              # `,"file":` without the leading quote
    call sb_cstr
    ld t0, 16(s7)
    lwu a0, N_FILE(t0)
    call rec_relpath
    mv a1, a0
    mv a0, s1
    call json_str_into
    mv a0, s1
    la a1, j_start
    call sb_cstr
    ld t0, 16(s7)
    mv a0, s1
    lwu a1, N_FILE(t0)
    lwu a2, N_START(t0)
    call json_pos
    mv a0, s1
    li a1, 125                  # '}'
    call sb_byte
    j 1b
9:
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

# void rec_review(int entry, void *files)
# `vo check` after a clean check: compares the program with its vo.effects (E0504, W0502, W0503),
# or W0504 when a multi-file program has none.
    .globl rec_review
rec_review:
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
    call rec_find
    bgez a0, 1f
    ld t0, VEC_N(s1)
    li t1, 2
    blt t0, t1, 9f
    la a0, m_no_rec
    call str_from_cstr
    mv a5, a0
    la a0, w0504
    li a1, SEV_WARNING
    mv a2, s0
    li a3, 0
    li a4, 0
    call diag_add
    j 9f
1:
    mv s2, a0                   # the record's file id
    la t0, diag_errors
    ld s3, 0(t0)
    call rec_parse
    mv s4, a0
    la t0, diag_errors
    ld t0, 0(t0)
    bne t0, s3, 9f              # a malformed record: E0510 only
    mv a0, s1
    call rec_entries
    mv a1, a0
    mv a0, s4
    call rec_compare
    mv s3, a0
    li s4, 0
2:
    ld t0, VEC_N(s3)
    bge s4, t0, 9f
    mv a0, s3
    mv a1, s4
    call vec_get
    addi s4, s4, 1
    call rec_change_diag
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

# void rec_change_diag(void *change)
    .globl rec_change_diag
rec_change_diag:
    addi sp, sp, -64
    sd ra, 56(sp)
    sd s0, 48(sp)
    sd s1, 40(sp)
    sd s2, 32(sp)
    sd s3, 24(sp)
    sd s4, 16(sp)
    mv s0, a0
    ld s1, RC_NEW(s0)
    ld s2, RC_OLD(s0)
    ld a0, RC_DELTA(s0)
    call fx_render
    mv s3, a0
    ld t0, RC_KIND(s0)
    li t1, CH_RENAMED
    beq t0, t1, 5f
    li t1, CH_REMOVED
    beq t0, t1, 3f
    li t1, CH_NARROWED
    beq t0, t1, 3f
    la a0, m_grew               # growth: E0504 with the calls that brought it in
    ld a1, RE_NAME(s1)
    mv a2, s3
    li a3, 0
    call msg
    mv a5, a0
    la a0, e0504
    li a1, SEV_ERROR
    ld a2, RE_FILE(s1)
    ld a3, RE_START(s1)
    ld a4, RE_END(s1)
    call diag_add
    mv s4, a0
    bnez s2, 1f
    li a0, 0
    call es_new
    j 2f
1:
    ld a0, RE_SET(s2)
2:
    mv a3, a0
    mv a0, s4
    li a1, 0
    mv a2, s1
    call rec_via
    j 9f
3:
    beqz s1, 4f                 # the function is still there: fewer effects
    la a0, m_shrank
    ld a1, RE_NAME(s1)
    mv a2, s3
    li a3, 0
    call msg
    mv a5, a0
    la a0, w0502
    li a1, SEV_WARNING
    ld a2, RE_FILE(s1)
    ld a3, RE_START(s1)
    ld a4, RE_END(s1)
    call diag_add
    j 9f
4:
    la a0, m_gone               # removed: point at its record line
    ld a1, RE_NAME(s2)
    mv a2, s3
    li a3, 0
    call msg
    mv a5, a0
    la a0, w0502
    li a1, SEV_WARNING
    ld a2, RE_FILE(s2)
    ld a3, RE_START(s2)
    ld a4, RE_END(s2)
    call diag_add
    j 9f
5:
    la a0, m_renamed
    ld a1, RE_NAME(s1)
    ld a2, RE_NAME(s2)
    mv a3, s3
    call msg
    mv a5, a0
    la a0, w0503
    li a1, SEV_WARNING
    ld a2, RE_FILE(s1)
    ld a3, RE_START(s1)
    ld a4, RE_END(s1)
    call diag_add
9:
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    addi sp, sp, 64
    ret

# void rec_line(void *sb, void *entry)
# One record line: `<path>:<name> uses <effects>` or `<path>:<name> pure`, with a newline.
    .globl rec_line
rec_line:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    mv s1, a1
    ld a1, RE_PATH(s1)
    call sb_str
    mv a0, s0
    li a1, 58
    call sb_byte
    mv a0, s0
    ld a1, RE_NAME(s1)
    call sb_str
    mv a0, s0
    ld a1, RE_SET(s1)
    call rec_uses_text
    mv a0, s0
    li a1, 10
    call sb_byte
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void rec_uses_text(void *sb, void *set)
# ` uses <effects>`, or ` pure`.
    .globl rec_uses_text
rec_uses_text:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    mv s1, a1
    mv a0, s1
    call es_is_empty
    beqz a0, 1f
    mv a0, s0
    la a1, s_pure
    call sb_cstr
    j 9f
1:
    mv a0, s0
    la a1, s_uses
    call sb_cstr
    mv a0, s1
    mv a1, s0
    call es_render
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void *rec_before_after(void *entry)
# `uses <effects>` or `pure` as a str, for effect_change records ("removed" for no entry).
    .globl rec_before_after
rec_before_after:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    beqz s0, 8f
    call sb_new
    mv s1, a0
    ld a0, RE_SET(s0)
    call es_is_empty
    beqz a0, 1f
    mv a0, s1
    la a1, s_pure_word
    call sb_cstr
    j 2f
1:
    mv a0, s1
    la a1, s_uses_word
    call sb_cstr
    ld a0, RE_SET(s0)
    mv a1, s1
    call es_render
2:
    mv a0, s1
    call sb_finish
    j 9f
8:
    la a0, w_removed
    call str_from_cstr
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void rec_print_change(void *sb, void *change, int json)
# A change as an `effect_change` record, or a human line (`+ path:fn: effects`).
    .globl rec_print_change
rec_print_change:
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
    ld s3, RC_NEW(s1)           # the entry that names the function
    bnez s3, 1f
    ld s3, RC_OLD(s1)
1:
    ld a0, RC_DELTA(s1)
    call fx_render
    mv s4, a0
    bnez s2, 5f
    ld t0, RC_KIND(s1)          # human
    la a1, h_plus
    li t1, CH_WIDENED
    ble t0, t1, 2f
    la a1, h_minus
    li t1, CH_RENAMED
    bne t0, t1, 2f
    la a1, h_tilde
2:
    mv a0, s0
    call sb_cstr
    mv a0, s0
    ld a1, RE_PATH(s3)
    call sb_str
    mv a0, s0
    li a1, 58
    call sb_byte
    ld t0, RC_KIND(s1)
    li t1, CH_RENAMED
    bne t0, t1, 3f
    ld t0, RC_OLD(s1)
    mv a0, s0
    ld a1, RE_NAME(t0)
    call sb_str
    mv a0, s0
    la a1, h_arrow
    call sb_cstr
3:
    mv a0, s0
    ld a1, RE_NAME(s3)
    call sb_str
    mv a0, s0
    la a1, h_colon
    call sb_cstr
    mv a0, s0
    mv a1, s4
    call sb_str
    mv a0, s0
    li a1, 10
    call sb_byte
    j 9f
5:
    mv a0, s0                   # JSON
    la a1, j_head
    call sb_cstr
    ld t0, RC_KIND(s1)
    la t1, change_words
    slli t0, t0, 3
    add t1, t1, t0
    ld a1, 0(t1)
    mv a0, s0
    call sb_cstr
    mv a0, s0
    la a1, j_file
    call sb_cstr
    mv a0, s0
    ld a1, RE_PATH(s3)
    call json_str_into
    mv a0, s0
    la a1, j_function
    call sb_cstr
    mv a0, s0
    ld a1, RE_NAME(s3)
    call json_str_into
    ld t0, RC_KIND(s1)
    li t1, CH_RENAMED
    bne t0, t1, 6f
    mv a0, s0
    la a1, j_old_fn
    call sb_cstr
    ld t0, RC_OLD(s1)
    mv a0, s0
    ld a1, RE_NAME(t0)
    call json_str_into
6:
    mv a0, s0
    la a1, j_effects
    call sb_cstr
    mv a0, s0
    mv a1, s4
    call json_str_into
    mv a0, s0
    la a1, j_before
    call sb_cstr
    ld a0, RC_OLD(s1)
    bnez a0, 61f
    la a0, s_pure_word          # a new function was nothing before
    call str_from_cstr
    j 62f
61:
    call rec_before_after
62:
    mv a1, a0
    mv a0, s0
    call json_str_into
    mv a0, s0
    la a1, j_after
    call sb_cstr
    ld a0, RC_NEW(s1)
    call rec_before_after
    mv a1, a0
    mv a0, s0
    call json_str_into
    mv a0, s0
    la a1, j_via
    call sb_cstr
    ld t0, RC_KIND(s1)
    li t1, CH_WIDENED
    bgt t0, t1, 8f
    ld a3, RC_OLD(s1)
    beqz a3, 7f
    ld a3, RE_SET(a3)
    j 71f
7:
    li a0, 0
    call es_new
    mv a3, a0
71:
    li a0, 0
    mv a1, s0
    ld a2, RC_NEW(s1)
    call rec_via
8:
    mv a0, s0
    la a1, j_end
    call sb_cstr
9:
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    addi sp, sp, 64
    ret

# int rec_command(int entry, void *files, int update, int json)
# `vo effects`: the capability summary, or (update) rewrite vo.effects and print its changes.
    .globl rec_command
rec_command:
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
    mv s2, a2
    mv s3, a3
    call rec_find
    mv s4, a0                   # record file id or -1
    call vec_new
    mv s5, a0                   # old entries
    bltz s4, 1f
    mv a0, s4
    call rec_parse
    mv s5, a0
    la t0, diag_errors
    ld t0, 0(t0)
    li a0, 1
    bnez t0, 99f                # a malformed record: report, change nothing
1:
    mv a0, s1
    call rec_entries
    mv s6, a0                   # new entries
    call sb_new
    mv s7, a0
    bnez s2, 5f
    li t0, 0                    # summary: entry points first (main, then the entry file's tests)
    sd t0, 0(sp)
2:
    ld t0, 0(sp)
    ld t1, VEC_N(s6)
    bge t0, t1, 4f
    addi t2, t0, 1
    sd t2, 0(sp)
    mv a0, s6
    mv a1, t0
    call vec_get
    sd a0, 8(sp)
    ld t0, RE_FILE(a0)
    bne t0, s0, 2b
    ld t0, RE_ITEM(a0)
    lhu t1, N_KIND(t0)
    li t2, N_TEST
    beq t1, t2, 3f
    ld a0, RE_NAME(a0)
    la a1, s_main
    call str_eq_cstr
    beqz a0, 2b
3:
    ld t0, 8(sp)
    beqz s3, 31f
    mv a0, s7                   # {"v":1,"kind":"capabilities","entry":...,"file":...,"uses":...}
    la a1, j_cap_head
    call sb_cstr
    ld t0, 8(sp)
    mv a0, s7
    ld a1, RE_NAME(t0)
    call json_str_into
    mv a0, s7
    la a1, j_file
    addi a1, a1, 1
    call sb_cstr
    ld t0, 8(sp)
    mv a0, s7
    ld a1, RE_PATH(t0)
    call json_str_into
    mv a0, s7
    la a1, j_uses
    call sb_cstr
    ld t0, 8(sp)
    ld a0, RE_SET(t0)
    call fx_render
    mv a1, a0
    mv a0, s7
    call json_str_into
    mv a0, s7
    la a1, j_close
    call sb_cstr
    j 2b
31:
    mv a0, s7                   # human: `main uses ...`
    ld a1, RE_NAME(t0)
    call sb_str
    ld t0, 8(sp)
    mv a0, s7
    ld a1, RE_SET(t0)
    call rec_uses_text
    mv a0, s7
    li a1, 10
    call sb_byte
    j 2b
4:
    bnez s3, 8f                 # human: then the record lines that `--update` would write
    mv a0, s7
    li a1, 10
    call sb_byte
    li t0, 0
    sd t0, 0(sp)
41:
    ld t0, 0(sp)
    ld t1, VEC_N(s6)
    bge t0, t1, 8f
    addi t2, t0, 1
    sd t2, 0(sp)
    mv a0, s6
    mv a1, t0
    call vec_get
    mv a1, a0
    mv a0, s7
    call rec_line
    j 41b
5:
    mv a0, s5                   # update: print the changes, write the file
    mv a1, s6
    call rec_compare
    sd a0, 16(sp)
    li t0, 0
    sd t0, 0(sp)
51:
    ld t0, 0(sp)
    ld t2, 16(sp)
    ld t1, VEC_N(t2)
    bge t0, t1, 6f
    addi t3, t0, 1
    sd t3, 0(sp)
    mv a0, t2
    mv a1, t0
    call vec_get
    mv a1, a0
    mv a0, s7
    mv a2, s3
    call rec_print_change
    j 51b
6:
    call sb_new                 # the new record: kept leading comments, then the lines
    sd a0, 8(sp)
    bltz s4, 63f
    mv a0, s4
    call file_get
    ld t1, FILE_TEXT(a0)
    ld t2, FILE_LEN(a0)
    li t3, 0
61:
    bge t3, t2, 64f             # leading `#` lines
    add t4, t1, t3
    lbu t4, 0(t4)
    li t5, 35
    bne t4, t5, 64f
    mv t4, t3
62:
    bge t4, t2, 621f
    add t5, t1, t4
    lbu t5, 0(t5)
    addi t4, t4, 1
    li t6, 10
    bne t5, t6, 62b
621:
    sd t4, 0(sp)
    add a1, t1, t3
    sub a2, t4, t3
    ld a0, 8(sp)
    sd t1, 16(sp)
    call sb_bytes
    mv a0, s4
    call file_get
    ld t1, FILE_TEXT(a0)
    ld t2, FILE_LEN(a0)
    ld t3, 0(sp)
    j 61b
63:
    ld a0, 8(sp)
    la a1, s_header
    call sb_cstr
64:
    li t0, 0
    sd t0, 0(sp)
65:
    ld t0, 0(sp)
    ld t1, VEC_N(s6)
    bge t0, t1, 66f
    addi t2, t0, 1
    sd t2, 0(sp)
    mv a0, s6
    mv a1, t0
    call vec_get
    mv a1, a0
    ld a0, 8(sp)
    call rec_line
    j 65b
66:
    call sb_new                 # <root>/vo.effects
    sd a0, 16(sp)
    la t0, rec_root
    ld a1, 0(t0)
    call sb_str
    ld a0, 16(sp)
    la a1, s_record
    call sb_cstr
    ld a0, 16(sp)
    call sb_finish
    sd a0, 16(sp)
    ld a0, 8(sp)
    call sb_finish
    mv a1, a0
    ld a0, 16(sp)
    call write_file_safely
8:
    mv a0, s7
    call sb_finish
    mv a1, a0
    li a0, 1
    call out_str
    li a0, 0
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
