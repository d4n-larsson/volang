# Source file table, diagnostics (collect, sort, cap, render), summaries, toolchain errors.
    .include "defs.inc"

    .section .bss
    .align 3
    .globl file_table
file_table:     .space MAX_FILES * FILE_SIZE
    .globl file_count
file_count:     .space 8
diag_arr:       .space 8
diag_n:         .space 8
diag_cap:       .space 8
    .globl diag_errors
diag_errors:    .space 8
    .globl diag_warnings
diag_warnings:  .space 8
    .globl diag_truncated
diag_truncated: .space 8
    .globl diag_mute
diag_mute:      .space 8            # 1: diagnostics are built but not recorded (scanning other files)
emitted_fixes:  .space 8            # array of fix pointers already shown
emitted_n:      .space 8

    .section .rodata
s_diag_head:    .string "{\"v\":1,\"kind\":\"diagnostic\",\"code\":\""
s_name:         .string "\",\"name\":\""
s_sev_err:      .string "\",\"severity\":\"error\",\"file\":"
s_sev_warn:     .string "\",\"severity\":\"warning\",\"file\":"
s_start:        .string ",\"start\":"
s_end:          .string ",\"end\":"
s_text:         .string ",\"text\":"
s_message:      .string ",\"message\":"
s_expected:     .string ",\"expected\":"
s_found:        .string ",\"found\":"
s_fixes:        .string ",\"fixes\":["
s_scope:        .string ",\"scope\":["
s_scope_name:   .string "{\"name\":"
s_scope_name2:  .string ",{\"name\":"
s_scope_type:   .string ",\"type\":"
s_in_scope:     .string "    in scope: "
s_nothing:      .string "nothing"
s_related:      .string "],\"related\":["
s_close:        .string "]}\n"
s_file_key:     .string "{\"file\":"
s_repl:         .string ",\"replacement\":"
s_msg_key:      .string ",\"message\":"
s_line:         .string "{\"line\":"
s_col:          .string ",\"col\":"
s_err_word:     .string ": error["
s_warn_word:    .string ": warning["
s_bracket:      .string "]: "
s_bar:          .string " | "
s_gutter:       .string "     | "
s_fix_remove:   .string "  fix: remove `"
s_fix_insert:   .string "  fix: insert `"
s_fix_replace:  .string "  fix: replace with `"
s_note:         .string "  note: "
s_expfound1:    .string "  expected `"
s_expfound2:    .string "`, found `"
s_tick_nl:      .string "`\n"
s_sum_head:     .string "{\"v\":1,\"kind\":\"summary\",\"command\":\""
s_sum_err:      .string "\",\"errors\":"
s_sum_warn:     .string ",\"warnings\":"
s_sum_trunc:    .string ",\"truncated\":"
s_sum_files:    .string ",\"files\":"
s_brace_nl:     .string "}\n"
s_sum_holes:    .string ",\"holes\":"
s_paren_sp:     .string " ("
s_hole_close:   .string " hole)"
s_holes_close:  .string " holes)"
s_error1:       .string " error"
s_errors:       .string " errors"
s_warning1:     .string ", 1 warning"
s_warnings:     .string " warnings"
s_comma_sp:     .string ", "
s_notshown1:    .string " ("
s_notshown2:    .string " more not shown)"
s_te_head:      .string "{\"v\":1,\"kind\":\"toolchain_error\",\"code\":\""
s_path_key:     .string ",\"path\":"
s_err_open:     .string "error["

    .text
# int file_add(void *path, char *text, long len)
# Registers a source file; builds its line table and directory. Returns the file id.
    .globl file_add
file_add:
    addi sp, sp, -64
    sd ra, 56(sp)
    sd s0, 48(sp)
    sd s1, 40(sp)
    sd s2, 32(sp)
    sd s3, 24(sp)
    sd s4, 16(sp)
    sd s5, 8(sp)
    mv s1, a0
    mv s2, a1
    mv s3, a2
    la t0, file_count
    ld s5, 0(t0)
    addi t1, s5, 1
    sd t1, 0(t0)
    li t0, FILE_SIZE
    mul t0, t0, s5
    la s0, file_table
    add s0, s0, t0
    sd s1, FILE_PATH(s0)
    sd s2, FILE_TEXT(s0)
    sd s3, FILE_LEN(s0)
    sd zero, FILE_TOKENS(s0)
    sd zero, FILE_AST(s0)
    li s4, 1                    # count lines
    li t0, 0
1:
    bge t0, s3, 2f
    add t1, s2, t0
    lbu t1, 0(t1)
    addi t0, t0, 1
    li t2, 10
    bne t1, t2, 1b
    addi s4, s4, 1
    j 1b
2:
    sd s4, FILE_NLINES(s0)
    slli a0, s4, 2
    call alloc
    sd a0, FILE_LINES(s0)
    sw zero, 0(a0)
    li t0, 0
    li t3, 1
3:
    bge t0, s3, 4f
    add t1, s2, t0
    lbu t1, 0(t1)
    addi t0, t0, 1
    li t2, 10
    bne t1, t2, 3b
    slli t4, t3, 2
    add t4, a0, t4
    sw t0, 0(t4)
    addi t3, t3, 1
    j 3b
4:
    ld t0, STR_LEN(s1)          # directory part: up to and including the last '/'
    addi t1, s1, STR_DATA
5:
    beqz t0, 6f
    add t2, t1, t0
    lbu t2, -1(t2)
    li t3, '/'
    beq t2, t3, 6f
    addi t0, t0, -1
    j 5b
6:
    mv a0, t1
    mv a1, t0
    call str_new
    sd a0, FILE_DIR(s0)
    mv a0, s5
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    ld s5, 8(sp)
    addi sp, sp, 64
    ret

# void *file_get(int id)
    .globl file_get
file_get:
    li t0, FILE_SIZE
    mul t0, t0, a0
    la a0, file_table
    add a0, a0, t0
    ret

# long pos_line(int file, long off)
# 1-based line of a byte offset.
    .globl pos_line
pos_line:
    li t0, FILE_SIZE
    mul t0, t0, a0
    la t1, file_table
    add t1, t1, t0
    ld t2, FILE_LINES(t1)
    ld t3, FILE_NLINES(t1)
    li t4, 0                    # lo
    mv t5, t3                   # hi (exclusive): find last line start <= off
1:
    addi t6, t4, 1
    bge t6, t5, 2f
    add t6, t4, t5
    srli t6, t6, 1
    slli a2, t6, 2
    add a2, t2, a2
    lwu a2, 0(a2)
    bgtu a2, a1, 3f
    mv t4, t6
    j 1b
3:
    mv t5, t6
    j 1b
2:
    addi a0, t4, 1
    ret

# long line_start(int file, long line)
    .globl line_start
line_start:
    li t0, FILE_SIZE
    mul t0, t0, a0
    la t1, file_table
    add t1, t1, t0
    ld t2, FILE_LINES(t1)
    addi a1, a1, -1
    slli a1, a1, 2
    add t2, t2, a1
    lwu a0, 0(t2)
    ret

# long line_end(int file, long line)
# Offset of the newline that ends the line (or the file length).
    .globl line_end
line_end:
    li t0, FILE_SIZE
    mul t0, t0, a0
    la t1, file_table
    add t1, t1, t0
    ld t2, FILE_LINES(t1)
    ld t3, FILE_NLINES(t1)
    blt a1, t3, 1f
    ld a0, FILE_LEN(t1)
    ret
1:
    slli a1, a1, 2
    add t2, t2, a1
    lwu a0, 0(t2)
    addi a0, a0, -1
    ret

# long pos_col(int file, long off)
# 1-based column, counted in code points.
    .globl pos_col
pos_col:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    mv s1, a1
    call pos_line
    mv a1, a0
    mv a0, s0
    call line_start
    mv t0, a0
    mv a0, s0
    sd t0, 0(sp)
    call file_get
    ld t0, 0(sp)
    ld a0, FILE_TEXT(a0)
    add a0, a0, t0
    sub a1, s1, t0
    call cp_count
    addi a0, a0, 1
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void *diag_add(char *code, int sev, int file, long start, long end, void *message)
    .globl diag_add
diag_add:
    addi sp, sp, -64
    sd ra, 56(sp)
    sd s0, 48(sp)
    sd s1, 40(sp)
    sd s2, 32(sp)
    sd s3, 24(sp)
    sd s4, 16(sp)
    sd s5, 8(sp)
    sd s6, 0(sp)
    mv s1, a0
    mv s2, a1
    mv s3, a2
    mv s4, a3
    mv s5, a4
    mv s6, a5
    li a0, D_SIZE
    call alloc
    mv s0, a0
    sd s1, D_CODE(s0)
    sd s2, D_SEV(s0)
    sd s3, D_FILE(s0)
    sd s4, D_START(s0)
    sd s5, D_END(s0)
    sd s6, D_MSG(s0)
    la t0, diag_mute
    ld t0, 0(t0)
    bnez t0, 5f
    la t0, diag_n
    ld t1, 0(t0)
    sd t1, D_SEQ(s0)
    la t2, diag_cap             # grow the pointer array
    ld t3, 0(t2)
    blt t1, t3, 1f
    slli s1, t3, 1
    bnez s1, 2f
    li s1, 64
2:
    slli a0, s1, 3
    call alloc
    la t0, diag_arr
    ld a1, 0(t0)
    sd a0, 0(t0)
    la t0, diag_n
    ld a2, 0(t0)
    slli a2, a2, 3
    call mem_copy
    la t2, diag_cap
    sd s1, 0(t2)
1:
    la t0, diag_n
    ld t1, 0(t0)
    la t2, diag_arr
    ld t2, 0(t2)
    slli t3, t1, 3
    add t3, t2, t3
    sd s0, 0(t3)
    addi t1, t1, 1
    sd t1, 0(t0)
    la t0, diag_errors
    bnez s2, 3f
    j 4f
3:
    la t0, diag_warnings
4:
    ld t1, 0(t0)
    addi t1, t1, 1
    sd t1, 0(t0)
5:
    mv a0, s0
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    ld s5, 8(sp)
    ld s6, 0(sp)
    addi sp, sp, 64
    ret

# void diag_set_ef(void *d, void *expected, void *found)
    .globl diag_set_ef
diag_set_ef:
    sd a1, D_EXPECTED(a0)
    sd a2, D_FOUND(a0)
    ret

# void diag_set_scope(void *d, void *vec)
# W0601: the names in scope, a vec of {name str, type str} pairs, innermost first.
    .globl diag_set_scope
diag_set_scope:
    sd a1, D_SCOPE(a0)
    ret

# int ranges_touch(long s1, long e1, long s2, long e2)
    .globl ranges_touch
ranges_touch:
    bgt a0, a3, 1f
    bgt a2, a1, 1f
    li a0, 1
    ret
1:
    li a0, 0
    ret

# void diag_add_fix(void *d, int file, long start, long end, void *replacement)
# Appends a fix unless it overlaps or touches one the diagnostic already has.
    .globl diag_add_fix
diag_add_fix:
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
    mv s4, a4
    addi s5, s0, D_FIXES        # s5 = address of the link to update
1:
    ld t0, 0(s5)
    beqz t0, 2f
    ld a0, FX_START(t0)
    ld a1, FX_END(t0)
    mv a2, s2
    mv a3, s3
    call ranges_touch
    bnez a0, 9f
    ld t0, 0(s5)
    addi s5, t0, FX_NEXT
    j 1b
2:
    li a0, FIX_SIZE
    call alloc
    sd zero, FX_NEXT(a0)
    sd s1, FX_FILE(a0)
    sd s2, FX_START(a0)
    sd s3, FX_END(a0)
    sd s4, FX_TEXT(a0)
    sd a0, 0(s5)
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

# void diag_add_related(void *d, int file, long start, long end, void *message)
    .globl diag_add_related
diag_add_related:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    sd s4, 0(sp)
    addi s0, a0, D_RELATED
    mv s1, a1
    mv s2, a2
    mv s3, a3
    mv s4, a4
1:
    ld t0, 0(s0)
    beqz t0, 2f
    addi s0, t0, FX_NEXT
    j 1b
2:
    li a0, FIX_SIZE
    call alloc
    sd zero, FX_NEXT(a0)
    sd s1, FX_FILE(a0)
    sd s2, FX_START(a0)
    sd s3, FX_END(a0)
    sd s4, FX_TEXT(a0)
    sd a0, 0(s0)
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    ld s4, 0(sp)
    addi sp, sp, 48
    ret

# int diag_before(void *a, void *b)
# Sort order: file, start, insertion order.
    .globl diag_before
diag_before:
    ld t0, D_FILE(a0)
    ld t1, D_FILE(a1)
    blt t0, t1, 1f
    bgt t0, t1, 2f
    ld t0, D_START(a0)
    ld t1, D_START(a1)
    blt t0, t1, 1f
    bgt t0, t1, 2f
    ld t0, D_SEQ(a0)
    ld t1, D_SEQ(a1)
    blt t0, t1, 1f
2:
    li a0, 0
    ret
1:
    li a0, 1
    ret

# void diag_sort(void)
# Insertion sort of the diagnostic array.
    .globl diag_sort
diag_sort:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    la t0, diag_arr
    ld s0, 0(t0)
    la t0, diag_n
    ld s1, 0(t0)
    li s2, 1
1:
    bge s2, s1, 9f
    slli t0, s2, 3
    add t0, s0, t0
    ld s3, 0(t0)                # element to insert
    addi t1, s2, -1
    sd t1, 0(sp)
2:
    ld t1, 0(sp)
    bltz t1, 3f
    slli t0, t1, 3
    add t0, s0, t0
    ld a1, 0(t0)
    mv a0, s3
    call diag_before
    beqz a0, 3f
    ld t1, 0(sp)
    slli t0, t1, 3
    add t0, s0, t0
    ld t2, 0(t0)
    sd t2, 8(t0)
    addi t1, t1, -1
    sd t1, 0(sp)
    j 2b
3:
    ld t1, 0(sp)
    addi t1, t1, 1
    slli t0, t1, 3
    add t0, s0, t0
    sd s3, 0(t0)
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

# void json_pos(void *sb, int file, long off)
    .globl json_pos
json_pos:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    mv s0, a0
    mv s1, a1
    mv s2, a2
    la a1, s_line
    call sb_cstr
    mv a0, s1
    mv a1, s2
    call pos_line
    mv a1, a0
    mv a0, s0
    call int_to_dec
    mv a0, s0
    la a1, s_col
    call sb_cstr
    mv a0, s1
    mv a1, s2
    call pos_col
    mv a1, a0
    mv a0, s0
    call int_to_dec
    mv a0, s0
    li a1, '}'
    call sb_byte
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    addi sp, sp, 48
    ret

# void json_span(void *sb, int file, long start, long end)
# Writes "file":"...","start":{..},"end":{..} (without braces around it).
    .globl json_span
json_span:
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
    mv a0, s1
    call file_get
    ld a1, FILE_PATH(a0)
    mv a0, s0
    call json_str_into
    mv a0, s0
    la a1, s_start
    call sb_cstr
    mv a0, s0
    mv a1, s1
    mv a2, s2
    call json_pos
    mv a0, s0
    la a1, s_end
    call sb_cstr
    mv a0, s0
    mv a1, s1
    mv a2, s3
    call json_pos
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# void json_src_text(void *sb, int file, long start, long end)
# The source text of a span as a JSON string.
    .globl json_src_text
json_src_text:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    mv s2, a2
    mv s3, a3
    mv a0, a1
    call file_get
    ld s1, FILE_TEXT(a0)
    mv a0, s0
    li a1, '"'
    call sb_byte
    mv a0, s0
    add a1, s1, s2
    sub a2, s3, s2
    call json_escape_into
    mv a0, s0
    li a1, '"'
    call sb_byte
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# int fix_already_shown(void *fix)
# 1 when the fix overlaps or touches a fix of an earlier diagnostic; otherwise records it.
    .globl fix_already_shown
fix_already_shown:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    mv s0, a0
    li s1, 0
1:
    la t0, emitted_n
    ld t1, 0(t0)
    bge s1, t1, 3f
    la t0, emitted_fixes
    ld t0, 0(t0)
    slli t2, s1, 3
    add t0, t0, t2
    ld s2, 0(t0)
    addi s1, s1, 1
    ld t0, FX_FILE(s2)
    ld t1, FX_FILE(s0)
    bne t0, t1, 1b
    ld a0, FX_START(s2)
    ld a1, FX_END(s2)
    ld a2, FX_START(s0)
    ld a3, FX_END(s0)
    call ranges_touch
    beqz a0, 1b
    li a0, 1
    j 9f
3:
    la t0, emitted_fixes        # record (array sized for the 50 shown diagnostics x 64 fixes)
    ld t1, 0(t0)
    bnez t1, 4f
    li a0, 8 * 4096
    call alloc
    la t0, emitted_fixes
    sd a0, 0(t0)
4:
    la t0, emitted_n
    ld t1, 0(t0)
    li t2, 4096
    bge t1, t2, 5f
    la t2, emitted_fixes
    ld t2, 0(t2)
    slli t3, t1, 3
    add t2, t2, t3
    sd s0, 0(t2)
    addi t1, t1, 1
    sd t1, 0(t0)
5:
    li a0, 0
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    addi sp, sp, 48
    ret

# void diag_filter_fixes(void *d)
# Drops fixes that overlap a fix of an earlier (already emitted) diagnostic.
    .globl diag_filter_fixes
diag_filter_fixes:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    addi s0, a0, D_FIXES
1:
    ld s1, 0(s0)
    beqz s1, 9f
    mv a0, s1
    call fix_already_shown
    beqz a0, 2f
    ld t0, FX_NEXT(s1)          # unlink
    sd t0, 0(s0)
    j 1b
2:
    addi s0, s1, FX_NEXT
    j 1b
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void diag_json(void *sb, void *d)
    .globl diag_json
diag_json:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    mv s1, a1
    la a1, s_diag_head
    call sb_cstr
    mv a0, s0
    ld a1, D_CODE(s1)
    call sb_cstr
    mv a0, s0
    la a1, s_name
    call sb_cstr
    ld a0, D_CODE(s1)
    call catalog_name
    mv a1, a0
    mv a0, s0
    call sb_str
    la a1, s_sev_err
    ld t0, D_SEV(s1)
    beqz t0, 1f
    la a1, s_sev_warn
1:
    mv a0, s0
    call sb_cstr
    mv a0, s0
    ld a1, D_FILE(s1)
    ld a2, D_START(s1)
    ld a3, D_END(s1)
    call json_span
    mv a0, s0
    la a1, s_text
    call sb_cstr
    mv a0, s0
    ld a1, D_FILE(s1)
    ld a2, D_START(s1)
    ld a3, D_END(s1)
    call json_src_text
    mv a0, s0
    la a1, s_message
    call sb_cstr
    mv a0, s0
    ld a1, D_MSG(s1)
    call json_str_into
    ld t0, D_EXPECTED(s1)
    beqz t0, 2f
    mv a0, s0
    la a1, s_expected
    call sb_cstr
    mv a0, s0
    ld a1, D_EXPECTED(s1)
    call json_str_into
2:
    ld t0, D_FOUND(s1)
    beqz t0, 3f
    mv a0, s0
    la a1, s_found
    call sb_cstr
    mv a0, s0
    ld a1, D_FOUND(s1)
    call json_str_into
3:
    ld s2, D_SCOPE(s1)
    beqz s2, 33f
    mv a0, s0
    la a1, s_scope
    call sb_cstr
    li s3, 0
31:
    ld t0, VEC_N(s2)
    bge s3, t0, 32f
    mv a0, s0
    la a1, s_scope_name
    beqz s3, 311f
    la a1, s_scope_name2
311:
    call sb_cstr
    mv a0, s2
    mv a1, s3
    call vec_get
    ld a1, 0(a0)
    mv a0, s0
    call json_str_into
    mv a0, s0
    la a1, s_scope_type
    call sb_cstr
    mv a0, s2
    mv a1, s3
    call vec_get
    ld a1, 8(a0)
    mv a0, s0
    call json_str_into
    mv a0, s0
    li a1, '}'
    call sb_byte
    addi s3, s3, 1
    j 31b
32:
    mv a0, s0
    li a1, ']'
    call sb_byte
33:
    mv a0, s0
    la a1, s_fixes
    call sb_cstr
    ld s2, D_FIXES(s1)
    li s3, 0                    # 0 = first element
4:
    beqz s2, 5f
    beqz s3, 41f
    mv a0, s0
    li a1, ','
    call sb_byte
41:
    li s3, 1
    mv a0, s0
    la a1, s_file_key
    call sb_cstr
    mv a0, s0
    ld a1, FX_FILE(s2)
    ld a2, FX_START(s2)
    ld a3, FX_END(s2)
    call json_span
    mv a0, s0
    la a1, s_repl
    call sb_cstr
    mv a0, s0
    ld a1, FX_TEXT(s2)
    call json_str_into
    mv a0, s0
    li a1, '}'
    call sb_byte
    ld s2, FX_NEXT(s2)
    j 4b
5:
    mv a0, s0
    la a1, s_related
    call sb_cstr
    ld s2, D_RELATED(s1)
    li s3, 0
6:
    beqz s2, 7f
    beqz s3, 61f
    mv a0, s0
    li a1, ','
    call sb_byte
61:
    li s3, 1
    mv a0, s0
    la a1, s_file_key
    call sb_cstr
    mv a0, s0
    ld a1, FX_FILE(s2)
    ld a2, FX_START(s2)
    ld a3, FX_END(s2)
    call json_span
    mv a0, s0
    la a1, s_msg_key
    call sb_cstr
    mv a0, s0
    ld a1, FX_TEXT(s2)
    call json_str_into
    mv a0, s0
    li a1, '}'
    call sb_byte
    ld s2, FX_NEXT(s2)
    j 6b
7:
    mv a0, s0
    la a1, s_close
    call sb_cstr
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# void human_loc(void *sb, int file, long off)
# Writes path:line:col.
    .globl human_loc
human_loc:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    mv s0, a0
    mv s1, a1
    mv s2, a2
    mv a0, s1
    call file_get
    ld a1, FILE_PATH(a0)
    mv a0, s0
    call sb_str
    mv a0, s0
    li a1, ':'
    call sb_byte
    mv a0, s1
    mv a1, s2
    call pos_line
    mv a1, a0
    mv a0, s0
    call int_to_dec
    mv a0, s0
    li a1, ':'
    call sb_byte
    mv a0, s1
    mv a1, s2
    call pos_col
    mv a1, a0
    mv a0, s0
    call int_to_dec
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    addi sp, sp, 48
    ret

# void sb_visible(void *sb, void *str)
# Appends str with newlines and tabs shown as \n and \t.
    .globl sb_visible
sb_visible:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    addi s1, a1, STR_DATA
    ld s2, STR_LEN(a1)
1:
    beqz s2, 9f
    lbu s3, 0(s1)
    li t0, 10
    beq s3, t0, 2f
    li t0, 9
    beq s3, t0, 3f
    mv a0, s0
    mv a1, s3
    call sb_byte
    j 4f
2:
    li s3, 'n'
    j 5f
3:
    li s3, 't'
5:
    mv a0, s0
    li a1, '\\'
    call sb_byte
    mv a0, s0
    mv a1, s3
    call sb_byte
4:
    addi s1, s1, 1
    addi s2, s2, -1
    j 1b
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# void human_snippet(void *sb, int file, long start, long end)
# The source line of start with a caret line under the span.
    .globl human_snippet
human_snippet:
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
    mv a0, s1
    mv a1, s2
    call pos_line
    mv s4, a0                   # line number
    mv a0, s1
    mv a1, s4
    call line_start
    mv s5, a0
    mv a0, s1
    mv a1, s4
    call line_end
    mv s6, a0
    mv a0, s1
    call file_get
    ld t0, FILE_TEXT(a0)
    sd t0, 0(sp)
    li t2, 3                    # gutter: line number right-aligned to 4 columns
    li t3, 10
    blt s4, t3, 1f
    li t2, 2
    li t3, 100
    blt s4, t3, 1f
    li t2, 1
    li t3, 1000
    blt s4, t3, 1f
    li t2, 0
1:
    sd t2, 8(sp)
11:
    ld t2, 8(sp)
    beqz t2, 2f
    addi t2, t2, -1
    sd t2, 8(sp)
    mv a0, s0
    li a1, ' '
    call sb_byte
    j 11b
2:
    mv a0, s0
    mv a1, s4
    call int_to_dec
    mv a0, s0
    la a1, s_bar
    call sb_cstr
    ld t0, 0(sp)
    mv a0, s0
    add a1, t0, s5
    sub a2, s6, s5
    call sb_bytes
    mv a0, s0
    li a1, 10
    call sb_byte
    mv a0, s0
    la a1, s_gutter
    call sb_cstr
    mv s4, s5                   # caret line: copy tabs, spaces elsewhere
3:
    bge s4, s2, 4f
    ld t0, 0(sp)
    add t0, t0, s4
    lbu t1, 0(t0)
    andi t2, t1, 0xC0
    li t3, 0x80
    beq t2, t3, 31f             # continuation byte: no column
    li a1, ' '
    li t3, 9
    bne t1, t3, 32f
    li a1, 9
32:
    mv a0, s0
    call sb_byte
31:
    addi s4, s4, 1
    j 3b
4:
    bleu s3, s6, 5f             # carets up to the span end, at most to the end of the line
    mv s3, s6
5:
    ld t0, 0(sp)
    add a0, t0, s2
    sub a1, s3, s2
    call cp_count
    mv s4, a0
    bnez s4, 6f
    li s4, 1
6:
    mv a0, s0
    li a1, '^'
    call sb_byte
    addi s4, s4, -1
    bnez s4, 6b
    mv a0, s0
    li a1, 10
    call sb_byte
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

# void diag_human(void *sb, void *d)
    .globl diag_human
diag_human:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    mv s1, a1
    ld a1, D_FILE(s1)
    ld a2, D_START(s1)
    call human_loc
    la a1, s_err_word
    ld t0, D_SEV(s1)
    beqz t0, 1f
    la a1, s_warn_word
1:
    mv a0, s0
    call sb_cstr
    mv a0, s0
    ld a1, D_CODE(s1)
    call sb_cstr
    mv a0, s0
    la a1, s_bracket
    call sb_cstr
    mv a0, s0
    ld a1, D_MSG(s1)
    call sb_str
    mv a0, s0
    li a1, 10
    call sb_byte
    mv a0, s0
    ld a1, D_FILE(s1)
    ld a2, D_START(s1)
    ld a3, D_END(s1)
    call human_snippet
    ld s2, D_SCOPE(s1)
    beqz s2, 15f
    mv a0, s0
    la a1, s_in_scope
    call sb_cstr
    ld t0, VEC_N(s2)
    bnez t0, 11f
    mv a0, s0
    la a1, s_nothing
    call sb_cstr
11:
    li s3, 0
12:
    ld t0, VEC_N(s2)
    bge s3, t0, 14f
    beqz s3, 13f
    mv a0, s0
    li a1, ','
    call sb_byte
    mv a0, s0
    li a1, ' '
    call sb_byte
13:
    mv a0, s2
    mv a1, s3
    call vec_get
    ld a1, 0(a0)
    mv a0, s0
    call sb_str
    mv a0, s0
    li a1, ':'
    call sb_byte
    mv a0, s0
    li a1, ' '
    call sb_byte
    mv a0, s2
    mv a1, s3
    call vec_get
    ld a1, 8(a0)
    mv a0, s0
    call sb_str
    addi s3, s3, 1
    j 12b
14:
    mv a0, s0
    li a1, 10
    call sb_byte
15:
    ld t0, D_EXPECTED(s1)
    beqz t0, 2f
    ld t0, D_FOUND(s1)
    beqz t0, 2f
    mv a0, s0
    la a1, s_expfound1
    call sb_cstr
    mv a0, s0
    ld a1, D_EXPECTED(s1)
    call sb_str
    mv a0, s0
    la a1, s_expfound2
    call sb_cstr
    mv a0, s0
    ld a1, D_FOUND(s1)
    call sb_str
    mv a0, s0
    la a1, s_tick_nl
    call sb_cstr
2:
    ld s2, D_FIXES(s1)
3:
    beqz s2, 6f
    ld t0, FX_TEXT(s2)
    ld t1, STR_LEN(t0)
    la a1, s_fix_remove
    beqz t1, 4f
    ld t0, FX_START(s2)
    ld t1, FX_END(s2)
    la a1, s_fix_insert
    beq t0, t1, 4f
    la a1, s_fix_replace
4:
    mv a0, s0
    call sb_cstr
    ld t0, FX_TEXT(s2)
    ld t1, STR_LEN(t0)
    bnez t1, 5f
    ld a0, FX_FILE(s2)          # removal: show the removed text
    call file_get
    ld t0, FILE_TEXT(a0)
    ld t1, FX_START(s2)
    ld t2, FX_END(s2)
    add a0, t0, t1
    sub a1, t2, t1
    call str_new
    mv a1, a0
    j 51f
5:
    ld a1, FX_TEXT(s2)
51:
    mv a0, s0
    call sb_visible
    mv a0, s0
    la a1, s_tick_nl
    call sb_cstr
    ld s2, FX_NEXT(s2)
    j 3b
6:
    ld s2, D_RELATED(s1)
7:
    beqz s2, 9f
    mv a0, s0
    la a1, s_note
    call sb_cstr
    mv a0, s0
    ld a1, FX_FILE(s2)
    ld a2, FX_START(s2)
    call human_loc
    mv a0, s0
    li a1, ':'
    call sb_byte
    mv a0, s0
    li a1, ' '
    call sb_byte
    mv a0, s0
    ld a1, FX_TEXT(s2)
    call sb_str
    mv a0, s0
    li a1, 10
    call sb_byte
    ld s2, FX_NEXT(s2)
    j 7b
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# void diag_emit_all(int json, int fd)
# Sorts, drops overlapping fixes, caps at 50, and writes every diagnostic once.
    .globl diag_emit_all
diag_emit_all:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    sd s4, 0(sp)
    mv s0, a0
    mv s1, a1
    call diag_sort
    li s2, 0
1:
    la t0, diag_n
    ld t1, 0(t0)
    bge s2, t1, 8f
    li t2, DIAG_CAP_SHOWN
    bge s2, t2, 7f
    la t0, diag_arr
    ld t0, 0(t0)
    slli t2, s2, 3
    add t0, t0, t2
    ld s3, 0(t0)
    mv a0, s3
    call diag_filter_fixes
    call sb_new
    mv s4, a0
    mv a1, s3
    beqz s0, 2f
    call diag_json
    j 3f
2:
    call diag_human
3:
    mv a0, s1
    ld a1, SB_PTR(s4)
    ld a2, SB_LEN(s4)
    call out_bytes
    addi s2, s2, 1
    j 1b
7:
    la t0, diag_n
    ld t1, 0(t0)
    sub t1, t1, s2
    la t0, diag_truncated
    sd t1, 0(t0)
8:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    ld s4, 0(sp)
    addi sp, sp, 48
    ret

# long holes_for(char *command)
# The number of W0601 diagnostics when command is "check" (spec 005 FR-005), else 0.
holes_for:
    li t0, 0
    lbu t1, 0(a0)               # "check" is the only command starting with "ch"
    li t2, 'c'
    bne t1, t2, 9f
    lbu t1, 1(a0)
    li t2, 'h'
    bne t1, t2, 9f
    la t1, diag_arr
    ld t1, 0(t1)
    la t2, diag_n
    ld t2, 0(t2)
1:
    beqz t2, 9f
    ld t3, 0(t1)
    ld t3, D_CODE(t3)
    lbu t4, 0(t3)               # "W06": W0601 is the only W06xx code
    li t5, 'W'
    bne t4, t5, 2f
    lbu t4, 2(t3)
    li t5, '6'
    bne t4, t5, 2f
    addi t0, t0, 1
2:
    addi t1, t1, 8
    addi t2, t2, -1
    j 1b
9:
    mv a0, t0
    ret

# void summary_emit(int json, int fd, char *command, long files, void *extra)
# extra: a str of additional JSON members (starting with ','), or 0.
    .globl summary_emit
summary_emit:
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
    mv s4, a4
    call sb_new
    mv s5, a0
    beqz s0, 5f
    la a1, s_sum_head
    call sb_cstr
    mv a0, s5
    mv a1, s2
    call sb_cstr
    mv a0, s5
    la a1, s_sum_err
    call sb_cstr
    la t0, diag_errors
    ld a1, 0(t0)
    mv a0, s5
    call int_to_dec
    mv a0, s5
    la a1, s_sum_warn
    call sb_cstr
    la t0, diag_warnings
    ld a1, 0(t0)
    mv a0, s5
    call int_to_dec
    mv a0, s5
    la a1, s_sum_trunc
    call sb_cstr
    la t0, diag_truncated
    ld a1, 0(t0)
    mv a0, s5
    call int_to_dec
    mv a0, s5
    la a1, s_sum_files
    call sb_cstr
    mv a0, s5
    mv a1, s3
    call int_to_dec
    mv a0, s5
    mv a1, s4
    call sb_str
    mv a0, s2
    call holes_for
    beqz a0, 4f
    sd a0, 0(sp)
    mv a0, s5
    la a1, s_sum_holes
    call sb_cstr
    mv a0, s5
    ld a1, 0(sp)
    call int_to_dec
4:
    mv a0, s5
    la a1, s_brace_nl
    call sb_cstr
    j 8f
5:
    la t0, diag_errors          # human: "N errors, M warnings"
    ld a1, 0(t0)
    mv a0, s5
    call int_to_dec
    la t0, diag_errors
    ld t1, 0(t0)
    la a1, s_errors
    li t2, 1
    bne t1, t2, 6f
    la a1, s_error1
6:
    mv a0, s5
    call sb_cstr
    la t0, diag_warnings
    ld t1, 0(t0)
    li t2, 1
    bne t1, t2, 7f
    mv a0, s5
    la a1, s_warning1
    call sb_cstr
    j 71f
7:
    mv a0, s5
    la a1, s_comma_sp
    call sb_cstr
    la t0, diag_warnings
    ld a1, 0(t0)
    mv a0, s5
    call int_to_dec
    mv a0, s5
    la a1, s_warnings
    call sb_cstr
71:
    mv a0, s2                   # " (N holes)"
    call holes_for
    beqz a0, 73f
    sd a0, 0(sp)
    mv a0, s5
    la a1, s_paren_sp
    call sb_cstr
    mv a0, s5
    ld a1, 0(sp)
    call int_to_dec
    ld t0, 0(sp)
    la a1, s_holes_close
    li t1, 1
    bne t0, t1, 74f
    la a1, s_hole_close
74:
    mv a0, s5
    call sb_cstr
73:
    la t0, diag_truncated
    ld t1, 0(t0)
    beqz t1, 72f
    mv a0, s5
    la a1, s_notshown1
    call sb_cstr
    la t0, diag_truncated
    ld a1, 0(t0)
    mv a0, s5
    call int_to_dec
    mv a0, s5
    la a1, s_notshown2
    call sb_cstr
72:
    mv a0, s5
    li a1, 10
    call sb_byte
8:
    mv a0, s1
    ld a1, SB_PTR(s5)
    ld a2, SB_LEN(s5)
    call out_bytes
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    ld s5, 8(sp)
    addi sp, sp, 64
    ret

# void toolchain_error(int json, int fd, char *code, void *message, void *path)
    .globl toolchain_error
toolchain_error:
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
    mv s4, a4
    call sb_new
    mv s5, a0
    beqz s0, 5f
    la a1, s_te_head
    call sb_cstr
    mv a0, s5
    mv a1, s2
    call sb_cstr
    mv a0, s5
    la a1, s_name
    call sb_cstr
    mv a0, s2
    call catalog_name
    mv a1, a0
    mv a0, s5
    call sb_str
    mv a0, s5
    li a1, '"'
    call sb_byte
    mv a0, s5
    la a1, s_message
    call sb_cstr
    mv a0, s5
    mv a1, s3
    call json_str_into
    beqz s4, 1f
    mv a0, s5
    la a1, s_path_key
    call sb_cstr
    mv a0, s5
    mv a1, s4
    call json_str_into
1:
    mv a0, s5
    la a1, s_brace_nl
    call sb_cstr
    j 8f
5:
    la a1, s_err_open
    call sb_cstr
    mv a0, s5
    mv a1, s2
    call sb_cstr
    mv a0, s5
    la a1, s_bracket
    call sb_cstr
    mv a0, s5
    mv a1, s3
    call sb_str
    mv a0, s5
    li a1, 10
    call sb_byte
8:
    mv a0, s1
    ld a1, SB_PTR(s5)
    ld a2, SB_LEN(s5)
    call out_bytes
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    ld s5, 8(sp)
    addi sp, sp, 64
    ret

# long levenshtein(char *a, long na, char *b, long nb, long max)
# Edit distance between two byte strings; returns max+1 as soon as it must exceed max.
    .globl levenshtein
levenshtein:
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
    mv s4, a4
    sub t0, s1, s3              # lengths alone differ by more than max
    bgez t0, 1f
    neg t0, t0
1:
    bgt t0, s4, 90f
    addi a0, s3, 1
    slli a0, a0, 3
    call alloc
    mv s5, a0                   # row[0..nb]
    li t0, 0
2:
    bgt t0, s3, 3f
    slli t1, t0, 3
    add t1, s5, t1
    sd t0, 0(t1)
    addi t0, t0, 1
    j 2b
3:
    li s6, 1                    # i over a
4:
    bgt s6, s1, 80f
    ld t2, 0(s5)                # diagonal (row[0] of the previous row)
    sd s6, 0(s5)
    mv t6, s6                   # row minimum
    li t0, 1                    # j over b
5:
    bgt t0, s3, 7f
    slli t1, t0, 3
    add t1, s5, t1
    ld t3, 0(t1)                # above
    ld t4, -8(t1)               # left (already updated)
    add t5, s0, s6
    lbu t5, -1(t5)
    add a5, s2, t0
    lbu a5, -1(a5)
    beq t5, a5, 51f
    addi t2, t2, 1              # substitution
51:
    addi t3, t3, 1
    bge t3, t2, 52f
    mv t2, t3
52:
    addi t4, t4, 1
    bge t4, t2, 53f
    mv t2, t4
53:
    ld t3, 0(t1)                # next diagonal = old above
    sd t2, 0(t1)
    bge t2, t6, 54f
    mv t6, t2
54:
    mv t2, t3
    addi t0, t0, 1
    j 5b
7:
    bgt t6, s4, 90f             # every path already exceeds max
    addi s6, s6, 1
    j 4b
80:
    slli t0, s3, 3
    add t0, s5, t0
    ld a0, 0(t0)
    ble a0, s4, 99f
90:
    addi a0, s4, 1
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

# long name_distance(void *a, void *b)
# Edit distance between two strs, capped at 3 (only 0-2 matter for suggestions).
    .globl name_distance
name_distance:
    mv t0, a1
    ld a1, STR_LEN(a0)
    addi a0, a0, STR_DATA
    ld a3, STR_LEN(t0)
    addi a2, t0, STR_DATA
    li a4, 2
    tail levenshtein

# void *diag_last(void)
# The most recently added diagnostic, or 0.
    .globl diag_last
diag_last:
    la t0, diag_n
    ld t1, 0(t0)
    beqz t1, 1f
    la t0, diag_arr
    ld t0, 0(t0)
    slli t1, t1, 3
    add t0, t0, t1
    ld a0, -8(t0)
    ret
1:
    li a0, 0
    ret
