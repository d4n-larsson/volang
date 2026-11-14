# Programs across files: import resolution, loading, cycle detection, binding (contracts/grammar.md §Imports).
    .include "defs.inc"

    .equ FS_NEW, 0
    .equ FS_LOADING, 1
    .equ FS_LOADED, 2
    .equ FS_CHECKED, 3

    .section .bss
    .align 3
ld_order:   .space 8                # files in dependency order (dependencies first)
ld_stack:   .space 8                # import nodes on the current DFS path
    .globl ld_hard
ld_hard:    .space 8                # a loaded file had a hard syntax error

    .section .rodata
s_std:      .string "std"
e0202: .string "E0202"
e0205: .string "E0205"
e0206: .string "E0206"
m_short:    .string "An import needs a file and a name: `import file.name;`."
m_no_file:  .string "Cannot find `$1` for this import."
m_no_item:  .string "`$1` has no item `$2`."
m_cycle:    .string "Import cycle: $1."
m_imported: .string "`$1` is imported here."
m_dup:      .string "`$1` is already defined."
s_arrow:    .string " -> "
s_dot_vo:   .string ".vo"

    .text
# long file_find(void *path)
# The id of the registered file with this path, or -1.
    .globl file_find
file_find:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    li s1, 0
1:
    la t0, file_count
    ld t0, 0(t0)
    bge s1, t0, 8f
    mv a0, s1
    call file_get
    ld t0, FILE_GEN(a0)         # spec 006: only files of the current generation
    la t1, cur_gen
    ld t1, 0(t1)
    bne t0, t1, 2f
    ld a0, FILE_PATH(a0)
    mv a1, s0
    call str_eq
    bnez a0, 9f
2:
    addi s1, s1, 1
    j 1b
8:
    li s1, -1
9:
    mv a0, s1
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void *import_rel(void *import)
# The imported file relative to the importer's directory: "a/b.vo" for `import a.b.name;`.
    .globl import_rel
import_rel:
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
    addi t1, t1, -1
    bge s2, t1, 2f
    beqz s2, 3f
    mv a0, s1
    li a1, '/'
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
    la a1, s_dot_vo
    call sb_cstr
    mv a0, s1
    call sb_finish
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    ld s2, 0(sp)
    addi sp, sp, 32
    ret

# void *import_path(void *import, void *rel, void *importer)
# The path an import loads (spec 007 research R2, R8): `std/MODULE.vo` for `import std.MODULE.x;`,
# a package's directory for a listed package name, else rel under the importer's directory.
    .globl import_path
import_path:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    sd s2, 0(sp)
    mv s0, a0
    mv s1, a1
    mv s2, a2
    ld a0, N_A(s0)
    li a1, 0
    call vec_get
    la a1, s_std
    call str_eq_cstr
    beqz a0, 1f
    mv a0, s1                   # std: the embedded file's own path
    j 9f
1:
    mv a0, s0                   # a package (0 when the first segment is none)
    mv a1, s2
    call pkg_import_path
    bnez a0, 9f
    ld a0, FILE_DIR(s2)
    mv a1, s1
    call str_join2
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    ld s2, 0(sp)
    addi sp, sp, 32
    ret

# long std_lookup(void *path)
# For a path under std/: registers the embedded file (FILE_STD) and returns its id, or -1 when
# there is no such module; 0 for any other path (read from the disk).
    .globl std_lookup
std_lookup:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    ld t0, STR_LEN(s0)
    li a0, 0
    li t1, 4
    blt t0, t1, 9f
    lbu t1, STR_DATA(s0)        # "std/"
    li t2, 's'
    bne t1, t2, 9f
    lbu t1, STR_DATA+1(s0)
    li t2, 't'
    bne t1, t2, 9f
    lbu t1, STR_DATA+2(s0)
    li t2, 'd'
    bne t1, t2, 9f
    lbu t1, STR_DATA+3(s0)
    li t2, '/'
    bne t1, t2, 9f
    la s1, std_files
1:
    ld a1, 0(s1)
    li a0, -1
    beqz a1, 9f
    mv a0, s0
    call str_eq_cstr
    bnez a0, 2f
    addi s1, s1, 24
    j 1b
2:
    mv a0, s0
    ld a1, 8(s1)
    ld a2, 16(s1)
    call file_add
    mv s1, a0
    call file_get
    li t0, 1
    sd t0, FILE_STD(a0)
    mv a0, s1
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void std_runtime_grants(void)
# Before a program runs: clears the declared effect sets of standard-library functions, so their
# frames take their caller's grant (as anonymous functions do) and scopes such as net("localhost")
# keep applying inside the library (spec 007 research R6). The checker is done with them.
    .globl std_runtime_grants
std_runtime_grants:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    sd s2, 0(sp)
    li s0, 0
1:
    la t0, file_count
    ld t0, 0(t0)
    bge s0, t0, 9f
    mv a0, s0
    call file_get
    addi s0, s0, 1
    ld t0, FILE_STD(a0)
    beqz t0, 1b
    ld t0, FILE_AST(a0)
    beqz t0, 1b
    ld s1, N_A(t0)              # items
    li s2, 0
2:
    ld t0, VEC_N(s1)
    bge s2, t0, 1b
    mv a0, s1
    mv a1, s2
    call vec_get
    addi s2, s2, 1
    lhu t0, N_KIND(a0)
    li t1, N_FN
    bne t0, t1, 2b
    call item_fx
    beqz a0, 2b
    sd zero, FX_DECLARED(a0)
    j 2b
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    ld s2, 0(sp)
    addi sp, sp, 32
    ret

# void *str_join2(void *a, void *b)
    .globl str_join2
str_join2:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    mv s1, a1
    call sb_new
    sd a0, 0(sp)
    mv a1, s0
    call sb_str
    ld a0, 0(sp)
    mv a1, s1
    call sb_str
    ld a0, 0(sp)
    call sb_finish
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# long load_file(void *path)
# Registers, lexes and parses a file once; returns its id (-1 when it cannot be read).
    .globl load_file
load_file:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    call file_find
    bltz a0, 0f
    mv s1, a0
    call file_get
    ld t0, FILE_AST(a0)
    mv a0, s1
    bnez t0, 9f
    j 2f                        # only lexed by a missing-import scan: lex again (reported) and parse
0:
    mv a0, s0                   # spec 007: std/MODULE.vo comes from the binary, never the disk
    call std_lookup
    beqz a0, 7f
    bltz a0, 8f
    j 2f
7:
    addi a0, s0, STR_DATA
    addi a1, sp, 0
    call read_whole_file
    bltz a0, 8f
    mv a2, a0
    ld a1, 0(sp)
    mv a0, s0
    call file_add
2:
    mv s1, a0
    call lex
    mv a0, s1
    call parse_file
    la t0, p_hard
    ld t0, 0(t0)
    beqz t0, 1f
    la t1, ld_hard
    sd t0, 0(t1)
1:
    mv a0, s1
    j 9f
8:
    li a0, -1
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void report_cycle(void *import, long target)
# E0206 at the import that closes a cycle; the message and notes follow the chain.
    .globl report_cycle
report_cycle:
    addi sp, sp, -64
    sd ra, 56(sp)
    sd s0, 48(sp)
    sd s1, 40(sp)
    sd s2, 32(sp)
    sd s3, 24(sp)
    sd s4, 16(sp)
    mv s0, a0
    mv s1, a1
    la t0, ld_stack             # find where the target file entered the path
    ld s2, 0(t0)
    li s3, 0
1:
    ld t0, VEC_N(s2)
    bge s3, t0, 2f
    mv a0, s2
    mv a1, s3
    call vec_get
    lwu t0, N_FILE(a0)
    beq t0, s1, 2f
    addi s3, s3, 1
    j 1b
2:
    call sb_new                 # "a.vo -> b.vo -> a.vo"
    mv s4, a0
    mv a0, s1
    call file_get
    ld a1, FILE_PATH(a0)
    mv a0, s4
    call sb_str
    mv t0, s3
    sd t0, 0(sp)
3:
    ld t0, 0(sp)
    ld t1, VEC_N(s2)
    bge t0, t1, 4f
    mv a0, s2
    mv a1, t0
    call vec_get
    ld t0, N_D(a0)              # target file + 1
    addi a0, t0, -1
    call file_get
    sd a0, 8(sp)
    mv a0, s4
    la a1, s_arrow
    call sb_cstr
    ld t0, 8(sp)
    ld a1, FILE_PATH(t0)
    mv a0, s4
    call sb_str
    ld t0, 0(sp)
    addi t0, t0, 1
    sd t0, 0(sp)
    j 3b
4:
    mv a0, s4
    la a1, s_arrow
    call sb_cstr
    mv a0, s1
    call file_get
    ld a1, FILE_PATH(a0)
    mv a0, s4
    call sb_str
    mv a0, s4
    call sb_finish
    mv a1, a0
    la a0, m_cycle
    li a2, 0
    li a3, 0
    call msg
    mv a2, a0
    la a0, e0206
    mv a1, s0
    call ck_err
    mv s4, a0
5:
    ld t0, VEC_N(s2)            # one note per import on the cycle
    bge s3, t0, 9f
    mv a0, s2
    mv a1, s3
    call vec_get
    sd a0, 0(sp)
    ld t0, N_D(a0)
    addi a0, t0, -1
    call file_get
    ld a1, FILE_PATH(a0)
    la a0, m_imported
    li a2, 0
    li a3, 0
    call msg
    mv a4, a0
    ld t0, 0(sp)
    mv a0, s4
    lwu a1, N_FILE(t0)
    lwu a2, N_START(t0)
    lwu a3, N_END(t0)
    call diag_add_related
    addi s3, s3, 1
    j 5b
9:
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    addi sp, sp, 64
    ret

# void load_imports(int file)
# Depth-first: loads every imported file, reports missing files and cycles, and appends files
# to ld_order after their dependencies.
    .globl load_imports
load_imports:
    addi sp, sp, -64
    sd ra, 56(sp)
    sd s0, 48(sp)
    sd s1, 40(sp)
    sd s2, 32(sp)
    sd s3, 24(sp)
    sd s4, 16(sp)
    mv s0, a0
    call file_get
    mv s1, a0                   # file record
    li t0, FS_LOADING
    sd t0, FILE_STATE(s1)
    ld t0, FILE_AST(s1)
    ld s2, N_A(t0)              # items
    li s3, 0
1:
    ld t0, VEC_N(s2)
    bge s3, t0, 8f
    mv a0, s2
    mv a1, s3
    call vec_get
    addi s3, s3, 1
    mv s4, a0
    lhu t0, N_KIND(s4)
    li t1, N_IMPORT
    bne t0, t1, 1b
    sd zero, N_D(s4)
    ld t0, N_A(s4)
    ld t0, VEC_N(t0)
    li t1, 2
    bge t0, t1, 2f
    la a0, m_short
    call str_from_cstr
    mv a2, a0
    la a0, e0205
    mv a1, s4
    call ck_err
    j 1b
2:
    mv a0, s4
    call import_rel
    sd a0, 0(sp)
    mv a1, a0
    mv a0, s4
    mv a2, s1
    call import_path            # spec 007: std/ and package roots
    li t0, -1
    beq a0, t0, 1b              # a package that failed its check (E0701/E0702, reported)
    call load_file
    bgez a0, 3f
    la a0, m_no_file            # missing file: E0205
    ld a1, 0(sp)
    li a2, 0
    li a3, 0
    call msg
    mv a2, a0
    la a0, e0205
    mv a1, s4
    call ck_err
    j 1b
3:
    addi t0, a0, 1
    sd t0, N_D(s4)              # target file + 1
    sd a0, 8(sp)
    call file_get
    ld t0, FILE_STATE(a0)
    li t1, FS_LOADING
    bne t0, t1, 4f
    mv a0, s4                   # back to a file on the current path: a cycle
    ld a1, 8(sp)
    call report_cycle
    sd zero, N_D(s4)
    j 1b
4:
    li t1, FS_NEW
    bne t0, t1, 1b
    la t0, ld_stack
    ld a0, 0(t0)
    mv a1, s4
    call vec_push
    ld a0, 8(sp)
    call load_imports
    la t0, ld_stack
    ld t0, 0(t0)
    ld t1, VEC_N(t0)
    addi t1, t1, -1
    sd t1, VEC_N(t0)
    j 1b
8:
    li t0, FS_LOADED
    sd t0, FILE_STATE(s1)
    la t0, ld_order
    ld a0, 0(t0)
    mv a1, s0
    call vec_push
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    addi sp, sp, 64
    ret

# void bind_imports(int file)
# Adds each imported item (a top-level fn, record or variant of the target file) to the scope.
    .globl bind_imports
bind_imports:
    addi sp, sp, -64
    sd ra, 56(sp)
    sd s0, 48(sp)
    sd s1, 40(sp)
    sd s2, 32(sp)
    sd s3, 24(sp)
    sd s4, 16(sp)
    call file_get
    ld t0, FILE_AST(a0)
    ld s0, N_A(t0)
    li s1, 0
1:
    ld t0, VEC_N(s0)
    bge s1, t0, 9f
    mv a0, s0
    mv a1, s1
    call vec_get
    addi s1, s1, 1
    mv s2, a0
    lhu t0, N_KIND(s2)
    li t1, N_IMPORT
    bne t0, t1, 1b
    ld t0, N_D(s2)
    bnez t0, 11f
    ld a0, N_B(s2)              # unresolved (already reported): a silent placeholder, no cascade
    beqz a0, 1b
    call bind_placeholder
    j 1b
11:
    addi s3, t0, -1             # target file
    ld s4, N_B(s2)              # name node
    mv a0, s3                   # search the target's own top-level items
    call file_get
    ld t0, FILE_SCOPE(a0)
    sd t0, 0(sp)
2:
    ld t0, 0(sp)
    beqz t0, 5f
    ld t1, SY_DECL(t0)
    beqz t1, 3f
    lwu t2, N_FILE(t1)
    bne t2, s3, 3f
    ld t3, SY_KIND(t0)
    li t4, SYM_LOCAL
    beq t3, t4, 3f
    ld a0, SY_NAME(t0)
    ld a1, N_A(s4)
    call str_eq
    bnez a0, 4f
3:
    ld t0, 0(sp)
    ld t0, SY_NEXT(t0)
    sd t0, 0(sp)
    j 2b
4:
    ld a0, N_A(s4)              # duplicate with something already visible?
    call scope_find
    beqz a0, 41f
    la a0, e0202
    mv a1, s4
    la a2, m_dup
    ld a3, N_A(s4)
    call ck_err1
    j 1b
41:
    ld t0, 0(sp)
    ld a0, SY_NAME(t0)
    ld a1, SY_KIND(t0)
    ld a2, SY_DECL(t0)
    ld a3, SY_TYPE(t0)
    mv a4, s4
    call sym_new
    call scope_add
    j 1b
5:
    mv a0, s3                   # no such item: E0205 naming the file
    call file_get
    ld a1, FILE_PATH(a0)
    la a0, m_no_item
    ld a2, N_A(s4)
    li a3, 0
    call msg
    mv a2, a0
    la a0, e0205
    mv a1, s4
    call ck_err
    mv a0, s4
    call bind_placeholder
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

# void *ld_files(void)
# The files of the last checked program (file ids, dependencies first).
    .globl ld_files
ld_files:
    la t0, ld_order
    ld a0, 0(t0)
    ret

# void check_program(int file, int mode)
# Loads the entry file and everything it imports, then checks them, dependencies first.
# mode 0 = check, 1 = run (main required in the entry), 2 = test.
    .globl check_program
check_program:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    mv s1, a1
    call ck_init
    call fx_program_begin
    mv a0, s0
    call file_get
    ld t0, FILE_STATE(a0)
    li t1, FS_CHECKED
    beq t0, t1, 9f              # already checked as another file's import
    ld t0, FILE_AST(a0)
    bnez t0, 1f
    mv a0, s0
    call lex
    mv a0, s0
    call parse_file
    la t0, p_hard
    ld t0, 0(t0)
    la t1, ld_hard
    sd t0, 0(t1)
1:
    call vec_new
    la t0, ld_order
    sd a0, 0(t0)
    call vec_new
    la t0, ld_stack
    sd a0, 0(t0)
    mv a0, s0
    call load_imports
    la t0, ld_hard
    ld t0, 0(t0)
    bnez t0, 9f
    li s2, 0
2:
    la t0, ld_order
    ld t0, 0(t0)
    ld t1, VEC_N(t0)
    bge s2, t1, 9f
    mv a0, t0
    mv a1, s2
    call vec_get
    mv s3, a0
    addi s2, s2, 1
    call file_get
    ld t0, FILE_STATE(a0)
    li t1, FS_CHECKED
    beq t0, t1, 2b
    li t1, FS_CHECKED
    sd t1, FILE_STATE(a0)
    li a1, 0
    bne s3, s0, 3f
    mv a1, s1
3:
    mv a0, s3
    call check_items
    j 2b
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# void bind_placeholder(void *name_node)
# A silent symbol for a name whose import failed (later uses are not reported).
    .globl bind_placeholder
bind_placeholder:
    addi sp, sp, -16
    sd ra, 8(sp)
    ld a0, N_A(a0)
    li a1, SYM_LOCAL
    li a2, 0
    la t0, t_error
    ld a3, 0(t0)
    li a4, 0
    call sym_new
    li t0, SYF_USED
    sd t0, SY_FLAGS(a0)
    li t0, -1
    sd t0, SY_SLOT(a0)
    call scope_add
    ld ra, 8(sp)
    addi sp, sp, 16
    ret

# int file_defines(int file, void *name)
# 1 when the file (lexed) declares a top-level fn, record or variant with this name.
    .globl file_defines
file_defines:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s1, a1
    call file_get
    ld s0, FILE_TOKENS(a0)
    ld s3, FILE_TEXT(a0)
    li s2, 0                    # brace depth
1:
    lhu t0, TOK_KIND(s0)
    beqz t0, 8f
    li t1, T_LBRACE
    bne t0, t1, 2f
    addi s2, s2, 1
    j 5f
2:
    li t1, T_RBRACE
    bne t0, t1, 3f
    addi s2, s2, -1
    j 5f
3:
    bnez s2, 5f
    li t1, T_FN
    beq t0, t1, 4f
    li t1, T_RECORD
    beq t0, t1, 4f
    li t1, T_VARIANT
    bne t0, t1, 5f
4:
    lhu t0, TOK_SIZE+TOK_KIND(s0)
    li t1, T_IDENT
    beq t0, t1, 41f
    li t1, T_TYPE_IDENT
    bne t0, t1, 5f
41:
    lwu t0, TOK_SIZE+TOK_START(s0)
    lwu t1, TOK_SIZE+TOK_END(s0)
    mv a0, s1
    add a1, s3, t0
    sub a2, t1, t0
    call str_eq_bytes
    bnez a0, 9f
5:
    addi s0, s0, TOK_SIZE
    j 1b
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

# void *str_join2_c(void *a, char *b)
    .globl str_join2_c
str_join2_c:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    mv s0, a0
    mv a0, a1
    call str_from_cstr
    mv a1, a0
    mv a0, s0
    call str_join2
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret

# long path_defines(void *path, void *name)
# The file's id when it declares a top-level item `name`, else -1. A file not loaded yet is
# read and lexed quietly (its own errors are reported only if it is imported later).
    .globl path_defines
path_defines:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    mv s1, a1
    call file_find
    bgez a0, 2f
    addi a0, s0, STR_DATA
    addi a1, sp, 0
    call read_whole_file
    bltz a0, 8f
    mv a2, a0
    ld a1, 0(sp)
    mv a0, s0
    call file_add
2:
    sd a0, 0(sp)
    call file_get
    ld t0, FILE_TOKENS(a0)
    bnez t0, 3f
    la t0, diag_mute
    li t1, 1
    sd t1, 0(t0)
    ld a0, 0(sp)
    call lex
    la t0, diag_mute
    sd zero, 0(t0)
3:
    ld a0, 0(sp)
    mv a1, s1
    call file_defines
    beqz a0, 8f
    ld a0, 0(sp)
    j 9f
8:
    li a0, -1
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void *import_text_for(void *rel, void *name)
# "a/b.vo" + name -> "a.b.name".
    .globl import_text_for
import_text_for:
    addi sp, sp, -48            # 0, 8: scratch; 16..47: saved registers
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    mv s0, a0
    mv s1, a1
    call sb_new
    mv s2, a0
    ld t0, STR_LEN(s0)
    addi t0, t0, -3             # without ".vo"
    sd t0, 8(sp)
    li t1, 0
1:
    ld t0, 8(sp)
    bge t1, t0, 2f
    add t2, s0, t1
    lbu a1, STR_DATA(t2)
    li t3, '/'
    bne a1, t3, 11f
    li a1, '.'
11:
    sd t1, 0(sp)
    mv a0, s2
    call sb_byte
    ld t1, 0(sp)
    addi t1, t1, 1
    j 1b
2:
    mv a0, s2
    li a1, '.'
    call sb_byte
    mv a0, s2
    mv a1, s1
    call sb_str
    mv a0, s2
    call sb_finish
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    addi sp, sp, 48
    ret

# void *scan_dir(void *dir, void *rel, void *name, int self)
# Searches the *.vo files below dir (ending in '/' or empty; rel is the same directory relative to
# the importing file) for a top-level item `name`, skipping the file `self`. Returns the import
# text ("a.b.name") or 0.
    .globl scan_dir
scan_dir:
    addi sp, sp, -128           # 0: offset, 8: entry, 16: entry name, 24: path; 48..127: saved
    sd ra, 120(sp)
    sd s0, 112(sp)
    sd s1, 104(sp)
    sd s2, 96(sp)
    sd s3, 88(sp)
    sd s4, 80(sp)
    sd s5, 72(sp)
    sd s6, 64(sp)
    sd s7, 56(sp)
    mv s0, a0
    mv s1, a1
    mv s2, a2
    mv s7, a3
    li s6, 0                    # result
    ld t0, STR_LEN(s0)
    bnez t0, 1f
    la a0, s_dot                # an empty directory part means "."
    j 2f
1:
    addi a0, s0, STR_DATA
2:
    li a1, O_DIRECTORY
    call sys_open
    bltz a0, 99f
    mv s3, a0
    li a0, 32768
    call alloc
    mv s4, a0
3:
    bnez s6, 90f
    mv a0, s3
    mv a1, s4
    li a2, 32768
    call sys_getdents64
    blez a0, 90f
    mv s5, a0
    sd zero, 0(sp)
4:
    bnez s6, 90f
    ld t0, 0(sp)
    bge t0, s5, 3b
    add t1, s4, t0
    lhu t2, 16(t1)
    add t0, t0, t2
    sd t0, 0(sp)
    sd t1, 8(sp)
    lbu t2, 19(t1)
    li t3, '.'
    beq t2, t3, 4b
    addi a0, t1, 19
    call str_from_cstr
    sd a0, 16(sp)
    ld t1, 8(sp)
    lbu t2, 18(t1)
    li t3, 4                    # DT_DIR: recurse
    bne t2, t3, 6f
    mv a0, s0
    ld a1, 16(sp)
    call str_join2
    la a1, s_slash
    call str_join2_c
    sd a0, 24(sp)
    mv a0, s1
    ld a1, 16(sp)
    call str_join2
    la a1, s_slash
    call str_join2_c
    mv a1, a0
    ld a0, 24(sp)
    mv a2, s2
    mv a3, s7
    call scan_dir
    mv s6, a0
    j 4b
6:
    ld a0, 16(sp)
    call ends_with_vo
    beqz a0, 4b
    mv a0, s0
    ld a1, 16(sp)
    call str_join2
    mv a1, s2
    call path_defines
    bltz a0, 4b
    beq a0, s7, 4b              # the importing file itself
    mv a0, s1
    ld a1, 16(sp)
    call str_join2
    mv a1, s2
    call import_text_for
    mv s6, a0
    j 4b
90:
    mv a0, s3
    call sys_close
99:
    mv a0, s6
    ld ra, 120(sp)
    ld s0, 112(sp)
    ld s1, 104(sp)
    ld s2, 96(sp)
    ld s3, 88(sp)
    ld s4, 80(sp)
    ld s5, 72(sp)
    ld s6, 64(sp)
    ld s7, 56(sp)
    addi sp, sp, 128
    ret

# void *find_import(int file, void *name)
# The import text that would bring `name` into the file from a .vo file under its directory, or 0.
    .globl find_import
find_import:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    mv s1, a1
    call file_get
    ld a0, FILE_DIR(a0)
    sd a0, 0(sp)
    li a0, 0
    li a1, 0
    call str_new
    mv a1, a0                   # rel: ""
    ld a0, 0(sp)
    mv a2, s1
    mv a3, s0
    call scan_dir
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# long import_insert_at(int file)
# Where a new import line goes: after the last import line, or at the start of the file.
    .globl import_insert_at
import_insert_at:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    sd s2, 0(sp)
    call file_get
    mv s2, a0
    ld t0, FILE_AST(s2)
    ld s0, N_A(t0)
    li s1, -1                   # end of the last import
    ld t1, VEC_N(s0)
    li t2, 0
1:
    bge t2, t1, 2f
    ld t3, VEC_DATA(s0)
    slli t4, t2, 3
    add t3, t3, t4
    ld t3, 0(t3)
    addi t2, t2, 1
    lhu t4, N_KIND(t3)
    li t5, N_IMPORT
    bne t4, t5, 1b
    lwu s1, N_END(t3)
    j 1b
2:
    li a0, 0
    bltz s1, 9f
    ld t0, FILE_TEXT(s2)        # just after the newline that ends that line
    ld t1, FILE_LEN(s2)
    mv a0, s1
3:
    bge a0, t1, 9f
    add t2, t0, a0
    lbu t2, 0(t2)
    addi a0, a0, 1
    li t3, 10
    bne t2, t3, 3b
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    ld s2, 0(sp)
    addi sp, sp, 32
    ret

    .section .rodata
s_dot:   .string "."
s_slash: .string "/"
