# Runtime support for the bytecode engine (src/vm.s): frames entered from native code, runtime
# errors and their reports, timeouts, structural equality, closures, run_main and test bodies.
# Programs run as bytecode since spec 004; the tree-walking evaluator is gone.
    .include "defs.inc"

    .section .bss
    .align 3
    .globl cur_frame
cur_frame:      .space 8
    .globl vsp
vsp:            .space 8
    .globl vbase
vbase:          .space 8
    .globl vstack_end
vstack_end:     .space 8
    .globl ret_val
ret_val:        .space 8
    .globl deadline_ms
deadline_ms:    .space 8
    .globl steps
steps:          .space 8
    .globl rt_budget
rt_budget:      .space 8            # work units left before rt_poll checks the clock
    .globl recover_point
recover_point:  .space 8            # set by the test runner: runtime errors jump here
    .globl rt_err_code
rt_err_code:    .space 8
    .globl rt_err_msg
rt_err_msg:     .space 8
    .globl rt_err_node
rt_err_node:    .space 8
    .globl rt_err_extra
rt_err_extra:   .space 8                # extra JSON fields for the report (R0010), or 0

    .section .rodata
r0001: .string "R0001"
r0002: .string "R0002"
r0003: .string "R0003"
r0004: .string "R0004"
r0005: .string "R0005"
r0006: .string "R0006"
    .globl r0009
r0009: .string "R0009"
m_overflow:     .string "Integer overflow."
m_divzero:      .string "Division by zero."
m_index:        .string "Index $1 is out of bounds for length $2."
m_key:          .string "Key $1 is not in the map."
m_stack:        .string "Stack exhausted: the recursion is too deep."
m_oom:          .string "Out of memory: the 512 MiB limit is reached."
m_timeout:      .string "Time limit of $1 s exceeded."
s_rt_err:       .string ": runtime error["
s_trace:        .string "trace:\n"
s_at:           .string "  at "
s_more:         .string "  ... more frames omitted\n"
s_rt_head:      .string "{\"v\":1,\"kind\":\"runtime_error\",\"code\":\""
s_name_key:     .string "\",\"name\":\""
s_msg_key:      .string "\",\"message\":"
s_file_key:     .string ",\"file\":"
s_trace_key:    .string ",\"trace\":["
s_fn_key:       .string "{\"function\":"
s_omitted:      .string ",\"trace_omitted\":"
s_test_prefix:  .string "test "
s_quote:        .string "\""
s_cmd_run:      .string "run"

    .text
# void gc_start(void)
# From here on, values live in the collected heap (spec 003).
    .globl gc_start
gc_start:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd zero, 0(sp)              # no stale word above the scrubbed part either
    la t0, steps                # deadline checks at the same steps whatever ran before (their
    sd zero, 0(t0)              # stack writes decide which stale words stay; spec 003 FR-009)
    call stack_scrub
    call gc_init
    la t0, gc_on
    li t1, 1
    sd t1, 0(t0)
    ld ra, 8(sp)
    addi sp, sp, 16
    ret

# void interp_init(void)
    .globl interp_init
interp_init:
    addi sp, sp, -16
    sd ra, 8(sp)
    la t0, vsp
    ld t1, 0(t0)
    bnez t1, 1f
    li a0, ADDR_VSTACK          # its own fixed region, outside the permanent arena (spec 003)
    li a1, VSTACK_SIZE
    li a2, 0
    call map_fixed
    la t0, vsp
    sd a0, 0(t0)
    la t0, vbase
    sd a0, 0(t0)
    li t1, VSTACK_SIZE
    add t1, a0, t1
    la t0, vstack_end
    sd t1, 0(t0)
1:
    la t0, oom_hook
    la t1, rt_oom
    sd t1, 0(t0)
    ld ra, 8(sp)
    addi sp, sp, 16
    ret

    .section .rodata
m_oom_live:     .string "The program holds $1 of live data, over the $2 limit."
s_in_use_key:   .string ",\"in_use\":"
s_limit_key:    .string ",\"limit\":"
    .text
# void rt_oom_live(long in_use)
# Live data passes the --max-memory limit (spec 003 US3): R0006 with in_use and limit. The report
# itself allocates from the permanent arena (the collector is switched off for it).
    .globl rt_oom_live
rt_oom_live:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    mv s0, a0
    la t0, gc_on
    sd zero, 0(t0)
    call sample_peak
    la t0, oom_hook             # if even the arena runs out, the plain handler
    la t1, oom_default
    sd t1, 0(t0)
    call sb_new
    mv s1, a0
    mv a1, s0
    call size_into_up
    call sb_new
    mv s2, a0
    la t0, gc_limit
    ld a1, 0(t0)
    call size_into
    mv a0, s1
    call sb_finish
    mv s1, a0
    mv a0, s2
    call sb_finish
    mv a2, a0
    la a0, m_oom_live
    mv a1, s1
    li a3, 0
    call msg
    mv s1, a0
    call sb_new
    mv s2, a0
    la a1, s_in_use_key
    call sb_cstr
    mv a0, s2
    mv a1, s0
    call int_to_dec
    mv a0, s2
    la a1, s_limit_key
    call sb_cstr
    la t0, gc_limit
    ld a1, 0(t0)
    mv a0, s2
    call int_to_dec
    mv a0, s2
    call sb_finish
    la t0, rt_err_extra
    sd a0, 0(t0)
    la a0, r0006
    mv a1, s1
    la t0, cur_node
    ld a2, 0(t0)
    call rt_raise
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    addi sp, sp, 48
    ret

# void rt_oom(void)
    .globl rt_oom
rt_oom:
    addi sp, sp, -16
    sd ra, 8(sp)
    la t0, oom_hook             # reporting allocates: fall back to the plain handler if that fails too
    la t1, oom_default
    sd t1, 0(t0)
    la t0, arena_cap            # and give the report itself 64 MiB of headroom
    ld t1, 0(t0)
    li t2, 0x4000000
    add t1, t1, t2
    sd t1, 0(t0)
    la a0, m_oom
    call str_from_cstr
    mv a1, a0
    la a0, r0006
    la t0, cur_node
    ld a2, 0(t0)
    call rt_raise
    ld ra, 8(sp)
    addi sp, sp, 16
    ret

    .section .bss
    .align 3
    .globl cur_node
cur_node:   .space 8                # the expression being evaluated (for out-of-memory reports)
    .text

# void *frame_fn_name(void *frame)
# The function name for a trace entry (tests show as test "name").
    .globl frame_fn_name
frame_fn_name:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    ld s0, FR_FN(a0)
    lhu t0, N_KIND(s0)
    li t1, N_TEST
    beq t0, t1, 1f
    ld t0, N_A(s0)
    ld a0, N_A(t0)
    j 9f
1:
    call sb_new
    sd a0, 0(sp)
    la a1, s_test_prefix
    call sb_cstr
    ld a0, 0(sp)
    la a1, s_quote
    call sb_cstr
    ld a0, 0(sp)
    ld a1, N_A(s0)
    call sb_str
    ld a0, 0(sp)
    la a1, s_quote
    call sb_cstr
    ld a0, 0(sp)
    call sb_finish
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    addi sp, sp, 32
    ret

# void rt_raise(char *code, void *message, void *node)
# Stops the program with a runtime error report (or returns to the test runner).
    .globl rt_raise
rt_raise:
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
    mv s2, a2
    la t0, rt_err_code
    sd s0, 0(t0)
    la t0, rt_err_msg
    sd s1, 0(t0)
    la t0, rt_err_node
    sd s2, 0(t0)
    la t0, recover_point
    ld a0, 0(t0)
    beqz a0, 1f
    la t0, rt_err_extra
    sd zero, 0(t0)
    li a1, 2                    # inside a test: let the runner record it
    call jmp_restore
1:
    call sb_new
    mv s3, a0
    la t0, opt_json
    ld t0, 0(t0)
    bnez t0, 50f
    mv a0, s3                   # human report
    lwu a1, N_FILE(s2)
    lwu a2, N_START(s2)
    call human_loc
    mv a0, s3
    la a1, s_rt_err
    call sb_cstr
    mv a0, s3
    mv a1, s0
    call sb_cstr
    mv a0, s3
    li a1, ']'
    call sb_byte
    mv a0, s3
    li a1, ':'
    call sb_byte
    mv a0, s3
    li a1, ' '
    call sb_byte
    mv a0, s3
    mv a1, s1
    call sb_str
    mv a0, s3
    li a1, 10
    call sb_byte
    mv a0, s3
    lwu a1, N_FILE(s2)
    lwu a2, N_START(s2)
    lwu a3, N_END(s2)
    call human_snippet
    mv a0, s3
    la a1, s_trace
    call sb_cstr
    la t0, cur_frame
    ld s4, 0(t0)
    mv s5, s2                   # location in the current frame
    li t0, 0
    sd t0, 0(sp)                # frames written
2:
    beqz s4, 8f
    ld t0, 0(sp)
    li t1, 10
    bge t0, t1, 7f
    addi t0, t0, 1
    sd t0, 0(sp)
    mv a0, s3
    la a1, s_at
    call sb_cstr
    mv a0, s4
    call frame_fn_name
    mv a1, a0
    mv a0, s3
    call sb_str
    mv a0, s3
    li a1, ' '
    call sb_byte
    mv a0, s3
    li a1, '('
    call sb_byte
    mv a0, s3
    lwu a1, N_FILE(s5)
    lwu a2, N_START(s5)
    call human_loc
    mv a0, s3
    li a1, ')'
    call sb_byte
    mv a0, s3
    li a1, 10
    call sb_byte
    ld s5, FR_CALL(s4)
    ld s4, FR_PARENT(s4)
    beqz s5, 8f
    j 2b
7:
    mv a0, s3
    la a1, s_more
    call sb_cstr
    j 8f
50:                             # JSON report
    mv a0, s3
    la a1, s_rt_head
    call sb_cstr
    mv a0, s3
    mv a1, s0
    call sb_cstr
    mv a0, s3
    la a1, s_name_key
    call sb_cstr
    mv a0, s0
    call catalog_name
    mv a1, a0
    mv a0, s3
    call sb_str
    mv a0, s3
    la a1, s_msg_key
    call sb_cstr
    mv a0, s3
    mv a1, s1
    call json_str_into
    mv a0, s3
    la a1, s_file_key
    call sb_cstr
    mv a0, s3
    lwu a1, N_FILE(s2)
    lwu a2, N_START(s2)
    lwu a3, N_END(s2)
    call json_span
    la t0, rt_err_extra
    ld a1, 0(t0)
    beqz a1, 501f
    mv a0, s3
    call sb_str
501:
    mv a0, s3
    la a1, s_trace_key
    call sb_cstr
    la t0, cur_frame
    ld s4, 0(t0)
    mv s5, s2
    sd zero, 0(sp)
51:
    beqz s4, 58f
    ld t0, 0(sp)
    li t1, 10
    bge t0, t1, 57f
    beqz t0, 52f
    mv a0, s3
    li a1, ','
    call sb_byte
52:
    ld t0, 0(sp)
    addi t0, t0, 1
    sd t0, 0(sp)
    mv a0, s3
    la a1, s_fn_key
    call sb_cstr
    mv a0, s4
    call frame_fn_name
    mv a1, a0
    mv a0, s3
    call json_str_into
    mv a0, s3
    la a1, s_file_key
    call sb_cstr
    lwu a0, N_FILE(s5)
    call file_get
    ld a1, FILE_PATH(a0)
    mv a0, s3
    call json_str_into
    mv a0, s3
    la a1, s_start_key
    call sb_cstr
    mv a0, s3
    lwu a1, N_FILE(s5)
    lwu a2, N_START(s5)
    call json_pos
    mv a0, s3
    li a1, '}'
    call sb_byte
    ld s5, FR_CALL(s4)
    ld s4, FR_PARENT(s4)
    beqz s5, 58f
    j 51b
57:
    li t0, 0                    # count what was left out
53:
    beqz s4, 54f
    addi t0, t0, 1
    ld s4, FR_PARENT(s4)
    j 53b
54:
    sd t0, 8(sp)
    mv a0, s3
    li a1, ']'
    call sb_byte
    mv a0, s3
    la a1, s_omitted
    call sb_cstr
    mv a0, s3
    ld a1, 8(sp)
    call int_to_dec
    mv a0, s3
    li a1, '}'
    call sb_byte
    j 59f
58:
    mv a0, s3
    li a1, ']'
    call sb_byte
    mv a0, s3
    li a1, '}'
    call sb_byte
59:
    mv a0, s3
    li a1, 10
    call sb_byte
8:
    li a0, 2
    ld a1, SB_PTR(s3)
    ld a2, SB_LEN(s3)
    call out_bytes
    li a0, EXIT_RUNTIME
    la a1, r0009
    mv t0, s0
    lbu t1, 4(t0)               # R0009 -> 124
    li t2, '9'
    bne t1, t2, 9f
    lbu t1, 3(t0)
    li t2, '0'
    bne t1, t2, 9f
    li a0, EXIT_TIMEOUT
9:
    sd a0, 0(sp)
    li a0, 2                    # --mem-stats: the run ends here
    la a1, s_cmd_run
    li a2, 0
    call gc_report
    ld a0, 0(sp)
    call vo_exit
    ld ra, 72(sp)
    ld s0, 64(sp)
    ld s1, 56(sp)
    ld s2, 48(sp)
    ld s3, 40(sp)
    ld s4, 32(sp)
    ld s5, 24(sp)
    addi sp, sp, 80
    ret

    .section .rodata
s_start_key: .string ",\"start\":"
    .text

# void rt_raise_c(char *code, char *message, void *node)
    .globl rt_raise_c
rt_raise_c:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    mv s1, a2
    mv a0, a1
    call str_from_cstr
    mv a1, a0
    mv a0, s0
    mv a2, s1
    call rt_raise
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void rt_div_zero(void *node)
# R0002 at node (the bytecode engine's division handlers).
    .globl rt_div_zero
rt_div_zero:
    mv a2, a0
    la a0, r0002
    la a1, m_divzero
    tail rt_raise_c

# void rt_stack_full(void *node)
# R0005 at node: a new frame does not fit the value stack.
    .globl rt_stack_full
rt_stack_full:
    mv a2, a0
    la a0, r0005
    la a1, m_stack
    tail rt_raise_c

# void rt_overflow(void *node)
    .globl rt_overflow
rt_overflow:
    mv a2, a0
    la a0, r0001
    la a1, m_overflow
    tail rt_raise_c

# void rt_index_error(void *node, long index, long length)
    .globl rt_index_error
rt_index_error:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    sd s2, 0(sp)
    mv s0, a0
    mv s2, a2
    mv a0, a1
    call int_to_str
    mv s1, a0
    mv a0, s2
    call int_to_str
    mv a2, a0
    mv a1, s1
    la a0, m_index
    li a3, 0
    call msg
    mv a1, a0
    la a0, r0003
    mv a2, s0
    call rt_raise
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    ld s2, 0(sp)
    addi sp, sp, 32
    ret

# void rt_key_error(void *node, long key, void *key_type)
    .globl rt_key_error
rt_key_error:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    mv s1, a1
    sd a2, 0(sp)                # key type
    call sb_new
    ld a2, 0(sp)
    sd a0, 0(sp)
    mv a1, s1
    call render_value
    ld a0, 0(sp)
    call sb_finish
    mv a1, a0
    la a0, m_key
    li a2, 0
    li a3, 0
    call msg
    mv a1, a0
    la a0, r0004
    mv a2, s0
    call rt_raise
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void tick(void *node)
# Counts a step; every 65,536 steps compares the clock with the deadline (R0009).
    .globl tick
tick:
    la t0, steps
    ld t1, 0(t0)
    addi t1, t1, 1
    sd t1, 0(t0)
    li t2, 0xFFFF
    and t1, t1, t2
    bnez t1, 9f
    la t0, deadline_ms
    ld t0, 0(t0)
    beqz t0, 9f
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    mv s0, a0
    call sys_clock_ms
    la t0, deadline_ms
    ld t0, 0(t0)
    blt a0, t0, 8f
    mv a0, s0
    call rt_timeout
8:
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
9:
    ret

# void rt_poll(void)
# The POLL budget ran out: refills it and stops with R0009 when the deadline has passed. The
# collector may be in the middle of a pass, so the report allocates from the permanent arena
# (the heap is rebuilt by the next gc_start).
    .globl rt_poll
rt_poll:
    la t0, rt_budget
    li t1, BUDGET_UNITS
    sd t1, 0(t0)
    la t0, deadline_ms
    ld t0, 0(t0)
    bnez t0, 1f
    ret
1:
    addi sp, sp, -16
    sd ra, 8(sp)
    call sys_clock_ms
    la t0, deadline_ms
    ld t0, 0(t0)
    blt a0, t0, 9f
    la t0, cur_node
    ld a0, 0(t0)
    beqz a0, 9f                 # no location yet: the next tick reports it
    la t0, gc_on
    sd zero, 0(t0)
    call rt_timeout
9:
    ld ra, 8(sp)
    addi sp, sp, 16
    ret

# void rt_timeout(void *node)
    .globl rt_timeout
rt_timeout:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    mv s0, a0
    la t0, deadline_ms          # one report: the polls inside it must not raise again
    sd zero, 0(t0)
    la t0, opt_timeout
    ld a0, 0(t0)
    call int_to_str
    mv a1, a0
    la a0, m_timeout
    li a2, 0
    li a3, 0
    call msg
    mv a1, a0
    la a0, r0009
    mv a2, s0
    call rt_raise
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret

# void check_stack(void *node)
# R0005 when the native stack is nearly used up.
    .globl check_stack
check_stack:
    la t0, stack_floor
    ld t0, 0(t0)
    bgeu sp, t0, 1f
    mv a2, a0
    la a0, r0005
    la a1, m_stack
    tail rt_raise_c
1:
    ret

# long *local_addr(long slot)
    .globl local_addr
local_addr:
    la t0, cur_frame
    ld t0, 0(t0)
    slli a0, a0, 3
    add a0, a0, t0
    addi a0, a0, FR_SLOTS
    ret

# long call_fn(void *fn, void *call, void *args, long *vals, void *closure)
# Calls a user function from native code (run_main, builtin callbacks): a frame for its compiled
# code, the arguments from vals (args is always 0 since spec 004), a step, then the bytecode
# engine. A closure adds its captured values and type arguments (vm_push).
    .globl call_fn
call_fn:
    addi sp, sp, -64
    sd ra, 56(sp)
    sd s0, 48(sp)
    sd s1, 40(sp)
    sd s2, 32(sp)
    sd s3, 24(sp)
    sd s4, 16(sp)
    sd s5, 8(sp)
    mv s0, a0                   # fn
    mv s1, a1                   # call node | 0
    mv s4, a3                   # argument values
    mv s5, a4                   # closure | 0
    beqz s1, 1f
    mv a0, s1
    call check_stack
1:
    ld t0, N_A(s0)
    ld t0, N_B(t0)
    ld s2, FX_CODE(t0)
    bnez s2, 2f
    call vm_not_compiled        # (every function is compiled before the program runs)
2:
    la t0, vsp
    ld s3, 0(t0)                # the value stack goes back here afterwards
    mv a0, s2
    mv a1, s0
    mv a2, s1
    mv a3, s4
    mv a4, s5
    call vm_push
    mv s2, a0
    beqz s1, 3f                 # count the call as a step
    mv a0, s1
    call tick
3:
    mv a0, s2
    call vm_run
    la t0, vsp
    sd s3, 0(t0)
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    ld s5, 8(sp)
    addi sp, sp, 64
    ret

# int values_equal(long a, long b, void *type)
# Structural equality (floats by IEEE rules).
    .globl values_equal
values_equal:
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
    la t6, rt_budget          # POLL 1: --timeout budget
    ld t5, 0(t6)
    addi t5, t5, -1
    sd t5, 0(t6)
    bgtz t5, 1000f
    call rt_poll
1000:
    mv a0, s2                   # type parameters: the running frame's type arguments
    call rt_concrete
    mv s2, a0
    mv a0, s0                   # the values as they arrived (str_eq below takes them)
    mv a1, s1
    ld t0, TY_KIND(s2)
    li t1, TY_FLOAT
    beq t0, t1, 1f
    li t1, TY_STR
    beq t0, t1, 2f
    li t1, TY_LIST
    beq t0, t1, 3f
    li t1, TY_MAP
    beq t0, t1, 4f
    li t1, TY_RECORD
    beq t0, t1, 5f
    li t1, TY_VARIANT
    beq t0, t1, 6f
    li t1, TY_RESULT
    beq t0, t1, 7f
    sub t0, s0, s1              # int, bool, void
    seqz a0, t0
    j 99f
1:
    fmv.d.x ft0, s0
    fmv.d.x ft1, s1
    feq.d a0, ft0, ft1
    j 99f
2:
    call str_eq
    j 99f
3:
    ld t0, L_LEN(s0)
    ld t1, L_LEN(s1)
    bne t0, t1, 98f
    li s3, 0
31:
    ld t0, L_LEN(s0)
    bge s3, t0, 97f
    ld t0, L_DATA(s0)
    ld t1, L_DATA(s1)
    slli t2, s3, 3
    add t0, t0, t2
    add t1, t1, t2
    ld a0, 0(t0)
    ld a1, 0(t1)
    ld a2, TY_A(s2)
    call values_equal
    beqz a0, 98f
    addi s3, s3, 1
    j 31b
4:
    ld t0, M_COUNT(s0)
    ld t1, M_COUNT(s1)
    bne t0, t1, 98f
    li s3, 0
41:
    ld t0, M_USED(s0)
    bge s3, t0, 97f
    ld t0, M_ENTRIES(s0)
    li t1, ME_SIZE
    mul t1, t1, s3
    add s4, t0, t1
    addi s3, s3, 1
    lwu t0, ME_LIVE(s4)
    beqz t0, 41b
    mv a0, s1
    ld a1, ME_KEY(s4)
    ld a2, TY_A(s2)
    call map_find
    bltz a0, 98f
    ld t0, M_ENTRIES(s1)
    li t1, ME_SIZE
    mul t1, t1, a0
    add t0, t0, t1
    ld a1, ME_VAL(t0)
    ld a0, ME_VAL(s4)
    ld a2, TY_B(s2)
    call values_equal
    beqz a0, 98f
    j 41b
5:
    ld t0, TY_DECL(s2)
    ld s4, N_B(t0)              # fields
    li s3, 0
51:
    mv a0, s4
    mv a1, s3
    call member_at
    beqz a0, 97f
    ld t0, N_B(a0)
    ld a1, N_TYPE(t0)
    mv a0, s2
    call field_type_in
    mv a2, a0
    slli t0, s3, 3
    add t1, s0, t0
    ld a0, R_FIELDS(t1)
    add t1, s1, t0
    ld a1, R_FIELDS(t1)
    call values_equal
    beqz a0, 98f
    addi s3, s3, 1
    j 51b
6:
    lwu t0, V_TAG(s0)
    lwu t1, V_TAG(s1)
    bne t0, t1, 98f
    ld t2, TY_DECL(s2)
    ld a0, N_B(t2)
    mv a1, t0
    call member_at
    ld t0, N_B(a0)              # payload type node
    beqz t0, 97f
    ld a1, N_TYPE(t0)
    mv a0, s2
    call field_type_in
    mv a2, a0
    ld a0, V_PAY(s0)
    ld a1, V_PAY(s1)
    call values_equal
    j 99f
7:
    lwu t0, V_TAG(s0)
    lwu t1, V_TAG(s1)
    bne t0, t1, 98f
    mv a0, s2
    mv a1, t0
    call case_payload
    beqz a0, 97f
    mv a2, a0
    ld a0, V_PAY(s0)
    ld a1, V_PAY(s1)
    call values_equal
    j 99f
97:
    li a0, 1
    j 99f
98:
    li a0, 0
99:
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    addi sp, sp, 64
    ret

# int run_main(void *main_fn)
# Runs `main`; returns the process exit status (main returning err prints "error: msg", status 1).
    .globl run_main
run_main:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    call interp_init
    call gc_start
    la t0, opt_timeout
    ld t0, 0(t0)
    beqz t0, 1f
    li t1, 1000
    mul s1, t0, t1
    call sys_clock_ms
    add a0, a0, s1
    la t0, deadline_ms
    sd a0, 0(t0)
1:
    mv a0, s0
    li a1, 0
    li a2, 0
    li a3, 0
    li a4, 0
    call call_fn
    mv s1, a0
    ld t0, N_TYPE(s0)
    ld t0, TY_KIND(t0)
    li t1, TY_RESULT
    bne t0, t1, 8f
    lwu t0, V_TAG(s1)
    beqz t0, 8f
    li a0, 2
    la a1, s_error_prefix
    call out_cstr
    li a0, 2
    ld a1, V_PAY(s1)
    call out_str
    li a0, 2
    la a1, s_nl
    call out_cstr
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

    .section .rodata
s_error_prefix: .string "error: "
s_nl: .string "\n"

    .text
# void interp_reset(void)
# Back to an empty call stack (after a test ends, normally or through its recovery point).
    .globl interp_reset
interp_reset:
    la t0, cur_frame
    sd zero, 0(t0)
    la t0, vbase
    ld t1, 0(t0)
    la t0, vsp
    sd t1, 0(t0)
    ret

# long call_value(void *closure, void *call, long *vals)
# Calls a function value with argument values.
    .globl call_value
call_value:
    ld t0, CL_FN(a0)            # compiled (spec 004): straight into the bytecode engine
    ld t0, N_A(t0)
    ld t0, N_B(t0)
    ld t0, FX_CODE(t0)
    bnez t0, 1f
    mv a3, a2
    mv a4, a0
    ld a0, CL_FN(a0)
    li a2, 0
    tail call_fn
1:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0                   # closure
    mv s1, a1                   # call node
    mv s2, a2                   # argument values
    beqz s1, 2f                 # (as call_fn: the native stack, then the frame, then a step)
    mv a0, s1
    call check_stack
2:
    la t0, vsp
    ld s3, 0(t0)
    ld a1, CL_FN(s0)
    ld t0, N_A(a1)
    ld t0, N_B(t0)
    ld a0, FX_CODE(t0)
    mv a2, s1
    mv a3, s2
    mv a4, s0
    call vm_push
    mv s2, a0
    beqz s1, 3f
    mv a0, s1
    call tick
3:
    mv a0, s2
    call vm_run
    la t0, vsp
    sd s3, 0(t0)
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# void *make_closure(void *lambda)
# An anonymous function as a value: its node, and copies of the values it captures.
    .globl make_closure
make_closure:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    ld t0, N_A(s0)
    ld t0, N_B(t0)
    ld s1, FX_CAPS(t0)          # capture entries
    li s2, 0
    beqz s1, 1f
    ld s2, VEC_N(s1)
1:
    li a0, OBJ_CLOSURE
    slli a1, s2, 3
    addi a1, a1, CL_CAPS - 8
    call alloc_obj
    mv s3, a0
    sd s0, CL_FN(s3)
    sd s2, CL_N(s3)
    la t0, cur_frame
    ld t0, 0(t0)
    ld t0, FR_TARGS(t0)
    sd t0, CL_TARGS(s3)
    sd zero, 0(sp)
2:
    ld t0, 0(sp)
    bge t0, s2, 9f
    addi t1, t0, 1
    sd t1, 0(sp)
    mv a0, s1
    mv a1, t0
    call vec_get
    ld a0, CP_OUTER(a0)
    call local_addr
    ld t1, 0(a0)
    ld t0, 0(sp)
    addi t0, t0, -1
    slli t0, t0, 3
    add t0, s3, t0
    sd t1, CL_CAPS(t0)
    j 2b
9:
    mv a0, s3
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# void *fn_closure(void *fn)
# A named function as a value (made once per function).
    .globl fn_closure
fn_closure:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    ld t0, N_A(s0)
    ld s1, N_B(t0)              # FX
    ld a0, FX_CLOSURE(s1)
    bnez a0, 9f
    call perm_begin             # cached in the function's node: must outlive every test
    li a0, OBJ_CLOSURE
    li a1, CL_CAPS - 8
    call alloc_obj
    sd s0, CL_FN(a0)
    sd a0, FX_CLOSURE(s1)
    call perm_end
    ld a0, FX_CLOSURE(s1)
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void run_test_body(void *test)
# Runs a test block's compiled code in a fresh frame (the test's declared effects are its grant).
    .globl run_test_body
run_test_body:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    mv s0, a0
    ld t0, N_D(s0)
    beqz t0, 1f
    ld a0, FX_CODE(t0)
    bnez a0, 2f
1:
    call vm_not_compiled        # (every test is compiled before the tests run)
2:
    ld t0, N_D(s0)
    ld a0, FX_CODE(t0)
    mv a1, s0
    li a2, 0
    li a3, 0
    li a4, 0
    call vm_push
    call vm_run
    call interp_reset
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret
