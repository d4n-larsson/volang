# Recordings and replay (spec 006 US5): what a run receives from outside, written with
# `vo run --record`, fed back by `vo replay` (research R15-R17, contracts/recording.md).
#
# Every outside input reaches a program through the builtin stubs of rt_call.s, which call the
# io_* wrappers below for read_line, read_all, read_file, write_file, env and now_ms. With
# rcd_mode 0 they go straight to the runtime; 1 records each result after it; 2 takes the next
# recorded event instead of touching the outside. Arguments and the seed go in the header.
# Writing the recording allocates nothing (the end is written from vo_exit, on every way out).
    .include "defs.inc"

    .section .data
    .align 3
    .globl rcd_mode
rcd_mode:   .dword 0            # 0 off, 1 recording, 2 replaying
rcd_fb:     .dword 0            # the recording (FB_*), or 0
rcd_failed: .dword 0            # it could not be created (T0009 at exit)
rcd_pending_kind: .dword 0      # a read/write being checked: written as `denied` if R0010 ends the run
rcd_pending_path: .dword 0
rp_events:  .dword 0            # replay: vec of EV_* records
rp_next:    .dword 0            # replay: index of the next event
rp_program: .dword 0            # replay: header fields
rp_fprint:  .dword 0
rp_seed:    .dword 0
rp_clock:   .dword -1
rp_args:    .dword 0            # vec of strs
rp_saw_end: .dword 0

    .section .rodata
    .align 3
ev_names:   .dword 0, k_read_line, k_read_all, k_read_file, k_write_file, k_env, k_now_ms, k_end
k_read_line: .string "read_line"
k_read_all: .string "read_all"
k_read_file: .string "read_file"
k_write_file: .string "write_file"
k_env:      .string "env"
k_now_ms:   .string "now_ms"
k_end:      .string "end"
k_recording: .string "recording"
r_ok:       .string "ok"
r_err:      .string "err"
r_denied:   .string "denied"
j_head:     .string "{\"v\":1,\"kind\":\""
j_rec_program: .string "{\"v\":1,\"kind\":\"recording\",\"program\":"
j_fprint:   .string ",\"fingerprint\":\""
j_seed:     .string "\",\"seed\":"
j_clock:    .string ",\"clock_ms\":"
j_null:     .string "null"
j_args:     .string ",\"args\":["
j_path:     .string "\",\"path\":"
j_name:     .string "\",\"name\":"
j_result:   .string ",\"result\":\""
j_value:    .string ",\"value\":"
j_bytes:    .string ",\"bytes\":true"
j_status:   .string "\",\"status\":"
j_close:    .string "}\n"
j_arr_close: .string "]}\n"
j_quote:    .string "\""
j_replay:   .string "{\"v\":1,\"kind\":\"replay\",\"program\":"
j_changed:  .string ",\"changed\":true}\n"
hexd:       .string "0123456789abcdef"
code_r14:   .string "R0014"
code_t8:    .string "T0008"
code_t2r:   .string "T0002"
m_bad_rec:  .string "`$1` is not a complete recording: record the run again with `vo run --record FILE`."
m_cannot_read_rec: .string "Cannot read the recording `$1`."
m_replay_args: .string "`replay` takes one recording: `vo replay r.jsonl`."
m_changed:  .string "note: the program differs from the recorded one (recorded $1)\n"
m_mismatch: .string "The program $1, but the recording has $2 next: replay the program that was recorded, or record this one."
m_mismatch_end: .string "The program $1, but the recording has no more inputs: replay the program that was recorded, or record this one."
d_read_line: .string "reads a line of stdin"
d_read_all: .string "reads the rest of stdin"
d_read_file: .string "reads the file `$1`"
d_write_file: .string "writes the file `$1`"
d_env:      .string "reads the variable `$1`"
d_now_ms:   .string "reads the clock"
n_read_line: .string "a line of stdin"
n_read_all: .string "the rest of stdin"
n_read_file: .string "a read of `$1`"
n_write_file: .string "a write of `$1`"
n_env:      .string "the variable `$1`"
n_now_ms:   .string "a clock reading"
n_end:      .string "the end"

    .text
# ---- writing (no allocation) ----

# void rw(char *s)
# Writes a C string to the recording.
    .globl rw
rw:
    mv a1, a0
    la t0, rcd_fb
    ld a0, 0(t0)
    beqz a0, 9f
    tail fb_cstr
9:
    ret

# void rw_dec(long v)
# Writes an integer (negative ones with a sign) to the recording.
    .globl rw_dec
rw_dec:
    la t0, rcd_fb
    ld t0, 0(t0)
    beqz t0, 9f
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    mv s0, a0
    bgez s0, 1f
    la a0, s_minus
    call rw
    neg s0, s0
1:
    la t0, rcd_fb
    ld a0, 0(t0)
    mv a1, s0
    call fb_dec
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
9:
    ret

# long rw_valid_utf8(char *p, long n)
# 1 when p[0..n) is valid UTF-8.
    .globl rw_valid_utf8
rw_valid_utf8:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    mv s1, a1
1:
    blez s1, 2f
    lbu t0, 0(s0)
    li t1, 0x80
    bltu t0, t1, 3f
    mv a0, s0
    mv a1, s1
    call utf8_seq_len
    beqz a0, 8f
    add s0, s0, a0
    sub s1, s1, a0
    j 1b
3:
    addi s0, s0, 1
    addi s1, s1, -1
    j 1b
2:
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

# void rw_str(char *p, long n, long bytes)
# A quoted JSON string: `"`, `\`, control bytes and (with bytes) every byte >= 0x80 escaped, the
# last two as \u00XX; anything else as it is.
    .globl rw_str
rw_str:
    la t0, rcd_fb
    ld t0, 0(t0)
    beqz t0, 9f
    addi sp, sp, -64
    sd ra, 56(sp)
    sd s0, 48(sp)
    sd s1, 40(sp)
    sd s2, 32(sp)
    sd s3, 24(sp)
    mv s0, a0
    mv s1, a1
    mv s2, a2
    la a0, j_quote
    call rw
1:
    blez s1, 7f
    lbu s3, 0(s0)
    addi s0, s0, 1
    addi s1, s1, -1
    li t0, '"'
    beq s3, t0, 2f
    li t0, 92
    beq s3, t0, 2f
    li t0, 0x20
    bltu s3, t0, 3f
    li t0, 0x80
    bltu s3, t0, 4f
    bnez s2, 3f
4:
    sb s3, 0(sp)                # the byte as it is
    la t0, rcd_fb
    ld a0, 0(t0)
    mv a1, sp
    li a2, 1
    call fbuf_write
    j 1b
2:
    li t0, 92                   # \" or \\
    sb t0, 0(sp)
    sb s3, 1(sp)
    la t0, rcd_fb
    ld a0, 0(t0)
    mv a1, sp
    li a2, 2
    call fbuf_write
    j 1b
3:
    li t0, 92                   # \u00XX
    sb t0, 0(sp)
    li t0, 'u'
    sb t0, 1(sp)
    li t0, '0'
    sb t0, 2(sp)
    sb t0, 3(sp)
    la t1, hexd
    srli t2, s3, 4
    add t2, t1, t2
    lbu t2, 0(t2)
    sb t2, 4(sp)
    andi t2, s3, 15
    add t2, t1, t2
    lbu t2, 0(t2)
    sb t2, 5(sp)
    la t0, rcd_fb
    ld a0, 0(t0)
    mv a1, sp
    li a2, 6
    call fbuf_write
    j 1b
7:
    la a0, j_quote
    call rw
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    addi sp, sp, 64
9:
    ret

# void rcd_event(long kind, void *name, long result, void *value, long int_value)
# One event record. name: the path or variable (str) or 0; result: EVR_* or -1 for none;
# value: a str, or 0 (then int_value is written when kind is now_ms).
    .globl rcd_event
rcd_event:
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
    li s5, 0                    # bytes
    beqz s3, 1f
    addi a0, s3, STR_DATA
    ld a1, STR_LEN(s3)
    call rw_valid_utf8
    seqz s5, a0
1:
    la a0, j_head
    call rw
    la t0, ev_names
    slli t1, s0, 3
    add t0, t0, t1
    ld a0, 0(t0)
    call rw
    beqz s1, 3f
    la a0, j_name
    li t0, EVK_ENV
    beq s0, t0, 2f
    la a0, j_path
2:
    call rw
    addi a0, s1, STR_DATA
    ld a1, STR_LEN(s1)
    li a2, 0
    call rw_str
    j 4f
3:
    la a0, j_quote
    call rw
4:
    bltz s2, 5f
    la a0, j_result
    call rw
    la t0, ev_results
    slli t1, s2, 3
    add t0, t0, t1
    ld a0, 0(t0)
    call rw
    la a0, j_quote
    call rw
5:
    li t0, EVK_NOW_MS
    bne s0, t0, 6f
    la a0, j_value
    call rw
    mv a0, s4
    call rw_dec
    j 8f
6:
    beqz s3, 8f
    la a0, j_value
    call rw
    addi a0, s3, STR_DATA
    ld a1, STR_LEN(s3)
    mv a2, s5
    call rw_str
    beqz s5, 8f
    la a0, j_bytes
    call rw
8:
    la a0, j_close
    call rw
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    ld s5, 8(sp)
    addi sp, sp, 64
    ret

    .section .rodata
    .align 3
ev_results: .dword r_ok, r_err, r_denied
s_minus:    .string "-"
    .text

# void rcd_result(long kind, void *name, void *result)
# Records a result value: ok(str)/ok() or err(str).
    .globl rcd_result
rcd_result:
    lwu t0, V_TAG(a2)
    ld a3, V_PAY(a2)
    mv a2, t0
    li a4, 0
    tail rcd_event

# ---- start and end ----

# long rp_fingerprint(void)
# FNV-1a 64 over each program file's path and text, in load order.
    .globl rp_fingerprint
rp_fingerprint:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    li s0, 0xcbf29ce484222325 & 0xffffffff
    li t0, 0xcbf29ce484222325 >> 32
    slli t0, t0, 32
    slli s0, s0, 32
    srli s0, s0, 32
    or s0, s0, t0
    call ld_files
    mv s1, a0
    li s2, 0
1:
    ld t0, VEC_N(s1)
    bge s2, t0, 9f
    mv a0, s1
    mv a1, s2
    call vec_get
    addi s2, s2, 1
    call file_get
    sd a0, 0(sp)
    ld t0, FILE_PATH(a0)
    addi a1, t0, STR_DATA
    ld a2, STR_LEN(t0)
    mv a0, s0
    call fnv_bytes
    mv s0, a0
    ld t0, 0(sp)
    ld a1, FILE_TEXT(t0)
    ld a2, FILE_LEN(t0)
    mv a0, s0
    call fnv_bytes
    mv s0, a0
    j 1b
9:
    mv a0, s0
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    addi sp, sp, 48
    ret

# long fnv_bytes(long h, char *p, long n)
    .globl fnv_bytes
fnv_bytes:
    li t0, 0x100000001b3
1:
    blez a2, 9f
    lbu t1, 0(a1)
    xor a0, a0, t1
    mul a0, a0, t0
    addi a1, a1, 1
    addi a2, a2, -1
    j 1b
9:
    ret

# void rw_hex64(long v)
# 16 hex digits.
    .globl rw_hex64
rw_hex64:
    addi sp, sp, -32
    sd ra, 24(sp)
    la t1, hexd
    li t2, 15
1:
    andi t3, a0, 15
    add t3, t1, t3
    lbu t3, 0(t3)
    add t4, sp, t2
    sb t3, 0(t4)
    srli a0, a0, 4
    addi t2, t2, -1
    bgez t2, 1b
    la t0, rcd_fb
    ld a0, 0(t0)
    beqz a0, 9f
    mv a1, sp
    li a2, 16
    call fbuf_write
9:
    ld ra, 24(sp)
    addi sp, sp, 32
    ret

# void rcd_start(void)
# With --record: opens the recording and writes its header; fixes the seed first (from the clock
# when --seed was not given) so the header can name it.
    .globl rcd_start
rcd_start:
    la t0, opt_record
    ld t0, 0(t0)
    beqz t0, 9f
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, t0
    la t0, opt_seed_set
    ld t0, 0(t0)
    bnez t0, 1f
    call sys_clock_ns
    mv a1, a0
    li a0, 1
    call rt_set_option
1:
    mv a0, s0
    call fbuf_open
    la t0, rcd_fb
    sd a0, 0(t0)
    bnez a0, 2f
    la t0, rcd_failed
    li t1, 1
    sd t1, 0(t0)
2:
    la t0, rcd_mode
    li t1, 1
    sd t1, 0(t0)
    la a0, j_rec_program
    call rw
    la t0, pos_args
    ld t0, 0(t0)
    ld s1, 0(t0)
    mv a0, s1
    call cstr_len
    mv a1, a0
    mv a0, s1
    li a2, 0
    call rw_str
    la a0, j_fprint
    call rw
    call rp_fingerprint
    call rw_hex64
    la a0, j_seed
    call rw
    la t0, opt_seed
    ld a0, 0(t0)
    call rw_dec
    la a0, j_clock
    call rw
    la t0, opt_clock_set
    ld t0, 0(t0)
    bnez t0, 3f
    la a0, j_null
    call rw
    j 4f
3:
    la t0, opt_clock
    ld a0, 0(t0)
    call rw_dec
4:
    la a0, j_args
    call rw
    li s1, 0
5:
    la t0, prog_argc
    ld t0, 0(t0)
    bge s1, t0, 6f
    beqz s1, 51f
    la a0, s_comma
    call rw
51:
    la t0, prog_argv
    ld t0, 0(t0)
    slli t1, s1, 3
    add t0, t0, t1
    ld s0, 0(t0)
    mv a0, s0
    call cstr_len
    mv a1, a0
    mv a0, s0
    li a2, 0
    call rw_str
    addi s1, s1, 1
    j 5b
6:
    la a0, j_arr_close
    call rw
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
9:
    ret

    .section .rodata
s_comma:    .string ","
    .text

# long rcd_finish(long status)
# Completes the recording (a pending read or write that R0010 stopped is written as `denied`, then
# the `end` record) and returns the exit status (74 when it could not be written). No allocation.
    .globl rcd_finish
rcd_finish:
    la t0, rcd_mode
    ld t1, 0(t0)
    li t2, 1
    bne t1, t2, 9f
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    mv s0, a0
    la t0, rcd_mode
    sd zero, 0(t0)
    la t0, rcd_fb
    ld t0, 0(t0)
    beqz t0, 7f
    la t0, rcd_pending_kind
    ld a0, 0(t0)
    beqz a0, 1f
    la t0, rcd_pending_path
    ld a1, 0(t0)
    li a2, EVR_DENIED
    li a3, 0
    li a4, 0
    call rcd_event_raw
1:
    la a0, j_head
    call rw
    la a0, k_end
    call rw
    la a0, j_status
    call rw
    mv a0, s0
    call rw_dec
    la a0, j_close
    call rw
    la t0, rcd_fb
    ld a0, 0(t0)
    call fbuf_flush
    la t0, rcd_fb
    ld t0, 0(t0)
    ld t0, FB_ERR(t0)
    beqz t0, 8f
7:
    la t0, opt_record
    ld a0, 0(t0)
    call out_unwritable
    bnez s0, 8f
    li s0, EXIT_OUTPUT
8:
    mv a0, s0
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
9:
    ret

# void rcd_event_raw(long kind, void *name, long result, void *value, long int_value)
# rcd_event with rcd_mode set to 1 for the call (rcd_finish has turned it off).
    .globl rcd_event_raw
rcd_event_raw:
    tail rcd_event

# ---- the wrappers the builtin stubs call ----

# void *io_read_line(void *node)
    .globl io_read_line
io_read_line:
    la t0, rcd_mode
    ld t0, 0(t0)
    bnez t0, 1f
    tail rt_read_line
1:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    li t1, 2
    bne t0, t1, 2f
    li a0, EVK_READ_LINE
    li a1, 0
    call rp_take
    call rp_result
    j 9f
2:
    call rt_read_line
    mv s0, a0
    mv a2, a0
    li a0, EVK_READ_LINE
    li a1, 0
    call rcd_result
    mv a0, s0
9:
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret

# void *io_read_all(void *node)
    .globl io_read_all
io_read_all:
    la t0, rcd_mode
    ld t0, 0(t0)
    bnez t0, 1f
    tail rt_read_all
1:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    li t1, 2
    bne t0, t1, 2f
    li a0, EVK_READ_ALL
    li a1, 0
    call rp_take
    ld a0, EV_VALUE(a0)
    j 9f
2:
    call rt_read_all
    mv s0, a0
    mv a3, a0
    li a0, EVK_READ_ALL
    li a1, 0
    li a2, -1
    li a4, 0
    call rcd_event
    mv a0, s0
9:
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret

# void *io_read_file(void *path)
# The path check (R0010) and the read; replayed `denied` repeats the check's error.
    .globl io_read_file
io_read_file:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    mv s0, a0
    la t0, rcd_mode
    ld t0, 0(t0)
    li t1, 2
    beq t0, t1, 5f
    li t1, 1
    bne t0, t1, 1f
    la t0, rcd_pending_kind     # recording: `denied` if the check stops the run
    li t1, EVK_READ_FILE
    sd t1, 0(t0)
    la t0, rcd_pending_path
    sd s0, 0(t0)
1:
    li a0, EF_FREAD
    mv a1, s0
    call rt_check_path
    la t0, rcd_pending_kind
    sd zero, 0(t0)
    mv a0, s0
    call rt_read_file
    la t0, rcd_mode
    ld t0, 0(t0)
    beqz t0, 9f
    mv a2, a0
    mv a1, s0
    mv s0, a0
    li a0, EVK_READ_FILE
    call rcd_result
    mv a0, s0
    j 9f
5:
    li a0, EVK_READ_FILE
    mv a1, s0
    call rp_take
    ld t0, EV_RESULT(a0)
    li t1, EVR_DENIED
    bne t0, t1, 6f
    li a0, EF_FREAD             # the same R0010 as the recorded run
    mv a1, s0
    call rt_check_path
    li a0, EVK_READ_FILE        # (allowed now: the program or its grant changed)
    mv a1, s0
    call rp_mismatch_now
6:
    call rp_result
9:
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret

# void *io_write_file(void *path, void *text)
# Replay writes nothing and returns the recorded result.
    .globl io_write_file
io_write_file:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    mv s1, a1
    la t0, rcd_mode
    ld t0, 0(t0)
    li t1, 2
    beq t0, t1, 5f
    li t1, 1
    bne t0, t1, 1f
    la t0, rcd_pending_kind
    li t1, EVK_WRITE_FILE
    sd t1, 0(t0)
    la t0, rcd_pending_path
    sd s0, 0(t0)
1:
    li a0, EF_FWRITE
    mv a1, s0
    call rt_check_path
    la t0, rcd_pending_kind
    sd zero, 0(t0)
    mv a0, s0
    mv a1, s1
    call rt_write_file
    la t0, rcd_mode
    ld t0, 0(t0)
    beqz t0, 9f
    mv a2, a0
    mv a1, s0
    mv s0, a0
    li a0, EVK_WRITE_FILE
    call rcd_result
    mv a0, s0
    j 9f
5:
    li a0, EVK_WRITE_FILE
    mv a1, s0
    call rp_take
    ld t0, EV_RESULT(a0)
    li t1, EVR_DENIED
    bne t0, t1, 6f
    li a0, EF_FWRITE
    mv a1, s0
    call rt_check_path
    li a0, EVK_WRITE_FILE
    mv a1, s0
    call rp_mismatch_now
6:
    call rp_result
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void *io_env(void *name)
    .globl io_env
io_env:
    la t0, rcd_mode
    ld t0, 0(t0)
    bnez t0, 1f
    tail rt_env
1:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    mv s0, a0
    li t1, 2
    bne t0, t1, 2f
    li a0, EF_ENV               # the name is still checked against the grant (no outside data)
    mv a1, s0
    call rt_check_name
    li a0, EVK_ENV
    mv a1, s0
    call rp_take
    call rp_result
    j 9f
2:
    call rt_env
    mv a2, a0
    mv a1, s0
    mv s0, a0
    li a0, EVK_ENV
    call rcd_result
    mv a0, s0
9:
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret

# long io_now_ms(void)
    .globl io_now_ms
io_now_ms:
    la t0, rcd_mode
    ld t0, 0(t0)
    bnez t0, 1f
    tail rt_now_ms
1:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    li t1, 2
    bne t0, t1, 2f
    li a0, EVK_NOW_MS
    li a1, 0
    call rp_take
    ld a0, EV_VALUE(a0)
    j 9f
2:
    call rt_now_ms
    mv s0, a0
    mv a4, a0
    li a0, EVK_NOW_MS
    li a1, 0
    li a2, -1
    li a3, 0
    call rcd_event
    mv a0, s0
9:
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret

# ---- replay ----

# void *rp_result(void *event)
# The value a recorded result stands for: ok(value) / ok() / err(value).
    .globl rp_result
rp_result:
    ld t0, EV_RESULT(a0)
    ld a1, EV_VALUE(a0)
    mv a0, t0
    tail make_result

# void *rp_take(long kind, void *name)
# The next event, which must be of this kind (and name); otherwise R0014.
    .globl rp_take
rp_take:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    mv s1, a1
    la t0, rp_events
    ld t0, 0(t0)
    la t1, rp_next
    ld t2, 0(t1)
    ld t3, VEC_N(t0)
    bge t2, t3, 8f
    addi t4, t2, 1
    sd t4, 0(t1)
    mv a0, t0
    mv a1, t2
    call vec_get
    ld t0, EV_KIND(a0)
    bne t0, s0, 7f
    beqz s1, 9f
    sd a0, 0(sp)
    ld a0, EV_NAME(a0)
    beqz a0, 6f
    mv a1, s1
    call str_eq
    beqz a0, 6f
    ld a0, 0(sp)
    j 9f
6:
    ld a0, 0(sp)
7:
    mv a2, a0                   # a different event
    mv a0, s0
    mv a1, s1
    call rp_mismatch
8:
    mv a0, s0                   # nothing left
    mv a1, s1
    li a2, 0
    call rp_mismatch
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void *rp_describe(char *tmpl_table_entry, void *name)
# One of the d_/n_ templates with the name filled in.
    .globl rp_describe
rp_describe:
    addi sp, sp, -16
    sd ra, 8(sp)
    mv t0, a1
    mv a1, t0
    li a2, 0
    li a3, 0
    beqz t0, 1f
    call msg
    j 9f
1:
    call str_from_cstr
9:
    ld ra, 8(sp)
    addi sp, sp, 16
    ret

    .section .rodata
    .align 3
d_table:    .dword 0, d_read_line, d_read_all, d_read_file, d_write_file, d_env, d_now_ms, n_end
n_table:    .dword 0, n_read_line, n_read_all, n_read_file, n_write_file, n_env, n_now_ms, n_end
    .text

# void rp_mismatch(long kind, void *name, void *event)
# R0014: what the program asked for, and what the recording has next (or that it has no more).
    .globl rp_mismatch
rp_mismatch:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s1, a2
    la t0, d_table
    slli a0, a0, 3
    add t0, t0, a0
    ld a0, 0(t0)
    call rp_describe
    mv s0, a0
    beqz s1, 1f
    la t0, n_table
    ld t1, EV_KIND(s1)
    slli t1, t1, 3
    add t0, t0, t1
    ld a0, 0(t0)
    ld a1, EV_NAME(s1)
    call rp_describe
    j 2f
1:
    mv a1, s0
    la a0, m_mismatch_end
    li a2, 0
    li a3, 0
    call msg
    j 3f
2:
    mv a2, a0
    mv a1, s0
    la a0, m_mismatch
    li a3, 0
    call msg
3:
    mv a1, a0
    la a0, code_r14
    la t0, cur_node
    ld a2, 0(t0)
    call rt_raise
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void rp_mismatch_now(long kind, void *name)
# R0014 for a recorded `denied` that the program is now allowed to do: the event just taken.
    .globl rp_mismatch_now
rp_mismatch_now:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    mv s1, a1
    la t0, rp_events
    ld a0, 0(t0)
    la t1, rp_next
    ld a1, 0(t1)
    addi a1, a1, -1
    call vec_get
    mv a2, a0
    mv a0, s0
    mv a1, s1
    call rp_mismatch
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# ---- reading a recording ----

    .section .data
    .align 3
rf_kind:    .dword 0            # fields of the line being read
rf_program: .dword 0
rf_fprint:  .dword 0
rf_seed:    .dword 0
rf_clock:   .dword -1
rf_args:    .dword 0
rf_name:    .dword 0
rf_result:  .dword 0
rf_value:   .dword 0
rf_ivalue:  .dword 0
rf_isint:   .dword 0
rf_status:  .dword 0

    .section .rodata
    .align 3
rk_keys:    .dword rk_kind, rk_program, rk_fingerprint, rk_seed, rk_clock, rk_args, rk_path, rk_name
    .dword rk_result, rk_value, rk_status, 0
rk_kind:    .string "kind"
rk_program: .string "program"
rk_fingerprint: .string "fingerprint"
rk_seed:    .string "seed"
rk_clock:   .string "clock_ms"
rk_args:    .string "args"
rk_path:    .string "path"
rk_name:    .string "name"
rk_result:  .string "result"
rk_value:   .string "value"
rk_status:  .string "status"
n_replay_cmd: .string "replay"
    .text

# void rp_bad(void)
# T0008 invalid_recording, exit 64.
    .globl rp_bad
rp_bad:
    addi sp, sp, -16
    sd ra, 8(sp)
    la t0, pos_args
    ld t0, 0(t0)
    ld a0, 0(t0)
    call str_from_cstr
    mv a1, a0
    la a0, m_bad_rec
    li a2, 0
    li a3, 0
    call msg
    mv a3, a0
    la t0, opt_json
    ld a0, 0(t0)
    la t0, report_fd
    ld a1, 0(t0)
    la a2, code_t8
    li a4, 0
    call toolchain_error
    li a0, EXIT_USAGE
    call vo_exit
    ld ra, 8(sp)
    addi sp, sp, 16
    ret

# char *rp_ws(char *p, char *end)
# Skips spaces.
    .globl rp_ws
rp_ws:
1:
    bge a0, a1, 9f
    lbu t0, 0(a0)
    li t1, ' '
    bne t0, t1, 9f
    addi a0, a0, 1
    j 1b
9:
    ret

# char *rp_string(char *p, char *end, void **out)
# A JSON string at p (after its opening quote is checked) into *out as a str; returns the
# position after the closing quote. \u00XX stands for one byte (contracts/recording.md).
    .globl rp_string
rp_string:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    mv s1, a1
    mv s2, a2
    bge s0, s1, 80f
    lbu t0, 0(s0)
    li t1, '"'
    bne t0, t1, 80f
    addi s0, s0, 1
    call sb_new
    mv s3, a0
1:
    bge s0, s1, 80f
    lbu a1, 0(s0)
    addi s0, s0, 1
    li t0, '"'
    beq a1, t0, 7f
    li t0, 92
    bne a1, t0, 5f
    bge s0, s1, 80f             # an escape
    lbu a1, 0(s0)
    addi s0, s0, 1
    li t0, 'n'
    li t1, 10
    beq a1, t0, 4f
    li t0, 't'
    li t1, 9
    beq a1, t0, 4f
    li t0, 'r'
    li t1, 13
    beq a1, t0, 4f
    li t0, 'u'
    bne a1, t0, 5f              # \" \\ \/: the character itself
    addi t2, s0, 4              # \uXXXX: only 00XX is written
    bgt t2, s1, 80f
    li t1, 0
    li t3, 4
2:
    lbu t4, 0(s0)
    addi s0, s0, 1
    slli t1, t1, 4
    addi t5, t4, -48
    li t6, 10
    bltu t5, t6, 3f
    addi t5, t4, -87            # a-f
    li t6, 6
    addi t4, t5, -10
    bgeu t4, t6, 80f
3:
    add t1, t1, t5
    addi t3, t3, -1
    bnez t3, 2b
    li t0, 256
    bgeu t1, t0, 80f
4:
    mv a1, t1
5:
    mv a0, s3
    call sb_byte
    j 1b
7:
    mv a0, s3
    call sb_finish
    sd a0, 0(s2)
    mv a0, s0
    j 9f
80:
    call rp_bad
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# char *rp_int(char *p, char *end, long *out)
    .globl rp_int
rp_int:
    li t0, 0
    li t3, 0                    # negative?
    bge a0, a1, 8f
    lbu t1, 0(a0)
    li t2, '-'
    bne t1, t2, 1f
    li t3, 1
    addi a0, a0, 1
1:
    mv t4, a0
2:
    bge a0, a1, 3f
    lbu t1, 0(a0)
    addi t1, t1, -48
    li t2, 10
    bgeu t1, t2, 3f
    li t2, 10
    mul t0, t0, t2
    add t0, t0, t1
    addi a0, a0, 1
    j 2b
3:
    beq a0, t4, 8f
    beqz t3, 4f
    neg t0, t0
4:
    sd t0, 0(a2)
    ret
8:
    tail rp_bad

# long rp_key(void *str)
# The index of a known key (rk_keys), or -1.
    .globl rp_key
rp_key:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    li s1, 0
1:
    la t0, rk_keys
    slli t1, s1, 3
    add t0, t0, t1
    ld a1, 0(t0)
    li a0, -1
    beqz a1, 9f
    mv a0, s0
    call str_eq_cstr
    bnez a0, 2f
    addi s1, s1, 1
    j 1b
2:
    mv a0, s1
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void rp_line(char *p, char *end)
# One record into the rf_* fields: a flat object of strings, integers, true/false/null and (for
# args) an array of strings. Anything else is T0008.
    .globl rp_line
rp_line:
    addi sp, sp, -64
    sd ra, 56(sp)
    sd s0, 48(sp)
    sd s1, 40(sp)
    sd s2, 32(sp)
    sd s3, 24(sp)
    mv s0, a0
    mv s1, a1
    la t0, rf_kind
    sd zero, 0(t0)
    la t0, rf_name
    sd zero, 0(t0)
    la t0, rf_result
    sd zero, 0(t0)
    la t0, rf_value
    sd zero, 0(t0)
    la t0, rf_isint
    sd zero, 0(t0)
    mv a0, s0
    mv a1, s1
    call rp_ws
    mv s0, a0
    bge s0, s1, 80f
    lbu t0, 0(s0)
    li t1, '{'
    bne t0, t1, 80f
    addi s0, s0, 1
1:
    mv a0, s0                   # "key":
    mv a1, s1
    call rp_ws
    mv a1, s1
    addi a2, sp, 0
    call rp_string
    mv a1, s1
    call rp_ws
    mv s0, a0
    bge s0, s1, 80f
    lbu t0, 0(s0)
    li t1, ':'
    bne t0, t1, 80f
    addi a0, s0, 1
    mv a1, s1
    call rp_ws
    mv s0, a0
    ld a0, 0(sp)
    call rp_key
    mv s2, a0                   # which field (-1: ignored)
    bge s0, s1, 80f
    lbu t0, 0(s0)
    li t1, '"'
    beq t0, t1, 2f
    li t1, '['
    beq t0, t1, 3f
    li t1, 't'
    beq t0, t1, 5f
    li t1, 'f'
    beq t0, t1, 51f
    li t1, 'n'
    beq t0, t1, 52f
    mv a0, s0                   # an integer
    mv a1, s1
    addi a2, sp, 8
    call rp_int
    mv s0, a0
    ld t1, 8(sp)
    li t0, 3
    beq s2, t0, 21f
    li t0, 4
    beq s2, t0, 22f
    li t0, 9
    beq s2, t0, 23f
    li t0, 10
    beq s2, t0, 24f
    j 6f
21:
    la t0, rf_seed
    sd t1, 0(t0)
    j 6f
22:
    la t0, rf_clock
    sd t1, 0(t0)
    j 6f
23:
    la t0, rf_ivalue
    sd t1, 0(t0)
    la t0, rf_isint
    li t1, 1
    sd t1, 0(t0)
    j 6f
24:
    la t0, rf_status
    sd t1, 0(t0)
    j 6f
2:
    mv a0, s0                   # a string
    mv a1, s1
    addi a2, sp, 8
    call rp_string
    mv s0, a0
    bltz s2, 6f
    la t0, rf_slots
    slli t1, s2, 3
    add t0, t0, t1
    ld t0, 0(t0)
    beqz t0, 6f
    ld t1, 8(sp)
    sd t1, 0(t0)
    j 6f
3:
    addi s0, s0, 1              # an array of strings (args)
    call vec_new
    mv s3, a0
31:
    mv a0, s0
    mv a1, s1
    call rp_ws
    mv s0, a0
    bge s0, s1, 80f
    lbu t0, 0(s0)
    li t1, ']'
    beq t0, t1, 33f
    mv a1, s1
    addi a2, sp, 8
    call rp_string
    mv a1, s1
    call rp_ws
    mv s0, a0
    mv a0, s3
    ld a1, 8(sp)
    call vec_push
    bge s0, s1, 80f
    lbu t0, 0(s0)
    li t1, ','
    bne t0, t1, 31b
    addi s0, s0, 1
    j 31b
33:
    addi s0, s0, 1
    li t0, 5
    bne s2, t0, 6f
    la t0, rf_args
    sd s3, 0(t0)
    j 6f
5:
    addi s0, s0, 4              # true
    j 6f
51:
    addi s0, s0, 5              # false
    j 6f
52:
    addi s0, s0, 4              # null (clock_ms)
    li t0, 4
    bne s2, t0, 6f
    li t1, -1
    la t0, rf_clock
    sd t1, 0(t0)
6:
    mv a0, s0                   # , or }
    mv a1, s1
    call rp_ws
    mv s0, a0
    bge s0, s1, 80f
    lbu t0, 0(s0)
    addi s0, s0, 1
    li t1, ','
    beq t0, t1, 1b
    li t1, '}'
    beq t0, t1, 9f
80:
    call rp_bad
9:
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    addi sp, sp, 64
    ret

    .section .rodata
    .align 3
# where each string-valued key is stored (rk_keys order; 0: not a string)
rf_slots:   .dword rf_kind, rf_program, rf_fprint, 0, 0, 0, rf_name, rf_name, rf_result, rf_value, 0
    .text

# long rp_kind_of(void *str)
# EVK_* for an event kind name; 0 for "recording"; -1 when unknown.
    .globl rp_kind_of
rp_kind_of:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    li a0, -1
    beqz s0, 9f
    mv a0, s0
    la a1, k_recording
    call str_eq_cstr
    beqz a0, 1f
    li a0, 0
    j 9f
1:
    li s1, 1
2:
    li t0, EVK_END
    bgt s1, t0, 8f
    la t0, ev_names
    slli t1, s1, 3
    add t0, t0, t1
    ld a1, 0(t0)
    mv a0, s0
    call str_eq_cstr
    bnez a0, 3f
    addi s1, s1, 1
    j 2b
3:
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

# long rp_result_of(void *str)
# EVR_* for "ok", "err", "denied"; -1 when there is none.
    .globl rp_result_of
rp_result_of:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    mv s0, a0
    li a0, -1
    beqz s0, 9f
    la a1, r_ok
    mv a0, s0
    call str_eq_cstr
    li t0, EVR_OK
    bnez a0, 8f
    la a1, r_err
    mv a0, s0
    call str_eq_cstr
    li t0, EVR_ERR
    bnez a0, 8f
    la a1, r_denied
    mv a0, s0
    call str_eq_cstr
    li t0, EVR_DENIED
    bnez a0, 8f
    call rp_bad                 # (does not return)
    li t0, -1
8:
    mv a0, t0
9:
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret

# void rp_read(char *text, long len)
# Every line: the header first, then events, then `end` (nothing after it).
    .globl rp_read
rp_read:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    add s1, a0, a1
    li s3, 0                    # records read
    call vec_new
    la t0, rp_events
    sd a0, 0(t0)
1:
    bge s0, s1, 8f
    mv s2, s0                   # the end of the line
2:
    bge s2, s1, 3f
    lbu t0, 0(s2)
    li t1, 10
    beq t0, t1, 3f
    addi s2, s2, 1
    j 2b
3:
    beq s2, s0, 7f              # (an empty line)
    la t0, rp_saw_end
    ld t0, 0(t0)
    bnez t0, 80f                # nothing after `end`
    mv a0, s0
    mv a1, s2
    call rp_line
    la t0, rf_kind
    ld a0, 0(t0)
    call rp_kind_of
    bltz a0, 80f
    bnez s3, 4f
    bnez a0, 80f                # the header comes first
    la t0, rf_program
    ld t1, 0(t0)
    beqz t1, 80f
    la t0, rp_program
    sd t1, 0(t0)
    la t0, rf_fprint
    ld t1, 0(t0)
    la t0, rp_fprint
    sd t1, 0(t0)
    la t0, rf_seed
    ld t1, 0(t0)
    la t0, rp_seed
    sd t1, 0(t0)
    la t0, rf_clock
    ld t1, 0(t0)
    la t0, rp_clock
    sd t1, 0(t0)
    la t0, rf_args
    ld t1, 0(t0)
    la t0, rp_args
    sd t1, 0(t0)
    j 6f
4:
    beqz a0, 80f                # a second header
    li t0, EVK_END
    bne a0, t0, 5f
    la t0, rp_saw_end
    li t1, 1
    sd t1, 0(t0)
    j 6f
5:
    sd a0, 0(sp)                # an event
    li a0, EV_SIZE
    call alloc
    mv s2, a0                   # (the line end is found again below)
    ld t0, 0(sp)
    sd t0, EV_KIND(s2)
    la t0, rf_name
    ld t0, 0(t0)
    sd t0, EV_NAME(s2)
    la t0, rf_result
    ld a0, 0(t0)
    call rp_result_of
    sd a0, EV_RESULT(s2)
    la t0, rf_value
    ld t0, 0(t0)
    sd t0, EV_VALUE(s2)
    la t0, rf_isint
    ld t0, 0(t0)
    beqz t0, 51f
    la t0, rf_ivalue
    ld t0, 0(t0)
    sd t0, EV_VALUE(s2)
51:
    mv a0, s2                   # the fields its kind needs (T0008 otherwise)
    call rp_check_event
    la t0, rp_events
    ld a0, 0(t0)
    mv a1, s2
    call vec_push
    mv s2, s0                   # back to the end of this line
52:
    bge s2, s1, 6f
    lbu t0, 0(s2)
    li t1, 10
    beq t0, t1, 6f
    addi s2, s2, 1
    j 52b
6:
    addi s3, s3, 1
7:
    addi s0, s2, 1
    j 1b
8:
    beqz s3, 80f
    la t0, rp_saw_end
    ld t0, 0(t0)
    bnez t0, 9f
80:
    call rp_bad
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# void rp_check_event(void *event)
# An event must have what replaying it needs: a path or name for files and env, a result where
# there is one, a str value for ok/err results and read_all, an integer for now_ms.
    .globl rp_check_event
rp_check_event:
    ld t0, EV_KIND(a0)
    ld t1, EV_RESULT(a0)
    ld t2, EV_VALUE(a0)
    ld t3, EV_NAME(a0)
    la t4, rf_isint
    ld t4, 0(t4)
    li t5, EVK_NOW_MS
    bne t0, t5, 1f
    beqz t4, 8f                 # now_ms: an integer
    ret
1:
    bnez t4, 8f                 # everything else: no integer value
    li t5, EVK_READ_ALL
    bne t0, t5, 2f
    beqz t2, 8f
    ret
2:
    bltz t1, 8f                 # a result
    li t5, EVK_READ_LINE
    beq t0, t5, 3f
    beqz t3, 8f                 # files and env: the path or name
3:
    li t5, EVR_DENIED
    beq t1, t5, 4f
    li t5, EVK_WRITE_FILE       # ok() of write_file has no value
    bne t0, t5, 5f
    li t5, EVR_OK
    beq t1, t5, 9f
5:
    beqz t2, 8f
    ret
4:
    li t5, EVK_READ_FILE        # denied: only for files
    beq t0, t5, 9f
    li t5, EVK_WRITE_FILE
    beq t0, t5, 9f
8:
    tail rp_bad
9:
    ret

# ---- the command ----

# int cmd_replay(void)
# `vo replay RECORDING` (contracts/cli.md, contracts/recording.md).
    .globl cmd_replay
cmd_replay:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    la t0, pos_n
    ld t0, 0(t0)
    li t1, 1
    beq t0, t1, 1f
    la a0, m_replay_args
    call str_from_cstr
    call usage_error
1:
    la t0, pos_args
    ld t0, 0(t0)
    ld s0, 0(t0)
    mv a0, s0
    addi a1, sp, 0
    call read_whole_file
    bgez a0, 2f
    mv a0, s0                   # unreadable: T0002, exit 66
    call str_from_cstr
    mv s1, a0
    mv a1, a0
    la a0, m_cannot_read_rec
    li a2, 0
    li a3, 0
    call msg
    mv a3, a0
    la t0, opt_json
    ld a0, 0(t0)
    la t0, report_fd
    ld a1, 0(t0)
    la a2, code_t2r
    mv a4, s1
    call toolchain_error
    li a0, EXIT_NOINPUT
    j 9f
2:
    mv a1, a0
    ld a0, 0(sp)
    call rp_read
    la t0, rp_seed              # the recorded run's seed, clock and arguments
    ld a1, 0(t0)
    li a0, 1
    call rt_set_option
    la t0, rp_clock
    ld a1, 0(t0)
    bltz a1, 3f
    li a0, 2
    call rt_set_option
3:
    la t0, rp_args
    ld s1, 0(t0)
    li s2, 0
    beqz s1, 4f
    ld s2, VEC_N(s1)
4:
    slli a0, s2, 3
    addi a0, a0, 8
    call alloc
    la t0, prog_argv
    sd a0, 0(t0)
    la t0, prog_argc
    sd s2, 0(t0)
    li t1, 0
5:
    bge t1, s2, 6f
    sd t1, 0(sp)
    mv a0, s1
    mv a1, t1
    call vec_get
    addi t2, a0, STR_DATA
    la t0, prog_argv
    ld t0, 0(t0)
    ld t1, 0(sp)
    slli t3, t1, 3
    add t0, t0, t3
    sd t2, 0(t0)
    addi t1, t1, 1
    j 5b
6:
    la t0, rp_program           # the program: as `vo run` would check it
    ld a0, 0(t0)
    call load_source
    mv s0, a0
    li a1, 1
    call check_program
    la t0, diag_errors
    ld t0, 0(t0)
    beqz t0, 7f
    li a0, 1
    call finish_check
    j 9f
7:
    call rp_note_changed
    mv a0, s0
    call file_get
    ld t0, FILE_AST(a0)
    ld a0, N_A(t0)
    call find_main
    sd a0, 0(sp)
    li a0, 0
    call outputs_start
    call compile_program
    li a0, 0
    call rt_effects_start
    la t0, rcd_mode             # from here every input comes from the recording
    li t1, 2
    sd t1, 0(t0)
    ld a0, 0(sp)
    call run_main
    sd a0, 0(sp)
    li a0, 2
    la a1, n_replay_cmd
    li a2, 0
    call gc_report
    ld a0, 0(sp)
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    addi sp, sp, 48
    ret

# void rp_note_changed(void)
# When the program's fingerprint differs from the recorded one: a note on stderr (contracts/reports.md).
    .globl rp_note_changed
rp_note_changed:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    call rp_fingerprint
    mv s0, a0
    call sb_new                 # its 16 hex digits
    mv s1, a0
    li t2, 60
1:
    srl t3, s0, t2
    andi t3, t3, 15
    la t1, hexd
    add t3, t1, t3
    lbu a1, 0(t3)
    sd t2, 0(sp)
    mv a0, s1
    call sb_byte
    ld t2, 0(sp)
    addi t2, t2, -4
    bgez t2, 1b
    mv a0, s1
    call sb_finish
    la t0, rp_fprint
    ld a1, 0(t0)
    beqz a1, 2f
    call str_eq
    bnez a0, 9f
2:
    call sb_new
    mv s1, a0
    la t0, opt_json
    ld t0, 0(t0)
    beqz t0, 3f
    la a1, j_replay
    call sb_cstr
    la t0, rp_program
    ld a1, 0(t0)
    mv a0, s1
    call json_str_into
    mv a0, s1
    la a1, j_changed
    call sb_cstr
    j 4f
3:
    la t0, rp_program
    ld a1, 0(t0)
    la a0, m_changed
    li a2, 0
    li a3, 0
    call msg
    mv a1, a0
    mv a0, s1
    call sb_str
4:
    li a0, 2
    ld a1, SB_PTR(s1)
    ld a2, SB_LEN(s1)
    call out_bytes
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret
