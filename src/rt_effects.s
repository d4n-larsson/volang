# rt_effects.s: run-time effect enforcement and the effect builtins (research §2, §9).
#
# Every effectful builtin is checked against FR_EFFECTS of the innermost frame: the effect bit
# (defense in depth; checked programs always pass) and, for file and env effects, the scopes.
# Paths are resolved like realpath: absolute against the start directory, `.`/`..` applied,
# symbolic links followed (at most 40); a missing tail stays lexical. A path is allowed when it
# lies under a resolved scope.

    .include "defs.inc"

    .equ PATH_MAX, 4096

    .section .bss
    .align 3
start_cwd:  .space 8                # str: the working directory at the first check ("" = /)
res_len:    .space 8
res_buf:    .space PATH_MAX + 8
link_buf:   .space PATH_MAX + 8
pcg_state:  .space 8
pcg_inc:    .space 8
    .globl opt_seed
opt_seed:   .space 8
    .globl opt_seed_set
opt_seed_set: .space 8
    .globl opt_clock
opt_clock:  .space 8
    .globl opt_clock_set
opt_clock_set: .space 8
clock_next: .space 8

    .section .rodata
r0010:      .string "R0010"
r0007:      .string "R0007"
m_denied:   .string "$1 is not allowed: `$2` declares $3."
m_range:    .string "`random_int` needs lo <= hi, got $1 and $2."
m_not_set:  .string "not set"
s_pure:     .string "nothing (pure)"
s_lpq:      .string "(\""
s_rpq:      .string "\")"
j_effect:   .string ",\"effect\":\""
j_target:   .string "\",\"target\":"
j_allowed:  .string ",\"allowed\":["
j_declared: .string "],\"declared_by\":"
s_null:     .string "null"

    .text

# void rt_set_option(long which, long value)
# which 1: --seed, 2: --clock-ms.
    .globl rt_set_option
rt_set_option:
    li t0, 1
    bne a0, t0, 1f
    la t0, opt_seed
    sd a1, 0(t0)
    la t0, opt_seed_set
    li t1, 1
    sd t1, 0(t0)
    ret
1:
    la t0, opt_clock
    sd a1, 0(t0)
    la t0, opt_clock_set
    li t1, 1
    sd t1, 0(t0)
    ret

# void rt_effects_start(int is_test)
# Seeds the generator and sets the clock for a run, or for one test (defaults: seed 1, clock 0).
    .globl rt_effects_start
rt_effects_start:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    mv s0, a0
    la t0, opt_clock_set
    ld t0, 0(t0)
    bnez t0, 1f
    beqz s0, 2f
    la t0, clock_next           # tests: a fixed clock from 0
    sd zero, 0(t0)
    j 3f
1:
    la t0, opt_clock
    ld t0, 0(t0)
    la t1, clock_next
    sd t0, 0(t1)
3:
    la t0, clock_fixed
    li t1, 1
    sd t1, 0(t0)
2:
    la t0, opt_seed_set
    ld t0, 0(t0)
    li a0, 1
    beqz t0, 4f
    la t0, opt_seed
    ld a0, 0(t0)
    j 5f
4:
    bnez s0, 5f                 # tests: seed 1
    call sys_clock_ns           # runs: seeded from the clock
5:
    call pcg_seed
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret

    .section .bss
    .align 3
    .globl clock_fixed
clock_fixed: .space 8
    .text

# long sys_clock_ns(void)
    .globl sys_clock_ns
sys_clock_ns:
    addi sp, sp, -32
    li a0, 1                    # CLOCK_MONOTONIC
    mv a1, sp
    li a7, SYS_clock_gettime
    ecall
    ld t0, 0(sp)
    ld t1, 8(sp)
    li t2, 1000000000
    mul t0, t0, t2
    add a0, t0, t1
    addi sp, sp, 32
    ret

# void pcg_seed(long seed)
    .globl pcg_seed
pcg_seed:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    mv s0, a0
    la t0, pcg_state
    sd zero, 0(t0)
    la t0, pcg_inc
    li t1, 109                  # (54 << 1) | 1
    sd t1, 0(t0)
    call pcg_next32
    la t0, pcg_state
    ld t1, 0(t0)
    add t1, t1, s0
    sd t1, 0(t0)
    call pcg_next32
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret

# long pcg_next32(void)
# PCG-XSH-RR 64/32.
    .globl pcg_next32
pcg_next32:
    la t0, pcg_state
    ld t1, 0(t0)                # old
    li t2, 6364136223846793005
    mul t3, t1, t2
    la t4, pcg_inc
    ld t4, 0(t4)
    add t3, t3, t4
    sd t3, 0(t0)
    srli t2, t1, 18
    xor t2, t2, t1
    srli t2, t2, 27
    slli t2, t2, 32
    srli t2, t2, 32             # xorshifted (32 bits)
    srli t3, t1, 59             # rot
    srlw t4, t2, t3
    neg t5, t3
    andi t5, t5, 31
    sllw t5, t2, t5
    or a0, t4, t5
    slli a0, a0, 32
    srli a0, a0, 32
    ret

# long rt_random_int(long lo, long hi)
# Uniform in [lo, hi] by rejection sampling over 64-bit draws; R0007 when lo > hi.
    .globl rt_random_int
rt_random_int:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    mv s1, a1
    ble s0, s1, 1f
    mv a0, s0
    call int_to_str
    mv s2, a0
    mv a0, s1
    call int_to_str
    mv a2, a0
    la a0, m_range
    mv a1, s2
    li a3, 0
    call msg
    mv a1, a0
    la a0, r0007
    la t0, cur_node
    ld a2, 0(t0)
    call rt_raise
1:
    sub s2, s1, s0
    addi s2, s2, 1              # span (0: the whole 64-bit range)
    neg s3, s2
    beqz s2, 2f
    remu s3, s3, s2             # rejection threshold: (2^64 - span) mod span
2:
    call pcg_next32
    slli t0, a0, 32
    sd t0, 0(sp)
    call pcg_next32
    ld t0, 0(sp)
    or a0, t0, a0
    bltu a0, s3, 2b
    beqz s2, 3f
    remu a0, a0, s2
3:
    add a0, s0, a0
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# long rt_now_ms(void)
    .globl rt_now_ms
rt_now_ms:
    la t0, clock_fixed
    ld t0, 0(t0)
    beqz t0, 1f
    la t0, clock_next
    ld a0, 0(t0)
    addi t1, a0, 1
    sd t1, 0(t0)
    ret
1:
    tail sys_clock_real_ms      # spec 007: wall-clock time, as documented (deadlines stay monotonic)

# void *rt_env(void *name)
# result[str]: the variable's value, or err("not set").
    .globl rt_env
rt_env:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    li a0, EF_ENV
    mv a1, s0
    call rt_check_name
    ld s1, STR_LEN(s0)
    la t0, envp
    ld s2, 0(t0)
    beqz s2, 8f
1:
    ld s3, 0(s2)
    beqz s3, 8f
    addi s2, s2, 8
    mv a0, s3
    addi a1, s0, STR_DATA
    mv a2, s1
    call mem_eq
    beqz a0, 1b
    add t0, s3, s1
    lbu t1, 0(t0)
    li t2, 61                   # '='
    bne t1, t2, 1b
    addi a0, t0, 1
    call str_from_cstr
    mv a1, a0
    li a0, 0
    call make_result
    j 9f
8:
    la a0, m_not_set
    call str_from_cstr
    mv a1, a0
    li a0, 1
    call make_result
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# void *rt_frame_effects(void)
# The effect set the running code is held to.
    .globl rt_frame_effects
rt_frame_effects:
    la t0, cur_frame
    ld t0, 0(t0)
    li a0, 0
    beqz t0, 1f
    ld a0, FR_EFFECTS(t0)
1:
    ret

# void rt_check_bit(long id)
# R0010 when builtin id's effect is not declared by the running function or test.
    .globl rt_check_bit
rt_check_bit:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    la t0, builtin_effects
    add t0, t0, a0
    lbu s0, 0(t0)
    beqz s0, 9f
    call rt_frame_effects
    beqz a0, 1f
    ld t0, ES_MASK(a0)
    ld t1, ES_ANY(a0)           # inferred effects (bound effect parameters) are any-scope bits
    or t0, t0, t1
    and t0, t0, s0
    bnez t0, 9f
1:
    mv a0, s0
    li a1, 0
    call rt_denied
9:
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret

# void rt_check_name(long bit, void *name)
# Scoped env (or net) effects: name must be one of the declared names.
    .globl rt_check_name
rt_check_name:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    sd s2, 0(sp)
    mv s0, a0
    mv s1, a1
    call rt_frame_effects
    beqz a0, 9f
    mv s2, a0
    mv a1, s0
    call es_scoped
    beqz a0, 9f
    mv a0, s2
    mv a1, s0
    mv a2, s1
    call es_has_pair
    bnez a0, 9f
    mv a0, s0
    mv a1, s1
    call rt_denied
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    ld s2, 0(sp)
    addi sp, sp, 32
    ret

# void rt_check_path(long bit, void *path)
# Scoped file effects: the resolved path must lie under a resolved scope.
    .globl rt_check_path
rt_check_path:
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
    call rt_frame_effects
    beqz a0, 9f
    mv s2, a0
    mv a1, s0
    call es_scoped
    beqz a0, 9f
    mv a0, s1
    call rt_resolve
    beqz a0, 8f
    mv s3, a0                   # resolved path
    ld s4, ES_SCOPES(s2)
    li s5, 0
1:
    ld t0, VEC_N(s4)
    bge s5, t0, 8f
    mv a0, s4
    mv a1, s5
    call vec_get
    addi s5, s5, 1
    ld t0, ESC_BIT(a0)
    bne t0, s0, 1b
    ld a0, ESC_STR(a0)
    call rt_resolve
    beqz a0, 1b
    mv a1, a0
    mv a0, s3
    call path_under
    beqz a0, 1b
    j 9f
8:
    mv a0, s0
    mv a1, s1
    call rt_denied
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

# int path_under(void *path, void *dir)
# 1 when the resolved path is dir or lies inside it ("" is the root).
    .globl path_under
path_under:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    mv s1, a1
    ld t1, STR_LEN(s1)
    beqz t1, 8f
    ld t0, STR_LEN(s0)
    bltu t0, t1, 7f
    beq t0, t1, 1f
    add t2, s0, t1              # the next byte must start a component
    lbu t2, STR_DATA(t2)
    li t3, 47
    bne t2, t3, 7f
1:
    addi a0, s0, STR_DATA
    addi a1, s1, STR_DATA
    mv a2, t1
    call mem_eq
    j 9f
7:
    li a0, 0
    j 9f
8:
    li a0, 1
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void *rt_resolve(void *path)
# The absolute real path as a str ("" for /), or 0 when it cannot be resolved (too long, loops).
# ponytail: paths over 4096 bytes are refused rather than resolved
    .globl rt_resolve
rt_resolve:
    addi sp, sp, -80
    sd ra, 72(sp)
    sd s0, 64(sp)
    sd s1, 56(sp)
    sd s2, 48(sp)
    sd s3, 40(sp)
    sd s4, 32(sp)
    sd s5, 24(sp)
    sd s6, 16(sp)
    sd s7, 8(sp)
    addi s0, a0, STR_DATA       # remaining path bytes
    ld s1, STR_LEN(a0)
    li s2, 0                    # index into the remaining path
    li s3, 0                    # resolved length (res_buf)
    li s7, 0                    # symbolic links followed
    beqz s1, 1f
    lbu t0, 0(s0)
    li t1, 47
    beq t0, t1, 2f
1:
    call rt_start_cwd           # relative: start from the working directory
    ld s3, STR_LEN(a0)
    la a1, res_buf
    mv a2, s3
    addi a0, a0, STR_DATA
    mv t0, a0
    mv a0, a1
    mv a1, t0
    call mem_copy
2:
    bge s2, s1, 8f
    add t0, s0, s2
    lbu t0, 0(t0)
    li t1, 47
    bne t0, t1, 3f
    addi s2, s2, 1
    j 2b
3:
    mv s4, s2                   # component [s4, s5)
    mv s5, s2
31:
    bge s5, s1, 32f
    add t0, s0, s5
    lbu t0, 0(t0)
    li t1, 47
    beq t0, t1, 32f
    addi s5, s5, 1
    j 31b
32:
    mv s2, s5
    sub t2, s5, s4
    add t3, s0, s4
    lbu t4, 0(t3)
    li t5, 46
    li t6, 1
    bne t2, t6, 33f
    beq t4, t5, 2b              # "."
33:
    li t6, 2
    bne t2, t6, 4f
    bne t4, t5, 4f
    lbu t4, 1(t3)
    bne t4, t5, 4f
    la t0, res_buf              # "..": drop the last component
34:
    beqz s3, 2b
    addi s3, s3, -1
    add t1, t0, s3
    lbu t1, 0(t1)
    li t2, 47
    bne t1, t2, 34b
    j 2b
4:
    add t0, s3, t2              # append "/" + component
    li t1, PATH_MAX - 2
    bgeu t0, t1, 99f
    la t0, res_buf
    add t0, t0, s3
    li t1, 47
    sb t1, 0(t0)
    mv s6, s3                   # length before the component
    addi s3, s3, 1
    la a0, res_buf
    add a0, a0, s3
    add a1, s0, s4
    mv a2, t2
    add s3, s3, t2
    call mem_copy
    la t0, res_buf
    add t0, t0, s3
    sb zero, 0(t0)
    li a0, AT_FDCWD             # a symbolic link?
    la a1, res_buf
    la a2, link_buf
    li a3, PATH_MAX
    li a7, SYS_readlinkat
    ecall
    blez a0, 2b
    addi s7, s7, 1
    li t0, 40
    bgt s7, t0, 99f
    mv s3, s6                   # replace the link with its target, then the rest
    mv s4, a0                   # target length
    la t0, link_buf
    lbu t0, 0(t0)
    li t1, 47
    bne t0, t1, 5f
    li s3, 0
5:
    call sb_new
    mv s5, a0
    la a1, link_buf
    mv a2, s4
    call sb_bytes
    mv a0, s5
    li a1, 47
    call sb_byte
    mv a0, s5
    add a1, s0, s2
    sub a2, s1, s2
    call sb_bytes
    mv a0, s5
    call sb_finish
    addi s0, a0, STR_DATA
    ld s1, STR_LEN(a0)
    li s2, 0
    j 2b
8:
    la a0, res_buf
    mv a1, s3
    call str_new
    j 100f
99:
    li a0, 0
100:
    ld ra, 72(sp)
    ld s0, 64(sp)
    ld s1, 56(sp)
    ld s2, 48(sp)
    ld s3, 40(sp)
    ld s4, 32(sp)
    ld s5, 24(sp)
    ld s6, 16(sp)
    ld s7, 8(sp)
    addi sp, sp, 80
    ret

# void *rt_start_cwd(void)
# The working directory at the first call, as a str without a trailing "/" ("" for /).
    .globl rt_start_cwd
rt_start_cwd:
    addi sp, sp, -16
    sd ra, 8(sp)
    la t0, start_cwd
    ld a0, 0(t0)
    bnez a0, 9f
    la a0, link_buf
    li a1, PATH_MAX
    li a7, SYS_getcwd
    ecall
    li a1, 0
    blez a0, 1f
    addi a1, a0, -1             # without the NUL
    li t0, 1
    bne a1, t0, 1f
    li a1, 0                    # "/" itself
1:
    la a0, link_buf
    call str_new
    la t0, start_cwd
    sd a0, 0(t0)
9:
    ld ra, 8(sp)
    addi sp, sp, 16
    ret

# void rt_denied(long bit, void *target)
# R0010: the effect (with target path or name, or 0) is outside the running declaration.
    .globl rt_denied
rt_denied:
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
    call rt_frame_effects
    mv s2, a0                   # the declaration (may be 0)
    la t0, cur_frame
    ld a0, 0(t0)
    call frame_fn_name
    mv s3, a0
    call sb_new                 # what was attempted: effect("target")
    mv s4, a0
    mv a0, s0
    call es_bit_name
    mv s6, a0
    mv a1, a0
    mv a0, s4
    call sb_cstr
    beqz s1, 1f
    mv a0, s4
    li a1, 40
    call sb_byte
    mv a0, s4
    mv a1, s1
    call es_render_scope
    mv a0, s4
    li a1, 41
    call sb_byte
1:
    mv a0, s4
    call sb_finish
    mv s4, a0
    la a0, s_pure               # what is declared
    call str_from_cstr
    mv s5, a0
    beqz s2, 2f
    mv a0, s2
    call es_is_empty
    bnez a0, 2f
    mv a0, s2
    call fx_render
    mv s5, a0
2:
    la a0, m_denied
    mv a1, s4
    mv a2, s3
    mv a3, s5
    call msg
    sd a0, 0(sp)
    call sb_new                 # JSON fields for the report
    mv s4, a0
    la a1, j_effect
    call sb_cstr
    mv a0, s4
    mv a1, s6
    call sb_cstr
    mv a0, s4
    la a1, j_target
    call sb_cstr
    beqz s1, 3f
    mv a0, s4
    mv a1, s1
    call json_str_into
    j 4f
3:
    mv a0, s4
    la a1, s_null
    call sb_cstr
4:
    mv a0, s4
    la a1, j_allowed
    call sb_cstr
    beqz s2, 6f
    ld s5, ES_SCOPES(s2)
    beqz s5, 6f
    li s3, 0                    # index
    li s1, 1                    # first
5:
    ld t0, VEC_N(s5)
    bge s3, t0, 6f
    mv a0, s5
    mv a1, s3
    call vec_get
    addi s3, s3, 1
    ld t0, ESC_BIT(a0)
    bne t0, s0, 5b
    ld t1, ESC_STR(a0)
    sd t1, 8(sp)
    bnez s1, 51f
    mv a0, s4
    li a1, 44                   # ','
    call sb_byte
51:
    li s1, 0
    call sb_new
    ld a1, 8(sp)
    sd a0, 8(sp)
    call es_render_scope
    ld a0, 8(sp)
    call sb_finish
    addi a1, a0, STR_DATA
    ld a2, STR_LEN(a0)
    mv a0, s4
    call sb_bytes
    j 5b
6:
    mv a0, s4
    la a1, j_declared
    call sb_cstr
    la t0, cur_frame
    ld a0, 0(t0)
    call frame_fn_name
    mv a1, a0
    mv a0, s4
    call json_str_into
    mv a0, s4
    call sb_finish
    la t0, rt_err_extra
    sd a0, 0(t0)
    la a0, r0010
    ld a1, 0(sp)
    la t0, cur_node
    ld a2, 0(t0)
    call rt_raise
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
