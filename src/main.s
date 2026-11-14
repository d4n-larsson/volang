# CLI: argument parsing, path expansion, command dispatch (contracts/cli.md).
    .include "defs.inc"

    .equ CMD_CHECK, 1
    .equ CMD_RUN, 2
    .equ CMD_TEST, 3
    .equ CMD_FMT, 4
    .equ CMD_EXPLAIN, 5
    .equ CMD_REFERENCE, 6
    .equ CMD_VERSION, 7
    .equ CMD_EFFECTS, 8

    .section .data
    .align 3
    .globl opt_json
opt_json:       .dword 0
    .globl opt_timeout
opt_timeout:    .dword 10
    .globl opt_mem_stats
opt_mem_stats:  .dword 0
    .globl opt_filter
opt_filter:     .dword 0
    .globl opt_check
opt_check:      .dword 0
    .globl opt_update
opt_update:     .dword 0
    .globl prog_argc
prog_argc:      .dword 0
    .globl prog_argv
prog_argv:      .dword 0
    .globl opt_cmd
opt_cmd:        .dword 0
    .globl report_fd
report_fd:      .dword 1
pos_args:       .dword 0            # array of char* (positional arguments)
pos_n:          .dword 0
    .globl path_list
path_list:      .dword 0            # array of str (expanded .vo paths)
    .globl path_n
path_n:         .dword 0
path_cap:       .dword 0

    .section .rodata
cmd_names:
    .dword n_check, n_run, n_test, n_fmt, n_explain, n_reference, n_version, n_effects, 0
n_check:    .string "check"
n_run:      .string "run"
n_test:     .string "test"
n_fmt:      .string "fmt"
n_explain:  .string "explain"
n_reference: .string "reference"
n_version:  .string "version"
n_effects:  .string "effects"
f_update:   .string "--update"
m_effects_one: .string "`effects` takes exactly one file."
f_json:     .string "--json"
f_timeout:  .string "--timeout"
f_filter:   .string "--filter"
f_check:    .string "--check"
f_seed:     .string "--seed"
f_clock_ms: .string "--clock-ms"
f_mem_stats: .string "--mem-stats"
f_max_memory: .string "--max-memory"
m_max_memory_val: .string "`--max-memory` needs a whole number of MiB from 1 to 65536."
m_number_val: .string "`--seed` and `--clock-ms` need a whole number."
f_dashdash: .string "--"
m_unknown_cmd:  .string "Unknown command `$1`. Commands: check, run, test, fmt, effects, explain, reference, version."
m_missing_cmd:  .string "Missing command. Commands: check, run, test, fmt, effects, explain, reference, version."
m_unknown_opt:  .string "Unknown option `$1`. Options: --json, --timeout SEC, --filter TEXT, --check, --update, --seed N, --clock-ms N, --max-memory MiB, --mem-stats."
m_timeout_val:  .string "`--timeout` needs a whole number of seconds."
m_filter_val:   .string "`--filter` needs a text to match."
m_needs_path:   .string "`$1` needs at least one path."
m_cannot_read:  .string "Cannot read `$1`."
m_not_impl:     .string "`$1` is not implemented yet."
m_run_one:      .string "`run` takes exactly one file."
m_unknown_code: .string "Unknown error code `$1`. Codes look like E0301, W0201, R0002 or T0001."
m_explain_one:  .string "`explain` takes one error code, e.g. `vo explain E0301`."
j_explain:      .string "{\"v\":1,\"kind\":\"explain\",\"code\":\""
j_name:         .string "\",\"name\":\""
j_fixable:      .string "\",\"fixable\":"
j_true:         .string "true"
j_false:        .string "false"
j_text:         .string ",\"text\":"
j_end:          .string "}\n"
j_changed:      .string ",\"changed\":"
j_fmt_head:     .string "{\"v\":1,\"kind\":\"fmt\",\"file\":"
j_true_close:   .string ",\"changed\":true}\n"
j_false_close:  .string ",\"changed\":false}\n"
h_of:           .string " of "
h_changed:      .string " files changed\n"
h_need:         .string " files need formatting\n"
h_formatted:    .string "formatted "
h_unformatted:  .string "not formatted: "
tmp_suffix:     .string ".vo-fmt.tmp"
j_reference:    .string "{\"v\":1,\"kind\":\"reference\",\"text\":"
code_t1:    .string "T0001"
code_t2:    .string "T0002"
code_t3:    .string "T0003"
version_h:  .string "vo 0.1.0 schema 1\n"
version_j:  .string "{\"v\":1,\"kind\":\"version\",\"version\":\"0.1.0\",\"schema\":1}\n"
dot_vo:     .string ".vo"

    .text
# int cstr_eq(char *a, char *b)
    .globl cstr_eq
cstr_eq:
1:
    lbu t0, 0(a0)
    lbu t1, 0(a1)
    bne t0, t1, 2f
    beqz t0, 3f
    addi a0, a0, 1
    addi a1, a1, 1
    j 1b
2:
    li a0, 0
    ret
3:
    li a0, 1
    ret

# void usage_error(void *message)
# Emits T0001 and exits 64.
    .globl usage_error
usage_error:
    addi sp, sp, -16
    sd ra, 8(sp)
    mv a3, a0
    la t0, opt_json
    ld a0, 0(t0)
    la t0, report_fd
    ld a1, 0(t0)
    la a2, code_t1
    li a4, 0
    call toolchain_error
    li a0, EXIT_USAGE
    call vo_exit
    ld ra, 8(sp)
    addi sp, sp, 16
    ret

# void usage_error_arg(char *tmpl, char *arg)
    .globl usage_error_arg
usage_error_arg:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    mv s0, a0
    mv a0, a1
    call str_from_cstr
    mv a1, a0
    mv a0, s0
    li a2, 0
    li a3, 0
    call msg
    call usage_error
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret

# void path_push(void *str)
    .globl path_push
path_push:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    la t0, path_n
    ld t1, 0(t0)
    la t2, path_cap
    ld t3, 0(t2)
    blt t1, t3, 1f
    slli s1, t3, 1
    bnez s1, 2f
    li s1, 64
2:
    slli a0, s1, 3
    call alloc
    la t0, path_list
    ld a1, 0(t0)
    sd a0, 0(t0)
    la t0, path_n
    ld a2, 0(t0)
    slli a2, a2, 3
    call mem_copy
    la t2, path_cap
    sd s1, 0(t2)
1:
    la t0, path_n
    ld t1, 0(t0)
    la t2, path_list
    ld t2, 0(t2)
    slli t3, t1, 3
    add t2, t2, t3
    sd s0, 0(t2)
    addi t1, t1, 1
    sd t1, 0(t0)
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void sort_paths(long from)
# Insertion sort (byte-wise) of path_list[from..path_n).
    .globl sort_paths
sort_paths:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    addi s1, a0, 1
1:
    la t0, path_n
    ld t0, 0(t0)
    bge s1, t0, 9f
    la t0, path_list
    ld t0, 0(t0)
    slli t1, s1, 3
    add t1, t0, t1
    ld s3, 0(t1)
    addi s2, s1, -1
2:
    blt s2, s0, 3f
    la t0, path_list
    ld t0, 0(t0)
    slli t1, s2, 3
    add t1, t0, t1
    ld a0, 0(t1)
    mv a1, s3
    call str_cmp
    blez a0, 3f
    la t0, path_list
    ld t0, 0(t0)
    slli t1, s2, 3
    add t1, t0, t1
    ld t2, 0(t1)
    sd t2, 8(t1)
    addi s2, s2, -1
    j 2b
3:
    la t0, path_list
    ld t0, 0(t0)
    addi t1, s2, 1
    slli t1, t1, 3
    add t1, t0, t1
    sd s3, 0(t1)
    addi s1, s1, 1
    j 1b
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# int ends_with_vo(void *str)
    .globl ends_with_vo
ends_with_vo:
    ld t0, STR_LEN(a0)
    li t1, 3
    blt t0, t1, 1f
    add t2, a0, t0
    lbu t3, STR_DATA-3(t2)
    li t4, '.'
    bne t3, t4, 1f
    lbu t3, STR_DATA-2(t2)
    li t4, 'v'
    bne t3, t4, 1f
    lbu t3, STR_DATA-1(t2)
    li t4, 'o'
    bne t3, t4, 1f
    li a0, 1
    ret
1:
    li a0, 0
    ret

# void walk_dir(void *dirpath, int fd)
# Adds every *.vo file below the open directory fd (recursively) to path_list.
    .globl walk_dir
walk_dir:
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
    li a0, 32768
    call alloc
    mv s2, a0                   # dirent buffer
1:
    mv a0, s1
    mv a1, s2
    li a2, 32768
    call sys_getdents64
    blez a0, 8f
    mv s3, a0                   # bytes in buffer
    li s4, 0                    # offset
2:
    bge s4, s3, 1b
    add s5, s2, s4              # struct linux_dirent64: ino 8, off 8, reclen 2, type 1, name
    lhu t0, 16(s5)
    add s4, s4, t0
    addi a0, s5, 19             # name
    lbu t0, 0(a0)
    li t1, '.'
    beq t0, t1, 2b              # skip ".", ".." and hidden entries
    call sb_new
    sd a0, 0(sp)
    mv a1, s0
    call sb_str
    ld a0, 0(sp)
    li a1, '/'
    call sb_byte
    ld a0, 0(sp)
    addi a1, s5, 19
    call sb_cstr
    ld a0, 0(sp)
    call sb_finish
    sd a0, 0(sp)                # full path (str)
    lbu t0, 18(s5)
    li t1, 4                    # DT_DIR
    beq t0, t1, 3f
    ld a0, 0(sp)
    call ends_with_vo
    beqz a0, 2b
    ld a0, 0(sp)
    call path_push
    j 2b
3:
    ld a0, 0(sp)
    addi a0, a0, STR_DATA
    li a1, O_DIRECTORY
    call sys_open
    bltz a0, 2b
    mv a1, a0
    ld a0, 0(sp)
    sd a1, 0(sp)
    call walk_dir
    ld a0, 0(sp)
    call sys_close
    j 2b
8:
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    ld s5, 8(sp)
    addi sp, sp, 64
    ret

# void expand_paths(void)
# Turns the positional arguments into path_list: directories become their sorted *.vo files.
    .globl expand_paths
expand_paths:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    li s0, 0
1:
    la t0, pos_n
    ld t0, 0(t0)
    bge s0, t0, 9f
    la t0, pos_args
    ld t0, 0(t0)
    slli t1, s0, 3
    add t0, t0, t1
    ld s1, 0(t0)                # char *
    addi s0, s0, 1
    mv a0, s1
    li a1, O_DIRECTORY
    call sys_open
    bltz a0, 2f
    mv s2, a0
    mv a0, s1                   # strip trailing '/' for clean joins
    call str_from_cstr
    mv s3, a0
3:
    ld t0, STR_LEN(s3)
    li t1, 1
    ble t0, t1, 4f
    add t2, s3, t0
    lbu t3, STR_DATA-1(t2)
    li t4, '/'
    bne t3, t4, 4f
    addi t0, t0, -1
    sd t0, STR_LEN(s3)
    j 3b
4:
    la t0, path_n
    ld t0, 0(t0)
    sd t0, 0(sp)
    mv a0, s3
    mv a1, s2
    call walk_dir
    mv a0, s2
    call sys_close
    ld a0, 0(sp)
    call sort_paths
    j 1b
2:
    mv a0, s1
    call str_from_cstr
    call path_push
    j 1b
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# void *load_source(void *path)
# Reads a source file and registers it; returns the file id, or exits 66 with T0002.
    .globl load_source
load_source:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    mv s0, a0
    call file_find              # already loaded (e.g. as another file's import)
    bgez a0, 3f
    addi a0, s0, STR_DATA
    addi a1, sp, 0
    call read_whole_file
    bltz a0, 1f
    mv a2, a0
    ld a1, 0(sp)
    mv a0, s0
    call file_add
3:
    ld ra, 24(sp)
    ld s0, 16(sp)
    addi sp, sp, 32
    ret
1:
    la a0, m_cannot_read
    mv a1, s0
    li a2, 0
    li a3, 0
    call msg
    mv a3, a0
    la t0, opt_json
    ld a0, 0(t0)
    la t0, report_fd
    ld a1, 0(t0)
    la a2, code_t2
    mv a4, s0
    call toolchain_error
    la t0, diag_errors
    ld t1, 0(t0)
    addi t1, t1, 1
    sd t1, 0(t0)
    la t0, opt_cmd
    ld t0, 0(t0)
    li t1, CMD_RUN
    beq t0, t1, 2f              # run prints no summary for an unreadable input
    la t0, opt_json
    ld a0, 0(t0)
    la t0, report_fd
    ld a1, 0(t0)
    call cmd_name
    mv a2, a0
    la t0, file_count
    ld a3, 0(t0)
    li a4, 0
    call summary_emit
2:
    li a0, EXIT_NOINPUT
    call vo_exit
    li a0, 0
    ld ra, 24(sp)
    ld s0, 16(sp)
    addi sp, sp, 32
    ret

# char *cmd_name(void)
    .globl cmd_name
cmd_name:
    la t0, opt_cmd
    ld t0, 0(t0)
    addi t0, t0, -1
    slli t0, t0, 3
    la t1, cmd_names
    add t1, t1, t0
    ld a0, 0(t1)
    ret

# void need_paths(void)
    .globl need_paths
need_paths:
    la t0, pos_n
    ld t0, 0(t0)
    bnez t0, 1f
    addi sp, sp, -16
    sd ra, 8(sp)
    call cmd_name
    mv a1, a0
    la a0, m_needs_path
    call usage_error_arg
    ld ra, 8(sp)
    addi sp, sp, 16
1:
    ret

# int vo_main(int argc, char **argv)
    .globl vo_main
vo_main:
    addi sp, sp, -64
    sd ra, 56(sp)
    sd s0, 48(sp)
    sd s1, 40(sp)
    sd s2, 32(sp)
    sd s3, 24(sp)
    mv s0, a0
    mv s1, a1
    li s2, 1                    # pre-scan for --json (before any "--")
1:
    bge s2, s0, 2f
    slli t0, s2, 3
    add t0, s1, t0
    ld a0, 0(t0)
    la a1, f_dashdash
    call cstr_eq
    bnez a0, 2f
    slli t0, s2, 3
    add t0, s1, t0
    ld a0, 0(t0)
    la a1, f_json
    call cstr_eq
    addi s2, s2, 1
    beqz a0, 1b
    la t0, opt_json
    li t1, 1
    sd t1, 0(t0)
    j 1b
2:
    slli a0, s0, 3
    call alloc
    la t0, pos_args
    sd a0, 0(t0)
    li t0, 2
    bge s0, t0, 3f
    la a0, m_missing_cmd
    call str_from_cstr
    call usage_error
3:
    ld s3, 8(s1)                # command name
    li s2, 0
4:
    la t0, cmd_names
    slli t1, s2, 3
    add t0, t0, t1
    ld a1, 0(t0)
    beqz a1, 5f
    mv a0, s3
    call cstr_eq
    addi s2, s2, 1
    beqz a0, 4b
    la t0, opt_cmd
    sd s2, 0(t0)
    li t1, CMD_RUN
    bne s2, t1, 6f
    la t0, report_fd
    li t1, 2
    sd t1, 0(t0)
    j 6f
5:
    la a0, m_unknown_cmd
    mv a1, s3
    call usage_error_arg
6:
    li s2, 2                    # options and positional arguments
7:
    bge s2, s0, 20f
    slli t0, s2, 3
    add t0, s1, t0
    ld s3, 0(t0)
    addi s2, s2, 1
    mv a0, s3
    la a1, f_dashdash
    call cstr_eq
    beqz a0, 8f
    sub t0, s0, s2              # the program's arguments
    la t1, prog_argc
    sd t0, 0(t1)
    slli t0, s2, 3
    add t0, s1, t0
    la t1, prog_argv
    sd t0, 0(t1)
    j 20f
8:
    lbu t0, 0(s3)
    li t1, '-'
    bne t0, t1, 15f
    lbu t0, 1(s3)
    bne t0, t1, 15f
    mv a0, s3
    la a1, f_json
    call cstr_eq
    bnez a0, 7b
    mv a0, s3
    la a1, f_check
    call cstr_eq
    beqz a0, 81f
    la t0, opt_check
    li t1, 1
    sd t1, 0(t0)
    j 7b
81:
    mv a0, s3
    la a1, f_update
    call cstr_eq
    beqz a0, 9f
    la t0, opt_update
    li t1, 1
    sd t1, 0(t0)
    j 7b
9:
    mv a0, s3
    la a1, f_timeout
    call cstr_eq
    beqz a0, 11f
    bge s2, s0, 10f
    slli t0, s2, 3
    add t0, s1, t0
    ld s3, 0(t0)
    addi s2, s2, 1
    mv a0, s3
    call cstr_len
    mv a1, a0
    mv a0, s3
    addi a2, sp, 0
    call dec_to_int
    bnez a0, 10f
    ld t0, 0(sp)
    bltz t0, 10f
    la t1, opt_timeout
    sd t0, 0(t1)
    j 7b
10:
    la a0, m_timeout_val
    call str_from_cstr
    call usage_error
11:
    mv a0, s3
    la a1, f_filter
    call cstr_eq
    beqz a0, 13f
    bge s2, s0, 12f
    slli t0, s2, 3
    add t0, s1, t0
    ld a0, 0(t0)
    addi s2, s2, 1
    call str_from_cstr
    la t1, opt_filter
    sd a0, 0(t1)
    j 7b
12:
    la a0, m_filter_val
    call str_from_cstr
    call usage_error
13:
    mv a0, s3
    la a1, f_seed
    call cstr_eq
    li t2, 1
    bnez a0, 131f
    mv a0, s3
    la a1, f_clock_ms
    call cstr_eq
    li t2, 2
    beqz a0, 139f
131:
    sd t2, 8(sp)
    bge s2, s0, 138f
    slli t0, s2, 3
    add t0, s1, t0
    ld s3, 0(t0)
    addi s2, s2, 1
    mv a0, s3
    call cstr_len
    mv a1, a0
    mv a0, s3
    addi a2, sp, 0
    call dec_to_int
    bnez a0, 138f
    ld a1, 0(sp)
    bltz a1, 138f
    ld a0, 8(sp)
    call rt_set_option
    j 7b
138:
    la a0, m_number_val
    call str_from_cstr
    call usage_error
139:
    mv a0, s3
    la a1, f_mem_stats
    call cstr_eq
    beqz a0, 140f
    la t0, opt_mem_stats
    li t1, 1
    sd t1, 0(t0)
    j 7b
140:
    mv a0, s3
    la a1, f_max_memory
    call cstr_eq
    beqz a0, 142f
    bge s2, s0, 141f
    slli t0, s2, 3
    add t0, s1, t0
    ld s3, 0(t0)
    addi s2, s2, 1
    mv a0, s3
    call cstr_len
    mv a1, a0
    mv a0, s3
    addi a2, sp, 0
    call dec_to_int
    bnez a0, 141f
    ld t0, 0(sp)
    li t1, 1
    blt t0, t1, 141f
    li t1, 65536
    bgt t0, t1, 141f
    slli t0, t0, 20             # MiB -> bytes
    la t1, gc_limit
    sd t0, 0(t1)
    j 7b
141:
    la a0, m_max_memory_val
    call str_from_cstr
    call usage_error
142:
    la a0, m_unknown_opt
    mv a1, s3
    call usage_error_arg
15:
    la t0, pos_args             # positional
    ld t0, 0(t0)
    la t1, pos_n
    ld t2, 0(t1)
    slli t3, t2, 3
    add t0, t0, t3
    sd s3, 0(t0)
    addi t2, t2, 1
    sd t2, 0(t1)
    j 7b
20:
    la t0, opt_cmd
    ld t0, 0(t0)
    li t1, CMD_VERSION
    beq t0, t1, 30f
    li t1, CMD_CHECK
    beq t0, t1, 31f
    li t1, CMD_EXPLAIN
    beq t0, t1, 32f
    li t1, CMD_REFERENCE
    beq t0, t1, 33f
    li t1, CMD_RUN
    beq t0, t1, 34f
    li t1, CMD_TEST
    beq t0, t1, 35f
    li t1, CMD_EFFECTS
    beq t0, t1, 37f
    j 36f                       # fmt
30:
    la a1, version_h
    la t0, opt_json
    ld t0, 0(t0)
    beqz t0, 301f
    la a1, version_j
301:
    li a0, 1
    call out_cstr
    li a0, 0
    j 90f
31:
    call cmd_check
    j 90f
32:
    call cmd_explain
    j 90f
33:
    call cmd_reference
    j 90f
34:
    call cmd_run
    j 90f
35:
    call cmd_test
    j 90f
36:
    call cmd_fmt
    j 90f
37:
    call cmd_effects
90:
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    addi sp, sp, 64
    ret

# int not_implemented(void)
    .globl not_implemented
not_implemented:
    addi sp, sp, -16
    sd ra, 8(sp)
    call cmd_name
    call str_from_cstr
    mv a1, a0
    la a0, m_not_impl
    li a2, 0
    li a3, 0
    call msg
    mv a3, a0
    la t0, opt_json
    ld a0, 0(t0)
    la t0, report_fd
    ld a1, 0(t0)
    la a2, code_t3
    li a4, 0
    call toolchain_error
    li a0, EXIT_INTERNAL
    ld ra, 8(sp)
    addi sp, sp, 16
    ret

# int finish_check(long files)
# Emits diagnostics and the summary; returns 65 when there were errors.
    .globl finish_check
finish_check:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    mv s0, a0
    la t0, opt_json
    ld a0, 0(t0)
    la t0, report_fd
    ld a1, 0(t0)
    call diag_emit_all
    la t0, opt_json
    ld a0, 0(t0)
    la t0, report_fd
    ld a1, 0(t0)
    call cmd_name
    mv a2, a0
    mv a3, s0
    li a4, 0
    la t0, opt_json
    ld a0, 0(t0)
    la t0, report_fd
    ld a1, 0(t0)
    call summary_emit
    li a0, 0
    la t0, diag_errors
    ld t0, 0(t0)
    beqz t0, 1f
    li a0, EXIT_CHECK
1:
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret

# int cmd_check(void)
    .globl cmd_check
cmd_check:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    call need_paths
    call expand_paths
    li s0, 0
1:
    la t0, path_n
    ld t0, 0(t0)
    bge s0, t0, 2f
    la t0, path_list
    ld t0, 0(t0)
    slli t1, s0, 3
    add t0, t0, t1
    ld a0, 0(t0)
    call load_source
    mv s1, a0
    la t0, diag_errors
    ld t0, 0(t0)
    sd t0, 0(sp)
    mv a0, s1
    li a1, 0                    # mode: check
    call check_program
    la t0, diag_errors          # a clean program is compared with its effects record
    ld t0, 0(t0)
    ld t1, 0(sp)
    bne t0, t1, 3f
    call ld_files
    mv a1, a0
    mv a0, s1
    call rec_review
3:
    addi s0, s0, 1
    j 1b
2:
    mv a0, s0
    call finish_check
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# int cmd_effects(void)
# `vo effects [--json] [--update] FILE` (contracts/cli-v2.md).
    .globl cmd_effects
cmd_effects:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    call need_paths
    la t0, pos_n
    ld t0, 0(t0)
    li t1, 1
    beq t0, t1, 1f
    la a0, m_effects_one
    call str_from_cstr
    call usage_error
1:
    la t0, pos_args
    ld t0, 0(t0)
    ld a0, 0(t0)
    call str_from_cstr
    call load_source
    mv s0, a0
    li a1, 0
    call check_program
    la t0, diag_errors
    ld t0, 0(t0)
    bnez t0, 8f
    call ld_files
    mv a1, a0
    mv a0, s0
    la t0, opt_update
    ld a2, 0(t0)
    la t0, opt_json
    ld a3, 0(t0)
    call rec_command
8:
    li a0, 1
    call finish_check
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# int cmd_explain(void)
# Prints the catalog section of one code (or an `explain` record under --json).
    .globl cmd_explain
cmd_explain:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    la t0, pos_n
    ld t0, 0(t0)
    li t1, 1
    bne t0, t1, 8f
    la t0, pos_args
    ld t0, 0(t0)
    ld s0, 0(t0)                # code (char *)
    mv a0, s0
    call cstr_len
    li t0, 5
    bne a0, t0, 7f
    lbu t0, 0(s0)               # [EWRT][0-9]{4}
    li t1, 'E'
    beq t0, t1, 1f
    li t1, 'W'
    beq t0, t1, 1f
    li t1, 'R'
    beq t0, t1, 1f
    li t1, 'T'
    bne t0, t1, 7f
1:
    li t2, 1
2:
    li t3, 5
    bge t2, t3, 3f
    add t4, s0, t2
    lbu t4, 0(t4)
    addi t4, t4, -'0'
    bltz t4, 7f
    li t5, 9
    bgt t4, t5, 7f
    addi t2, t2, 1
    j 2b
3:
    mv a0, s0
    call catalog_section
    beqz a0, 7f
    mv s1, a0
    la t0, opt_json
    ld t0, 0(t0)
    bnez t0, 4f
    li a0, 1
    mv a1, s1
    call out_str
    li a0, 0
    j 9f
4:
    call sb_new                 # {"v":1,"kind":"explain","code":..,"name":..,"fixable":..,"text":..}
    mv s2, a0
    la a1, j_explain
    call sb_cstr
    mv a0, s2
    mv a1, s0
    call sb_cstr
    mv a0, s2
    la a1, j_name
    call sb_cstr
    mv a0, s0
    call catalog_name
    mv a1, a0
    mv a0, s2
    call sb_str
    mv a0, s2
    la a1, j_fixable
    call sb_cstr
    mv a0, s0
    call catalog_fixable
    la a1, j_false
    beqz a0, 5f
    la a1, j_true
5:
    mv a0, s2
    call sb_cstr
    mv a0, s2
    la a1, j_text
    call sb_cstr
    mv a0, s2
    mv a1, s1
    call json_str_into
    mv a0, s2
    la a1, j_end
    call sb_cstr
    li a0, 1
    ld a1, SB_PTR(s2)
    ld a2, SB_LEN(s2)
    call out_bytes
    li a0, 0
    j 9f
7:
    la a0, m_unknown_code
    mv a1, s0
    call usage_error_arg
8:
    la a0, m_explain_one
    call str_from_cstr
    call usage_error
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    addi sp, sp, 48
    ret

# int cmd_reference(void)
# Prints the embedded language reference unchanged (or a `reference` record under --json).
    .globl cmd_reference
cmd_reference:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    la t0, opt_json
    ld t0, 0(t0)
    bnez t0, 1f
    li a0, 1
    la a1, ref_md
    la t0, ref_md_len
    ld a2, 0(t0)
    call out_bytes
    li a0, 0
    j 9f
1:
    call sb_new
    mv s0, a0
    la a1, j_reference
    call sb_cstr
    la a0, ref_md
    la t0, ref_md_len
    ld a1, 0(t0)
    call str_new
    mv a1, a0
    mv a0, s0
    call json_str_into
    mv a0, s0
    la a1, j_end
    call sb_cstr
    li a0, 1
    ld a1, SB_PTR(s0)
    ld a2, SB_LEN(s0)
    call out_bytes
    li a0, 0
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    addi sp, sp, 32
    ret

# int cmd_run(void)
# Checks one file (main required), then runs it.
    .globl cmd_run
cmd_run:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    call need_paths
    la t0, pos_n
    ld t0, 0(t0)
    li t1, 1
    beq t0, t1, 1f
    la a0, m_run_one
    call str_from_cstr
    call usage_error
1:
    la t0, pos_args
    ld t0, 0(t0)
    ld a0, 0(t0)
    call str_from_cstr
    call load_source
    mv s0, a0
    li a1, 1
    call check_program
    la t0, diag_errors
    ld t0, 0(t0)
    beqz t0, 2f
    li a0, 1
    call finish_check
    j 9f
2:
    la t0, diag_warnings        # warnings do not stop the run (not shown either)
    mv a0, s0
    call file_get
    ld t0, FILE_AST(a0)
    ld a0, N_A(t0)
    call find_main
    sd a0, 0(sp)
    call compile_program        # bytecode for every function (spec 004)
    li a0, 0
    call rt_effects_start
    ld a0, 0(sp)
    call run_main
    sd a0, 0(sp)
    li a0, 2
    la a1, n_run
    li a2, 0
    call gc_report
    ld a0, 0(sp)
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# int cmd_test(void)
# Checks every file (test mode), then runs their tests; 1 when any test did not pass.
    .globl cmd_test
cmd_test:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    call need_paths
    call expand_paths
    la t0, path_n
    ld s1, 0(t0)
    slli a0, s1, 3
    call alloc
    mv s2, a0                   # entry file ids
    li s0, 0
1:
    bge s0, s1, 2f
    la t0, path_list
    ld t0, 0(t0)
    slli t1, s0, 3
    add t0, t0, t1
    ld a0, 0(t0)
    call load_source
    slli t1, s0, 3
    add t1, s2, t1
    sd a0, 0(t1)
    li a1, 2                    # mode: test
    call check_program
    addi s0, s0, 1
    j 1b
2:
    la t0, diag_errors
    ld t0, 0(t0)
    beqz t0, 3f
    mv a0, s1
    call finish_check
    j 9f
3:
    la t0, opt_json             # warnings first, then the tests
    ld a0, 0(t0)
    li a1, 1
    call diag_emit_all
    call compile_program        # bytecode for every function and test (spec 004)
    call interp_init
    call gc_start
    li s0, 0
4:
    bge s0, s1, 5f
    slli t1, s0, 3
    add t1, s2, t1
    ld a0, 0(t1)
    call run_file_tests
    addi s0, s0, 1
    j 4b
5:
    mv a0, s1
    call test_summary
    li a0, 0
    la t0, t_failed
    ld t1, 0(t0)
    la t0, t_errored
    ld t2, 0(t0)
    add t1, t1, t2
    la t0, t_timed_out
    ld t2, 0(t0)
    add t1, t1, t2
    beqz t1, 9f
    li a0, EXIT_FAIL
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    addi sp, sp, 48
    ret

# int cmd_fmt(void)
# Formats files in place (or lists them with --check). Any syntax error: nothing is written, 65.
    .globl cmd_fmt
cmd_fmt:
    addi sp, sp, -80            # 0..23: scratch, 32..79: saved registers
    sd ra, 72(sp)
    sd s0, 64(sp)
    sd s1, 56(sp)
    sd s2, 48(sp)
    sd s3, 40(sp)
    sd s4, 32(sp)
    call need_paths
    call expand_paths
    la t0, path_n
    ld s1, 0(t0)
    slli a0, s1, 3
    call alloc
    mv s2, a0                   # file ids
    li s0, 0
1:
    bge s0, s1, 2f
    la t0, path_list
    ld t0, 0(t0)
    slli t1, s0, 3
    add t0, t0, t1
    ld a0, 0(t0)
    call load_source
    slli t1, s0, 3
    add t1, s2, t1
    sd a0, 0(t1)
    mv s3, a0
    call lex
    mv a0, s3
    call parse_file
    addi s0, s0, 1
    j 1b
2:
    la t0, diag_errors
    ld t0, 0(t0)
    beqz t0, 3f
    mv a0, s1                   # refuse: report the syntax errors, change nothing
    call finish_check
    j 99f
3:
    li s0, 0
    li s4, 0                    # files changed
4:
    bge s0, s1, 8f
    slli t1, s0, 3
    add t1, s2, t1
    ld s3, 0(t1)                # file id
    addi s0, s0, 1
    mv a0, s3
    call fmt_file
    sd a0, 0(sp)                # formatted text
    mv a0, s3
    call file_get
    sd a0, 8(sp)
    ld a1, FILE_TEXT(a0)
    ld a2, FILE_LEN(a0)
    ld a0, 0(sp)
    call str_eq_bytes
    seqz t0, a0                 # changed?
    add s4, s4, t0
    sd t0, 16(sp)
    beqz t0, 5f
    la t0, opt_check
    ld t0, 0(t0)
    bnez t0, 5f
    ld t0, 8(sp)
    ld a0, FILE_PATH(t0)
    ld a1, 0(sp)
    call write_file_safely
5:
    ld t0, 8(sp)
    ld a0, FILE_PATH(t0)
    ld a1, 16(sp)
    call fmt_report
    j 4b
8:
    sd s4, 0(sp)
    call sb_new                 # summary
    mv s3, a0
    la t0, opt_json
    ld t0, 0(t0)
    beqz t0, 81f
    la a1, j_changed
    call sb_cstr
    mv a0, s3
    ld a1, 0(sp)
    call int_to_dec
    mv a0, s3
    call sb_finish
    mv a4, a0
    li a0, 1
    li a1, 1
    la a2, n_fmt
    mv a3, s1
    call summary_emit
    j 82f
81:
    ld a1, 0(sp)
    call int_to_dec
    mv a0, s3
    la a1, h_of
    call sb_cstr
    mv a0, s3
    mv a1, s1
    call int_to_dec
    la a1, h_changed
    la t0, opt_check
    ld t0, 0(t0)
    beqz t0, 811f
    la a1, h_need
811:
    mv a0, s3
    call sb_cstr
    li a0, 1
    ld a1, SB_PTR(s3)
    ld a2, SB_LEN(s3)
    call out_bytes
82:
    li a0, 0
    la t0, opt_check
    ld t0, 0(t0)
    beqz t0, 99f
    ld t0, 0(sp)
    beqz t0, 99f
    li a0, EXIT_FAIL
99:
    ld ra, 72(sp)
    ld s0, 64(sp)
    ld s1, 56(sp)
    ld s2, 48(sp)
    ld s3, 40(sp)
    ld s4, 32(sp)
    addi sp, sp, 80
    ret

# void fmt_report(void *path, int changed)
# JSON: one fmt record per file. Human: a line per changed file.
    .globl fmt_report
fmt_report:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    sd s2, 0(sp)
    mv s0, a0
    mv s1, a1
    call sb_new
    mv s2, a0
    la t0, opt_json
    ld t0, 0(t0)
    beqz t0, 1f
    la a1, j_fmt_head
    call sb_cstr
    mv a0, s2
    mv a1, s0
    call json_str_into
    la a1, j_false_close
    beqz s1, 2f
    la a1, j_true_close
2:
    mv a0, s2
    call sb_cstr
    j 8f
1:
    beqz s1, 9f
    la a1, h_formatted
    la t0, opt_check
    ld t0, 0(t0)
    beqz t0, 3f
    la a1, h_unformatted
3:
    mv a0, s2
    call sb_cstr
    mv a0, s2
    mv a1, s0
    call sb_str
    mv a0, s2
    li a1, 10
    call sb_byte
8:
    li a0, 1
    ld a1, SB_PTR(s2)
    ld a2, SB_LEN(s2)
    call out_bytes
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    ld s2, 0(sp)
    addi sp, sp, 32
    ret

# void write_file_safely(void *path, void *text)
# Writes <path>.vo-fmt.tmp, then renames it over path, so a crash never truncates the source.
    .globl write_file_safely
write_file_safely:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    mv s1, a1
    call sb_new
    mv s2, a0
    mv a1, s0
    call sb_str
    mv a0, s2
    la a1, tmp_suffix
    call sb_cstr
    mv a0, s2
    call sb_finish
    mv s2, a0                   # temp path (NUL-terminated str)
    addi a0, s2, STR_DATA
    li a1, O_WRCREAT
    call sys_open
    bltz a0, 9f
    mv s3, a0
    addi a1, s1, STR_DATA
    ld a2, STR_LEN(s1)
    call write_all
    mv a0, s3
    call sys_close
    addi a0, s2, STR_DATA
    addi a1, s0, STR_DATA
    call sys_rename
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret
