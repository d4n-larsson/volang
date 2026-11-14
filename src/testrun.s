# Test runner: runs each `test` block with its own recovery point and deadline, and reports it.
    .include "defs.inc"

    .equ ST_PASS, 0
    .equ ST_FAIL, 1
    .equ ST_ERROR, 2
    .equ ST_TIMEOUT, 3

    .section .bss
    .align 3
    .globl t_passed
t_passed:   .space 8
    .globl t_failed
t_failed:   .space 8
    .globl t_errored
t_errored:  .space 8
    .globl t_timed_out
t_timed_out: .space 8

    .section .rodata
s_cmd_test: .string "test"
    .align 3
status_words:
    .dword w_pass, w_fail, w_error, w_timeout
status_upper:
    .dword u_pass, u_fail, u_error, u_timeout
w_pass: .string "pass"
w_fail: .string "fail"
w_error: .string "error"
w_timeout: .string "timeout"
u_pass: .string "PASS "
u_fail: .string "FAIL "
u_error: .string "ERROR "
u_timeout: .string "TIMEOUT "
j_head: .string "{\"v\":1,\"kind\":\"test\",\"name\":"
j_status: .string ",\"status\":\""
j_file: .string "\",\"file\":"
j_start: .string ",\"start\":"
j_ms: .string ",\"ms\":"
j_uses: .string ",\"uses\":"
j_failure: .string ",\"failure\":{\"file\":"
j_message: .string ",\"message\":"
j_expected: .string ",\"expected\":"
j_actual: .string ",\"actual\":"
j_code: .string ",\"code\":\""
j_trace_key: .string ",\"trace\":"
j_close: .string "}\n"
h_ms_open: .string " ("
h_ms_close: .string "ms)\n"
h_indent: .string "  "
h_rt: .string ": runtime error["
h_rt2: .string "]: "
h_expected: .string "    expected: "
h_actual: .string "    actual:   "
s_passed: .string " passed, "
s_failed: .string " failed"
s_errored: .string ", "
s_errored2: .string " errored"
s_timed: .string " timed out"
s_comma: .string ", "
x_passed: .string ",\"passed\":"
x_failed: .string ",\"failed\":"
x_errored: .string ",\"errored\":"
x_timed: .string ",\"timed_out\":"

    .text
# void report_test(void *test, int status, long ms)
    .globl report_test
report_test:
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
    call sb_new
    mv s3, a0
    la s4, tf_node              # the failure location: expect call, or the runtime error node
    ld s4, 0(s4)
    li t0, ST_FAIL
    beq s1, t0, 1f
    la s4, rt_err_node
    ld s4, 0(s4)
1:
    la t0, opt_json
    ld t0, 0(t0)
    beqz t0, 50f
    mv a0, s3                   # JSON record
    la a1, j_head
    call sb_cstr
    mv a0, s3
    ld a1, N_A(s0)
    call json_str_into
    mv a0, s3
    la a1, j_status
    call sb_cstr
    la t0, status_words
    slli t1, s1, 3
    add t0, t0, t1
    ld a1, 0(t0)
    mv a0, s3
    call sb_cstr
    mv a0, s3
    la a1, j_file
    call sb_cstr
    lwu a0, N_FILE(s0)
    call file_get
    ld a1, FILE_PATH(a0)
    mv a0, s3
    call json_str_into
    mv a0, s3
    la a1, j_start
    call sb_cstr
    mv a0, s3
    lwu a1, N_FILE(s0)
    lwu a2, N_START(s0)
    call json_pos
    ld t0, N_D(s0)              # the test's grant
    beqz t0, 41f
    ld a0, FX_DECLARED(t0)
    beqz a0, 41f
    call fx_render
    sd a0, 0(sp)
    mv a0, s3
    la a1, j_uses
    call sb_cstr
    mv a0, s3
    ld a1, 0(sp)
    call json_str_into
41:
    mv a0, s3
    la a1, j_ms
    call sb_cstr
    mv a0, s3
    mv a1, s2
    call int_to_dec
    lhu t0, N_FLAGS(s0)
    andi t0, t0, NF_PROP
    beqz t0, 42f
    mv a0, s3
    mv a1, s0
    mv a2, s1
    call prop_json
42:
    beqz s1, 48f
    beqz s4, 48f
    mv a0, s3
    la a1, j_failure
    call sb_cstr
    lwu a0, N_FILE(s4)
    call file_get
    ld a1, FILE_PATH(a0)
    mv a0, s3
    call json_str_into
    mv a0, s3
    la a1, j_start
    call sb_cstr
    mv a0, s3
    lwu a1, N_FILE(s4)
    lwu a2, N_START(s4)
    call json_pos
    mv a0, s3
    la a1, j_end_key
    call sb_cstr
    mv a0, s3
    lwu a1, N_FILE(s4)
    lwu a2, N_END(s4)
    call json_pos
    mv a0, s3
    la a1, j_message
    call sb_cstr
    li t0, ST_FAIL
    bne s1, t0, 45f
    la t0, tf_msg
    ld a1, 0(t0)
    mv a0, s3
    call json_str_into
    la t0, tf_expected
    ld t0, 0(t0)
    beqz t0, 47f
    mv a0, s3
    la a1, j_expected
    call sb_cstr
    la t0, tf_expected
    ld a1, 0(t0)
    mv a0, s3
    call json_str_into
    mv a0, s3
    la a1, j_actual
    call sb_cstr
    la t0, tf_actual
    ld a1, 0(t0)
    mv a0, s3
    call json_str_into
    j 47f
45:
    la t0, rt_err_msg
    ld a1, 0(t0)
    mv a0, s3
    call json_str_into
    mv a0, s3
    la a1, j_code
    call sb_cstr
    la t0, rt_err_code
    ld a1, 0(t0)
    mv a0, s3
    call sb_cstr
    mv a0, s3
    li a1, '"'
    call sb_byte
    la t0, rt_err_trace_json    # spec 006: the frames and their values
    ld t0, 0(t0)
    beqz t0, 47f
    mv a0, s3
    la a1, j_trace_key
    call sb_cstr
    la t0, rt_err_trace_json
    ld a1, 0(t0)
    mv a0, s3
    call sb_str
47:
    mv a0, s3
    li a1, '}'
    call sb_byte
48:
    mv a0, s3
    la a1, j_close
    call sb_cstr
    j 90f
50:                             # human line: STATUS name (Nms) + failure details
    la t0, status_upper
    slli t1, s1, 3
    add t0, t0, t1
    ld a1, 0(t0)
    mv a0, s3
    call sb_cstr
    mv a0, s3
    ld a1, N_A(s0)
    call sb_str
    mv a0, s3
    la a1, h_ms_open
    call sb_cstr
    lhu t0, N_FLAGS(s0)
    andi t0, t0, NF_PROP
    beqz t0, 51f
    mv a0, s3
    call prop_cases_text
51:
    mv a0, s3
    mv a1, s2
    call int_to_dec
    mv a0, s3
    la a1, h_ms_close
    call sb_cstr
    beqz s1, 89f
    beqz s4, 89f
    mv a0, s3
    la a1, h_indent
    call sb_cstr
    mv a0, s3
    lwu a1, N_FILE(s4)
    lwu a2, N_START(s4)
    call human_loc
    li t0, ST_FAIL
    bne s1, t0, 55f
    mv a0, s3
    li a1, ':'
    call sb_byte
    mv a0, s3
    li a1, ' '
    call sb_byte
    la t0, tf_msg
    ld a1, 0(t0)
    mv a0, s3
    call sb_str
    mv a0, s3
    li a1, 10
    call sb_byte
    la t0, tf_expected
    ld t0, 0(t0)
    beqz t0, 89f
    mv a0, s3
    la a1, h_expected
    call sb_cstr
    la t0, tf_expected
    ld a1, 0(t0)
    mv a0, s3
    call sb_str
    mv a0, s3
    li a1, 10
    call sb_byte
    mv a0, s3
    la a1, h_actual
    call sb_cstr
    la t0, tf_actual
    ld a1, 0(t0)
    mv a0, s3
    call sb_str
    mv a0, s3
    li a1, 10
    call sb_byte
    j 89f
55:
    mv a0, s3
    la a1, h_rt
    call sb_cstr
    la t0, rt_err_code
    ld a1, 0(t0)
    mv a0, s3
    call sb_cstr
    mv a0, s3
    la a1, h_rt2
    call sb_cstr
    la t0, rt_err_msg
    ld a1, 0(t0)
    mv a0, s3
    call sb_str
    mv a0, s3
    li a1, 10
    call sb_byte
    la t0, rt_err_trace_txt     # spec 006: the frames and their values
    ld a1, 0(t0)
    beqz a1, 89f
    mv a0, s3
    call sb_str
89:
    lhu t0, N_FLAGS(s0)
    andi t0, t0, NF_PROP
    beqz t0, 90f
    mv a0, s3
    mv a1, s0
    mv a2, s1
    call prop_text
90:
    li a0, 1
    ld a1, SB_PTR(s3)
    ld a2, SB_LEN(s3)
    call out_bytes
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    addi sp, sp, 64
    ret

    .section .rodata
j_end_key: .string ",\"end\":"
    .text

# void run_file_tests(int file)
# Runs the file's tests in source order (only those whose name contains --filter).
    .globl run_file_tests
run_file_tests:
    addi sp, sp, -176           # 0..111: recovery buffer, 112: start ms
    sd ra, 168(sp)
    sd s0, 160(sp)
    sd s1, 152(sp)
    sd s2, 144(sp)
    sd s3, 136(sp)
    call file_get
    ld t0, FILE_AST(a0)
    ld s0, N_A(t0)              # items
    li s1, 0
1:
    ld t0, VEC_N(s0)
    bge s1, t0, 9f
    mv a0, s0
    mv a1, s1
    call vec_get
    addi s1, s1, 1
    lhu t0, N_KIND(a0)
    li t1, N_TEST
    bne t0, t1, 1b
    mv s2, a0                   # the test
    la t0, opt_filter
    ld a1, 0(t0)
    beqz a1, 2f
    ld a0, N_A(s2)
    li a2, 0
    call str_find_from
    bltz a0, 1b
2:
    call gc_start               # every test starts with an empty heap (spec 003 FR-009)
    la t0, tf_node
    sd zero, 0(t0)
    la t0, rt_err_code
    sd zero, 0(t0)
    la t0, rt_err_node
    sd zero, 0(t0)
    call sys_clock_ms
    sd a0, 112(sp)
    la t0, opt_timeout          # a deadline per test
    ld t0, 0(t0)
    la t1, deadline_ms
    sd zero, 0(t1)
    beqz t0, 3f
    li t2, 1000
    mul t0, t0, t2
    add t0, t0, a0
    sd t0, 0(t1)
3:
    lhu t0, N_FLAGS(s2)         # a property runs its own cases (prop.s)
    andi t0, t0, NF_PROP
    beqz t0, 31f
    mv a0, s2
    addi a1, s1, -1
    call prop_run
    mv s3, a0
    j 41f
31:
    addi a0, sp, 0
    la t0, recover_point
    sd a0, 0(t0)
    call jmp_save
    bnez a0, 4f
    li a0, 1                    # every test starts from seed and clock defaults
    call rt_effects_start
    mv a0, s2
    call run_test_body
    li a0, ST_PASS
4:
    mv s3, a0                   # 0 pass, 1 expect failed, 2 runtime error
41:
    la t0, recover_point
    sd zero, 0(t0)
    la t0, deadline_ms
    sd zero, 0(t0)
    call interp_reset
    li t0, 2
    bne s3, t0, 5f
    la t0, rt_err_code          # R0009 is a timeout, not an error
    ld t0, 0(t0)
    la t1, r0009
    bne t0, t1, 5f
    li s3, ST_TIMEOUT
5:
    la t0, t_passed
    li t1, ST_FAIL
    blt s3, t1, 6f
    la t0, t_failed
    beq s3, t1, 6f
    la t0, t_errored
    li t1, ST_ERROR
    beq s3, t1, 6f
    la t0, t_timed_out
6:
    ld t1, 0(t0)
    addi t1, t1, 1
    sd t1, 0(t0)
    call sys_clock_ms
    ld t0, 112(sp)
    sub a2, a0, t0
    mv a0, s2
    mv a1, s3
    call report_test
    li a0, 1                    # --mem-stats: right after the test's record
    la a1, s_cmd_test
    ld a2, N_A(s2)
    call gc_report
    la t0, tf_msg               # the test's leftovers point into a heap about to be reset; they
    sd zero, 0(t0)              # must not keep anything alive in the next test (spec 003)
    la t0, tf_expected
    sd zero, 0(t0)
    la t0, tf_actual
    sd zero, 0(t0)
    la t0, rt_err_msg
    sd zero, 0(t0)
    la t0, rt_err_extra
    sd zero, 0(t0)
    la t0, ret_val
    sd zero, 0(t0)
    la t0, pr_args
    sd zero, 0(t0)
    la t0, pr_best
    sd zero, 0(t0)
    la t0, pr_cand
    sd zero, 0(t0)
    j 1b
9:
    ld ra, 168(sp)
    ld s0, 160(sp)
    ld s1, 152(sp)
    ld s2, 144(sp)
    ld s3, 136(sp)
    addi sp, sp, 176
    ret

# void test_summary(long files)
    .globl test_summary
test_summary:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s1, a0
    call sb_new
    mv s0, a0
    la t0, opt_json
    ld t0, 0(t0)
    beqz t0, 5f
    la a1, x_passed             # JSON: the common summary plus the test counts
    call sb_cstr
    la t0, t_passed
    ld a1, 0(t0)
    mv a0, s0
    call int_to_dec
    mv a0, s0
    la a1, x_failed
    call sb_cstr
    la t0, t_failed
    ld a1, 0(t0)
    mv a0, s0
    call int_to_dec
    mv a0, s0
    la a1, x_errored
    call sb_cstr
    la t0, t_errored
    ld a1, 0(t0)
    mv a0, s0
    call int_to_dec
    mv a0, s0
    la a1, x_timed
    call sb_cstr
    la t0, t_timed_out
    ld a1, 0(t0)
    mv a0, s0
    call int_to_dec
    mv a0, s0
    call sb_finish
    mv a4, a0
    li a0, 1
    li a1, 1
    la a2, w_test
    mv a3, s1
    call summary_emit
    j 9f
5:
    la t0, t_passed             # human: "2 passed, 1 failed[, 1 errored][, 1 timed out]"
    ld a1, 0(t0)
    call int_to_dec
    mv a0, s0
    la a1, s_passed
    call sb_cstr
    la t0, t_failed
    ld a1, 0(t0)
    mv a0, s0
    call int_to_dec
    mv a0, s0
    la a1, s_failed
    call sb_cstr
    la t0, t_errored
    ld t1, 0(t0)
    beqz t1, 6f
    mv a0, s0
    la a1, s_comma
    call sb_cstr
    la t0, t_errored
    ld a1, 0(t0)
    mv a0, s0
    call int_to_dec
    mv a0, s0
    la a1, s_errored2
    call sb_cstr
6:
    la t0, t_timed_out
    ld t1, 0(t0)
    beqz t1, 7f
    mv a0, s0
    la a1, s_comma
    call sb_cstr
    la t0, t_timed_out
    ld a1, 0(t0)
    mv a0, s0
    call int_to_dec
    mv a0, s0
    la a1, s_timed
    call sb_cstr
7:
    mv a0, s0
    li a1, 10
    call sb_byte
    li a0, 1
    ld a1, SB_PTR(s0)
    ld a2, SB_LEN(s0)
    call out_bytes
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

    .section .rodata
w_test: .string "test"
