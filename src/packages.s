# Packages (spec 007 US3): the vo.packages manifest, content hashes, import resolution and
# `vo package` (research R8, contracts/packages.md).
    .include "defs.inc"

# A manifest entry
    .equ PE_NAME, 0             # str
    .equ PE_DIR, 8              # str: the directory, absolute ("" is /)
    .equ PE_RAW, 16             # str: the directory as written in the manifest
    .equ PE_PIN, 24             # 32 bytes: the pinned hash
    .equ PE_STATE, 56           # 0 not checked yet, 1 verified, 2 failed (reported)
    .equ PE_START, 64           # the line's span in the manifest
    .equ PE_END, 72
    .equ PE_SIZE, 80

    .section .rodata
s_manifest: .string "/vo.packages"
s_header:   .string "# vo packages: update with `vo package ENTRY NAME DIRECTORY`\n"
s_sha:      .string "sha256:"
s_std_word: .string "std"
s_dotdot:   .string "../"
hexchars:   .string "0123456789abcdef"
e0701: .string "E0701"
e0702: .string "E0702"
e0703: .string "E0703"
e0704: .string "E0704"
e0705: .string "E0705"
m_changed:  .string "Package $1 changed: the manifest pins $2, its files hash to $3."
m_review:   .string "$1 Review the change, then run `vo package $2`."
m_missing:  .string "Package $1: the directory $2 is missing or has no .vo files."
m_bad_line: .string "Write a package as `NAME DIRECTORY sha256:HEX` (NAME an identifier other than std, 64 lowercase hex digits)."
m_dup:      .string "Package $1 is listed twice."
m_clash:    .string "`$1` is both a listed package and a directory next to this file: rename one of them."
m_pkg_args: .string "`package` takes ENTRY NAME DIRECTORY, or --remove ENTRY NAME."
m_pkg_name: .string "A package name is an identifier other than std: `$1` is not."
m_no_entry: .string "No package `$1` is listed in the manifest."
m_dir_bad:  .string "Cannot find the directory `$1`."
w_added:    .string "added "
w_updated:  .string "updated "
w_unchanged: .string "unchanged "
w_removed:  .string "removed "
w_arrow:    .string " -> "
w_colon_sp: .string ": "
j_pkg:      .string "{\"v\":1,\"kind\":\"package\",\"name\":"
j_change:   .string ",\"change\":\""
j_dir:      .string "\",\"directory\":"
j_hash:     .string ",\"hash\":\""
j_old:      .string "\",\"old_hash\":\""
j_end:      .string "\"}\n"
c_added:    .string "added"
c_updated:  .string "updated"
c_unchanged: .string "unchanged"
c_removed:  .string "removed"

    .bss
    .align 3
pkg_searched: .space 8          # 1 once the manifest was looked for
    .globl pkg_manifest
pkg_manifest: .space 8          # its file id, or -1
    .globl pkg_dir
pkg_dir:      .space 8          # str: its directory (or where it would be)
    .globl pkg_entries
pkg_entries:  .space 8          # vec of entries
pkg_tok_end:  .space 8
pkg_hash_buf: .space 32

    .text
# ---- the manifest ----

# void pkg_load(void *dir)
# Looks for the nearest vo.packages from dir up, once, and parses it (E0703, E0704).
    .globl pkg_load
pkg_load:
    la t0, pkg_searched
    ld t1, 0(t0)
    bnez t1, 9f
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    li t1, 1
    sd t1, 0(t0)
    mv s0, a0
    call vec_new
    la t0, pkg_entries
    sd a0, 0(t0)
    mv a0, s0
    la a1, s_manifest
    la a2, pkg_dir
    call find_up_dir
    la t0, pkg_manifest
    sd a0, 0(t0)
    bltz a0, 8f
    call pkg_parse
8:
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
9:
    ret

# char *pkg_token(char *p, char *end)
# The next space-separated token at or after p (p == end: none); pkg_tok_end gets its end.
    .globl pkg_token
pkg_token:
1:
    bgeu a0, a1, 3f
    lbu t0, 0(a0)
    li t1, ' '
    beq t0, t1, 2f
    li t1, 9
    beq t0, t1, 2f
    li t1, 13
    bne t0, t1, 3f
2:
    addi a0, a0, 1
    j 1b
3:
    mv t2, a0
4:
    bgeu t2, a1, 5f
    lbu t0, 0(t2)
    li t1, ' '
    beq t0, t1, 5f
    li t1, 9
    beq t0, t1, 5f
    li t1, 13
    beq t0, t1, 5f
    addi t2, t2, 1
    j 4b
5:
    la t0, pkg_tok_end
    sd t2, 0(t0)
    ret

# long pkg_name_ok(char *p, long n)
# 1 when p[0..n) is an identifier other than `std`.
    .globl pkg_name_ok
pkg_name_ok:
    li t0, 0
    blez a1, 8f
1:
    bge t0, a1, 6f
    add t1, a0, t0
    lbu t1, 0(t1)
    li t2, '_'
    beq t1, t2, 5f
    ori t3, t1, 32              # a letter, either case
    li t2, 'a'
    blt t3, t2, 2f
    li t2, 'z'
    ble t3, t2, 5f
2:
    beqz t0, 8f                 # digits only after the first character
    li t2, '0'
    blt t1, t2, 8f
    li t2, '9'
    bgt t1, t2, 8f
5:
    addi t0, t0, 1
    j 1b
6:
    li t0, 3                    # not std
    bne a1, t0, 7f
    lbu t1, 0(a0)
    li t2, 's'
    bne t1, t2, 7f
    lbu t1, 1(a0)
    li t2, 't'
    bne t1, t2, 7f
    lbu t1, 2(a0)
    li t2, 'd'
    beq t1, t2, 8f
7:
    li a0, 1
    ret
8:
    li a0, 0
    ret

# long pkg_hex_decode(char *p, char *out)
# 64 lowercase hex digits to 32 bytes; 1 when they are, else 0.
    .globl pkg_hex_decode
pkg_hex_decode:
    li t0, 0
1:
    li t1, 64
    bge t0, t1, 8f
    add t1, a0, t0
    lbu t2, 0(t1)
    lbu t3, 1(t1)
    li t5, 0                    # two nibbles
    mv t4, t2
    li a2, 0
2:
    li t6, '0'
    blt t4, t6, 9f
    li t6, '9'
    bgt t4, t6, 3f
    addi t4, t4, -'0'
    j 4f
3:
    li t6, 'a'
    blt t4, t6, 9f
    li t6, 'f'
    bgt t4, t6, 9f
    addi t4, t4, -87
4:
    slli a2, a2, 4
    or a2, a2, t4
    addi t5, t5, 1
    mv t4, t3
    li t6, 2
    blt t5, t6, 2b
    srli t1, t0, 1
    add t1, a1, t1
    sb a2, 0(t1)
    addi t0, t0, 2
    j 1b
8:
    li a0, 1
    ret
9:
    li a0, 0
    ret

# void pkg_parse(long file)
# Parses the manifest into pkg_entries; bad lines are E0703, a second entry of a name E0704.
    .globl pkg_parse
pkg_parse:
    addi sp, sp, -112
    sd ra, 104(sp)
    sd s0, 96(sp)
    sd s1, 88(sp)
    sd s2, 80(sp)
    sd s3, 72(sp)
    sd s4, 64(sp)
    sd s5, 56(sp)
    sd s6, 48(sp)
    mv s0, a0
    call file_get
    ld s1, FILE_TEXT(a0)        # text
    ld t0, FILE_LEN(a0)
    add s2, s1, t0              # end
    mv s3, s1                   # the current line
1:
    bgeu s3, s2, 90f
    mv s4, s3                   # its end
2:
    bgeu s4, s2, 3f
    lbu t0, 0(s4)
    li t1, 10
    beq t0, t1, 3f
    addi s4, s4, 1
    j 2b
3:
    mv a0, s3                   # first token: blank or a comment line is nothing
    mv a1, s4
    call pkg_token
    beq a0, s4, 80f
    lbu t0, 0(a0)
    li t1, 35                   # #
    beq t0, t1, 80f
    mv s5, a0                   # NAME
    la t0, pkg_tok_end
    ld s6, 0(t0)
    mv a0, s5
    sub a1, s6, s5
    call pkg_name_ok
    beqz a0, 70f
    mv a0, s6                   # DIRECTORY
    mv a1, s4
    call pkg_token
    beq a0, s4, 70f
    sd a0, 0(sp)
    la t0, pkg_tok_end
    ld t0, 0(t0)
    sd t0, 8(sp)
    mv a1, s4                   # sha256:HEX
    mv a0, t0
    call pkg_token
    sd a0, 16(sp)
    la t0, pkg_tok_end
    ld t0, 0(t0)
    sd t0, 24(sp)
    sub t1, t0, a0
    li t2, 71
    bne t1, t2, 70f
    mv a0, t0                   # nothing after it
    mv a1, s4
    call pkg_token
    bne a0, s4, 70f
    ld a0, 16(sp)
    la a1, s_sha
    call doc_starts
    beqz a0, 70f
    li a0, PE_SIZE
    call alloc
    sd a0, 40(sp)               # the entry
    ld a0, 16(sp)
    addi a0, a0, 7
    ld a1, 40(sp)
    addi a1, a1, PE_PIN
    call pkg_hex_decode
    beqz a0, 70f
    mv a0, s5
    sub a1, s6, s5
    call str_new
    ld t0, 40(sp)
    sd a0, PE_NAME(t0)
    call pkg_find                # a second entry of the name: E0704
    beqz a0, 4f
    la a0, m_dup
    ld t0, 40(sp)
    ld a1, PE_NAME(t0)
    li a2, 0
    li a3, 0
    call msg
    mv a5, a0
    la a0, e0704
    j 71f
4:
    ld a0, 0(sp)
    ld a1, 8(sp)
    sub a1, a1, a0
    call str_new
    ld t0, 40(sp)
    sd a0, PE_RAW(t0)
    lbu t1, STR_DATA(a0)        # absolute, or under the manifest's directory
    li t2, '/'
    beq t1, t2, 5f
    la t1, pkg_dir
    ld a0, 0(t1)
    ld a1, PE_RAW(t0)
    call path_join
5:
    ld t0, 40(sp)
    sd a0, PE_DIR(t0)
    sd zero, PE_STATE(t0)
    sub t1, s3, s1
    sd t1, PE_START(t0)
    sub t1, s4, s1
    sd t1, PE_END(t0)
    la t1, pkg_entries
    ld a0, 0(t1)
    mv a1, t0
    call vec_push
    j 80f
70:
    la a0, m_bad_line           # E0703 on the line
    call str_from_cstr
    mv a5, a0
    la a0, e0703
71:
    li a1, SEV_ERROR
    mv a2, s0
    sub a3, s3, s1
    sub a4, s4, s1
    call diag_add
80:
    addi s3, s4, 1
    j 1b
90:
    ld ra, 104(sp)
    ld s0, 96(sp)
    ld s1, 88(sp)
    ld s2, 80(sp)
    ld s3, 72(sp)
    ld s4, 64(sp)
    ld s5, 56(sp)
    ld s6, 48(sp)
    addi sp, sp, 112
    ret

# void *path_join(void *dir, void *rel)
# dir + "/" + rel ("" is the root).
    .globl path_join
path_join:
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
    li a1, '/'
    call sb_byte
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

# void *pkg_find(void *name)
# The entry listed under name, or 0.
    .globl pkg_find
pkg_find:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    mv s0, a0
    la t0, pkg_entries
    ld s1, 0(t0)
    li s2, 0
    li a0, 0
    beqz s1, 9f
1:
    ld t0, VEC_N(s1)
    li a0, 0
    bge s2, t0, 9f
    mv a0, s1
    mv a1, s2
    call vec_get
    sd a0, 0(sp)
    ld a0, PE_NAME(a0)
    mv a1, s0
    call str_eq
    addi s2, s2, 1
    beqz a0, 1b
    ld a0, 0(sp)
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    addi sp, sp, 48
    ret

# ---- the content hash ----

# long pkg_hash(void *dir, char *out)
# SHA-256 over every .vo file under dir, in byte order of the relative paths:
# PATH 0 LENGTH 0 BYTES each. 0 when hashed; -1 when dir is missing or has no .vo file.
    .globl pkg_hash
pkg_hash:
    addi sp, sp, -80
    sd ra, 72(sp)
    sd s0, 64(sp)
    sd s1, 56(sp)
    sd s2, 48(sp)
    sd s3, 40(sp)
    sd s4, 32(sp)
    mv s0, a0
    mv s1, a1
    addi a0, s0, STR_DATA
    li a1, O_DIRECTORY
    call sys_open
    li t0, -1
    bltz a0, 90f
    mv s4, a0
    la t0, path_n               # the files, appended to path_list and taken back after
    ld s2, 0(t0)
    mv a0, s0
    mv a1, s4
    call walk_dir
    mv a0, s4
    call sys_close
    mv a0, s2
    call sort_paths
    la t0, path_n
    ld t0, 0(t0)
    li a0, -1
    beq t0, s2, 80f
    call sha256_init
    mv s3, s2
1:
    la t0, path_n
    ld t0, 0(t0)
    bge s3, t0, 8f
    la t0, path_list
    ld t0, 0(t0)
    slli t1, s3, 3
    add t0, t0, t1
    ld s4, 0(t0)                # DIR/REL
    ld t0, STR_LEN(s0)
    addi t0, t0, 1
    addi a0, s4, STR_DATA
    add a0, a0, t0
    ld a1, STR_LEN(s4)
    sub a1, a1, t0
    call sha256_update
    la a0, pkg_hash_buf         # NUL
    sb zero, 0(a0)
    li a1, 1
    call sha256_update
    addi a0, s4, STR_DATA
    addi a1, sp, 0
    call read_whole_file
    bltz a0, 2f
    sd a0, 8(sp)
    call sb_new
    sd a0, 16(sp)
    ld a1, 8(sp)
    call int_to_dec
    ld t0, 16(sp)
    ld a0, SB_PTR(t0)
    ld a1, SB_LEN(t0)
    call sha256_update
    la a0, pkg_hash_buf
    sb zero, 0(a0)
    li a1, 1
    call sha256_update
    ld a0, 0(sp)
    ld a1, 8(sp)
    call sha256_update
    addi s3, s3, 1
    j 1b
2:
    li a0, -1                   # a file that cannot be read
    j 80f
8:
    mv a0, s1
    call sha256_final
    li a0, 0
80:
    la t0, path_n
    sd s2, 0(t0)
    mv t0, a0
90:
    mv a0, t0
    ld ra, 72(sp)
    ld s0, 64(sp)
    ld s1, 56(sp)
    ld s2, 48(sp)
    ld s3, 40(sp)
    ld s4, 32(sp)
    addi sp, sp, 80
    ret

# void *pkg_hash_text(char *hash)
# "sha256:" and 64 lowercase hex digits.
    .globl pkg_hash_text
pkg_hash_text:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    sd s2, 0(sp)
    mv s0, a0
    call sb_new
    mv s1, a0
    la a1, s_sha
    call sb_cstr
    li s2, 0
1:
    li t0, 32
    bge s2, t0, 9f
    add t0, s0, s2
    lbu t0, 0(t0)
    srli t1, t0, 4
    la t2, hexchars
    add t1, t2, t1
    lbu a1, 0(t1)
    mv a0, s1
    call sb_byte
    add t0, s0, s2
    lbu t0, 0(t0)
    andi t1, t0, 15
    la t2, hexchars
    add t1, t2, t1
    lbu a1, 0(t1)
    mv a0, s1
    call sb_byte
    addi s2, s2, 1
    j 1b
9:
    mv a0, s1
    call sb_finish
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    ld s2, 0(sp)
    addi sp, sp, 32
    ret

# long pkg_verify(void *entry, void *node)
# Hashes the package once: 1 when it matches its pin, else 0 (E0701 or E0702 at node, once).
    .globl pkg_verify
pkg_verify:
    ld t0, PE_STATE(a0)
    beqz t0, 1f
    xori a0, t0, 2
    snez a0, a0
    ret
1:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    mv s0, a0
    mv s1, a1
    ld a0, PE_DIR(s0)
    la a1, pkg_hash_buf
    call pkg_hash
    bltz a0, 5f
    la a0, pkg_hash_buf
    addi a1, s0, PE_PIN
    li a2, 32
    call mem_eq
    beqz a0, 4f
    li t0, 1
    sd t0, PE_STATE(s0)
    li a0, 1
    j 9f
4:
    addi a0, s0, PE_PIN         # E0701 with both hashes
    call pkg_hash_text
    sd a0, 0(sp)
    la a0, pkg_hash_buf
    call pkg_hash_text
    mv a3, a0
    ld a2, 0(sp)
    ld a1, PE_NAME(s0)
    la a0, m_changed
    call msg
    sd a0, 0(sp)
    call sb_new                 # the command that pins the package again: ENTRY NAME DIRECTORY
    sd a0, 8(sp)
    li a0, 0
    call file_get
    ld a1, FILE_PATH(a0)
    ld a0, 8(sp)
    call sb_str
    ld a0, 8(sp)
    li a1, ' '
    call sb_byte
    ld a0, 8(sp)
    ld a1, PE_NAME(s0)
    call sb_str
    ld a0, 8(sp)
    li a1, ' '
    call sb_byte
    ld a0, 8(sp)
    ld a1, PE_RAW(s0)
    call sb_str
    ld a0, 8(sp)
    call sb_finish
    mv a2, a0
    ld a1, 0(sp)
    la a0, m_review
    li a3, 0
    call msg
    mv a2, a0
    la a0, e0701
    j 6f
5:
    la a0, m_missing            # E0702
    ld a1, PE_NAME(s0)
    ld a2, PE_RAW(s0)
    li a3, 0
    call msg
    mv a2, a0
    la a0, e0702
6:
    mv a1, s1
    call ck_err
    li t0, 2
    sd t0, PE_STATE(s0)
    li a0, 0
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    addi sp, sp, 48
    ret

# ---- imports ----

# void *pkg_import_path(void *import, void *importer)
# For `import NAME.a.b.item;` with NAME listed: the file DIRECTORY/a/b.vo, after the package's
# hash is checked; -1 when the package failed its check (reported once); 0 when NAME is no package.
    .globl pkg_import_path
pkg_import_path:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    mv s0, a0
    mv s1, a1
    ld a0, FILE_DIR(s1)         # the manifest: from the first importer (the entry) up
    call rt_resolve
    bnez a0, 1f
    call rt_start_cwd
1:
    call pkg_load
    ld a0, N_A(s0)              # NAME.a.b.item: at least three segments
    ld t0, VEC_N(a0)
    li t1, 3
    li s2, 0
    blt t0, t1, 9f
    li a1, 0
    call vec_get
    call pkg_find
    beqz a0, 9f
    mv s2, a0                   # the entry
    ld a0, FILE_DIR(s1)         # E0705: NAME is also a directory next to the importer
    call sb_new
    sd a0, 0(sp)
    ld a1, FILE_DIR(s1)
    call sb_str
    ld a0, 0(sp)
    ld a1, PE_NAME(s2)
    call sb_str
    ld a0, 0(sp)
    call sb_finish
    addi a0, a0, STR_DATA
    li a1, O_DIRECTORY
    call sys_open
    bltz a0, 2f
    call sys_close
    la a0, m_clash
    ld a1, PE_NAME(s2)
    li a2, 0
    li a3, 0
    call msg
    mv a2, a0
    la a0, e0705
    mv a1, s0
    call ck_err
2:
    mv a0, s2
    mv a1, s0
    call pkg_verify
    li t0, -1
    beqz a0, 8f
    call sb_new                 # DIRECTORY/a/b.vo
    sd a0, 0(sp)
    ld a1, PE_DIR(s2)
    call sb_str
    li s1, 1
3:
    ld a0, N_A(s0)
    ld t0, VEC_N(a0)
    addi t0, t0, -1
    bge s1, t0, 4f
    mv a1, s1
    call vec_get
    sd a0, 8(sp)
    ld a0, 0(sp)
    li a1, '/'
    call sb_byte
    ld a0, 0(sp)
    ld a1, 8(sp)
    call sb_str
    addi s1, s1, 1
    j 3b
4:
    ld a0, 0(sp)
    la a1, s_dot_vo_pkg
    call sb_cstr
    ld a0, 0(sp)
    call sb_finish
    mv t0, a0
8:
    mv s2, t0
9:
    mv a0, s2
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    addi sp, sp, 48
    ret

# void *pkg_relpath(void *path)
# "@NAME/relative/path.vo" when the resolved path lies in a listed package, else 0.
    .globl pkg_relpath
pkg_relpath:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    mv s0, a0
    la t0, pkg_entries
    ld s1, 0(t0)
    li s2, 0
    li a0, 0
    beqz s1, 9f
1:
    ld t0, VEC_N(s1)
    li a0, 0
    bge s2, t0, 9f
    mv a0, s1
    mv a1, s2
    call vec_get
    addi s2, s2, 1
    sd a0, 0(sp)
    ld a1, PE_DIR(a0)
    mv a0, s0
    call path_under
    beqz a0, 1b
    call sb_new
    sd a0, 8(sp)
    li a1, '@'
    call sb_byte
    ld t0, 0(sp)
    ld a0, 8(sp)
    ld a1, PE_NAME(t0)
    call sb_str
    ld t0, 0(sp)                # the rest of the path, from its "/"
    ld t0, PE_DIR(t0)
    ld t0, STR_LEN(t0)
    ld a0, 8(sp)
    addi a1, s0, STR_DATA
    add a1, a1, t0
    ld a2, STR_LEN(s0)
    sub a2, a2, t0
    call sb_bytes
    ld a0, 8(sp)
    call sb_finish
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    addi sp, sp, 48
    ret

# ---- vo package ----

# void *pkg_rel_dir(void *from, void *to)
# to relative to the directory from (both absolute, "" is /): "shared/money", "../shared/money".
    .globl pkg_rel_dir
pkg_rel_dir:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    mv s1, a1
    ld t0, STR_LEN(s0)
    ld t1, STR_LEN(s1)
    addi t2, s0, STR_DATA
    addi t3, s1, STR_DATA
    li t4, 0                    # i
    li s2, 0                    # the last common component boundary
1:
    bge t4, t0, 2f
    bge t4, t1, 2f
    add t5, t2, t4
    lbu t5, 0(t5)
    add t6, t3, t4
    lbu t6, 0(t6)
    bne t5, t6, 3f
    li a0, '/'
    bne t5, a0, 11f
    mv s2, t4
11:
    addi t4, t4, 1
    j 1b
2:
    bne t4, t0, 21f             # from ends here: to continues with "/" or ends
    beq t4, t1, 22f
    add t6, t3, t4
    lbu t6, 0(t6)
    li a0, '/'
    bne t6, a0, 3f
22:
    mv s2, t4
    j 3f
21:
    add t5, t2, t4              # to ends here and from continues with "/"
    lbu t5, 0(t5)
    li a0, '/'
    bne t5, a0, 3f
    mv s2, t4
3:
    call sb_new
    mv s3, a0
    mv t4, s2                   # one "../" per component of from after the boundary
4:
    ld t0, STR_LEN(s0)
    bge t4, t0, 5f
    addi t1, s0, STR_DATA
    add t1, t1, t4
    lbu t1, 0(t1)
    li t2, '/'
    bne t1, t2, 41f
    sd t4, 0(sp)
    mv a0, s3
    la a1, s_dotdot
    call sb_cstr
    ld t4, 0(sp)
41:
    addi t4, t4, 1
    j 4b
5:
    ld t0, STR_LEN(s1)          # then to after the boundary
    addi t1, s2, 1
    bge t1, t0, 6f
    mv a0, s3
    addi a1, s1, STR_DATA
    add a1, a1, t1
    sub a2, t0, t1
    call sb_bytes
    j 7f
6:
    ld t0, SB_LEN(s3)           # no rest: drop the last "/", or "." for the same directory
    beqz t0, 61f
    addi t0, t0, -1
    sd t0, SB_LEN(s3)
    j 7f
61:
    mv a0, s3
    li a1, '.'
    call sb_byte
7:
    mv a0, s3
    call sb_finish
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# void pkg_sort(void)
# Insertion sort of pkg_entries by name.
    .globl pkg_sort
pkg_sort:
    addi sp, sp, -64
    sd ra, 56(sp)
    sd s0, 48(sp)
    sd s1, 40(sp)
    sd s2, 32(sp)
    la t0, pkg_entries
    ld s0, 0(t0)
    ld s0, VEC_DATA(s0)
    la t0, pkg_entries
    ld t0, 0(t0)
    ld t0, VEC_N(t0)
    sd t0, 0(sp)
    li s1, 1
1:
    ld t0, 0(sp)
    bge s1, t0, 9f
    mv s2, s1
2:
    beqz s2, 3f
    slli t0, s2, 3
    add t0, s0, t0
    ld t1, -8(t0)
    ld t2, 0(t0)
    sd t1, 8(sp)
    sd t2, 16(sp)
    ld a0, PE_NAME(t1)
    ld a1, PE_NAME(t2)
    call str_cmp
    blez a0, 3f
    slli t0, s2, 3
    add t0, s0, t0
    ld t1, 8(sp)
    ld t2, 16(sp)
    sd t2, -8(t0)
    sd t1, 0(t0)
    addi s2, s2, -1
    j 2b
3:
    addi s1, s1, 1
    j 1b
9:
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    addi sp, sp, 64
    ret

# void pkg_write(void)
# Writes the manifest: the header comment, then one line per entry in name order.
    .globl pkg_write
pkg_write:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    sd s2, 0(sp)
    call pkg_sort
    call sb_new
    mv s0, a0
    la a1, s_header
    call sb_cstr
    li s1, 0
1:
    la t0, pkg_entries
    ld a0, 0(t0)
    ld t0, VEC_N(a0)
    bge s1, t0, 8f
    mv a1, s1
    call vec_get
    mv s2, a0
    mv a0, s0
    ld a1, PE_NAME(s2)
    call sb_str
    mv a0, s0
    li a1, ' '
    call sb_byte
    mv a0, s0
    ld a1, PE_RAW(s2)
    call sb_str
    mv a0, s0
    li a1, ' '
    call sb_byte
    addi a0, s2, PE_PIN
    call pkg_hash_text
    mv a1, a0
    mv a0, s0
    call sb_str
    mv a0, s0
    li a1, 10
    call sb_byte
    addi s1, s1, 1
    j 1b
8:
    la t0, pkg_dir
    ld a0, 0(t0)
    call sb_new
    mv s1, a0
    la t0, pkg_dir
    ld a1, 0(t0)
    call sb_str
    mv a0, s1
    la a1, s_manifest
    call sb_cstr
    mv a0, s1
    call sb_finish
    mv s1, a0
    mv a0, s0
    call sb_finish
    mv a1, a0
    mv a0, s1
    call write_file_safely
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    ld s2, 0(sp)
    addi sp, sp, 32
    ret

# void pkg_report(char *change, void *entry, void *old_hash_text)
# `added NAME DIR HASH`, `updated NAME: OLD -> NEW`, `unchanged NAME`, `removed NAME`; or JSON.
    .globl pkg_report
pkg_report:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    mv s1, a1
    mv s2, a2
    addi a0, s1, PE_PIN
    call pkg_hash_text
    sd a0, 0(sp)                # the hash now
    call sb_new
    mv s3, a0
    la t0, opt_json
    ld t0, 0(t0)
    beqz t0, 5f
    la a1, j_pkg
    call sb_cstr
    mv a0, s3
    ld a1, PE_NAME(s1)
    call json_str_into
    mv a0, s3
    la a1, j_change
    call sb_cstr
    mv a0, s3
    mv a1, s0
    call sb_cstr
    mv a0, s3
    la a1, j_dir
    call sb_cstr
    mv a0, s3
    ld a1, PE_RAW(s1)
    call json_str_into
    mv a0, s3
    la a1, j_hash
    call sb_cstr
    mv a0, s3
    ld a1, 0(sp)
    call sb_str
    beqz s2, 4f
    mv a0, s3
    la a1, j_old
    call sb_cstr
    mv a0, s3
    mv a1, s2
    call sb_str
4:
    mv a0, s3
    la a1, j_end
    call sb_cstr
    j 8f
5:
    mv a0, s0                   # text
    la a1, c_added
    call cstr_eq
    beqz a0, 6f
    mv a0, s3
    la a1, w_added
    call sb_cstr
    mv a0, s3
    ld a1, PE_NAME(s1)
    call sb_str
    mv a0, s3
    li a1, ' '
    call sb_byte
    mv a0, s3
    ld a1, PE_RAW(s1)
    call sb_str
    mv a0, s3
    li a1, ' '
    call sb_byte
    mv a0, s3
    ld a1, 0(sp)
    call sb_str
    j 7f
6:
    mv a0, s0
    la a1, c_updated
    call cstr_eq
    beqz a0, 61f
    mv a0, s3
    la a1, w_updated
    call sb_cstr
    mv a0, s3
    ld a1, PE_NAME(s1)
    call sb_str
    mv a0, s3
    la a1, w_colon_sp
    call sb_cstr
    mv a0, s3
    mv a1, s2
    call sb_str
    mv a0, s3
    la a1, w_arrow
    call sb_cstr
    mv a0, s3
    ld a1, 0(sp)
    call sb_str
    j 7f
61:
    mv a0, s0                   # unchanged NAME / removed NAME
    la a1, c_removed
    call cstr_eq
    la a1, w_unchanged
    beqz a0, 62f
    la a1, w_removed
62:
    mv a0, s3
    call sb_cstr
    mv a0, s3
    ld a1, PE_NAME(s1)
    call sb_str
7:
    mv a0, s3
    li a1, 10
    call sb_byte
8:
    mv t0, s3
    li a0, 1
    ld a1, SB_PTR(t0)
    ld a2, SB_LEN(t0)
    call out_bytes
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# void pkg_fail(char *code, void *message)
# A package error outside any source line (vo package): reported, exit 65.
    .globl pkg_fail
pkg_fail:
    addi sp, sp, -16
    sd ra, 8(sp)
    mv a3, a1
    mv a2, a0
    la t0, opt_json
    ld a0, 0(t0)
    la t0, report_fd
    ld a1, 0(t0)
    li a4, 0
    call toolchain_error
    li a0, EXIT_CHECK
    call vo_exit
    ld ra, 8(sp)
    addi sp, sp, 16
    ret

# int cmd_package(void)
# `vo package [--json] ENTRY NAME DIRECTORY` and `vo package --remove ENTRY NAME`.
    .globl cmd_package
cmd_package:
    addi sp, sp, -80
    sd ra, 72(sp)
    sd s0, 64(sp)
    sd s1, 56(sp)
    sd s2, 48(sp)
    sd s3, 40(sp)
    sd s4, 32(sp)
    la t0, pos_n                # ENTRY NAME [DIRECTORY]
    ld t0, 0(t0)
    li t1, 3
    la t2, opt_remove
    ld t2, 0(t2)
    sub t1, t1, t2
    beq t0, t1, 1f
    la a0, m_pkg_args
    call str_from_cstr
    call usage_error
1:
    la t0, pos_args
    ld s0, 0(t0)
    ld a0, 8(s0)                # NAME
    call cstr_len
    mv a1, a0
    ld a0, 8(s0)
    call pkg_name_ok
    bnez a0, 2f
    la a0, m_pkg_name
    ld a1, 8(s0)
    call usage_error_arg
2:
    ld a0, 8(s0)
    call str_from_cstr
    mv s1, a0                   # NAME (str)
    ld a0, 0(s0)                # ENTRY's directory: its path up to the last "/"
    call str_from_cstr
    mv s2, a0
    ld t0, STR_LEN(s2)
3:
    beqz t0, 31f
    add t1, s2, t0
    lbu t1, STR_DATA-1(t1)
    li t2, '/'
    beq t1, t2, 31f
    addi t0, t0, -1
    j 3b
31:
    addi a0, s2, STR_DATA
    mv a1, t0
    call str_new
    call rt_resolve
    bnez a0, 32f
    call rt_start_cwd
32:
    call pkg_load
    la t0, diag_errors          # a malformed manifest: report it, change nothing
    ld t0, 0(t0)
    beqz t0, 4f
    la t0, opt_json
    ld a0, 0(t0)
    la t0, report_fd
    ld a1, 0(t0)
    call diag_emit_all
    li a0, EXIT_CHECK
    call vo_exit
4:
    mv a0, s1
    call pkg_find
    mv s3, a0                   # the entry, or 0
    la t0, opt_remove
    ld t0, 0(t0)
    beqz t0, 6f
    bnez s3, 5f                 # --remove
    la a0, m_no_entry
    ld a1, 8(s0)
    call usage_error_arg
5:
    la t0, pkg_entries          # take it out (swap with the last; pkg_write sorts)
    ld t0, 0(t0)
    ld t1, VEC_N(t0)
    ld t2, VEC_DATA(t0)
    li t3, 0
51:
    slli t4, t3, 3
    add t4, t2, t4
    ld t5, 0(t4)
    beq t5, s3, 52f
    addi t3, t3, 1
    j 51b
52:
    addi t1, t1, -1
    slli t6, t1, 3
    add t6, t2, t6
    ld t6, 0(t6)
    sd t6, 0(t4)
    sd t1, VEC_N(t0)
    call pkg_write
    la a0, c_removed
    mv a1, s3
    li a2, 0
    call pkg_report
    j 90f
6:
    ld a0, 16(s0)               # DIRECTORY: resolved, hashed
    call str_from_cstr
    mv s4, a0
    call rt_resolve
    bnez a0, 61f
    la a0, m_dir_bad
    mv a1, s4
    li a2, 0
    li a3, 0
    call msg
    mv a1, a0
    la a0, e0702
    call pkg_fail
61:
    mv s4, a0                   # the absolute directory
    la a1, pkg_hash_buf
    call pkg_hash
    bgez a0, 62f
    la a0, m_missing
    mv a1, s1
    ld t0, 16(s0)
    sd t0, 0(sp)
    ld a0, 16(s0)
    call str_from_cstr
    mv a2, a0
    mv a1, s1
    la a0, m_missing
    li a3, 0
    call msg
    mv a1, a0
    la a0, e0702
    call pkg_fail
62:
    la t0, pkg_dir              # the directory as the manifest writes it
    ld a0, 0(t0)
    mv a1, s4
    call pkg_rel_dir
    sd a0, 0(sp)
    bnez s3, 7f
    li a0, PE_SIZE              # added
    call alloc
    mv s3, a0
    sd s1, PE_NAME(s3)
    sd s4, PE_DIR(s3)
    ld t0, 0(sp)
    sd t0, PE_RAW(s3)
    addi a0, s3, PE_PIN
    la a1, pkg_hash_buf
    li a2, 32
    call mem_copy
    la t0, pkg_entries
    ld a0, 0(t0)
    mv a1, s3
    call vec_push
    call pkg_write
    la a0, c_added
    mv a1, s3
    li a2, 0
    call pkg_report
    j 90f
7:
    addi a0, s3, PE_PIN         # unchanged: same hash and directory
    la a1, pkg_hash_buf
    li a2, 32
    call mem_eq
    beqz a0, 71f
    ld a0, PE_RAW(s3)
    ld a1, 0(sp)
    call str_eq
    beqz a0, 71f
    la a0, c_unchanged
    mv a1, s3
    li a2, 0
    call pkg_report
    j 90f
71:
    addi a0, s3, PE_PIN         # updated
    call pkg_hash_text
    sd a0, 8(sp)
    ld t0, 0(sp)
    sd t0, PE_RAW(s3)
    sd s4, PE_DIR(s3)
    addi a0, s3, PE_PIN
    la a1, pkg_hash_buf
    li a2, 32
    call mem_copy
    call pkg_write
    la a0, c_updated
    mv a1, s3
    ld a2, 8(sp)
    call pkg_report
90:
    li a0, 0
    ld ra, 72(sp)
    ld s0, 64(sp)
    ld s1, 56(sp)
    ld s2, 48(sp)
    ld s3, 40(sp)
    ld s4, 32(sp)
    addi sp, sp, 80
    ret

    .section .rodata
s_dot_vo_pkg: .string ".vo"
