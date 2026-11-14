# Traces and frame values (spec 006 US3, US4): the tracing handler table, trace records, and the
# parameters and locals shown in runtime errors (research R11-R14).
#
# With --trace, compile_program copies its handlers from vm_ops_trace instead of vm_ops: a copy of
# vm_ops whose OPC_RET and OPC_TRY entries write a `return` record first, and whose
# OPC_TRACE_ENTER entry (emitted at the start of every function only when tracing) writes the
# `call` record. Runs without --trace execute exactly the code they did before (FR-016).
# ponytail: 100,000 records by default, then only counted; a per-function filter if traces of
# long runs need more than the first records.
    .include "defs.inc"

    .equ TRACE_DEFAULT_LIMIT, 100000

    .section .data
    .align 3
    .globl tr_on
tr_on:      .dword 0            # 1 with --trace (the compiler emits OPC_TRACE_ENTER)
tr_fb:      .dword 0            # the trace file (FB_*), or 0 when it could not be created
tr_failed:  .dword 0            # 1 when it could not be created (T0009 at exit)
tr_limit:   .dword 0
tr_written: .dword 0
tr_omitted: .dword 0
tr_depth:   .dword 0
tr_is_test: .dword 0

    .section .bss
    .align 3
    .globl vm_ops_trace
vm_ops_trace: .space OPC_COUNT * 8

    .section .rodata
j_call_head: .string "{\"v\":1,\"kind\":\"call\",\"depth\":"
j_ret_head: .string "{\"v\":1,\"kind\":\"return\",\"depth\":"
j_test_head: .string "{\"v\":1,\"kind\":\"test\",\"name\":"
j_err_head: .string "{\"v\":1,\"kind\":\"error\",\"code\":\""
j_end_head: .string "{\"v\":1,\"kind\":\"end\",\"records\":"
j_function: .string ",\"function\":"
j_file:     .string ",\"file\":"
j_start:    .string ",\"start\":"
j_args:     .string ",\"args\":["
j_name:     .string "{\"name\":"
j_value:    .string ",\"value\":"
j_name_key: .string "\",\"name\":\""
j_message:  .string ",\"message\":"
j_omitted:  .string ",\"omitted\":"
j_status:   .string ",\"status\":"
j_close_nl: .string "}\n"
j_arr_close: .string "]}\n"
code_t9:    .string "T0009"
m_unwritable: .string "Cannot write the trace or recording `$1`."

    .text
# void trace_start(long is_test)
# With --trace: opens the trace file and makes compile_program use the tracing handlers.
    .globl trace_start
trace_start:
    la t0, opt_trace
    ld t0, 0(t0)
    beqz t0, 9f
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    mv s0, t0
    la t1, tr_is_test
    sd a0, 0(t1)
    la t1, tr_on
    li t2, 1
    sd t2, 0(t1)
    la t1, opt_trace_limit
    ld t2, 0(t1)
    bnez t2, 1f
    li t2, TRACE_DEFAULT_LIMIT
1:
    la t1, tr_limit
    sd t2, 0(t1)
    mv a0, s0
    call fbuf_open
    la t1, tr_fb
    sd a0, 0(t1)
    bnez a0, 2f
    la t1, tr_failed
    li t2, 1
    sd t2, 0(t1)
2:
    la t0, vm_ops               # vm_ops_trace: vm_ops with the tracing handlers
    la t1, vm_ops_trace
    li t2, OPC_COUNT
3:
    ld t3, 0(t0)
    sd t3, 0(t1)
    addi t0, t0, 8
    addi t1, t1, 8
    addi t2, t2, -1
    bnez t2, 3b
    la t1, vm_ops_trace
    la t0, op_trace_ret
    sd t0, OPC_RET * 8(t1)
    la t0, op_trace_try
    sd t0, OPC_TRY * 8(t1)
    la t0, op_trace_enter
    sd t0, OPC_TRACE_ENTER * 8(t1)
    la t0, c_ops
    sd t1, 0(t0)
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
9:
    ret

# ---- the tracing handlers (reached through vm_ops_trace, inside vm_run: s1 pc, s2 frame,
# s3 slots; each continues like the handler it stands for). They are VM handlers, not functions:
# they advance vm_run's pc in s1 on purpose, hence `sesi: allow clobbers_callee_saved`. ----

# void op_trace_enter(void) sesi: allow clobbers_callee_saved
    .globl op_trace_enter
op_trace_enter:
    mv a0, s2
    call tr_call
    addi s1, s1, I_SIZE
    ld t6, I_HANDLER(s1)
    jr t6

# void op_trace_ret(void) sesi: allow clobbers_callee_saved
    .globl op_trace_ret
op_trace_ret:
    lw t0, I_B(s1)
    li a1, 0
    bltz t0, 1f
    add t0, s3, t0
    ld a1, 0(t0)
1:
    mv a0, s2
    call tr_return
    la t6, vm_ops               # continue in op_ret (through vm_ops: vm.s exports no handler)
    ld t6, OPC_RET * 8(t6)
    jr t6

# void op_trace_try(void) sesi: allow clobbers_callee_saved
    .globl op_trace_try
op_trace_try:
    lw t1, I_B(s1)
    add t1, s3, t1
    ld a1, 0(t1)
    lwu t2, V_TAG(a1)
    beqz t2, 1f
    mv a0, s2                   # an err: the function returns it
    call tr_return
1:
    la t6, vm_ops               # continue in op_try
    ld t6, OPC_TRY * 8(t6)
    jr t6

# ---- records ----

# long tr_room(void)
# 1 when another record may be written; otherwise counts it as omitted and returns 0.
    .globl tr_room
tr_room:
    la t0, tr_written
    ld t1, 0(t0)
    la t2, tr_limit
    ld t2, 0(t2)
    bge t1, t2, 1f
    addi t1, t1, 1
    sd t1, 0(t0)
    li a0, 1
    ret
1:
    la t0, tr_omitted
    ld t1, 0(t0)
    addi t1, t1, 1
    sd t1, 0(t0)
    li a0, 0
    ret

# void tr_emit(void *sb)
    .globl tr_emit
tr_emit:
    la t0, tr_fb
    ld t0, 0(t0)
    beqz t0, 9f
    ld a1, SB_PTR(a0)
    ld a2, SB_LEN(a0)
    mv a0, t0
    tail fbuf_write
9:
    ret

# void tr_value(void *sb, long value, void *type)
# A value in vo syntax (shortened to 200 bytes) as a JSON string.
    .globl tr_value
tr_value:
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
    mv a1, s1
    mv a2, s2
    call render_capped
    mv a0, s0
    li a1, '"'
    call sb_byte
    mv a0, s0
    ld a1, SB_PTR(s3)
    ld a2, SB_LEN(s3)
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

# void *tr_params(void *decl)
# The parameter vec of a function, lambda or property; 0 for a test.
    .globl tr_params
tr_params:
    lhu t0, N_KIND(a0)
    li t1, N_TEST
    bne t0, t1, 1f
    lhu t0, N_FLAGS(a0)
    andi t0, t0, NF_PROP
    beqz t0, 2f
    ld t0, N_D(a0)
    ld a0, FX_PARAMS(t0)
    ret
1:
    ld a0, N_B(a0)
    ret
2:
    li a0, 0
    ret

# void tr_call(void *frame)
# The `call` record of a frame that starts running (contracts/trace.md).
    .globl tr_call
tr_call:
    addi sp, sp, -64
    sd ra, 56(sp)
    sd s0, 48(sp)
    sd s1, 40(sp)
    sd s2, 32(sp)
    sd s3, 24(sp)
    sd s4, 16(sp)
    mv s0, a0
    la t0, tr_depth
    ld t1, 0(t0)
    addi t1, t1, 1
    sd t1, 0(t0)
    call tr_room
    beqz a0, 9f
    call sb_new
    mv s1, a0
    la a1, j_call_head
    call sb_cstr
    la t0, tr_depth
    ld a1, 0(t0)
    mv a0, s1
    call int_to_dec
    mv a0, s1
    la a1, j_function
    call sb_cstr
    mv a0, s0
    call frame_fn_name
    mv a1, a0
    mv a0, s1
    call json_str_into
    ld s2, FR_CALL(s0)          # the call site
    beqz s2, 1f
    mv a0, s1
    la a1, j_file
    call sb_cstr
    lwu a0, N_FILE(s2)
    call file_get
    ld a1, FILE_PATH(a0)
    mv a0, s1
    call json_str_into
    mv a0, s1
    la a1, j_start
    call sb_cstr
    mv a0, s1
    lwu a1, N_FILE(s2)
    lwu a2, N_START(s2)
    call json_pos
1:
    mv a0, s1
    la a1, j_args
    call sb_cstr
    ld a0, FR_FN(s0)
    call tr_params
    mv s2, a0
    li s3, 0
    beqz s2, 4f
2:
    ld t0, VEC_N(s2)
    bge s3, t0, 4f
    beqz s3, 3f
    mv a0, s1
    li a1, ','
    call sb_byte
3:
    mv a0, s2
    mv a1, s3
    call vec_get
    mv s4, a0                   # N_PARAM: name node, slot, type
    mv a0, s1
    la a1, j_name
    call sb_cstr
    ld t0, N_A(s4)
    ld a1, N_A(t0)
    mv a0, s1
    call json_str_into
    mv a0, s1
    la a1, j_value
    call sb_cstr
    ld t0, N_E(s4)
    slli t0, t0, 3
    add t0, s0, t0
    ld a1, FR_SLOTS(t0)
    ld a2, N_TYPE(s4)
    mv a0, s1
    call tr_value
    mv a0, s1
    li a1, '}'
    call sb_byte
    addi s3, s3, 1
    j 2b
4:
    mv a0, s1
    la a1, j_arr_close
    call sb_cstr
    mv a0, s1
    call tr_emit
9:
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    addi sp, sp, 64
    ret

# void *tr_ret_type(void *decl)
# The return type of a function or lambda; 0 when it returns nothing (and for tests).
    .globl tr_ret_type
tr_ret_type:
    lhu t0, N_KIND(a0)
    li t1, N_TEST
    beq t0, t1, 8f
    ld t2, N_TYPE(a0)
    li t1, N_LAMBDA
    bne t0, t1, 1f
    beqz t2, 8f                 # a lambda: its function type's result
    ld t2, TY_B(t2)
1:
    beqz t2, 8f
    ld t1, TY_KIND(t2)
    li t3, TY_VOID
    beq t1, t3, 8f
    mv a0, t2
    ret
8:
    li a0, 0
    ret

# void tr_return(void *frame, long value)
# The `return` record of a frame about to return value.
    .globl tr_return
tr_return:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    mv s2, a1
    call tr_room
    beqz a0, 8f
    call sb_new
    mv s1, a0
    la a1, j_ret_head
    call sb_cstr
    la t0, tr_depth
    ld a1, 0(t0)
    mv a0, s1
    call int_to_dec
    mv a0, s1
    la a1, j_function
    call sb_cstr
    mv a0, s0
    call frame_fn_name
    mv a1, a0
    mv a0, s1
    call json_str_into
    ld a0, FR_FN(s0)
    call tr_ret_type
    beqz a0, 1f
    mv s3, a0
    mv a0, s1
    la a1, j_value
    call sb_cstr
    mv a0, s1
    mv a1, s2
    mv a2, s3
    call tr_value
1:
    mv a0, s1
    la a1, j_close_nl
    call sb_cstr
    mv a0, s1
    call tr_emit
8:
    la t0, tr_depth
    ld t1, 0(t0)
    addi t1, t1, -1
    sd t1, 0(t0)
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# void tr_test(void *test)
# `vo test`: the record before a test's (or a property case's) calls.
    .globl tr_test
tr_test:
    la t0, tr_on
    ld t0, 0(t0)
    beqz t0, 9f
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    la t0, tr_depth
    sd zero, 0(t0)
    call tr_room
    beqz a0, 8f
    call sb_new
    mv s1, a0
    la a1, j_test_head
    call sb_cstr
    mv a0, s0
    call fx_item_name
    mv a1, a0
    mv a0, s1
    call json_str_into
    mv a0, s1
    la a1, j_close_nl
    call sb_cstr
    mv a0, s1
    call tr_emit
8:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
9:
    ret

# void tr_error(char *code, void *message, void *node)
# The runtime error that ends the run (or the test).
    .globl tr_error
tr_error:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    mv s1, a1
    mv s2, a2
    call tr_room
    beqz a0, 9f
    call sb_new
    mv s3, a0
    la a1, j_err_head
    call sb_cstr
    mv a0, s3
    mv a1, s0
    call sb_cstr
    mv a0, s3
    la a1, j_name_key
    call sb_cstr
    mv a0, s0
    call catalog_name
    mv a1, a0
    mv a0, s3
    call sb_str
    mv a0, s3
    li a1, '"'
    call sb_byte
    mv a0, s3
    la a1, j_message
    call sb_cstr
    mv a0, s3
    mv a1, s1
    call json_str_into
    beqz s2, 1f
    mv a0, s3
    la a1, j_file
    call sb_cstr
    lwu a0, N_FILE(s2)
    call file_get
    ld a1, FILE_PATH(a0)
    mv a0, s3
    call json_str_into
    mv a0, s3
    la a1, j_start
    call sb_cstr
    mv a0, s3
    lwu a1, N_FILE(s2)
    lwu a2, N_START(s2)
    call json_pos
1:
    mv a0, s3
    la a1, j_close_nl
    call sb_cstr
    mv a0, s3
    call tr_emit
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# ---- the end ----

# void fb_cstr(void *fb, char *s)
# Writes a C string to a buffered file (no allocation).
    .globl fb_cstr
fb_cstr:
    mv t0, a1
1:
    lbu t1, 0(t0)
    beqz t1, 2f
    addi t0, t0, 1
    j 1b
2:
    sub a2, t0, a1
    tail fbuf_write

# void fb_dec(void *fb, long v)
# Writes a non-negative decimal number (no allocation).
    .globl fb_dec
fb_dec:
    addi sp, sp, -48
    sd ra, 40(sp)
    addi t0, sp, 32             # digits backwards into sp[0..32)
    li t2, 10
    bnez a1, 1f
    addi t0, t0, -1
    li t3, '0'
    sb t3, 0(t0)
1:
    beqz a1, 2f
    rem t3, a1, t2
    div a1, a1, t2
    addi t3, t3, '0'
    addi t0, t0, -1
    sb t3, 0(t0)
    j 1b
2:
    mv a1, t0
    addi a2, sp, 32
    sub a2, a2, t0
    call fbuf_write
    ld ra, 40(sp)
    addi sp, sp, 48
    ret

# long tr_finish(long status)
# Writes the `end` record and flushes the trace; returns the exit status (74 when the trace
# could not be written and the run had not already failed). No allocation (OOM path).
    .globl tr_finish
tr_finish:
    la t0, tr_on
    ld t0, 0(t0)
    beqz t0, 9f
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    la t0, tr_on                # once
    sd zero, 0(t0)
    la t0, tr_fb
    ld s1, 0(t0)
    beqz s1, 7f
    mv a0, s1
    la a1, j_end_head
    call fb_cstr
    la t0, tr_written
    ld a1, 0(t0)
    mv a0, s1
    call fb_dec
    mv a0, s1
    la a1, j_omitted
    call fb_cstr
    la t0, tr_omitted
    ld a1, 0(t0)
    mv a0, s1
    call fb_dec
    la t0, tr_is_test
    ld t0, 0(t0)
    bnez t0, 1f
    mv a0, s1
    la a1, j_status
    call fb_cstr
    mv a0, s1
    mv a1, s0
    call fb_dec
1:
    mv a0, s1
    la a1, j_close_nl
    call fb_cstr
    mv a0, s1
    call fbuf_flush
    ld t0, FB_ERR(s1)
    beqz t0, 8f
7:
    la t0, opt_trace
    ld a0, 0(t0)
    call out_unwritable
    bnez s0, 8f
    li s0, EXIT_OUTPUT
8:
    mv a0, s0
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
9:
    ret

# void out_unwritable(char *path)
# T0009 for a trace or recording that could not be written (reported on stderr).
    .globl out_unwritable
out_unwritable:
    addi sp, sp, -16
    sd ra, 8(sp)
    call str_from_cstr
    mv a1, a0
    la a0, m_unwritable
    li a2, 0
    li a3, 0
    call msg
    mv a3, a0
    la t0, opt_json
    ld a0, 0(t0)
    li a1, 2
    la a2, code_t9
    li a4, 0
    call toolchain_error
    ld ra, 8(sp)
    addi sp, sp, 16
    ret

# long exit_hooks(long status)
# Called by vo_exit before the output is flushed: completes the trace and the recording, and
# returns the exit status (74 when one of them could not be written, T0009). Must not allocate
# on the way of a normal end (it runs on the out-of-memory path too).
    .globl exit_hooks
exit_hooks:
    addi sp, sp, -16
    sd ra, 8(sp)
    call tr_finish
    call rcd_finish
    ld ra, 8(sp)
    addi sp, sp, 16
    ret

# ---- values in runtime errors (research R13) ----

    .section .data
    .align 3
    .globl rt_reporting
rt_reporting: .dword 0          # 1 while a runtime error report is built (no timeouts)

    .section .rodata
s_fl_indent: .string "      "
s_fl_colon: .string ": "
s_fl_eq:    .string " = "
s_fl_sep:   .string ", "
j_locals:   .string ",\"locals\":["
j_fl_name:  .string "{\"name\":"
j_fl_type:  .string ",\"type\":"
j_fl_value: .string ",\"value\":"

    .text
# void fl_add(void *vec, void *name, void *type, long slot)
# One visible local: {name str, type, slot}.
    .globl fl_add
fl_add:
    bltz a3, 9f
    beqz a1, 9f
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
    li a0, 24
    call alloc
    sd s1, 0(a0)
    sd s2, 8(a0)
    sd s3, 16(a0)
    mv a1, a0
    mv a0, s0
    call vec_push
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
9:
    ret

# long fl_contains(void *node, long off)
fl_contains:
    beqz a0, 8f
    lwu t0, N_START(a0)
    lwu t1, N_END(a0)
    bgt t0, a1, 8f
    bge a1, t1, 8f
    li a0, 1
    ret
8:
    li a0, 0
    ret

# void fl_block(void *vec, void *block, long off)
# The bindings of a block visible at off: the `let`s of the statements that end before it, then
# whatever the statement containing off declares around it.
    .globl fl_block
fl_block:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    mv s2, a2
    beqz a1, 9f
    ld s1, N_A(a1)              # statements
    li s3, 0
1:
    ld t0, VEC_N(s1)
    bge s3, t0, 9f
    mv a0, s1
    mv a1, s3
    call vec_get
    addi s3, s3, 1
    lwu t0, N_END(a0)
    bgt t0, s2, 3f
    lhu t1, N_KIND(a0)          # ended before off: a `let` is visible
    li t2, N_LET
    bne t1, t2, 1b
    ld t0, N_A(a0)
    beqz t0, 1b
    ld a1, N_A(t0)
    ld a2, N_TYPE(a0)
    ld a3, N_E(a0)
    mv a0, s0
    call fl_add
    j 1b
3:
    lwu t0, N_START(a0)         # the statement containing off (later ones are not reached yet)
    bgt t0, s2, 9f
    mv a1, a0
    mv a0, s0
    mv a2, s2
    call fl_stmt
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# void fl_stmt(void *vec, void *stmt, long off)
# Inside a statement that contains off: its block that contains off, with the loop variable or
# match binding of that block.
    .globl fl_stmt
fl_stmt:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    mv s1, a1
    mv s2, a2
    lhu t0, N_KIND(s1)
    li t1, N_IF
    beq t0, t1, 10f
    li t1, N_WHILE
    beq t0, t1, 20f
    li t1, N_FOR
    beq t0, t1, 30f
    li t1, N_MATCH
    beq t0, t1, 40f
    li t1, N_BLOCK
    bne t0, t1, 9f
    mv a0, s0
    mv a1, s1
    mv a2, s2
    call fl_block
    j 9f
10:
    ld a0, N_B(s1)              # if: then, else, else-if
    mv a1, s2
    call fl_contains
    beqz a0, 11f
    mv a0, s0
    ld a1, N_B(s1)
    mv a2, s2
    call fl_block
    j 9f
11:
    ld s3, N_C(s1)
    beqz s3, 9f
    mv a0, s3
    mv a1, s2
    call fl_contains
    beqz a0, 9f
    mv a0, s0
    mv a1, s3
    mv a2, s2
    lhu t0, N_KIND(s3)
    li t1, N_IF
    beq t0, t1, 12f
    call fl_block
    j 9f
12:
    call fl_stmt
    j 9f
20:
    ld a0, N_B(s1)              # while
    mv a1, s2
    call fl_contains
    beqz a0, 9f
    mv a0, s0
    ld a1, N_B(s1)
    mv a2, s2
    call fl_block
    j 9f
30:
    ld a0, N_D(s1)              # for: the variable, inside the body
    mv a1, s2
    call fl_contains
    beqz a0, 9f
    ld t0, N_C(s1)
    la t1, t_int
    ld a2, 0(t1)
    bnez t0, 31f                # a range: int
    ld t0, N_B(s1)              # a list: its element type
    li a2, 0
    ld t0, N_TYPE(t0)
    beqz t0, 31f
    ld t1, TY_KIND(t0)
    li t2, TY_LIST
    bne t1, t2, 31f
    ld a2, TY_A(t0)
31:
    ld t0, N_A(s1)
    li a1, 0
    beqz t0, 32f
    ld a1, N_A(t0)
32:
    beqz a2, 33f
    ld a3, N_E(s1)
    mv a0, s0
    call fl_add
33:
    mv a0, s0
    ld a1, N_D(s1)
    mv a2, s2
    call fl_block
    j 9f
40:
    ld s1, N_B(s1)              # match: the arm containing off
    li s3, 0
41:
    ld t0, VEC_N(s1)
    bge s3, t0, 9f
    mv a0, s1
    mv a1, s3
    call vec_get
    addi s3, s3, 1
    sd a0, 0(sp)
    ld a0, N_C(a0)
    mv a1, s2
    call fl_contains
    beqz a0, 41b
    ld t0, 0(sp)
    ld t1, N_B(t0)
    beqz t1, 42f
    ld a1, N_A(t1)
    ld a2, N_TYPE(t0)
    ld a3, N_E(t0)
    beqz a2, 42f
    mv a0, s0
    call fl_add
42:
    ld t0, 0(sp)
    mv a0, s0
    ld a1, N_C(t0)
    mv a2, s2
    call fl_block
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# void *frame_locals(void *frame, void *pos)
# The parameters (and an anonymous function's captures) of a frame, then its locals visible at
# the node pos, in declaration order: a vec of {name str, type, slot}.
    .globl frame_locals
frame_locals:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    mv s1, a1
    call vec_new
    mv s2, a0
    ld a0, FR_FN(s0)
    call tr_params
    mv s3, a0
    beqz s3, 2f
    sd zero, 0(sp)
1:
    ld t0, 0(sp)
    ld t1, VEC_N(s3)
    bge t0, t1, 2f
    mv a0, s3
    mv a1, t0
    call vec_get
    ld t0, N_A(a0)
    ld a1, N_A(t0)
    ld a2, N_TYPE(a0)
    ld a3, N_E(a0)
    mv a0, s2
    call fl_add
    ld t0, 0(sp)
    addi t0, t0, 1
    sd t0, 0(sp)
    j 1b
2:
    ld t0, FR_FN(s0)            # an anonymous function's captures
    lhu t1, N_KIND(t0)
    li t2, N_LAMBDA
    bne t1, t2, 4f
    ld t1, N_A(t0)
    ld t1, N_B(t1)
    beqz t1, 4f
    ld s3, FX_CAPS(t1)
    beqz s3, 4f
    sd zero, 0(sp)
3:
    ld t0, 0(sp)
    ld t1, VEC_N(s3)
    bge t0, t1, 4f
    mv a0, s3
    mv a1, t0
    call vec_get
    ld t0, CP_SYM(a0)
    ld a3, CP_INNER(a0)
    ld a1, SY_NAME(t0)
    ld a2, SY_TYPE(t0)
    mv a0, s2
    call fl_add
    ld t0, 0(sp)
    addi t0, t0, 1
    sd t0, 0(sp)
    j 3b
4:
    beqz s1, 9f                 # the body's bindings visible at pos
    ld t0, FR_FN(s0)
    lhu t1, N_KIND(t0)
    ld a1, N_D(t0)
    li t2, N_TEST
    bne t1, t2, 5f
    ld a1, N_B(t0)
5:
    mv a0, s2
    lwu a2, N_START(s1)
    call fl_block
9:
    mv a0, s2
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# long fl_value(void *frame, void *local, long *out)
# The local's value from the frame's slot in *out; 0 when it cannot be shown (a reference that
# is still unset).
fl_value:
    ld t0, 16(a1)
    slli t0, t0, 3
    add t0, a0, t0
    ld t0, FR_SLOTS(t0)
    sd t0, 0(a2)
    bnez t0, 1f
    ld t1, 8(a1)                # 0 is a value only for int, float and bool
    beqz t1, 8f
    ld t1, TY_KIND(t1)
    li t2, TY_INT
    beq t1, t2, 1f
    li t2, TY_FLOAT
    beq t1, t2, 1f
    li t2, TY_BOOL
    beq t1, t2, 1f
    li t2, TY_PARAM
    beq t1, t2, 1f
8:
    li a0, 0
    ret
1:
    li a0, 1
    ret

# void frame_values(void *sb, void *frame, void *pos, long json)
# The frame's visible parameters and locals, rendered with the frame's type arguments (none for
# R0009, whose values depend on the machine's speed):
# `      name: type = value, ...` and a newline (nothing when there are none), or
# `,"locals":[{"name","type","value"},...]` in JSON.
    .globl frame_values
frame_values:
    la t0, rt_err_code          # a timeout's values depend on the machine's speed (Principle III):
    ld t0, 0(t0)                # R0009 shows the frames without them
    beqz t0, 1f
    lbu t1, 0(t0)
    li t2, 'R'
    bne t1, t2, 1f
    lbu t1, 3(t0)
    li t2, '0'
    bne t1, t2, 1f
    lbu t1, 4(t0)
    li t2, '9'
    bne t1, t2, 1f
    ret
1:
    addi sp, sp, -96
    sd ra, 88(sp)
    sd s0, 80(sp)
    sd s1, 72(sp)
    sd s2, 64(sp)
    sd s3, 56(sp)
    sd s4, 48(sp)
    sd s5, 40(sp)
    mv s0, a0
    mv s1, a1
    mv s5, a3
    mv a0, s1
    mv a1, a2
    call frame_locals
    mv s2, a0
    la t0, cur_frame            # type parameters resolve through this frame
    ld t1, 0(t0)
    sd t1, 16(sp)
    sd s1, 0(t0)
    beqz s5, 1f
    mv a0, s0
    la a1, j_locals
    call sb_cstr
1:
    li s3, 0                    # index
    li s4, 0                    # shown so far
2:
    ld t0, VEC_N(s2)
    bge s3, t0, 7f
    mv a0, s2
    mv a1, s3
    call vec_get
    addi s3, s3, 1
    sd a0, 8(sp)
    mv a1, a0
    mv a0, s1
    addi a2, sp, 0
    call fl_value
    beqz a0, 2b
    beqz s4, 3f
    mv a0, s0
    la a1, s_fl_sep
    beqz s5, 21f
    li a1, 0
21:
    beqz a1, 22f
    call sb_cstr
    j 3f
22:
    li a1, ','
    call sb_byte
3:
    bnez s4, 31f
    bnez s5, 31f
    mv a0, s0
    la a1, s_fl_indent
    call sb_cstr
31:
    addi s4, s4, 1
    ld t0, 8(sp)
    ld a0, 8(t0)                # the concrete type, for its name
    call rt_concrete
    sd a0, 24(sp)
    beqz s5, 5f
    mv a0, s0                   # {"name":..,"type":..,"value":..}
    la a1, j_fl_name
    call sb_cstr
    ld t0, 8(sp)
    ld a1, 0(t0)
    mv a0, s0
    call json_str_into
    mv a0, s0
    la a1, j_fl_type
    call sb_cstr
    ld a0, 24(sp)
    call ty_name
    mv a1, a0
    mv a0, s0
    call json_str_into
    mv a0, s0
    la a1, j_fl_value
    call sb_cstr
    ld t0, 8(sp)
    mv a0, s0
    ld a1, 0(sp)
    ld a2, 8(t0)
    call tr_value
    mv a0, s0
    li a1, '}'
    call sb_byte
    j 2b
5:
    ld t0, 8(sp)                # name: type = value
    ld a1, 0(t0)
    mv a0, s0
    call sb_str
    mv a0, s0
    la a1, s_fl_colon
    call sb_cstr
    ld a0, 24(sp)
    call ty_name
    mv a1, a0
    mv a0, s0
    call sb_str
    mv a0, s0
    la a1, s_fl_eq
    call sb_cstr
    ld t0, 8(sp)
    mv a0, s0
    ld a1, 0(sp)
    ld a2, 8(t0)
    call render_capped
    j 2b
7:
    beqz s5, 8f
    mv a0, s0
    li a1, ']'
    call sb_byte
    j 9f
8:
    beqz s4, 9f
    mv a0, s0
    li a1, 10
    call sb_byte
9:
    la t0, cur_frame
    ld t1, 16(sp)
    sd t1, 0(t0)
    ld ra, 88(sp)
    ld s0, 80(sp)
    ld s1, 72(sp)
    ld s2, 64(sp)
    ld s3, 56(sp)
    ld s4, 48(sp)
    ld s5, 40(sp)
    addi sp, sp, 96
    ret

# ---- the trace of an error inside a test (vo test reports it with the failure) ----

    .section .data
    .align 3
    .globl rt_err_trace_txt
rt_err_trace_txt: .dword 0      # str: "    at f (loc)\n        values\n..." or 0
    .globl rt_err_trace_json
rt_err_trace_json: .dword 0     # str: [{"function",...,"locals":[...]},...] or 0

    .section .rodata
s_ct_at:    .string "    at "
s_ct_open:  .string " ("
s_ct_close: .string ")\n"
s_ct_more:  .string "    ... more frames omitted\n"
s_ct_vals:  .string "  "
j_ct_fn:    .string "{\"function\":"
j_ct_file:  .string ",\"file\":"
j_ct_start: .string ",\"start\":"

    .text
# long err_is_timeout(void)
# 1 when the runtime error being reported (rt_err_code) is R0009.
    .globl err_is_timeout
err_is_timeout:
    la t0, rt_err_code
    ld t0, 0(t0)
    li a0, 0
    beqz t0, 9f
    lbu t1, 0(t0)
    li t2, 'R'
    bne t1, t2, 9f
    lbu t1, 3(t0)
    li t2, '0'
    bne t1, t2, 9f
    lbu t1, 4(t0)
    li t2, '9'
    bne t1, t2, 9f
    li a0, 1
9:
    ret

# void capture_trace(void *node)
# Spec 006 FR-017 in vo test: the frames of a runtime error (at most 10), with their values, as
# text and JSON, kept until the test runner reports the error.
    .globl capture_trace
capture_trace:
    addi sp, sp, -64
    sd ra, 56(sp)
    sd s0, 48(sp)
    sd s1, 40(sp)
    sd s2, 32(sp)
    sd s3, 24(sp)
    sd s4, 16(sp)
    mv s1, a0                   # location in the current frame
    la t0, rt_err_trace_txt
    sd zero, 0(t0)
    la t0, rt_err_trace_json
    sd zero, 0(t0)
    call err_is_timeout         # a timeout keeps its report as it was (no frames)
    bnez a0, 9f
    la t0, rt_reporting
    li t1, 1
    sd t1, 0(t0)
    call sb_new
    mv s2, a0                   # text
    call sb_new
    mv s3, a0                   # JSON
    li a1, '['
    call sb_byte
    la t0, cur_frame
    ld s0, 0(t0)
    li s4, 0
1:
    beqz s0, 8f
    beqz s1, 8f
    li t0, 10
    bge s4, t0, 7f
    beqz s4, 2f
    mv a0, s3
    li a1, ','
    call sb_byte
2:
    addi s4, s4, 1
    mv a0, s2                   # text: at f (loc), then the values indented two more
    la a1, s_ct_at
    call sb_cstr
    mv a0, s0
    call frame_fn_name
    sd a0, 0(sp)
    mv a1, a0
    mv a0, s2
    call sb_str
    mv a0, s2
    la a1, s_ct_open
    call sb_cstr
    mv a0, s2
    lwu a1, N_FILE(s1)
    lwu a2, N_START(s1)
    call human_loc
    mv a0, s2
    la a1, s_ct_close
    call sb_cstr
    ld t0, SB_LEN(s2)
    sd t0, 8(sp)
    mv a0, s2
    mv a1, s0
    mv a2, s1
    li a3, 0
    call frame_values
    ld t0, 8(sp)                # indent the values line by two more spaces
    ld t1, SB_LEN(s2)
    beq t0, t1, 3f
    mv a0, s2
    la a1, s_ct_vals
    mv a2, t0
    call sb_insert_cstr
3:
    mv a0, s3                   # JSON
    la a1, j_ct_fn
    call sb_cstr
    mv a0, s3
    ld a1, 0(sp)
    call json_str_into
    mv a0, s3
    la a1, j_ct_file
    call sb_cstr
    lwu a0, N_FILE(s1)
    call file_get
    ld a1, FILE_PATH(a0)
    mv a0, s3
    call json_str_into
    mv a0, s3
    la a1, j_ct_start
    call sb_cstr
    mv a0, s3
    lwu a1, N_FILE(s1)
    lwu a2, N_START(s1)
    call json_pos
    mv a0, s3
    mv a1, s0
    mv a2, s1
    li a3, 1
    call frame_values
    mv a0, s3
    li a1, '}'
    call sb_byte
    ld s1, FR_CALL(s0)
    ld s0, FR_PARENT(s0)
    j 1b
7:
    mv a0, s2
    la a1, s_ct_more
    call sb_cstr
8:
    mv a0, s3
    li a1, ']'
    call sb_byte
    mv a0, s2
    call sb_finish
    la t0, rt_err_trace_txt
    sd a0, 0(t0)
    mv a0, s3
    call sb_finish
    la t0, rt_err_trace_json
    sd a0, 0(t0)
    la t0, rt_reporting
    sd zero, 0(t0)
9:
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    addi sp, sp, 64
    ret

# void sb_insert_cstr(void *sb, char *s, long at)
# Inserts a C string at byte `at` of the builder.
    .globl sb_insert_cstr
sb_insert_cstr:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    mv s1, a1
    mv s2, a2
    mv a0, s1
    call cstr_len
    mv s3, a0
    mv a0, s0                   # room at the end, then move the tail up
    mv a1, s3
    call sb_reserve
    ld t0, SB_PTR(s0)
    ld t1, SB_LEN(s0)
    add t2, t0, t1              # old end
1:
    add t3, t0, s2
    ble t2, t3, 2f
    addi t2, t2, -1
    lbu t4, 0(t2)
    add t5, t2, s3
    sb t4, 0(t5)
    j 1b
2:
    add t3, t0, s2              # the string
    mv t4, s1
3:
    lbu t5, 0(t4)
    beqz t5, 4f
    sb t5, 0(t3)
    addi t3, t3, 1
    addi t4, t4, 1
    j 3b
4:
    ld t1, SB_LEN(s0)
    add t1, t1, s3
    sd t1, SB_LEN(s0)
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret
