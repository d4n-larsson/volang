# Runtime maps: open addressing (linear probing) over an insertion-ordered entries array.
# header | count | cap | index (u32[cap*2]: 0 empty, 0xFFFFFFFF removed, else entry+1) | entries | used
# Hashing is FNV-1a with a fixed seed, so iteration order and behavior are deterministic (FR-009).
    .include "defs.inc"

    .equ TOMB, 0xFFFFFFFF

    .text
# void *map_new(void)
    .globl map_new
map_new:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    li a0, OBJ_MAP
    li a1, MAP_SIZE
    call alloc_obj
    mv s0, a0
    li a1, 8
    call map_alloc
    mv a0, s0
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret

# void map_alloc(void *map, long cap)
# Fresh, empty index and entries for cap entries.
    .globl map_alloc
map_alloc:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    mv s1, a1
    sd s1, M_CAP(s0)
    sd zero, M_COUNT(s0)
    sd zero, M_USED(s0)
    slli a0, s1, 3              # u32[cap*2]
    call alloc
    sd a0, M_INDEX(s0)
    li t0, ME_SIZE
    mul a0, s1, t0
    call alloc
    sd a0, M_ENTRIES(s0)
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# long map_hash(long key, int key_kind)
    .globl map_hash
map_hash:
    li t0, 0xcbf29ce484222325   # FNV-1a 64 offset basis
    li t1, 0x100000001b3        # FNV prime
    li t2, TY_STR
    beq a1, t2, 2f
    li t3, 8                    # int: its 8 bytes
1:
    andi t4, a0, 0xff
    xor t0, t0, t4
    mul t0, t0, t1
    srli a0, a0, 8
    addi t3, t3, -1
    bnez t3, 1b
    j 3f
2:
    ld t3, STR_LEN(a0)
    addi t4, a0, STR_DATA
21:
    beqz t3, 3f
    lbu t5, 0(t4)
    xor t0, t0, t5
    mul t0, t0, t1
    addi t4, t4, 1
    addi t3, t3, -1
    j 21b
3:
    srli a0, t0, 32             # keep 32 bits (stored as u32)
    xor a0, a0, t0
    slli a0, a0, 32
    srli a0, a0, 32
    ret

# long map_find(void *map, long key, void *key_type)
# The entry index of the key, or -1.
    .globl map_find
map_find:
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
    ld s2, TY_KIND(a2)          # key kind
    mv a0, s1
    mv a1, s2
    call map_hash
    mv s3, a0                   # hash
    ld t0, M_CAP(s0)
    slli t0, t0, 1
    addi s5, t0, -1             # index mask
    and s4, s3, s5              # probe position
1:
    ld t0, M_INDEX(s0)
    slli t1, s4, 2
    add t0, t0, t1
    lwu t2, 0(t0)
    beqz t2, 8f
    li t3, TOMB
    beq t2, t3, 2f
    addi t2, t2, -1
    sd t2, 0(sp)
    ld t0, M_ENTRIES(s0)
    li t1, ME_SIZE
    mul t1, t1, t2
    add t0, t0, t1
    lwu t1, ME_HASH(t0)
    bne t1, s3, 2f
    ld t1, ME_KEY(t0)
    li t3, TY_STR
    beq s2, t3, 11f
    beq t1, s1, 9f
    j 2f
11:
    mv a0, t1
    mv a1, s1
    call str_eq
    bnez a0, 9f
2:
    addi s4, s4, 1
    and s4, s4, s5
    j 1b
8:
    li a0, -1
    j 99f
9:
    ld a0, 0(sp)
99:
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    ld s5, 8(sp)
    addi sp, sp, 64
    ret

# void map_index_insert(void *map, long entry, long hash)
# Puts entry+1 into the first free index slot of its probe sequence.
    .globl map_index_insert
map_index_insert:
    ld t0, M_CAP(a0)
    slli t0, t0, 1
    addi t5, t0, -1
    and t1, a2, t5
    ld t2, M_INDEX(a0)
1:
    slli t3, t1, 2
    add t3, t2, t3
    lwu t4, 0(t3)
    beqz t4, 2f
    li t6, TOMB
    beq t4, t6, 2f
    addi t1, t1, 1
    and t1, t1, t5
    j 1b
2:
    addi t4, a1, 1
    sw t4, 0(t3)
    ret

# void map_grow(void *map, void *key_type)
# Rebuilds with room for twice the live entries, dropping removed ones (keeps insertion order). The
# capacity follows the live count, so a small map with heavy churn stays small (spec 003).
    .globl map_grow
map_grow:
    addi sp, sp, -64
    sd ra, 56(sp)
    sd s0, 48(sp)
    sd s1, 40(sp)
    sd s2, 32(sp)
    sd s3, 24(sp)
    sd s4, 16(sp)
    mv s0, a0
    ld s1, M_ENTRIES(s0)        # old entries
    ld s2, M_USED(s0)
    ld t0, M_COUNT(s0)
    li a1, 8
    slli t1, t0, 1              # new capacity: the smallest power of two (probing masks with
1:                              # cap*2-1), at least 8, that holds twice the live count
    bge a1, t1, 2f
    slli a1, a1, 1
    j 1b
2:
    mv a0, s0
    call map_alloc
    li s3, 0
3:
    la t6, rt_budget          # POLL 1: --timeout budget
    ld t5, 0(t6)
    addi t5, t5, -1
    sd t5, 0(t6)
    bgtz t5, 1000f
    call rt_poll
1000:
    bge s3, s2, 9f
    li t0, ME_SIZE
    mul t0, t0, s3
    add s4, s1, t0
    addi s3, s3, 1
    lwu t0, ME_LIVE(s4)
    beqz t0, 3b
    ld t1, M_USED(s0)           # append
    ld t2, M_ENTRIES(s0)
    li t3, ME_SIZE
    mul t3, t3, t1
    add t2, t2, t3
    ld t4, ME_KEY(s4)
    sd t4, ME_KEY(t2)
    ld t4, ME_VAL(s4)
    sd t4, ME_VAL(t2)
    lwu t4, ME_HASH(s4)
    sw t4, ME_HASH(t2)
    li t4, 1
    sw t4, ME_LIVE(t2)
    addi t3, t1, 1
    sd t3, M_USED(s0)
    ld t3, M_COUNT(s0)
    addi t3, t3, 1
    sd t3, M_COUNT(s0)
    mv a0, s0
    mv a1, t1
    lwu a2, ME_HASH(s4)
    call map_index_insert
    j 3b
9:
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    addi sp, sp, 64
    ret

# void map_set(void *map, long key, long value, void *key_type)
    .globl map_set
map_set:
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
.ifdef GC_STRESS_BUILD
    mv a0, s0
    li a1, OBJ_MAP
    call gc_check              # stress build: a live OBJ_MAP (spec 003 FR-011)
.endif
    mv a0, s0
    mv a1, s1
    mv a2, s3
    call map_find
    bltz a0, 1f
    ld t0, M_ENTRIES(s0)        # replace the value, keep the position
    li t1, ME_SIZE
    mul t1, t1, a0
    add t0, t0, t1
    sd s2, ME_VAL(t0)
    j 9f
1:
    ld t0, M_USED(s0)
    ld t1, M_CAP(s0)
    blt t0, t1, 2f
    mv a0, s0
    mv a1, s3
    call map_grow
2:
    mv a0, s1
    ld a1, TY_KIND(s3)
    call map_hash
    sd a0, 0(sp)
    ld t1, M_USED(s0)
    ld t2, M_ENTRIES(s0)
    li t3, ME_SIZE
    mul t3, t3, t1
    add t2, t2, t3
    sd s1, ME_KEY(t2)
    sd s2, ME_VAL(t2)
    sw a0, ME_HASH(t2)
    li t4, 1
    sw t4, ME_LIVE(t2)
    addi t3, t1, 1
    sd t3, M_USED(s0)
    ld t3, M_COUNT(s0)
    addi t3, t3, 1
    sd t3, M_COUNT(s0)
    mv a1, t1                   # the new entry's index
    mv a0, s0
    ld a2, 0(sp)
    call map_index_insert
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# void map_remove(void *map, long key, void *key_type)
# Does nothing when the key is missing.
    .globl map_remove
map_remove:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    sd s2, 0(sp)
    mv s0, a0
    mv s1, a1
    mv s2, a2
    call map_find
    bltz a0, 9f
    mv s1, a0                   # entry index
    ld t0, M_ENTRIES(s0)
    li t1, ME_SIZE
    mul t1, t1, s1
    add t0, t0, t1
    sw zero, ME_LIVE(t0)
    sd zero, ME_KEY(t0)         # let the collector reclaim them
    sd zero, ME_VAL(t0)
    ld t2, M_COUNT(s0)
    addi t2, t2, -1
    sd t2, M_COUNT(s0)
    # ponytail: linear scan of the index for the removed entry, O(capacity) per remove; upgrade: probe from the stored hash
    ld t0, M_CAP(s0)            # find the index slot holding entry+1 and mark it removed
    slli t0, t0, 1
    ld t3, M_INDEX(s0)
    addi t4, s1, 1
    li t5, 0
1:
    bge t5, t0, 9f
    slli t6, t5, 2
    add t6, t3, t6
    lwu a1, 0(t6)
    addi t5, t5, 1
    bne a1, t4, 1b
    li a1, TOMB
    sw a1, 0(t6)
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    ld s2, 0(sp)
    addi sp, sp, 32
    ret

# void *map_keys(void *map)
# The live keys in insertion order.
    .globl map_keys
map_keys:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    ld a0, M_COUNT(s0)
    call list_new
    mv s1, a0
    li s2, 0
1:
    ld t0, M_USED(s0)
    bge s2, t0, 9f
    ld t0, M_ENTRIES(s0)
    li t1, ME_SIZE
    mul t1, t1, s2
    add s3, t0, t1
    addi s2, s2, 1
    lwu t0, ME_LIVE(s3)
    beqz t0, 1b
    mv a0, s1
    ld a1, ME_KEY(s3)
    call list_push
    j 1b
9:
    mv a0, s1
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret
