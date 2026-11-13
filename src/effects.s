# effects.s: effect sets with scopes (data-model.md "Effect set", research §3).
#
# A set is ES_MASK (bits declared, unscoped unless the set has scope pairs for the bit),
# ES_SCOPES (vec of {bit, str} pairs, sorted by bit then bytes, no duplicates) and ES_ANY
# (bits needed "in some scope", from builtin uses; never part of a declaration).
# ES_ANY may overlap ES_MASK: then the scopes are still the requirement, and none counts as unused.
# Sets are built with es_new / es_add_scope / es_put_pair and not changed afterwards.
# Directory scopes are normalized: no leading "./", no empty or "." components, one trailing
# "/"; "" stands for the working directory itself and renders as "./".

    .include "defs.inc"

    .section .rodata
es_names:
    .dword n_console, n_input, n_fread, n_fwrite, n_net, n_clock, n_random, n_env, 0
n_console: .string "console"
n_input:   .string "input"
n_fread:   .string "file.read"
n_fwrite:  .string "file.write"
n_net:     .string "net"
n_clock:   .string "clock"
n_random:  .string "random"
n_env:     .string "env"
s_comma:   .string ", "
s_dot:     .string "./"

    .text

# void *es_new(long mask)
    .globl es_new
es_new:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    mv s0, a0
    li a0, ES_SIZE
    call alloc
    sd s0, ES_MASK(a0)
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret

# char *es_bit_name(long bit)
    .globl es_bit_name
es_bit_name:
    li t0, 0
1:
    srli a0, a0, 1
    beqz a0, 2f
    addi t0, t0, 1
    j 1b
2:
    la t1, es_names
    slli t0, t0, 3
    add t1, t1, t0
    ld a0, 0(t1)
    ret

# long es_bit_by_name(char *p, long n)
# The effect bit named by the n bytes at p, or 0.
    .globl es_bit_by_name
es_bit_by_name:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    mv s1, a1
    li s2, 0                    # index
1:
    la t0, es_names
    slli t1, s2, 3
    add t0, t0, t1
    ld s3, 0(t0)
    beqz s3, 8f
    mv a0, s3
    call cstr_len
    bne a0, s1, 2f
    mv a0, s0
    mv a1, s3
    mv a2, s1
    call mem_eq
    bnez a0, 3f
2:
    addi s2, s2, 1
    j 1b
3:
    li a0, 1
    sll a0, a0, s2
    j 9f
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

# int es_scoped(void *set, long bit)
# 1 when the set has at least one scope pair for bit.
    .globl es_scoped
es_scoped:
    ld t0, ES_SCOPES(a0)
    beqz t0, 8f
    ld t1, VEC_N(t0)
    ld t2, VEC_DATA(t0)
    li t3, 0
1:
    bge t3, t1, 8f
    slli t4, t3, 3
    add t4, t2, t4
    ld t4, 0(t4)
    ld t5, ESC_BIT(t4)
    beq t5, a1, 9f
    addi t3, t3, 1
    j 1b
8:
    li a0, 0
    ret
9:
    li a0, 1
    ret

# void es_put_pair(void *set, long bit, void *str)
# Sets bit and inserts the scope pair in (bit, bytes) order unless it is already present.
    .globl es_put_pair
es_put_pair:
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
    mv s0, a0
    mv s1, a1
    mv s2, a2
    ld t0, ES_MASK(s0)
    or t0, t0, s1
    sd t0, ES_MASK(s0)
    call vec_new
    mv s3, a0                   # the new pair vector
    ld s4, ES_SCOPES(s0)        # the old one, or 0
    li s5, 0                    # index into the old one
    li s6, 0                    # the new pair is placed
1:
    beqz s4, 5f
    ld t0, VEC_N(s4)
    bge s5, t0, 5f
    mv a0, s4
    mv a1, s5
    call vec_get
    mv s7, a0
    bnez s6, 4f
    ld t0, ESC_BIT(s7)
    bltu t0, s1, 4f
    bltu s1, t0, 3f
    ld a0, ESC_STR(s7)
    mv a1, s2
    call str_cmp
    bltz a0, 4f
    bgtz a0, 3f
    li s6, 1                    # already present: keep the old pair
    j 4f
3:
    mv a0, s3                   # place the new pair before this one
    mv a1, s1
    mv a2, s2
    call es_pair_push
    li s6, 1
4:
    mv a0, s3
    mv a1, s7
    call vec_push
    addi s5, s5, 1
    j 1b
5:
    bnez s6, 6f
    mv a0, s3
    mv a1, s1
    mv a2, s2
    call es_pair_push
6:
    sd s3, ES_SCOPES(s0)
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

# void es_pair_push(void *vec, long bit, void *str)
# Appends a new {bit, str} pair to vec.
    .globl es_pair_push
es_pair_push:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    sd s2, 0(sp)
    mv s0, a0
    mv s1, a1
    mv s2, a2
    li a0, ESC_SIZE
    call alloc
    sd s1, ESC_BIT(a0)
    sd s2, ESC_STR(a0)
    mv a1, a0
    mv a0, s0
    call vec_push
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    ld s2, 0(sp)
    addi sp, sp, 32
    ret

# int es_add_scope(void *set, long bit, void *str)
# Normalizes the scope (directories for file effects) and adds it. 0 when the scope is invalid:
# empty, absolute, or with a ".." component (E0506).
    .globl es_add_scope
es_add_scope:
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
    mv s0, a0
    mv s1, a1
    mv s2, a2
    ld s3, STR_LEN(s2)          # n
    addi s4, s2, STR_DATA       # p
    beqz s3, 8f
    andi t0, s1, EF_FREAD | EF_FWRITE
    beqz t0, 7f                 # env, net: kept verbatim
    lbu t0, 0(s4)
    li t1, 47                   # '/': absolute
    beq t0, t1, 8f
    call sb_new
    mv s5, a0
    li s6, 0                    # i
1:
    bge s6, s3, 6f
    mv s7, s6                   # j: end of the component
2:
    bge s7, s3, 3f
    add t0, s4, s7
    lbu t0, 0(t0)
    li t1, 47
    beq t0, t1, 3f
    addi s7, s7, 1
    j 2b
3:
    sub t2, s7, s6              # component length
    beqz t2, 5f
    add t3, s4, s6
    lbu t4, 0(t3)
    li t5, 46                   # '.'
    li t6, 1
    bne t2, t6, 4f
    beq t4, t5, 5f              # "."
4:
    li t6, 2
    bne t2, t6, 41f
    bne t4, t5, 41f
    lbu t4, 1(t3)
    beq t4, t5, 8f              # ".."
41:
    mv a0, s5
    mv a1, t3
    mv a2, t2
    call sb_bytes
    mv a0, s5
    li a1, 47
    call sb_byte
5:
    addi s6, s7, 1
    j 1b
6:
    mv a0, s5
    call sb_finish
    mv s2, a0
7:
    mv a0, s0
    mv a1, s1
    mv a2, s2
    call es_put_pair
    li a0, 1
    j 9f
8:
    li a0, 0
9:
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

# void es_copy_pairs(void *dst, void *src, long bit)
# Puts every scope pair of src for bit into dst.
    .globl es_copy_pairs
es_copy_pairs:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    sd s4, 0(sp)
    mv s0, a0
    mv s2, a2
    ld s1, ES_SCOPES(a1)
    beqz s1, 9f
    li s3, 0
1:
    ld t0, VEC_N(s1)
    bge s3, t0, 9f
    mv a0, s1
    mv a1, s3
    call vec_get
    mv s4, a0
    ld t0, ESC_BIT(s4)
    bne t0, s2, 2f
    mv a0, s0
    mv a1, s2
    ld a2, ESC_STR(s4)
    call es_put_pair
2:
    addi s3, s3, 1
    j 1b
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    ld s4, 0(sp)
    addi sp, sp, 48
    ret

# int es_pair_covered(void *set, long bit, void *str)
# 1 when a scope of set for bit covers str: a byte prefix for directories, equal otherwise.
    .globl es_pair_covered
es_pair_covered:
    addi sp, sp, -64
    sd ra, 56(sp)
    sd s0, 48(sp)
    sd s1, 40(sp)
    sd s2, 32(sp)
    sd s3, 24(sp)
    sd s4, 16(sp)
    mv s1, a1
    mv s2, a2
    ld s0, ES_SCOPES(a0)
    beqz s0, 8f
    li s3, 0
1:
    ld t0, VEC_N(s0)
    bge s3, t0, 8f
    mv a0, s0
    mv a1, s3
    call vec_get
    ld t0, ESC_BIT(a0)
    bne t0, s1, 3f
    ld s4, ESC_STR(a0)
    andi t0, s1, EF_FREAD | EF_FWRITE
    beqz t0, 2f
    ld t1, STR_LEN(s4)          # prefix test
    ld t2, STR_LEN(s2)
    bltu t2, t1, 3f
    addi a0, s4, STR_DATA
    addi a1, s2, STR_DATA
    mv a2, t1
    call mem_eq
    bnez a0, 9f
    j 3f
2:
    mv a0, s4
    mv a1, s2
    call str_eq
    bnez a0, 9f
3:
    addi s3, s3, 1
    j 1b
8:
    li a0, 0
    j 10f
9:
    li a0, 1
10:
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    addi sp, sp, 64
    ret

# int es_has_pair(void *set, long bit, void *str)
# 1 when the set has exactly this scope pair.
    .globl es_has_pair
es_has_pair:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s1, a1
    mv s2, a2
    ld s0, ES_SCOPES(a0)
    beqz s0, 8f
    li s3, 0
1:
    ld t0, VEC_N(s0)
    bge s3, t0, 8f
    mv a0, s0
    mv a1, s3
    call vec_get
    ld t0, ESC_BIT(a0)
    bne t0, s1, 2f
    ld a0, ESC_STR(a0)
    mv a1, s2
    call str_eq
    bnez a0, 9f
2:
    addi s3, s3, 1
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

# void *es_union(void *a, void *b)
    .globl es_union
es_union:
    addi sp, sp, -64
    sd ra, 56(sp)
    sd s0, 48(sp)
    sd s1, 40(sp)
    sd s2, 32(sp)
    sd s3, 24(sp)
    mv s0, a0
    mv s1, a1
    ld t0, ES_MASK(s0)
    ld t1, ES_MASK(s1)
    or a0, t0, t1
    call es_new
    mv s2, a0
    li s3, 1                    # bit
1:
    li t0, EF_ALL
    bgtu s3, t0, 6f
    ld t0, ES_MASK(s2)
    and t0, t0, s3
    beqz t0, 5f
    ld t0, ES_MASK(s0)          # a has it unscoped?
    and t0, t0, s3
    beqz t0, 2f
    mv a0, s0
    mv a1, s3
    call es_scoped
    beqz a0, 5f
2:
    ld t0, ES_MASK(s1)          # b has it unscoped?
    and t0, t0, s3
    beqz t0, 3f
    mv a0, s1
    mv a1, s3
    call es_scoped
    beqz a0, 5f
3:
    mv a0, s2
    mv a1, s0
    mv a2, s3
    call es_copy_pairs
    mv a0, s2
    mv a1, s1
    mv a2, s3
    call es_copy_pairs
5:
    slli s3, s3, 1
    j 1b
6:
    ld t0, ES_ANY(s0)           # kept even where the mask has the bit: a builtin use makes
    ld t1, ES_ANY(s1)           # every declared scope of it count as needed (es_unused)
    or t0, t0, t1
    sd t0, ES_ANY(s2)
    mv a0, s2
    mv a1, s0
    li a2, 0
    call es_evars_diff
    mv a0, s2
    mv a1, s1
    li a2, 0
    call es_evars_diff
    mv a0, s2
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    addi sp, sp, 64
    ret

# void *es_missing(void *d, void *r)
# The part of requirement r that declaration d does not cover. A bit in the result's mask
# with no pairs means "needed unscoped"; pairs are the scopes d does not cover.
    .globl es_missing
es_missing:
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
    li a0, 0
    call es_new
    mv s2, a0
    li s3, 1                    # bit
1:
    li t0, EF_ALL
    bgtu s3, t0, 9f
    ld t0, ES_MASK(s1)
    and t0, t0, s3
    beqz t0, 7f
    ld t0, ES_MASK(s0)
    and t0, t0, s3
    bnez t0, 3f
    mv a0, s1                   # d lacks the bit: copy r's form of it
    mv a1, s3
    call es_scoped
    beqz a0, 2f
    mv a0, s2
    mv a1, s1
    mv a2, s3
    call es_copy_pairs
    j 7f
2:
    ld t0, ES_MASK(s2)
    or t0, t0, s3
    sd t0, ES_MASK(s2)
    j 7f
3:
    mv a0, s0                   # d unscoped: covered
    mv a1, s3
    call es_scoped
    beqz a0, 7f
    mv a0, s1                   # d scoped, r unscoped: needs the whole effect
    mv a1, s3
    call es_scoped
    beqz a0, 2b
    ld s4, ES_SCOPES(s1)        # both scoped: each r scope needs a covering d scope
    li s5, 0
4:
    ld t0, VEC_N(s4)
    bge s5, t0, 7f
    mv a0, s4
    mv a1, s5
    call vec_get
    mv s6, a0
    ld t0, ESC_BIT(s6)
    bne t0, s3, 5f
    mv a0, s0
    mv a1, s3
    ld a2, ESC_STR(s6)
    call es_pair_covered
    bnez a0, 5f
    mv a0, s2
    mv a1, s3
    ld a2, ESC_STR(s6)
    call es_put_pair
5:
    addi s5, s5, 1
    j 4b
7:
    ld t0, ES_ANY(s1)           # needed in any scope
    and t0, t0, s3
    beqz t0, 8f
    ld t0, ES_MASK(s0)
    ld t1, ES_ANY(s0)
    or t0, t0, t1
    ld t1, ES_MASK(s2)
    or t0, t0, t1
    and t0, t0, s3
    bnez t0, 8f
    ld t0, ES_ANY(s2)
    or t0, t0, s3
    sd t0, ES_ANY(s2)
8:
    slli s3, s3, 1
    j 1b
9:
    mv a0, s2
    mv a1, s1
    mv a2, s0
    call es_evars_diff
    mv a0, s2
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

# int es_is_empty(void *set)
    .globl es_is_empty
es_is_empty:
    ld t0, ES_MASK(a0)
    ld t1, ES_ANY(a0)
    or t0, t0, t1
    ld t1, ES_EVARS(a0)
    beqz t1, 1f
    ld t1, VEC_N(t1)
    or t0, t0, t1
1:
    seqz a0, t0
    ret

# int es_covers(void *d, void *r)
    .globl es_covers
es_covers:
    addi sp, sp, -16
    sd ra, 8(sp)
    call es_missing
    call es_is_empty
    ld ra, 8(sp)
    addi sp, sp, 16
    ret

# int es_lacks_bits(void *d, void *missing)
# 1 when some missing bit is absent from d altogether (E0501), 0 when only scopes fall short (E0503).
    .globl es_lacks_bits
es_lacks_bits:
    ld t0, ES_MASK(a1)
    ld t1, ES_ANY(a1)
    or t0, t0, t1
    ld t1, ES_MASK(a0)
    ld t2, ES_ANY(a0)
    or t1, t1, t2
    not t1, t1
    and t0, t0, t1
    ld t1, ES_EVARS(a1)
    beqz t1, 1f
    ld t1, VEC_N(t1)
    or t0, t0, t1
1:
    snez a0, t0
    ret

# void *es_unused(void *d, void *r)
# The part of declaration d that requirement r never needs: whole effects, and scopes under
# which no required scope lies. Scopes count as used when r needs the effect in any scope.
    .globl es_unused
es_unused:
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
    mv s0, a0
    mv s1, a1
    li a0, 0
    call es_new
    mv s2, a0
    li s3, 1
1:
    li t0, EF_ALL
    bgtu s3, t0, 9f
    ld t0, ES_MASK(s0)
    and t0, t0, s3
    beqz t0, 8f
    ld t0, ES_MASK(s1)
    ld t1, ES_ANY(s1)
    or t0, t0, t1
    and t0, t0, s3
    bnez t0, 3f
    mv a0, s0                   # never needed: the whole effect is unused
    mv a1, s3
    call es_scoped
    beqz a0, 2f
    mv a0, s2
    mv a1, s0
    mv a2, s3
    call es_copy_pairs
    j 8f
2:
    ld t0, ES_MASK(s2)
    or t0, t0, s3
    sd t0, ES_MASK(s2)
    j 8f
3:
    ld t0, ES_ANY(s1)           # needed in any scope: every declared scope counts as used
    and t0, t0, s3
    bnez t0, 8f
    ld t0, ES_MASK(s1)          # only scoped strict requirements can leave scopes unused
    and t0, t0, s3
    beqz t0, 8f
    mv a0, s1
    mv a1, s3
    call es_scoped
    beqz a0, 8f
    mv a0, s0
    mv a1, s3
    call es_scoped
    beqz a0, 8f
    ld s4, ES_SCOPES(s0)
    li s5, 0
4:
    ld t0, VEC_N(s4)
    bge s5, t0, 8f
    mv a0, s4
    mv a1, s5
    call vec_get
    mv s6, a0
    ld t0, ESC_BIT(s6)
    bne t0, s3, 7f
    li a0, 0                    # does some required scope lie under this declared one?
    call es_new
    mv s7, a0
    mv a0, s7
    mv a1, s3
    ld a2, ESC_STR(s6)
    call es_put_pair
    mv a0, s1
    mv a1, s7
    call es_any_pair_covered
    bnez a0, 7f
    mv a0, s2
    mv a1, s3
    ld a2, ESC_STR(s6)
    call es_put_pair
7:
    addi s5, s5, 1
    j 4b
8:
    slli s3, s3, 1
    j 1b
9:
    mv a0, s2
    mv a1, s0
    mv a2, s1
    call es_evars_diff
    mv a0, s2
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

# int es_any_pair_covered(void *r, void *one)
# 1 when some scope pair of r is covered by the single-pair set one.
    .globl es_any_pair_covered
es_any_pair_covered:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s1, a1
    ld s0, ES_SCOPES(a0)
    beqz s0, 8f
    li s2, 0
1:
    ld t0, VEC_N(s0)
    bge s2, t0, 8f
    mv a0, s0
    mv a1, s2
    call vec_get
    mv s3, a0
    mv a0, s1
    ld a1, ESC_BIT(s3)
    ld a2, ESC_STR(s3)
    call es_pair_covered
    bnez a0, 9f
    addi s2, s2, 1
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

# void *es_minus(void *a, void *b)
# a without b: bits b holds unscoped are dropped, b's scope pairs are removed, and a scoped
# bit left with no pairs is dropped.
    .globl es_minus
es_minus:
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
    li a0, 0
    call es_new
    mv s2, a0
    li s3, 1
1:
    li t0, EF_ALL
    bgtu s3, t0, 9f
    ld t0, ES_MASK(s0)
    and t0, t0, s3
    beqz t0, 8f
    ld t0, ES_MASK(s1)
    and t0, t0, s3
    beqz t0, 2f
    mv a0, s1
    mv a1, s3
    call es_scoped
    beqz a0, 8f                 # b drops the whole effect
2:
    mv a0, s0
    mv a1, s3
    call es_scoped
    bnez a0, 3f
    ld t0, ES_MASK(s2)          # a unscoped: keep the bit
    or t0, t0, s3
    sd t0, ES_MASK(s2)
    j 8f
3:
    ld s4, ES_SCOPES(s0)
    li s5, 0
4:
    ld t0, VEC_N(s4)
    bge s5, t0, 8f
    mv a0, s4
    mv a1, s5
    call vec_get
    mv s6, a0
    ld t0, ESC_BIT(s6)
    bne t0, s3, 5f
    mv a0, s1
    mv a1, s3
    ld a2, ESC_STR(s6)
    call es_has_pair
    bnez a0, 5f
    mv a0, s2
    mv a1, s3
    ld a2, ESC_STR(s6)
    call es_put_pair
5:
    addi s5, s5, 1
    j 4b
8:
    slli s3, s3, 1
    j 1b
9:
    ld t0, ES_ANY(s0)
    ld t1, ES_MASK(s1)
    ld t2, ES_ANY(s1)
    or t1, t1, t2
    not t1, t1
    and t0, t0, t1
    sd t0, ES_ANY(s2)
    mv a0, s2
    mv a1, s0
    mv a2, s1
    call es_evars_diff
    mv a0, s2
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

# int es_equal(void *a, void *b)
    .globl es_equal
es_equal:
    addi sp, sp, -64
    sd ra, 56(sp)
    sd s0, 48(sp)
    sd s1, 40(sp)
    sd s2, 32(sp)
    sd s3, 24(sp)
    sd s4, 16(sp)
    mv s0, a0
    mv s1, a1
    ld t0, ES_MASK(s0)
    ld t1, ES_MASK(s1)
    bne t0, t1, 8f
    ld t0, ES_ANY(s0)
    ld t1, ES_ANY(s1)
    bne t0, t1, 8f
    mv a0, s0
    call es_evar_count
    mv s2, a0
    mv a0, s1
    call es_evar_count
    bne a0, s2, 8f
    li s3, 0
77:
    bge s3, s2, 78f
    ld t0, ES_EVARS(s0)
    mv a0, t0
    mv a1, s3
    call vec_get
    mv a1, a0
    mv a0, s1
    call es_has_evar
    beqz a0, 8f
    addi s3, s3, 1
    j 77b
78:
    ld a0, ES_SCOPES(s0)
    call vec_len
    mv s2, a0
    ld a0, ES_SCOPES(s1)
    call vec_len
    bne a0, s2, 8f
    li s3, 0
1:
    bge s3, s2, 9f
    ld a0, ES_SCOPES(s0)
    mv a1, s3
    call vec_get
    mv s4, a0
    ld a0, ES_SCOPES(s1)
    mv a1, s3
    call vec_get
    ld t0, ESC_BIT(s4)
    ld t1, ESC_BIT(a0)
    bne t0, t1, 8f
    ld a1, ESC_STR(a0)
    ld a0, ESC_STR(s4)
    call str_eq
    beqz a0, 8f
    addi s3, s3, 1
    j 1b
8:
    li a0, 0
    j 10f
9:
    li a0, 1
10:
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    addi sp, sp, 64
    ret

# void es_render(void *set, void *sb)
# Canonical text: effects in bit order, scopes in parentheses; nothing for the empty set.
    .globl es_render
es_render:
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
    li s2, 1                    # bit
    li s3, 1                    # first effect
1:
    li t0, EF_ALL
    bgtu s2, t0, 9f
    ld t0, ES_MASK(s0)
    ld t1, ES_ANY(s0)
    or t0, t0, t1
    and t0, t0, s2
    beqz t0, 8f
    bnez s3, 2f
    mv a0, s1
    la a1, s_comma
    call sb_cstr
2:
    li s3, 0
    mv a0, s2
    call es_bit_name
    mv a1, a0
    mv a0, s1
    call sb_cstr
    ld t0, ES_MASK(s0)
    and t0, t0, s2
    beqz t0, 8f
    mv a0, s0
    mv a1, s2
    call es_scoped
    beqz a0, 8f
    mv a0, s1
    li a1, 40                   # '('
    call sb_byte
    ld s4, ES_SCOPES(s0)
    li s5, 0
    li s6, 1                    # first scope
3:
    ld t0, VEC_N(s4)
    bge s5, t0, 7f
    mv a0, s4
    mv a1, s5
    call vec_get
    ld t0, ESC_BIT(a0)
    bne t0, s2, 6f
    ld a2, ESC_STR(a0)
    bnez s6, 4f
    mv s6, a2
    mv a0, s1
    la a1, s_comma
    call sb_cstr
    mv a2, s6
4:
    li s6, 0
    mv a0, s1
    mv a1, a2
    call es_render_scope
6:
    addi s5, s5, 1
    j 3b
7:
    mv a0, s1
    li a1, 41                   # ')'
    call sb_byte
8:
    slli s2, s2, 1
    j 1b
9:
    ld s4, ES_EVARS(s0)         # effect parameters by name
    beqz s4, 99f
    li s5, 0
91:
    ld t0, VEC_N(s4)
    bge s5, t0, 99f
    bnez s3, 92f
    mv a0, s1
    la a1, s_comma
    call sb_cstr
92:
    li s3, 0
    mv a0, s4
    mv a1, s5
    call vec_get
    call ty_param_name
    mv a1, a0
    mv a0, s1
    call sb_str
    addi s5, s5, 1
    j 91b
99:
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

# void es_render_scope(void *sb, void *str)
# A scope as a vo string literal; the working directory ("") is "./".
    .globl es_render_scope
es_render_scope:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    mv s1, a1
    li a1, 34                   # '"'
    call sb_byte
    ld s2, STR_LEN(s1)
    bnez s2, 1f
    mv a0, s0
    la a1, s_dot
    call sb_cstr
    j 8f
1:
    li s3, 0
2:
    bge s3, s2, 8f
    add t0, s1, s3
    lbu t0, STR_DATA(t0)
    li t1, 34
    beq t0, t1, 3f
    li t1, 92                   # backslash
    bne t0, t1, 4f
3:
    mv a0, s0
    li a1, 92
    call sb_byte
4:
    add t0, s1, s3
    lbu a1, STR_DATA(t0)
    mv a0, s0
    call sb_byte
    addi s3, s3, 1
    j 2b
8:
    mv a0, s0
    li a1, 34
    call sb_byte
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# ---- effect parameters (ES_EVARS: TY_PARAM types with TPC_EFFECTS) ----

# int es_has_evar(void *set, void *param)
    .globl es_has_evar
es_has_evar:
    li t2, 0
    beqz a0, 9f
    ld t0, ES_EVARS(a0)
    beqz t0, 9f
    ld t1, VEC_N(t0)
    ld t0, VEC_DATA(t0)
1:
    beqz t1, 9f
    ld t3, 0(t0)
    beq t3, a1, 8f
    addi t0, t0, 8
    addi t1, t1, -1
    j 1b
8:
    li t2, 1
9:
    mv a0, t2
    ret

# void es_add_evar(void *set, void *param)
    .globl es_add_evar
es_add_evar:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    mv s1, a1
    call es_has_evar
    bnez a0, 9f
    ld a0, ES_EVARS(s0)
    bnez a0, 1f
    call vec_new
    sd a0, ES_EVARS(s0)
1:
    mv a1, s1
    call vec_push
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void es_evars_diff(void *dst, void *a, void *b)
# Adds to dst the effect parameters of a that b (which may be 0) does not have.
    .globl es_evars_diff
es_evars_diff:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    mv s2, a2
    ld s1, ES_EVARS(a1)
    beqz s1, 9f
    li s3, 0
1:
    ld t0, VEC_N(s1)
    bge s3, t0, 9f
    mv a0, s1
    mv a1, s3
    call vec_get
    addi s3, s3, 1
    sd a0, 0(sp)
    mv a1, a0
    mv a0, s2
    call es_has_evar
    bnez a0, 1b
    mv a0, s0
    ld a1, 0(sp)
    call es_add_evar
    j 1b
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# long es_evar_count(void *set)
    .globl es_evar_count
es_evar_count:
    ld t0, ES_EVARS(a0)
    li a0, 0
    beqz t0, 1f
    ld a0, VEC_N(t0)
1:
    ret

# void *es_subst(void *set, void *owner, void *args)
# The set with owner's effect parameters replaced by the effect sets bound in args. Its results
# are permanent: they are kept in frames and closures across collections and tests (spec 003).
    .globl es_subst
es_subst:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    sd s2, 0(sp)
    mv s0, a0
    mv s1, a1
    mv s2, a2
    call perm_begin
    mv a0, s0
    mv a1, s1
    mv a2, s2
    call es_subst_raw
    mv s0, a0
    call perm_end
    mv a0, s0
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    ld s2, 0(sp)
    addi sp, sp, 32
    ret

# void *es_subst_raw(void *set, void *owner, void *args)
es_subst_raw:
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
    call es_evar_count
    beqz a0, 8f
    ld a0, ES_MASK(s0)          # the same bits, scopes and any-scope bits, no parameters
    call es_new
    mv s3, a0
    ld t0, ES_SCOPES(s0)
    sd t0, ES_SCOPES(s3)
    ld t0, ES_ANY(s0)
    sd t0, ES_ANY(s3)
    li s4, 0
1:
    ld t0, ES_EVARS(s0)
    ld t1, VEC_N(t0)
    bge s4, t1, 7f
    mv a0, t0
    mv a1, s4
    call vec_get
    addi s4, s4, 1
    ld t0, TY_DECL(a0)
    li t1, -1                   # owner -1: any owner (at run time, a frame's own parameters)
    beq s1, t1, 11f
    bne t0, s1, 2f
11:
    ld t0, TY_A(a0)
    ld t1, VEC_N(s2)
    bgeu t0, t1, 2f
    ld t1, VEC_DATA(s2)
    slli t0, t0, 3
    add t1, t1, t0
    ld t1, 0(t1)
    beqz t1, 2f
    ld t2, TY_KIND(t1)          # a binding (TY_EFFSET) holds the effect set
    li t3, TY_EFFSET
    bne t2, t3, 2f
    mv a0, s3
    ld a1, TY_DECL(t1)
    call es_union
    mv s3, a0
    j 1b
2:
    mv a1, a0
    mv a0, s3
    call es_add_evar
    j 1b
7:
    mv a0, s3
    j 9f
8:
    mv a0, s0
9:
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    addi sp, sp, 64
    ret
