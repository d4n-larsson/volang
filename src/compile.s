# The bytecode compiler (spec 004, research R1, R6, R15): after checking, every function, lambda
# and test body becomes a compiled function (data-model.md) that src/vm.s runs. One recursive
# pass per body; instructions follow the tree walker's evaluation order exactly
# (contracts/equivalence.md).
#
# Slots are byte offsets (slot * 8) from FR_SLOTS. Locals keep the checker's slot numbers;
# temporaries are allocated above them in stack order. cexpr(node, dst) returns the slot holding
# the value: an identifier's own slot, or dst / a fresh temporary written by the expression's
# last instruction. Locals cannot change while an expression is evaluated (no references to
# locals, closures capture copies), so reading them in place is safe.
# ponytail: recursive over the tree (the parser bounds nesting at 5,000 levels); temporaries are
# never cleared, so a dead one can keep garbage alive until it is overwritten. Upgrade: an
# explicit work stack; clearing temporaries at statement ends or precise frame maps.
    .include "defs.inc"

    .section .data
    .align 3
    .globl c_ops
c_ops:      .dword vm_ops           # the handler table compile_program copies from (spec 006: vm_ops_trace)

    .section .bss
    .align 3
c_buf:      .space 8                # instruction buffer of the function being compiled
c_len:      .space 8                # bytes used
c_cap:      .space 8
c_temp:     .space 8                # next free slot (bytes)
c_max:      .space 8                # frame size so far (bytes)
c_fix:      .space 8                # vec of instruction offsets whose I_C is a code offset
c_fail:     .space 8                # 1: a construct this compiler does not handle (T0003 if it runs)
c_brk:      .space 8                # vec of `break` jumps of the innermost loop
c_cont:     .space 8                # vec of `continue` jumps of the innermost loop
c_done:     .space 8                # compile_program ran
c_code_ptr: .space 8                # next free byte of the code region (0: not mapped yet)
c_lams:     .space 8                # vec of lambdas reached, compiled after their enclosing body
c_ens:      .space 8                # spec 005: vec of jumps to the `ensures` epilogue, or 0 (none)
c_reqj:     .space 8                # vec of `requires` jumps taken when a condition is false
c_retslot:  .space 8                # the `ret` slot (bytes), or -1 (no return type)
c_exitslot: .space 8                # the slot holding the exit node for R0013 (bytes)

    .text
# void compile_program(void)
# Compiles every function and test of every loaded file (once).
    .globl compile_program
compile_program:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    la t0, c_done
    ld t1, 0(t0)
    bnez t1, 9f
    li t1, 1
    sd t1, 0(t0)
    call std_runtime_grants      # spec 007: library functions run with their caller's grant
    li a0, 65536
    call c_code_alloc
    la t0, c_buf
    sd a0, 0(t0)
    la t0, c_cap
    li t1, 65536
    sd t1, 0(t0)
    call vec_new
    la t0, c_fix
    sd a0, 0(t0)
    call vec_new
    la t0, c_lams
    sd a0, 0(t0)
    li s0, 0                    # file index
1:
    la t0, file_count
    ld t0, 0(t0)
    bge s0, t0, 9f
    mv a0, s0
    call file_get
    ld t0, FILE_AST(a0)
    beqz t0, 4f                 # not a source file (vo.packages, vo.effects)
    ld s1, N_A(t0)              # items
    li s2, 0
2:
    ld t0, VEC_N(s1)
    bge s2, t0, 4f
    mv a0, s1
    mv a1, s2
    call vec_get
    addi s2, s2, 1
    lhu t0, N_KIND(a0)
    li t1, N_FN
    beq t0, t1, 3f
    li t1, N_TEST
    bne t0, t1, 2b
3:
    call compile_fn
31:
    la t0, c_lams               # then the lambdas it reached (and theirs)
    ld t0, 0(t0)
    ld t1, VEC_N(t0)
    beqz t1, 2b
    addi t1, t1, -1
    sd t1, VEC_N(t0)
    ld t2, VEC_DATA(t0)
    slli t1, t1, 3
    add t2, t2, t1
    ld a0, 0(t2)
    call compile_fn
    j 31b
4:
    addi s0, s0, 1
    j 1b
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# void *c_code_alloc(long size)
# Memory in the code region: compiled functions, their scratch buffer and generic call-site
# records. Instructions pack two 32-bit slot offsets into one word, which can look like a heap
# address; the collector never scans this region, so they pin nothing (nothing here points into
# the heap: constants and nodes are permanent).
# ponytail: one 256 MiB reservation, never freed; upgrade: map more on demand if a program needs it.
    .globl c_code_alloc
c_code_alloc:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    addi s0, a0, 7
    andi s0, s0, -8
    la t0, c_code_ptr
    ld t1, 0(t0)
    bnez t1, 1f
    li a0, ADDR_CODE
    li a1, CODE_SIZE
    li a2, 0x4000               # MAP_NORESERVE
    call map_fixed
    la t0, c_code_ptr
    li t1, ADDR_CODE
    sd t1, 0(t0)
1:
    la t0, c_code_ptr
    ld a0, 0(t0)
    add t1, a0, s0
    li t2, ADDR_CODE + CODE_SIZE
    bgtu t1, t2, 2f
    sd t1, 0(t0)
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret
2:
    call vm_not_compiled        # (T0003: the program's code does not fit in 256 MiB)
    li a0, 0
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret

# void compile_fn(void *decl)
# One N_FN, N_LAMBDA or N_TEST into a compiled function stored in FX_CODE of its FX record;
# FX_CODE stays 0 when the body uses something not compiled yet (the tree walker runs it).
    .globl compile_fn
compile_fn:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    lhu t0, N_KIND(s0)
    li t1, N_TEST
    bne t0, t1, 1f
    ld s1, N_B(s0)              # test: body N_B, no parameters (a property: FX_PARAMS)
    li s3, 0
    lhu t0, N_FLAGS(s0)
    andi t0, t0, NF_PROP
    beqz t0, 2f
    ld t0, N_D(s0)
    ld t0, FX_PARAMS(t0)
    ld s3, VEC_N(t0)
    j 2f
1:
    ld s1, N_D(s0)              # fn / lambda: body N_D, parameters N_B
    ld t0, N_B(s0)
    ld s3, VEC_N(t0)
2:
    la t0, c_len
    sd zero, 0(t0)
    ld t1, N_E(s0)              # locals
    slli t1, t1, 3
    la t0, c_temp
    sd t1, 0(t0)
    la t0, c_max
    sd t1, 0(t0)
    la t0, c_fix
    ld t0, 0(t0)
    sd zero, VEC_N(t0)
    la t0, c_fail
    sd zero, 0(t0)
    la t0, c_brk
    sd zero, 0(t0)
    la t0, c_cont
    sd zero, 0(t0)
    la t0, tr_on                # spec 006: with --trace, every function starts with its call record
    ld t0, 0(t0)
    beqz t0, 5f
    li a0, OPC_TRACE_ENTER
    li a1, 0
    li a2, 0
    li a3, 0
    mv a4, s0
    call c_emit
5:
    mv a0, s0                   # contracts (spec 005): `requires` checks first
    call c_prologue
    mv a0, s1
    call cblock
    mv a0, s0                   # falling off the end returns 0 (or goes to the `ensures` checks)
    call c_fn_end
    la t0, c_fail
    ld t0, 0(t0)
    bnez t0, 9f
    la t0, c_len                # copy out: header + code
    ld a0, 0(t0)
    addi a0, a0, CF_HDR
    call c_code_alloc
    mv s2, a0
    addi a0, s2, CF_HDR
    la t0, c_buf
    ld a1, 0(t0)
    la t0, c_len
    ld a2, 0(t0)
    call mem_copy8
    addi t0, s2, CF_HDR
    sd t0, CF_CODE(s2)
    la t0, c_len
    ld t0, 0(t0)
    srli t0, t0, 5
    sd t0, CF_N(s2)
    la t0, c_max
    ld t0, 0(t0)
    srli t0, t0, 3
    sd t0, CF_SLOTS(s2)
    sd s3, CF_NPARAMS(s2)
    sd s0, CF_DECL(s2)
    la t0, c_fix                # code offsets in I_C become addresses
    ld t0, 0(t0)
    ld t1, VEC_N(t0)
    ld t2, VEC_DATA(t0)
    addi t3, s2, CF_HDR
3:
    beqz t1, 4f
    ld t4, 0(t2)
    add t4, t3, t4
    ld t5, I_C(t4)
    add t5, t5, t3
    sd t5, I_C(t4)
    addi t2, t2, 8
    addi t1, t1, -1
    j 3b
4:
    mv a0, s0
    call item_fx
    sd s2, FX_CODE(a0)
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# ---- contracts (spec 005 research R12, R13) ----

# void *c_fx_of(void *decl)
# The FX record of a named function, or 0 (lambdas and tests have no contracts).
c_fx_of:
    li t0, N_FN
    lhu t1, N_KIND(a0)
    bne t0, t1, 1f
    ld t0, N_A(a0)
    beqz t0, 1f
    ld a0, N_B(t0)
    ret
1:
    li a0, 0
    ret

# void c_prologue(void *decl)
# One jump per `requires`, taken when it is false (to a stub after the code); the epilogue state
# when the function has `ensures`. A function without contracts gets nothing.
c_prologue:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    sd s2, 0(sp)
    mv s2, a0
    la t0, c_ens
    sd zero, 0(t0)
    la t0, c_reqj
    sd zero, 0(t0)
    call c_fx_of
    beqz a0, 9f
    mv s0, a0
    ld t0, FX_REQUIRES(s0)
    ld t1, FX_ENSURES(s0)
    or t0, t0, t1
    beqz t0, 9f
    call c_new_temp
    la t0, c_exitslot
    sd a0, 0(t0)
    call vec_new
    la t0, c_reqj
    sd a0, 0(t0)
    ld t0, FX_REQUIRES(s0)
    beqz t0, 3f
    li s1, 0
1:
    ld t0, FX_REQUIRES(s0)
    ld t1, VEC_N(t0)
    bge s1, t1, 3f
    mv a0, t0
    mv a1, s1
    call vec_get
    call ccond
    mv a1, a0
    la t0, c_reqj
    ld a0, 0(t0)
    call vec_push
    addi s1, s1, 1
    j 1b
3:
    ld t0, FX_ENSURES(s0)
    beqz t0, 9f
    call vec_new
    la t0, c_ens
    sd a0, 0(t0)
    li t1, -1
    ld t0, N_TYPE(s2)
    ld t0, TY_KIND(t0)
    li t2, TY_VOID
    beq t0, t2, 4f
    ld t1, FX_RETSLOT(s0)
    slli t1, t1, 3
4:
    la t0, c_retslot
    sd t1, 0(t0)
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    ld s2, 0(sp)
    addi sp, sp, 32
    ret

# void c_ens_exit(long value_slot, void *exit_node)
# An exit of a function with `ensures`: ret = value, the exit node for R0013, then the epilogue.
c_ens_exit:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    mv s1, a1
    la t0, c_retslot
    ld a0, 0(t0)
    bltz a0, 1f
    bltz s0, 1f
    mv a1, s0
    mv a2, s1
    call c_mov
1:
    li a0, OPC_LOADK
    la t0, c_exitslot
    ld a1, 0(t0)
    li a2, 0
    mv a3, s1
    mv a4, s1
    call c_emit
    li a0, OPC_JMP
    li a1, 0
    li a2, 0
    mv a3, s1
    call c_jump
    mv a1, a0
    la t0, c_ens
    ld a0, 0(t0)
    call vec_push
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void *c_fail_node(long id, void *cond, void *decl)
# The node of a compiled contract failure: a builtin call (id B_REQ_FAIL / B_ENS_FAIL) with the
# condition's span, the condition in N_A and the function in N_C.
c_fail_node:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    sd s2, 0(sp)
    mv s0, a0
    mv s1, a1
    mv s2, a2
    li a0, NODE_SIZE
    call alloc
    li t0, N_CALL
    sh t0, N_KIND(a0)
    lwu t0, N_FILE(s1)
    sw t0, N_FILE(a0)
    lwu t0, N_START(s1)
    sw t0, N_START(a0)
    lwu t0, N_END(s1)
    sw t0, N_END(a0)
    sd s1, N_A(a0)
    sd s2, N_C(a0)
    sd s0, N_D(a0)
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    ld s2, 0(sp)
    addi sp, sp, 32
    ret

# void c_stubs(void *conds, void *jumps, long id, void *decl)
# For each condition: its false-jump lands on a CALL_BUILTIN id (argument: the exit-node slot).
c_stubs:
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
    mv s3, a3
    li s4, 0
1:
    ld t0, VEC_N(s1)
    bge s4, t0, 9f
    mv a0, s1
    mv a1, s4
    call vec_get
    sd a0, 0(sp)
    call c_here
    mv a1, a0
    ld a0, 0(sp)
    call c_target
    mv a0, s0
    mv a1, s4
    call vec_get
    mv a1, a0
    mv a0, s2
    mv a2, s3
    call c_fail_node
    mv a4, a0
    li a0, OPC_CALL_BUILTIN
    la t0, c_exitslot
    ld a1, 0(t0)
    mv a2, a1
    li a3, 1
    call c_emit
    addi s4, s4, 1
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

# void c_fn_end(void *decl)
# The end of a function: RET 0 (as before), or with contracts: a void function's fall-through
# goes to the epilogue, which checks each `ensures` and returns `ret`; the failure stubs follow.
c_fn_end:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    call c_fx_of
    mv s1, a0
    la t0, c_ens
    ld t0, 0(t0)
    beqz t0, 5f
    la t0, c_retslot            # a void function's closing `}` is an exit
    ld t0, 0(t0)
    bgez t0, 5f
    li a0, NODE_SIZE
    call alloc
    mv s2, a0
    ld t0, N_D(s0)
    lhu t1, N_KIND(t0)
    sh t1, N_KIND(s2)
    lwu t1, N_FILE(t0)
    sw t1, N_FILE(s2)
    lwu t1, N_END(t0)
    sw t1, N_END(s2)
    addi t1, t1, -1
    sw t1, N_START(s2)
    li a0, -1
    mv a1, s2
    call c_ens_exit
5:
    li a0, OPC_RET
    li a1, 0
    li a2, -1
    li a3, 0
    mv a4, s0
    call c_emit
    la t0, c_ens
    ld t0, 0(t0)
    beqz t0, 7f
    call c_here                 # the epilogue
    mv a1, a0
    la t0, c_ens
    ld a0, 0(t0)
    call c_patch_all
    call vec_new
    mv s3, a0
    li s2, 0
6:
    ld t0, FX_ENSURES(s1)
    ld t1, VEC_N(t0)
    bge s2, t1, 61f
    mv a0, t0
    mv a1, s2
    call vec_get
    call ccond
    mv a1, a0
    mv a0, s3
    call vec_push
    addi s2, s2, 1
    j 6b
61:
    li a0, OPC_RET
    li a1, 0
    la t0, c_retslot
    ld a2, 0(t0)
    li a3, 0
    mv a4, s0
    call c_emit
    ld a0, FX_ENSURES(s1)
    mv a1, s3
    li a2, B_ENS_FAIL
    mv a3, s0
    call c_stubs
7:
    la t0, c_reqj
    ld t0, 0(t0)
    beqz t0, 9f
    ld a0, FX_REQUIRES(s1)
    beqz a0, 9f
    la t0, c_reqj
    ld a1, 0(t0)
    li a2, B_REQ_FAIL
    mv a3, s0
    call c_stubs
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# long c_emit(long opc, long a, long b, long c, void *node)
# Appends one instruction; returns its offset in the buffer.
    .globl c_emit
c_emit:
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
    mv s3, a3
    mv s4, a4
    la t0, c_len
    ld t1, 0(t0)
    la t2, c_cap
    ld t3, 0(t2)
    addi t1, t1, I_SIZE
    bleu t1, t3, 1f
    slli a0, t3, 1              # grow: double into a new buffer
    sd a0, 8(sp)
    call c_code_alloc
    sd a0, 0(sp)
    la t0, c_buf
    ld a1, 0(t0)
    la t0, c_len
    ld a2, 0(t0)
    call mem_copy8
    la t0, c_buf
    ld t1, 0(sp)
    sd t1, 0(t0)
    la t0, c_cap
    ld t1, 8(sp)
    sd t1, 0(t0)
1:
    la t0, c_len
    ld t1, 0(t0)
    la t2, c_buf
    ld t2, 0(t2)
    add t2, t2, t1
    la t3, c_ops                # vm_ops, or vm_ops_trace with --trace (spec 006)
    ld t3, 0(t3)
    slli t4, s0, 3
    add t3, t3, t4
    ld t3, 0(t3)
    sd t3, I_HANDLER(t2)
    sw s1, I_A(t2)
    sw s2, I_B(t2)
    sd s3, I_C(t2)
    sd s4, I_NODE(t2)
    addi t3, t1, I_SIZE
    sd t3, 0(t0)
    mv a0, t1
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    addi sp, sp, 64
    ret

# long c_jump(long opc, long b, long target, void *node)
# Emits a jump (JMP, JT, JF, FOR_RANGE, FOR_LIST: a = 0) whose I_C is a code offset; returns it.
    .globl c_jump
c_jump:
    addi sp, sp, -16
    sd ra, 8(sp)
    mv a4, a3
    mv a3, a2
    mv a2, a1
    li a1, 0
    call c_emit
    sd a0, 0(sp)
    mv a1, a0
    la t0, c_fix
    ld a0, 0(t0)
    call vec_push
    ld a0, 0(sp)
    ld ra, 8(sp)
    addi sp, sp, 16
    ret

# void c_target(long at, long target)
# Sets the jump target (a code offset) of the instruction at offset at.
    .globl c_target
c_target:
    la t0, c_buf
    ld t0, 0(t0)
    add t0, t0, a0
    sd a1, I_C(t0)
    ret

# long c_here(void)
    .globl c_here
c_here:
    la t0, c_len
    ld a0, 0(t0)
    ret

# void c_patch_all(void *vec, long target)
# Points every jump in vec at target.
    .globl c_patch_all
c_patch_all:
    beqz a0, 2f
    ld t1, VEC_N(a0)
    ld t2, VEC_DATA(a0)
    la t0, c_buf
    ld t0, 0(t0)
1:
    beqz t1, 2f
    ld t3, 0(t2)
    add t3, t0, t3
    sd a1, I_C(t3)
    addi t2, t2, 8
    addi t1, t1, -1
    j 1b
2:
    ret

# long c_new_temp(void)
# A fresh temporary slot (bytes); the frame grows to fit it.
    .globl c_new_temp
c_new_temp:
    la t0, c_temp
    ld a0, 0(t0)
    addi t1, a0, 8
    sd t1, 0(t0)
    la t2, c_max
    ld t3, 0(t2)
    bleu t1, t3, 1f
    sd t1, 0(t2)
1:
    ret

# long c_dst(long dst)
# The slot an expression writes its value to: dst, or a fresh temporary when dst < 0.
    .globl c_dst
c_dst:
    bltz a0, c_new_temp
    ret

# void c_unsupported(void)
    .globl c_unsupported
c_unsupported:
    la t0, c_fail
    li t1, 1
    sd t1, 0(t0)
    ret

# void c_mov(long dst, long src, void *node)
# dst = src unless they are the same slot.
    .globl c_mov
c_mov:
    beq a0, a1, 1f
    mv a4, a2
    mv a2, a1
    mv a1, a0
    li a0, OPC_MOV
    li a3, 0
    tail c_emit
1:
    ret

# ---- statements ----

# void cblock(void *block)
    .globl cblock
cblock:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    ld s0, N_A(a0)
    li s1, 0
1:
    ld t0, VEC_N(s0)
    bge s1, t0, 9f
    mv a0, s0
    mv a1, s1
    call vec_get
    addi s1, s1, 1
    call cstmt
    j 1b
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void cstmt(void *stmt)
# Temporaries are free again after every statement.
    .globl cstmt
cstmt:
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
    la t0, c_temp
    ld s6, 0(t0)                # temporaries mark
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
    li t1, N_RETURN
    beq t0, t1, 17f
    li t1, N_BREAK
    beq t0, t1, 18f
    li t1, N_CONTINUE
    beq t0, t1, 19f
    li t1, N_MATCH
    bne t0, t1, 99f             # anything else does nothing (as before spec 004)
    ld a0, N_A(s0)              # match: the scrutinee once, then the arms in source order
    li a1, -1
    call cexpr
    mv s1, a0
    call vec_new
    mv s4, a0                   # jumps to the end
    li s2, 0
161:
    ld t0, N_B(s0)
    ld t1, VEC_N(t0)
    bge s2, t1, 169f
    mv a0, t0
    mv a1, s2
    call vec_get
    addi s2, s2, 1
    mv s3, a0
    lhu t0, N_KIND(s3)
    li t1, N_ARM
    bne t0, t1, 161b
    li a0, OPC_JTAG_NE          # not this case: the next arm
    ld a1, N_D(s3)
    mv a2, s1
    li a3, 0
    mv a4, s3
    call c_emit
    mv s5, a0
    la t0, c_fix
    ld a0, 0(t0)
    mv a1, s5
    call vec_push
    ld t0, N_E(s3)              # the binding
    bltz t0, 162f
    li a0, OPC_PAYLOAD
    slli a1, t0, 3
    mv a2, s1
    li a3, 0
    mv a4, s3
    call c_emit
162:
    ld a0, N_C(s3)
    call cblock
    li a0, OPC_JMP
    li a1, 0
    li a2, 0
    mv a3, s3
    call c_jump
    mv a1, a0
    mv a0, s4
    call vec_push
    call c_here
    mv a1, a0
    mv a0, s5
    call c_target
    j 161b
169:
    call c_here
    mv a1, a0
    mv a0, s4
    call c_patch_all
    j 99f
10:                             # expression statement
    ld a0, N_A(s0)
    li a1, -1
    call cexpr
    j 99f
11:                             # let / var: into the local's slot (E < 0: `_`)
    ld s1, N_E(s0)
    bltz s1, 111f
    slli s1, s1, 3
    ld a0, N_C(s0)
    mv a1, s1
    call cexpr
    mv a1, a0
    mv a0, s1
    mv a2, s0
    call c_mov
    j 99f
111:
    ld a0, N_C(s0)
    li a1, -1
    call cexpr
    j 99f
12:                             # set
    ld s1, N_A(s0)
    lhu t0, N_KIND(s1)
    li t1, N_IDENT
    beq t0, t1, 121f
    li t1, N_FIELD
    beq t0, t1, 122f
    ld a0, N_A(s1)              # index: object, key/index, value, then the check
    li a1, -1
    call cexpr
    mv s2, a0
    ld a0, N_B(s1)
    li a1, -1
    call cexpr
    mv s3, a0
    ld a0, N_B(s0)
    li a1, -1
    call cexpr
    mv a3, a0
    mv a1, s2
    mv a2, s3
    lhu t0, N_FLAGS(s1)
    andi t0, t0, NF_MAP
    li a0, OPC_SET_INDEX_LIST
    mv a4, s1                   # R0003 at the place
    beqz t0, 123f
    li a0, OPC_SET_INDEX_MAP
    mv a4, s0                   # cur_node: the statement
123:
    call c_emit
    j 99f
122:                            # field: object, value, store
    ld a0, N_A(s1)
    li a1, -1
    call cexpr
    mv s2, a0
    ld a0, N_B(s0)
    li a1, -1
    call cexpr
    mv a2, a0
    li a0, OPC_SET_FIELD
    mv a1, s2
    ld a3, N_C(s1)
    slli a3, a3, 3
    addi a3, a3, R_FIELDS
    mv a4, s0
    call c_emit
    j 99f
121:
    ld s2, N_C(s1)
    slli s2, s2, 3
    ld a0, N_B(s0)
    mv a1, s2
    call cexpr
    mv a1, a0
    mv a0, s2
    mv a2, s0
    call c_mov
    j 99f
13:                             # if c { } else ...
    ld a0, N_A(s0)
    call ccond
    mv s1, a0                   # to the else part
    la t0, c_temp
    sd s6, 0(t0)
    ld a0, N_B(s0)
    call cblock
    ld s2, N_C(s0)
    beqz s2, 132f
    li a0, OPC_JMP
    li a1, 0
    li a2, 0
    mv a3, s0
    call c_jump
    mv s3, a0                   # over the else part
    call c_here
    mv a1, a0
    mv a0, s1
    call c_target
    mv a0, s2
    lhu t0, N_KIND(a0)
    li t1, N_IF
    beq t0, t1, 131f
    call cblock
    j 133f
131:
    call cstmt
133:
    call c_here
    mv a1, a0
    mv a0, s3
    call c_target
    j 99f
132:
    call c_here
    mv a1, a0
    mv a0, s1
    call c_target
    j 99f
14:                             # while: a step, the condition, the body
    la t0, c_brk                # the outer loop's lists, restored by c_loop_leave
    ld s4, 0(t0)
    la t0, c_cont
    ld s5, 0(t0)
    call c_loop_enter
    call c_here
    mv s1, a0                   # top
    li a0, OPC_TICK
    li a1, 0
    li a2, 0
    li a3, 0
    mv a4, s0
    call c_emit
    ld a0, N_A(s0)
    call ccond
    mv s2, a0                   # exit
    la t0, c_temp
    sd s6, 0(t0)
    ld a0, N_B(s0)
    call cblock
    li a0, OPC_JMP
    li a1, 0
    mv a2, s1
    mv a3, s0
    call c_jump
    call c_here
    mv s3, a0                   # exit
    mv a0, s2
    mv a1, s3
    call c_target
    mv a0, s1                   # continue: back to the step
    mv a1, s3
    mv a2, s4
    mv a3, s5
    call c_loop_leave
    j 99f
15:                             # for: hidden counter/list and end/index in two temporaries
    la t0, c_brk                # the outer loop's lists, restored by c_loop_leave
    ld s4, 0(t0)
    la t0, c_cont
    ld s5, 0(t0)
    call c_loop_enter
    call c_new_temp
    mv s1, a0                   # counter | list
    call c_new_temp             # end | index (s1 + 8)
    ld t0, N_C(s0)
    beqz t0, 155f
    ld a0, N_B(s0)              # range: start, then end, each once
    mv a1, s1
    call cexpr
    mv a1, a0
    mv a0, s1
    mv a2, s0
    call c_mov
    ld a0, N_C(s0)
    addi a1, s1, 8
    call cexpr
    mv a1, a0
    addi a0, s1, 8
    mv a2, s0
    call c_mov
    li s3, OPC_FOR_RANGE
    j 156f
155:
    ld a0, N_B(s0)              # list: evaluated once; the index starts at 0
    mv a1, s1
    call cexpr
    mv a1, a0
    mv a0, s1
    mv a2, s0
    call c_mov
    li a0, OPC_LOADK
    addi a1, s1, 8
    li a2, 0
    li a3, 0
    mv a4, s0
    call c_emit
    li s3, OPC_FOR_LIST
156:
    mv a0, s3                   # the head: the first test (its exit target comes later)
    ld a1, N_E(s0)
    slli a1, a1, 3
    mv a2, s1
    li a3, 0
    mv a4, s0
    call c_emit
    mv s3, a0
    la t0, c_fix
    ld a0, 0(t0)
    mv a1, s3
    call vec_push
    call c_here
    mv s2, a0                   # the body
    ld a0, N_D(s0)
    call cblock
    call c_here
    sd a0, 0(sp)                # continue: the back-edge (step, test, next value, jump)
    li a0, OPC_FOR_STEP
    ld t1, N_C(s0)
    bnez t1, 157f
    li a0, OPC_FOR_LIST_STEP
157:
    ld a1, N_E(s0)
    slli a1, a1, 3
    mv a2, s1
    mv a3, s2
    mv a4, s0
    call c_emit
    mv a1, a0
    la t0, c_fix
    ld a0, 0(t0)
    call vec_push
    call c_here
    sd a0, 8(sp)                # exit
    mv a0, s3
    ld a1, 8(sp)
    call c_target
    ld a0, 0(sp)
    ld a1, 8(sp)
    mv a2, s4
    mv a3, s5
    call c_loop_leave
    j 99f
17:                             # return [e]
    li s1, -1
    ld a0, N_A(s0)
    beqz a0, 171f
    li a1, -1
    call cexpr
    mv s1, a0
171:
    la t0, c_ens                # with `ensures`: to the epilogue
    ld t0, 0(t0)
    beqz t0, 172f
    mv a0, s1
    mv a1, s0
    call c_ens_exit
    j 99f
172:
    li a0, OPC_RET
    li a1, 0
    mv a2, s1
    li a3, 0
    mv a4, s0
    call c_emit
    j 99f
18:                             # break
    la t0, c_brk
    ld s1, 0(t0)
    j 191f
19:                             # continue
    la t0, c_cont
    ld s1, 0(t0)
191:
    li a0, OPC_JMP
    li a1, 0
    li a2, 0
    mv a3, s0
    call c_jump
    mv a1, a0
    mv a0, s1
    call vec_push
99:
    la t0, c_temp
    sd s6, 0(t0)
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

# void c_loop_enter(void)
# New break/continue lists for a loop (the caller keeps the outer ones).
    .globl c_loop_enter
c_loop_enter:
    addi sp, sp, -16
    sd ra, 8(sp)
    call vec_new
    la t0, c_brk
    sd a0, 0(t0)
    call vec_new
    la t0, c_cont
    sd a0, 0(t0)
    ld ra, 8(sp)
    addi sp, sp, 16
    ret

# void c_loop_leave(long cont, long exit, void *outer_brk, void *outer_cont)
# Points the loop's continue jumps at cont and break jumps at exit; restores the outer lists.
    .globl c_loop_leave
c_loop_leave:
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
    la t0, c_cont
    ld a0, 0(t0)
    mv a1, s0
    call c_patch_all
    la t0, c_brk
    ld a0, 0(t0)
    mv a1, s1
    call c_patch_all
    la t0, c_brk
    sd s2, 0(t0)
    la t0, c_cont
    sd s3, 0(t0)
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# ---- expressions ----

# int c_fits32(long v)
# 1 when v survives a round trip through 32 bits (a _K instruction's constant).
    .globl c_fits32
c_fits32:
    sext.w t0, a0
    sub t0, t0, a0
    seqz a0, t0
    ret

# long c_const_op(void *binary)
# The _K opcode for `x op c` (op + - * / % on int, c an int literal that fits in 32 bits; / not
# by 0 or -1, % not by 0, so the checks of the slot form stay where they can fire), else -1.
    .globl c_const_op
c_const_op:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    ld s0, N_D(a0)
    ld t0, N_B(a0)
    lhu t1, N_KIND(t0)
    li t2, N_INT
    bne t1, t2, 8f
    li t2, OP_REM_INT
    bgt s0, t2, 8f
    li t2, OP_ADD_INT
    blt s0, t2, 8f
    ld a0, N_A(t0)
    beqz a0, 1f                 # 0: only + - * may use it
    li t1, -1
    bne a0, t1, 2f
    li t2, OP_DIV_INT           # -1: not /
    beq s0, t2, 8f
    j 2f
1:
    li t2, OP_DIV_INT
    bge s0, t2, 8f
2:
    call c_fits32
    beqz a0, 8f
    addi a0, s0, OPC_ADD_INT_K - OP_ADD_INT
    j 9f
8:
    li a0, -1
9:
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret

# int c_int_eq(void *binary)
# 1 when `==` / `!=` compares int or bool operands (EQ_INT: no call needed).
    .globl c_int_eq
c_int_eq:
    ld t0, N_A(a0)
    ld t0, N_TYPE(t0)
    li a0, 0
    beqz t0, 9f
    ld t0, TY_KIND(t0)
    li t1, TY_INT
    beq t0, t1, 1f
    li t1, TY_BOOL
    bne t0, t1, 9f
1:
    li a0, 1
9:
    ret

# long ccond(void *cond)
# Compiles a condition into a jump taken when it is false; returns the jump's offset (its
# target comes later). Int comparisons fuse into one compare-and-branch (US1).
    .globl ccond
ccond:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    lhu t0, N_KIND(s0)
    li t1, N_BINARY
    bne t0, t1, 7f
    ld t0, N_D(s0)
    li t1, OP_LT_INT            # < <= > >= on int: the opposite jump
    blt t0, t1, 1f
    li t1, OP_GE_INT
    bgt t0, t1, 7f
    la t1, c_inverse
    addi t0, t0, -OP_LT_INT
    add t1, t1, t0
    lbu s1, 0(t1)
    j 3f
1:
    li t1, OP_EQ
    li s1, OPC_JNE
    beq t0, t1, 2f
    li t1, OP_NE
    li s1, OPC_JEQ
    bne t0, t1, 7f
2:
    mv a0, s0
    call c_int_eq
    beqz a0, 7f
3:
    ld a0, N_A(s0)
    li a1, -1
    call cexpr
    mv s2, a0
    ld t0, N_B(s0)              # a constant right side
    lhu t1, N_KIND(t0)
    li t2, N_INT
    bne t1, t2, 4f
    ld a0, N_A(t0)
    call c_fits32
    beqz a0, 4f
    ld t0, N_B(s0)
    ld s3, N_A(t0)
    addi s1, s1, OPC_JLT_K - OPC_JLT
    j 5f
4:
    ld a0, N_B(s0)
    li a1, -1
    call cexpr
    mv s3, a0
5:
    mv a0, s1
    mv a1, s2
    mv a2, s3
    li a3, 0
    mv a4, s0
    call c_emit
    mv s1, a0
    la t0, c_fix
    ld a0, 0(t0)
    mv a1, s1
    call vec_push
    mv a0, s1
    j 9f
7:
    mv a0, s0                   # any other condition: compute it, jump when false
    li a1, -1
    call cexpr
    mv a1, a0
    li a0, OPC_JF
    li a2, 0
    mv a3, s0
    call c_jump
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

    .section .rodata
c_inverse:  .byte OPC_JGE, OPC_JGT, OPC_JLE, OPC_JLT    # for < <= > >=: jump when false
    .text

# long cexpr(void *node, long dst)
# Compiles an expression; returns the slot holding its value (dst < 0: any).
    .globl cexpr
cexpr:
    addi sp, sp, -96            # 0..39: scratch (16: a value call's callee, 24: the first argument)
    sd ra, 88(sp)
    sd s0, 80(sp)
    sd s1, 72(sp)
    sd s2, 64(sp)
    sd s3, 56(sp)
    sd s4, 48(sp)
    sd s5, 40(sp)
    mv s0, a0
    mv s1, a1
    la t0, c_temp
    ld s5, 0(t0)                # temporaries mark
    lhu t0, N_KIND(s0)
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
    li t1, N_UNARY
    beq t0, t1, 7f
    li t1, N_CALL
    beq t0, t1, 4f
    li t1, N_FIELD
    beq t0, t1, 50f
    li t1, N_INDEX
    beq t0, t1, 51f
    li t1, N_LIST
    beq t0, t1, 52f
    li t1, N_NEW_RECORD
    beq t0, t1, 53f
    li t1, N_NEW_LIST
    beq t0, t1, 54f
    li t1, N_NEW_MAP
    beq t0, t1, 55f
    li t1, N_VCASE
    beq t0, t1, 56f
    li t1, N_TRY
    beq t0, t1, 57f
    li t1, N_LAMBDA
    beq t0, t1, 58f
    li t1, N_CALL_VALUE
    beq t0, t1, 59f
    call c_unsupported
    li a0, 0
    j 99f
50:                             # r.f
    ld a0, N_A(s0)
    li a1, -1
    call cexpr
    mv s2, a0
    la t0, c_temp
    sd s5, 0(t0)
    mv a0, s1
    call c_dst
    mv s1, a0
    li a0, OPC_GET_FIELD
    mv a1, s1
    mv a2, s2
    ld a3, N_C(s0)
    slli a3, a3, 3
    addi a3, a3, R_FIELDS
    mv a4, s0
    call c_emit
    mv a0, s1
    j 99f
51:                             # xs[i] / m[k]
    ld a0, N_A(s0)
    li a1, -1
    call cexpr
    mv s2, a0
    ld a0, N_B(s0)
    li a1, -1
    call cexpr
    mv s3, a0
    la t0, c_temp
    sd s5, 0(t0)
    mv a0, s1
    call c_dst
    mv s1, a0
    lhu t0, N_FLAGS(s0)
    andi t0, t0, NF_MAP
    li a0, OPC_INDEX_LIST
    beqz t0, 511f
    li a0, OPC_INDEX_MAP
511:
    mv a1, s1
    mv a2, s2
    mv a3, s3
    mv a4, s0
    call c_emit
    mv a0, s1
    j 99f
52:                             # [a, b, c]: the list first, then each element pushed
    call c_new_temp
    mv s1, a0
    li a0, OPC_NEW_LIST
    mv a1, s1
    li a2, 0
    ld t0, N_A(s0)
    ld a3, VEC_N(t0)
    mv a4, s0
    call c_emit
    li s2, 0
521:
    ld t0, N_A(s0)
    ld t1, VEC_N(t0)
    bge s2, t1, 529f
    mv a0, t0
    mv a1, s2
    call vec_get
    li a1, -1
    call cexpr
    mv a2, a0
    li a0, OPC_LIST_PUSH
    mv a1, s1
    li a3, 0
    mv a4, s0
    call c_emit
    addi t0, s1, 8
    la t1, c_temp
    sd t0, 0(t1)
    addi s2, s2, 1
    j 521b
529:
    mv a0, s1
    j 99f
53:                             # new R { ... }: the record first, then the initialisers in order
    call c_new_temp
    mv s1, a0
    ld t0, N_C(s0)
    ld a0, N_B(t0)
    call count_members
    mv a3, a0
    li a0, OPC_NEW_RECORD
    mv a1, s1
    li a2, 0
    mv a4, s0
    call c_emit
    li s2, 0
531:
    ld t0, N_B(s0)
    ld t1, VEC_N(t0)
    bge s2, t1, 529b
    mv a0, t0
    mv a1, s2
    call vec_get
    mv s3, a0
    ld a0, N_B(s3)
    li a1, -1
    call cexpr
    mv a2, a0
    li a0, OPC_SET_FIELD
    mv a1, s1
    ld a3, N_C(s3)
    slli a3, a3, 3
    addi a3, a3, R_FIELDS
    mv a4, s3
    call c_emit
    addi t0, s1, 8
    la t1, c_temp
    sd t0, 0(t1)
    addi s2, s2, 1
    j 531b
54:                             # new list[T]
    mv a0, s1
    call c_dst
    mv s1, a0
    li a0, OPC_NEW_LIST
    mv a1, s1
    li a2, 0
    li a3, 4
    mv a4, s0
    call c_emit
    mv a0, s1
    j 99f
55:                             # new map[K, V]
    mv a0, s1
    call c_dst
    mv s1, a0
    li a0, OPC_NEW_MAP
    mv a1, s1
    li a2, 0
    li a3, 0
    mv a4, s0
    call c_emit
    mv a0, s1
    j 99f
56:                             # V.case(payload): the variant first, then the payload
    call c_new_temp
    mv s1, a0
    li a0, OPC_NEW_VARIANT
    mv a1, s1
    li a2, 0
    ld a3, N_E(s0)
    mv a4, s0
    call c_emit
    ld a0, N_C(s0)
    beqz a0, 529b
    li a1, -1
    call cexpr
    mv a2, a0
    li a0, OPC_SET_PAYLOAD
    mv a1, s1
    li a3, 0
    mv a4, s0
    call c_emit
    addi t0, s1, 8
    la t1, c_temp
    sd t0, 0(t1)
    j 529b
57:                             # try e
    ld a0, N_A(s0)
    li a1, -1
    call cexpr
    mv s2, a0
    la t0, c_temp
    sd s5, 0(t0)
    mv a0, s1
    call c_dst
    mv s1, a0
    la t0, c_ens                # with `ensures`: an err goes to the epilogue
    ld t0, 0(t0)
    bnez t0, 571f
    li a0, OPC_TRY
    mv a1, s1
    mv a2, s2
    li a3, 0
    mv a4, s0
    call c_emit
    mv a0, s1
    j 99f
571:
    li a0, OPC_JTAG_NE          # not err: take the payload below
    li a1, 1
    mv a2, s2
    li a3, 0
    mv a4, s0
    call c_emit
    sd a0, 0(sp)
    mv a1, a0
    la t0, c_fix
    ld a0, 0(t0)
    call vec_push
    mv a0, s2
    mv a1, s0
    call c_ens_exit
    ld a0, 0(sp)
    call c_here
    mv a1, a0
    ld a0, 0(sp)
    call c_target
    li a0, OPC_PAYLOAD
    mv a1, s1
    mv a2, s2
    li a3, 0
    mv a4, s0
    call c_emit
    mv a0, s1
    j 99f
58:                             # fn(...) { ... }: a closure; the body is compiled later
    la t0, c_lams
    ld a0, 0(t0)
    mv a1, s0
    call vec_push
    mv a0, s1
    call c_dst
    mv s1, a0
    li a0, OPC_CLOSURE
    mv a1, s1
    li a2, 0
    li a3, 0
    mv a4, s0
    call c_emit
    mv a0, s1
    j 99f
59:                             # callee(args): the callee, then the arguments, then the call
    ld a0, N_A(s0)
    li a1, -1
    call cexpr
    sd a0, 16(sp)
    li s4, OPC_CALL_VALUE
    j 40f
1:                              # constant
    mv a0, s1
    call c_dst
    mv s2, a0
    li a0, OPC_LOADK
    mv a1, s2
    li a2, 0
    ld a3, N_A(s0)
    mv a4, s0
    call c_emit
    mv a0, s2
    j 99f
2:                              # a local: its own slot
    lhu t0, N_FLAGS(s0)
    andi t0, t0, NF_FNVAL
    beqz t0, 21f
    mv a0, s1                   # a named function as a value
    call c_dst
    mv s1, a0
    li a0, OPC_FNVAL
    mv a1, s1
    li a2, 0
    ld a3, N_D(s0)
    mv a4, s0
    call c_emit
    mv a0, s1
    j 99f
21:
    ld a0, N_C(s0)
    slli a0, a0, 3
    j 99f
3:                              # binary
    ld s4, N_D(s0)
    li t0, OP_AND
    beq s4, t0, 30f
    li t0, OP_OR
    beq s4, t0, 30f
    mv a0, s0                   # x op constant: one instruction (US1)
    call c_const_op
    bltz a0, 32f
    mv s3, a0
    ld a0, N_A(s0)
    li a1, -1
    call cexpr
    mv s2, a0
    la t0, c_temp
    sd s5, 0(t0)
    mv a0, s1
    call c_dst
    mv s1, a0
    mv a1, a0
    mv a0, s3
    mv a2, s2
    ld a3, N_B(s0)
    ld a3, N_A(a3)
    mv a4, s0
    call c_emit
    mv a0, s1
    j 99f
32:
    ld a0, N_A(s0)
    li a1, -1
    call cexpr
    mv s2, a0
    ld a0, N_B(s0)
    li a1, -1
    call cexpr
    mv s3, a0
    la t0, c_temp               # operands read, then the result written: temporaries reusable
    sd s5, 0(t0)
    mv a0, s1
    call c_dst
    mv s1, a0
    mv a0, s0
    call c_int_eq
    mv t2, a0
    mv a1, s1
    addi a0, s4, OPC_ADD_INT - OP_ADD_INT
    li t0, OP_LT_PARAM
    blt s4, t0, 33f
    li a0, OPC_CMP_PARAM
33:
    li t0, OP_EQ
    bne s4, t0, 34f
    beqz t2, 34f
    li a0, OPC_EQ_INT
34:
    li t0, OP_NE
    bne s4, t0, 31f
    beqz t2, 31f
    li a0, OPC_NE_INT
31:
    mv a2, s2
    mv a3, s3
    mv a4, s0
    call c_emit
    mv a0, s1
    j 99f
30:                             # and / or: a fresh temporary, a jump over the right side
    call c_new_temp
    mv s2, a0
    ld a0, N_A(s0)
    mv a1, s2
    call cexpr
    mv a1, a0
    mv a0, s2
    mv a2, s0
    call c_mov
    li a0, OPC_JF
    li t0, OP_AND
    beq s4, t0, 301f
    li a0, OPC_JT
301:
    mv a1, s2
    li a2, 0
    mv a3, s0
    call c_jump
    mv s3, a0
    ld a0, N_B(s0)
    mv a1, s2
    call cexpr
    mv a1, a0
    mv a0, s2
    mv a2, s0
    call c_mov
    call c_here
    mv a1, a0
    mv a0, s3
    call c_target
    addi t0, s2, 8
    la t1, c_temp
    sd t0, 0(t1)
    mv a0, s2
    j 99f
7:                              # unary
    ld a0, N_A(s0)
    li a1, -1
    call cexpr
    mv s2, a0
    la t0, c_temp
    sd s5, 0(t0)
    mv a0, s1
    call c_dst
    mv s3, a0
    ld t0, N_D(s0)
    li a0, OPC_NEG_INT
    li t1, OP_NOT
    bne t0, t1, 71f
    li a0, OPC_NOT
71:
    li t1, OP_NEG_FLOAT
    bne t0, t1, 72f
    li a0, OPC_NEG_FLOAT
72:
    mv a1, s3
    mv a2, s2
    li a3, 0
    mv a4, s0
    call c_emit
    mv a0, s3
    j 99f
4:                              # call: arguments into consecutive temporaries
    li s4, OPC_CALL
    ld t0, N_D(s0)
    beqz t0, 40f
    li s4, OPC_CALL_BUILTIN
    li t1, BI_LEN               # len and push: their own instructions, on the argument slots
    beq t0, t1, 48f
    li t1, BI_PUSH
    beq t0, t1, 49f
    li t1, BI_HAS
    beq t0, t1, 39f
    j 38f
40:
    ld s2, N_B(s0)
    ld s3, VEC_N(s2)
    li t1, OPC_CALL_BUILTIN
    bne s4, t1, 41f
    li t1, 3                    # builtins take at most three
    ble s3, t1, 41f
    li s3, 3
41:
    la t0, c_temp
    ld t0, 0(t0)
    sd t0, 24(sp)               # the first argument's slot
    li t1, 0
42:
    bge t1, s3, 43f
    sd t1, 0(sp)
    call c_new_temp
    ld t1, 0(sp)
    addi t1, t1, 1
    j 42b
43:
    sd zero, 0(sp)
44:
    ld t1, 0(sp)
    bge t1, s3, 45f
    mv a0, s2
    mv a1, t1
    call vec_get
    ld t1, 0(sp)
    slli a1, t1, 3
    ld t0, 24(sp)
    add a1, t0, a1
    sd a1, 8(sp)
    call cexpr
    mv a1, a0
    ld a0, 8(sp)
    mv a2, s0
    call c_mov
    ld t1, 0(sp)
    addi t1, t1, 1
    sd t1, 0(sp)
    j 44b
45:
    la t0, c_temp               # the arguments are read by the call itself
    sd s5, 0(t0)
    mv a0, s1
    call c_dst
    mv s2, a0
    mv a0, s4
    mv a1, s2
    ld a2, 24(sp)               # the first argument's slot
    mv a3, s3                   # CALL_BUILTIN: the argument count
    li t0, OPC_CALL
    bne s4, t0, 46f
    ld a3, N_C(s0)              # CALL: the declaration
    ld t1, N_E(s0)
    li a0, OPC_CALL_FAST
    beqz t1, 46f
    sd a1, 0(sp)                # generic: a call-site record holds the callee
    sd a2, 8(sp)
    sd a3, 32(sp)
    li a0, GS_SIZE
    call c_code_alloc
    ld a3, 32(sp)
    sd a3, GS_DECL(a0)
    sd zero, GS_VALID(a0)
    mv a3, a0
    ld a1, 0(sp)
    ld a2, 8(sp)
    li a0, OPC_CALL_GENERIC
46:
    li t0, OPC_CALL_VALUE
    bne s4, t0, 47f
    ld a3, 16(sp)               # CALL_VALUE: the callee's slot
47:
    mv a4, s0
    call c_emit
    mv a0, s2
    j 99f
48:                             # len(x)
    ld a0, N_B(s0)
    ld a0, VEC_DATA(a0)
    ld a0, 0(a0)
    li a1, -1
    call cexpr
    mv s2, a0
    la t0, c_temp
    sd s5, 0(t0)
    mv a0, s1
    call c_dst
    mv s1, a0
    li a0, OPC_LEN
    mv a1, s1
    mv a2, s2
    li a3, 0
    mv a4, s0
    call c_emit
    mv a0, s1
    j 99f
38:                             # any other builtin: up to three arguments, each read in place
    sd zero, 0(sp)              # 0/8/16(sp): the argument slots; 32(sp): the index
    sd zero, 8(sp)
    sd zero, 16(sp)
    ld s2, N_B(s0)
    ld s3, VEC_N(s2)
    li t1, 3                    # builtins take at most three
    ble s3, t1, 381f
    li s3, 3
381:
    sd zero, 32(sp)
382:
    ld t1, 32(sp)
    bge t1, s3, 383f
    mv a0, s2
    mv a1, t1
    call vec_get
    li a1, -1
    call cexpr
    ld t1, 32(sp)
    slli t2, t1, 3
    add t2, sp, t2
    sd a0, 0(t2)
    addi t1, t1, 1
    sd t1, 32(sp)
    j 382b
383:
    la t0, c_temp               # the arguments are read by the call itself
    sd s5, 0(t0)
    mv a0, s1
    call c_dst
    mv s1, a0
    ld a3, 8(sp)                # c: count | second slot << 16 | third slot << 40
    slli a3, a3, 16
    ld t0, 16(sp)
    slli t0, t0, 40
    or a3, a3, t0
    or a3, a3, s3
    li a0, OPC_CALL_BUILTIN
    mv a1, s1
    ld a2, 0(sp)
    mv a4, s0
    call c_emit
    mv a0, s1
    j 99f
39:                             # has(m, k)
    ld a0, N_B(s0)
    ld a0, VEC_DATA(a0)
    ld a0, 0(a0)
    li a1, -1
    call cexpr
    mv s2, a0
    ld a0, N_B(s0)
    ld a0, VEC_DATA(a0)
    ld a0, 8(a0)
    li a1, -1
    call cexpr
    mv s3, a0
    la t0, c_temp
    sd s5, 0(t0)
    mv a0, s1
    call c_dst
    mv s1, a0
    li a0, OPC_HAS
    mv a1, s1
    mv a2, s2
    mv a3, s3
    mv a4, s0
    call c_emit
    mv a0, s1
    j 99f
49:                             # push(xs, v): no value
    ld a0, N_B(s0)
    ld a0, VEC_DATA(a0)
    ld a0, 0(a0)
    li a1, -1
    call cexpr
    mv s2, a0
    ld a0, N_B(s0)
    ld a0, VEC_DATA(a0)
    ld a0, 8(a0)
    li a1, -1
    call cexpr
    mv a2, a0
    li a0, OPC_PUSH
    mv a1, s2
    li a3, 0
    mv a4, s0
    call c_emit
    la t0, c_temp
    sd s5, 0(t0)
    mv a0, s1                   # (a void value: any slot)
    call c_dst
    j 99f
99:
    ld ra, 88(sp)
    ld s0, 80(sp)
    ld s1, 72(sp)
    ld s2, 64(sp)
    ld s3, 56(sp)
    ld s4, 48(sp)
    ld s5, 40(sp)
    addi sp, sp, 96
    ret

