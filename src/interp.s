# Tree-walking evaluator over the checked AST (research §1, §3, §8, §9).
# ponytail: tree-walking evaluator, ~50-100x slower than native; upgrade: bytecode VM behind the same checker
    .include "defs.inc"

    .equ VSTACK_SIZE, 0x2000000     # 32 MiB value stack for frames
    .equ JMP_SIZE, 112

    .section .bss
    .align 3
    .globl cur_frame
cur_frame:      .space 8
vsp:            .space 8
vbase:          .space 8
vstack_end:     .space 8
    .globl ret_val
ret_val:        .space 8
    .globl deadline_ms
deadline_ms:    .space 8
steps:          .space 8
try_target:     .space 8            # jmp buffer of the innermost user-function call
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

    .text
# void interp_init(void)
    .globl interp_init
interp_init:
    addi sp, sp, -16
    sd ra, 8(sp)
    la t0, vsp
    ld t1, 0(t0)
    bnez t1, 1f
    li a0, VSTACK_SIZE
    call alloc
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

# void rt_timeout(void *node)
    .globl rt_timeout
rt_timeout:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    mv s0, a0
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
# Calls a user function: a new frame, arguments into its first slots, the body, the return value.
# args is a vec of argument expressions evaluated in the caller's frame (or 0 for none); or, with
# args 0, vals holds the argument values. A closure adds its captured values and type arguments.
    .globl call_fn
call_fn:
    addi sp, sp, -192
    sd ra, 184(sp)
    sd s0, 176(sp)
    sd s1, 168(sp)
    sd s2, 160(sp)
    sd s3, 152(sp)
    sd s4, 144(sp)
    sd s5, 136(sp)
    sd s6, 128(sp)
    sd s7, 120(sp)
    sd s8, 112(sp)
    mv s0, a0                   # fn
    mv s1, a1                   # call node
    mv s2, a2                   # args
    mv s7, a3                   # argument values (args = 0)
    mv s8, a4                   # closure
    mv a0, s1
    beqz a0, 1f
    call check_stack
1:
    la t0, vsp
    ld s3, 0(t0)                # new frame
    ld t1, N_E(s0)              # local count
    slli t1, t1, 3
    addi t1, t1, FR_SLOTS
    add s4, s3, t1              # frame end
    la t3, vstack_end
    ld t3, 0(t3)
    bltu s4, t3, 2f
    la a0, r0005
    la a1, m_stack
    mv a2, s1
    call rt_raise_c
2:
    la t0, vsp
    sd s4, 0(t0)                # reserve it
    addi t3, s3, FR_SLOTS       # zero the slots
3:
    bgeu t3, s4, 4f
    sd zero, 0(t3)
    addi t3, t3, 8
    j 3b
4:
    bnez s2, 41f                # values given: copy them
    beqz s7, 6f
    ld t0, N_B(s0)
    ld t0, VEC_N(t0)
    li t1, 0
42:
    bge t1, t0, 6f
    slli t2, t1, 3
    add t3, s7, t2
    ld t3, 0(t3)
    add t2, s3, t2
    sd t3, FR_SLOTS(t2)
    addi t1, t1, 1
    j 42b
41:
    li s4, 0                    # evaluate the arguments into the slots
    beqz s2, 6f
5:
    ld t0, VEC_N(s2)
    bge s4, t0, 6f
    mv a0, s2
    mv a1, s4
    call vec_get
    call eval_expr
    slli t0, s4, 3
    add t0, s3, t0
    sd a0, FR_SLOTS(t0)
    addi s4, s4, 1
    j 5b
6:
    la t0, cur_frame
    ld s5, 0(t0)                # caller frame
    sd s5, FR_PARENT(s3)
    sd s0, FR_FN(s3)
    sd s1, FR_CALL(s3)
    sd zero, FR_TARGS(s3)
    sd zero, FR_EFFECTS(s3)
    ld t1, N_A(s0)              # the callee's declared effects
    beqz t1, 66f
    ld t1, N_B(t1)
    beqz t1, 66f
    ld t1, FX_DECLARED(t1)
    bnez t1, 65f                # an anonymous function runs under its caller's declaration
    beqz s5, 66f
    ld t1, FR_EFFECTS(s5)
65:
    sd t1, FR_EFFECTS(s3)
66:
    beqz s1, 69f                # a generic call: its type arguments (the caller's resolved)
    lhu t1, N_KIND(s1)
    li t2, N_CALL
    bne t1, t2, 69f
    ld a0, N_E(s1)
    beqz a0, 69f
    mv a1, s5
    call rt_targs
    sd a0, FR_TARGS(s3)
    mv a2, a0                   # its declared effects with the effect parameters bound
    ld a0, FR_EFFECTS(s3)
    beqz a0, 69f
    mv a1, s0
    call es_subst
    sd a0, FR_EFFECTS(s3)
69:
    beqz s8, 67f                # a closure: its type arguments and captured values
    ld t1, CL_TARGS(s8)
    sd t1, FR_TARGS(s3)
    ld t1, N_A(s0)
    ld t1, N_B(t1)
    ld t2, FX_CAPS(t1)
    beqz t2, 67f
    ld t4, CL_N(s8)
    li t3, 0
68:
    bge t3, t4, 67f
    ld t5, VEC_DATA(t2)
    slli t6, t3, 3
    add t5, t5, t6
    ld t5, 0(t5)
    ld t5, CP_INNER(t5)
    slli t5, t5, 3
    add t5, s3, t5
    add t6, s8, t6
    ld t6, CL_CAPS(t6)
    sd t6, FR_SLOTS(t5)
    addi t3, t3, 1
    j 68b
67:
    la t0, cur_frame
    sd s3, 0(t0)
    la t0, try_target
    ld s6, 0(t0)
    addi a0, sp, 0              # jmp buffer for `try` at 0(sp)
    sd a0, 0(t0)
    call jmp_save
    bnez a0, 7f
    mv a0, s1                   # count the call as a step
    beqz a0, 61f
    call tick
61:
    la t0, ret_val
    sd zero, 0(t0)
    ld a0, N_D(s0)
    call exec_block
7:
    la t0, ret_val
    ld a0, 0(t0)
    la t0, cur_frame
    sd s5, 0(t0)
    la t0, try_target
    sd s6, 0(t0)
    la t0, vsp
    sd s3, 0(t0)
    ld ra, 184(sp)
    ld s0, 176(sp)
    ld s1, 168(sp)
    ld s2, 160(sp)
    ld s3, 152(sp)
    ld s4, 144(sp)
    ld s5, 136(sp)
    ld s6, 128(sp)
    ld s7, 120(sp)
    ld s8, 112(sp)
    addi sp, sp, 192
    ret

# long eval_binary(void *node)
    .globl eval_binary
eval_binary:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    ld s3, N_D(s0)              # op
    li t0, OP_LT_PARAM          # an `ordered` type parameter: the op for its actual type
    blt s3, t0, 0f
    ld t0, N_A(s0)
    ld a0, N_TYPE(t0)
    call rt_concrete
    ld t0, TY_KIND(a0)
    addi s3, s3, -OP_LT_PARAM
    li t1, TY_FLOAT
    li t2, OP_LT_FLOAT
    beq t0, t1, 1f
    li t1, TY_STR
    li t2, OP_LT_STR
    beq t0, t1, 1f
    li t2, OP_LT_INT
1:
    add s3, s3, t2
0:
    li t0, OP_AND
    beq s3, t0, 60f
    li t0, OP_OR
    beq s3, t0, 61f
    ld a0, N_A(s0)
    call eval_expr
    mv s1, a0
    ld a0, N_B(s0)
    call eval_expr
    mv s2, a0
    li t0, OP_ADD_INT
    beq s3, t0, 10f
    li t0, OP_SUB_INT
    beq s3, t0, 11f
    li t0, OP_MUL_INT
    beq s3, t0, 12f
    li t0, OP_DIV_INT
    beq s3, t0, 13f
    li t0, OP_REM_INT
    beq s3, t0, 14f
    li t0, OP_ADD_FLOAT
    beq s3, t0, 20f
    li t0, OP_SUB_FLOAT
    beq s3, t0, 21f
    li t0, OP_MUL_FLOAT
    beq s3, t0, 22f
    li t0, OP_DIV_FLOAT
    beq s3, t0, 23f
    li t0, OP_CONCAT
    beq s3, t0, 30f
    li t0, OP_EQ
    beq s3, t0, 31f
    li t0, OP_NE
    beq s3, t0, 32f
    li t0, OP_LT_INT
    beq s3, t0, 40f
    li t0, OP_LE_INT
    beq s3, t0, 41f
    li t0, OP_GT_INT
    beq s3, t0, 42f
    li t0, OP_GE_INT
    beq s3, t0, 43f
    li t0, OP_LT_FLOAT
    beq s3, t0, 44f
    li t0, OP_LE_FLOAT
    beq s3, t0, 45f
    li t0, OP_GT_FLOAT
    beq s3, t0, 46f
    li t0, OP_GE_FLOAT
    beq s3, t0, 47f
    j 50f                       # string orderings
10:                             # checked add: overflow iff (t < a) != (b < 0)
    add a0, s1, s2
    slt t0, a0, s1
    slti t1, s2, 0
    bne t0, t1, 90f
    j 99f
11:                             # checked sub: overflow iff (t < a) != (b > 0)
    sub a0, s1, s2
    slt t0, a0, s1
    sgtz t1, s2
    bne t0, t1, 90f
    j 99f
12:                             # checked mul: overflow iff high word != sign of low word
    mul a0, s1, s2
    mulh t0, s1, s2
    srai t1, a0, 63
    bne t0, t1, 90f
    j 99f
13:
    beqz s2, 91f
    li t0, -1
    bne s2, t0, 131f
    li t1, 1
    slli t1, t1, 63
    beq s1, t1, 90f             # MIN / -1
131:
    div a0, s1, s2
    j 99f
14:
    beqz s2, 91f
    rem a0, s1, s2
    j 99f
20:
    fmv.d.x ft0, s1
    fmv.d.x ft1, s2
    fadd.d ft0, ft0, ft1
    fmv.x.d a0, ft0
    j 99f
21:
    fmv.d.x ft0, s1
    fmv.d.x ft1, s2
    fsub.d ft0, ft0, ft1
    fmv.x.d a0, ft0
    j 99f
22:
    fmv.d.x ft0, s1
    fmv.d.x ft1, s2
    fmul.d ft0, ft0, ft1
    fmv.x.d a0, ft0
    j 99f
23:
    fmv.d.x ft0, s1
    fmv.d.x ft1, s2
    fdiv.d ft0, ft0, ft1
    fmv.x.d a0, ft0
    j 99f
30:
    la t0, cur_node
    sd s0, 0(t0)
    mv a0, s1
    mv a1, s2
    call str_concat
    j 99f
31:
    mv a0, s1
    mv a1, s2
    ld t0, N_A(s0)
    ld a2, N_TYPE(t0)
    call values_equal
    j 99f
32:
    mv a0, s1
    mv a1, s2
    ld t0, N_A(s0)
    ld a2, N_TYPE(t0)
    call values_equal
    xori a0, a0, 1
    j 99f
40:
    slt a0, s1, s2
    j 99f
41:
    slt a0, s2, s1
    xori a0, a0, 1
    j 99f
42:
    slt a0, s2, s1
    j 99f
43:
    slt a0, s1, s2
    xori a0, a0, 1
    j 99f
44:
    fmv.d.x ft0, s1
    fmv.d.x ft1, s2
    flt.d a0, ft0, ft1
    j 99f
45:
    fmv.d.x ft0, s1
    fmv.d.x ft1, s2
    fle.d a0, ft0, ft1
    j 99f
46:
    fmv.d.x ft0, s1
    fmv.d.x ft1, s2
    flt.d a0, ft1, ft0
    j 99f
47:
    fmv.d.x ft0, s1
    fmv.d.x ft1, s2
    fle.d a0, ft1, ft0
    j 99f
50:
    mv a0, s1
    mv a1, s2
    call str_cmp
    li t0, OP_LT_STR
    beq s3, t0, 51f
    li t0, OP_LE_STR
    beq s3, t0, 52f
    li t0, OP_GT_STR
    beq s3, t0, 53f
    slti a0, a0, 0              # >=: not <
    xori a0, a0, 1
    j 99f
51:
    slti a0, a0, 0
    j 99f
52:
    slti a0, a0, 1
    j 99f
53:
    sgtz a0, a0
    j 99f
60:                             # and
    ld a0, N_A(s0)
    call eval_expr
    beqz a0, 99f
    ld a0, N_B(s0)
    call eval_expr
    j 99f
61:                             # or
    ld a0, N_A(s0)
    call eval_expr
    bnez a0, 99f
    ld a0, N_B(s0)
    call eval_expr
    j 99f
90:
    mv a0, s0
    call rt_overflow
91:
    mv a2, s0
    la a0, r0002
    la a1, m_divzero
    call rt_raise_c
99:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# long eval_expr(void *node)
    .globl eval_expr
eval_expr:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    lhu t0, N_KIND(s0)          # (stack depth is checked at calls; nesting is bounded by the parser)
    li t1, N_INT
    beq t0, t1, 1f
    li t1, N_FLOAT
    beq t0, t1, 1f
    li t1, N_STR
    beq t0, t1, 1f
    li t1, N_BOOL
    beq t0, t1, 1f
    li t1, N_IDENT
    beq t0, t1, 2f
    li t1, N_BINARY
    beq t0, t1, 3f
    li t1, N_CALL
    beq t0, t1, 4f
    li t1, N_FIELD
    beq t0, t1, 5f
    li t1, N_INDEX
    beq t0, t1, 6f
    li t1, N_UNARY
    beq t0, t1, 7f
    li t1, N_TRY
    beq t0, t1, 8f
    li t1, N_LIST
    beq t0, t1, 10f
    li t1, N_NEW_RECORD
    beq t0, t1, 11f
    li t1, N_NEW_LIST
    beq t0, t1, 12f
    li t1, N_NEW_MAP
    beq t0, t1, 13f
    li t1, N_VCASE
    beq t0, t1, 14f
    li t1, N_LAMBDA
    beq t0, t1, 170f
    li t1, N_CALL_VALUE
    beq t0, t1, 171f
    li a0, 0
    j 99f
170:
    mv a0, s0
    call make_closure
    j 99f
171:
    mv a0, s0
    call eval_call_value
    j 99f
1:
    ld a0, N_A(s0)
    j 99f
2:
    lhu t0, N_FLAGS(s0)
    andi t0, t0, NF_FNVAL
    beqz t0, 21f
    ld a0, N_D(s0)              # a named function as a value
    call fn_closure
    j 99f
21:
    ld a0, N_C(s0)
    call local_addr
    ld a0, 0(a0)
    j 99f
3:
    mv a0, s0
    call eval_binary
    j 99f
4:
    ld t0, N_D(s0)
    beqz t0, 41f
    mv a0, s0
    call eval_builtin
    j 99f
41:
    ld a0, N_C(s0)
    mv a1, s0
    ld a2, N_B(s0)
    li a3, 0
    li a4, 0
    call call_fn
    j 99f
5:
    ld a0, N_A(s0)
    call eval_expr
    ld t0, N_C(s0)
    slli t0, t0, 3
    add a0, a0, t0
    ld a0, R_FIELDS(a0)
    j 99f
6:
    ld a0, N_A(s0)
    call eval_expr
    mv s1, a0
    ld a0, N_B(s0)
    call eval_expr
    mv s2, a0
    lhu t0, N_FLAGS(s0)
    andi t0, t0, NF_MAP
    bnez t0, 61f
    ld t1, L_LEN(s1)
    bltz s2, 62f
    bge s2, t1, 62f
    ld t0, L_DATA(s1)
    slli t2, s2, 3
    add t0, t0, t2
    ld a0, 0(t0)
    j 99f
62:
    mv a0, s0
    mv a1, s2
    ld a2, L_LEN(s1)
    call rt_index_error
61:
    ld t0, N_A(s0)              # map lookup
    ld t0, N_TYPE(t0)
    ld s3, TY_A(t0)             # key type
    mv a0, s1
    mv a1, s2
    mv a2, s3
    call map_find
    bltz a0, 63f
    ld t0, M_ENTRIES(s1)
    li t1, ME_SIZE
    mul t1, t1, a0
    add t0, t0, t1
    ld a0, ME_VAL(t0)
    j 99f
63:
    mv a0, s0
    mv a1, s2
    mv a2, s3
    call rt_key_error
7:
    ld a0, N_A(s0)
    call eval_expr
    ld t0, N_D(s0)
    li t1, OP_NOT
    beq t0, t1, 71f
    li t1, OP_NEG_FLOAT
    beq t0, t1, 72f
    li t1, 1                    # -MIN overflows
    slli t1, t1, 63
    bne a0, t1, 73f
    mv a0, s0
    call rt_overflow
73:
    neg a0, a0
    j 99f
71:
    xori a0, a0, 1
    j 99f
72:
    li t1, 1
    slli t1, t1, 63
    xor a0, a0, t1
    j 99f
8:                              # try: unwrap ok, or return the err from the current function
    ld a0, N_A(s0)
    call eval_expr
    lwu t0, V_TAG(a0)
    bnez t0, 81f
    ld a0, V_PAY(a0)
    j 99f
81:
    la t0, ret_val
    sd a0, 0(t0)
    la t0, try_target
    ld a0, 0(t0)
    li a1, 1
    call jmp_restore
10:
    mv a0, s0
    call eval_list_literal
    j 99f
11:
    mv a0, s0
    call eval_new_record
    j 99f
12:
    li a0, 4
    call list_new
    j 99f
13:
    call map_new
    j 99f
14:
    li a0, OBJ_VARIANT
    li a1, VARIANT_SIZE
    call alloc_obj
    mv s1, a0
    ld t0, N_E(s0)
    sw t0, V_TAG(s1)
    ld a0, N_C(s0)
    beqz a0, 141f
    call eval_expr
    sd a0, V_PAY(s1)
141:
    mv a0, s1
99:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# void *eval_list_literal(void *node)
    .globl eval_list_literal
eval_list_literal:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    sd s2, 0(sp)
    mv s0, a0
    ld t0, N_A(s0)
    ld a0, VEC_N(t0)
    call list_new
    mv s1, a0
    li s2, 0
1:
    ld t0, N_A(s0)
    ld t1, VEC_N(t0)
    bge s2, t1, 2f
    mv a0, t0
    mv a1, s2
    call vec_get
    call eval_expr
    mv a1, a0
    mv a0, s1
    call list_push
    addi s2, s2, 1
    j 1b
2:
    mv a0, s1
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    ld s2, 0(sp)
    addi sp, sp, 32
    ret

# void *eval_new_record(void *node)
    .globl eval_new_record
eval_new_record:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    ld t0, N_C(s0)              # record decl
    ld a0, N_B(t0)
    call count_members
    slli a1, a0, 3
    li a0, OBJ_RECORD
    call alloc_obj
    mv s1, a0
    li s2, 0
1:
    ld t0, N_B(s0)
    ld t1, VEC_N(t0)
    bge s2, t1, 2f
    mv a0, t0
    mv a1, s2
    call vec_get
    mv s3, a0
    ld a0, N_B(s3)
    call eval_expr
    ld t0, N_C(s3)
    slli t0, t0, 3
    add t0, s1, t0
    sd a0, R_FIELDS(t0)
    addi s2, s2, 1
    j 1b
2:
    mv a0, s1
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
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
    mv a0, a2                   # type parameters: the running frame's type arguments
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

# int exec_block(void *block)
# Runs statements until one does not complete normally; returns the statement status.
    .globl exec_block
exec_block:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    sd s2, 0(sp)
    ld s0, N_A(a0)
    li s1, 0
1:
    ld t0, VEC_N(s0)
    bge s1, t0, 8f
    mv a0, s0
    mv a1, s1
    call vec_get
    addi s1, s1, 1
    call exec_stmt
    beqz a0, 1b
    j 9f
8:
    li a0, ST_NORMAL
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    ld s2, 0(sp)
    addi sp, sp, 32
    ret

# int exec_stmt(void *stmt)
    .globl exec_stmt
exec_stmt:
    addi sp, sp, -64
    sd ra, 56(sp)
    sd s0, 48(sp)
    sd s1, 40(sp)
    sd s2, 32(sp)
    sd s3, 24(sp)
    sd s4, 16(sp)
    mv s0, a0
    lhu t0, N_KIND(s0)
    li t1, N_EXPR_STMT
    beq t0, t1, 10f
    li t1, N_LET
    beq t0, t1, 11f
    li t1, N_SET
    beq t0, t1, 12f
    li t1, N_IF
    beq t0, t1, 13f
    li t1, N_WHILE
    beq t0, t1, 14f
    li t1, N_FOR
    beq t0, t1, 15f
    li t1, N_MATCH
    beq t0, t1, 16f
    li t1, N_RETURN
    beq t0, t1, 17f
    li t1, N_BREAK
    li a0, ST_BREAK
    beq t0, t1, 99f
    li t1, N_CONTINUE
    li a0, ST_CONTINUE
    beq t0, t1, 99f
    li a0, ST_NORMAL
    j 99f
10:
    ld a0, N_A(s0)
    call eval_expr
    li a0, ST_NORMAL
    j 99f
11:                             # let / var
    ld a0, N_C(s0)
    call eval_expr
    ld t0, N_E(s0)
    bltz t0, 111f
    mv s1, a0
    mv a0, t0
    call local_addr
    sd s1, 0(a0)
111:
    li a0, ST_NORMAL
    j 99f
12:                             # set
    ld s1, N_A(s0)              # place
    lhu t0, N_KIND(s1)
    li t1, N_IDENT
    beq t0, t1, 121f
    li t1, N_FIELD
    beq t0, t1, 122f
    ld a0, N_A(s1)              # index: object, key/index, value
    call eval_expr
    mv s2, a0
    ld a0, N_B(s1)
    call eval_expr
    mv s3, a0
    ld a0, N_B(s0)
    call eval_expr
    mv s4, a0
    lhu t0, N_FLAGS(s1)
    andi t0, t0, NF_MAP
    bnez t0, 123f
    ld t1, L_LEN(s2)
    bltz s3, 124f
    bge s3, t1, 124f
    ld t0, L_DATA(s2)
    slli t2, s3, 3
    add t0, t0, t2
    sd s4, 0(t0)
    j 129f
124:
    mv a0, s1
    mv a1, s3
    ld a2, L_LEN(s2)
    call rt_index_error
123:
    la t0, cur_node
    sd s0, 0(t0)
    ld t0, N_A(s1)
    ld t0, N_TYPE(t0)
    mv a0, s2
    mv a1, s3
    mv a2, s4
    ld a3, TY_A(t0)
    call map_set
    j 129f
121:
    ld a0, N_B(s0)
    call eval_expr
    mv s2, a0
    ld a0, N_C(s1)
    call local_addr
    sd s2, 0(a0)
    j 129f
122:
    ld a0, N_A(s1)
    call eval_expr
    mv s2, a0
    ld a0, N_B(s0)
    call eval_expr
    ld t0, N_C(s1)
    slli t0, t0, 3
    add t0, s2, t0
    sd a0, R_FIELDS(t0)
129:
    li a0, ST_NORMAL
    j 99f
13:                             # if
    ld a0, N_A(s0)
    call eval_expr
    beqz a0, 131f
    ld a0, N_B(s0)
    call exec_block
    j 99f
131:
    ld a0, N_C(s0)
    beqz a0, 132f
    lhu t0, N_KIND(a0)
    li t1, N_IF
    beq t0, t1, 133f
    call exec_block
    j 99f
133:
    call exec_stmt
    j 99f
132:
    li a0, ST_NORMAL
    j 99f
14:                             # while
    mv a0, s0
    call tick
    ld a0, N_A(s0)
    call eval_expr
    beqz a0, 149f
    ld a0, N_B(s0)
    call exec_block
    li t0, ST_BREAK
    beq a0, t0, 149f
    li t0, ST_RETURN
    beq a0, t0, 99f
    j 14b
149:
    li a0, ST_NORMAL
    j 99f
15:                             # for
    ld a0, N_B(s0)
    call eval_expr
    mv s1, a0                   # range start or list
    ld t0, N_C(s0)
    beqz t0, 155f
    mv a0, t0
    call eval_expr
    mv s2, a0                   # range end
151:
    bge s1, s2, 159f
    mv a0, s0
    call tick
    ld a0, N_E(s0)
    call local_addr
    sd s1, 0(a0)
    ld a0, N_D(s0)
    call exec_block
    li t0, ST_BREAK
    beq a0, t0, 159f
    li t0, ST_RETURN
    beq a0, t0, 99f
    addi s1, s1, 1
    j 151b
155:
    li s2, 0                    # list index
156:
    ld t0, L_LEN(s1)
    bge s2, t0, 159f
    mv a0, s0
    call tick
    ld t0, L_DATA(s1)
    slli t1, s2, 3
    add t0, t0, t1
    ld s3, 0(t0)
    ld a0, N_E(s0)
    call local_addr
    sd s3, 0(a0)
    ld a0, N_D(s0)
    call exec_block
    li t0, ST_BREAK
    beq a0, t0, 159f
    li t0, ST_RETURN
    beq a0, t0, 99f
    addi s2, s2, 1
    j 156b
159:
    li a0, ST_NORMAL
    j 99f
16:                             # match
    ld a0, N_A(s0)
    call eval_expr
    mv s1, a0
    lwu s2, V_TAG(s1)
    ld s3, N_B(s0)
    li s4, 0
161:
    ld t0, VEC_N(s3)
    bge s4, t0, 169f
    mv a0, s3
    mv a1, s4
    call vec_get
    addi s4, s4, 1
    lhu t0, N_KIND(a0)
    li t1, N_ARM
    bne t0, t1, 161b
    ld t0, N_D(a0)
    bne t0, s2, 161b
    mv s4, a0                   # the arm
    ld t0, N_E(s4)
    bltz t0, 162f
    mv a0, t0
    call local_addr
    ld t0, V_PAY(s1)
    sd t0, 0(a0)
162:
    ld a0, N_C(s4)
    call exec_block
    j 99f
169:
    li a0, ST_NORMAL
    j 99f
17:                             # return
    li a0, 0
    ld t0, N_A(s0)
    beqz t0, 171f
    mv a0, t0
    call eval_expr
171:
    la t0, ret_val
    sd a0, 0(t0)
    li a0, ST_RETURN
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
    la t0, try_target
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
    mv a3, a2
    mv a4, a0
    ld a0, CL_FN(a0)
    li a2, 0
    tail call_fn

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
    li a0, OBJ_CLOSURE
    li a1, CL_CAPS - 8
    call alloc_obj
    sd s0, CL_FN(a0)
    sd a0, FX_CLOSURE(s1)
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# long eval_call_value(void *call)
# `callee(args)` for any callee expression: the arguments go to the value stack, then the call.
    .globl eval_call_value
eval_call_value:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    sd s4, 0(sp)
    mv s0, a0
    ld a0, N_A(s0)
    call eval_expr
    mv s1, a0                   # the closure
    ld t0, N_B(s0)
    ld s2, VEC_N(t0)
    la t0, vsp
    ld s3, 0(t0)                # argument area
    slli t1, s2, 3
    add t1, s3, t1
    la t2, vstack_end
    ld t2, 0(t2)
    bltu t1, t2, 1f
    la a0, r0005
    la a1, m_stack
    mv a2, s0
    call rt_raise_c
1:
    la t0, vsp
    slli t1, s2, 3
    add t1, s3, t1
    sd t1, 0(t0)
    li s4, 0
2:
    bge s4, s2, 3f
    ld a0, N_B(s0)
    mv a1, s4
    call vec_get
    call eval_expr
    slli t0, s4, 3
    add t0, s3, t0
    sd a0, 0(t0)
    addi s4, s4, 1
    j 2b
3:
    mv a0, s1
    mv a1, s0
    mv a2, s3
    call call_value
    la t0, vsp
    sd s3, 0(t0)
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    ld s4, 0(sp)
    addi sp, sp, 48
    ret

# void run_test_body(void *test)
# Runs a test block in a fresh frame (its locals are counted in N_E).
    .globl run_test_body
run_test_body:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    la t0, vsp
    ld s1, 0(t0)                # frame
    ld t1, N_E(s0)
    slli t1, t1, 3
    addi t1, t1, FR_SLOTS
    add t2, s1, t1
    sd t2, 0(t0)
    addi t3, s1, FR_SLOTS
1:
    bgeu t3, t2, 2f
    sd zero, 0(t3)
    addi t3, t3, 8
    j 1b
2:
    sd zero, FR_PARENT(s1)
    sd s0, FR_FN(s1)
    sd zero, FR_CALL(s1)
    sd zero, FR_TARGS(s1)
    sd zero, FR_EFFECTS(s1)
    ld t1, N_D(s0)              # the test's declared effects are its grant
    beqz t1, 21f
    ld t1, FX_DECLARED(t1)
    sd t1, FR_EFFECTS(s1)
21:
    la t0, cur_frame
    sd s1, 0(t0)
    ld a0, N_B(s0)
    call exec_block
    call interp_reset
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret
