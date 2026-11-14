# The collected heap (spec 003): non-moving mark-sweep over size-classed 64 KiB pages.
#
# Regions (defs.inc, data-model.md): pages at ADDR_HEAP, one 8-byte page-table entry per page at
# ADDR_PTAB, the mark stack at ADDR_MARKSTACK. All fixed, so conservative scanning sees the same
# words on every run and collection is deterministic (research §3).
#
# Objects keep the runtime's header (size:u32 | kind:u32); gc_alloc writes it. A free small slot
# has kind OBJ_FREE and links to the next free slot through its first payload word. The mark bit
# is OBJ_MARK in the kind word, set only during a collection.
#
# Roots (research §4): the s registers (spilled into gc_collect's frame), the native stack, the
# value stack, the globals (__DATA_BEGIN__ .. _end; the default linker script defines both) and
# the permanent arena. Every word that points into an allocated object (interior pointers too)
# keeps it alive.
# ponytail: conservative roots and object contents can retain garbage that a stale word happens to
# point at; deterministic thanks to the fixed map. Precise layouts per kind would fix it.
# ponytail: the whole permanent arena is scanned at every collection (a few MiB for typical
# programs); a remembered set of perm objects written at run time would avoid it.
    .include "defs.inc"

    .section .data
    .align 3
    .globl gc_on
gc_on:      .dword 0            # 1 while a program runs: plain allocations go to the heap
    .globl gc_perm
gc_perm:    .dword 0            # perm-bracket depth: > 0 sends allocations to the permanent arena
    .globl gc_limit
gc_limit:   .dword GC_DEFAULT_LIMIT     # live-data limit in bytes (--max-memory)

    .section .bss
    .align 3
    .globl gc_since
gc_since:   .space 8            # bytes allocated since the last collection
gc_trigger: .space 8            # collect when gc_since would reach this
    .globl gc_live
gc_live:    .space 8            # live bytes after the last collection
    .globl gc_peak
gc_peak:    .space 8            # highest gc_live + gc_since seen (the reported peak)
    .globl gc_total
gc_total:   .space 8            # bytes allocated before the last collection (report: + gc_since)
    .globl gc_count
gc_count:   .space 8            # collections so far
gc_inited:  .space 8
heap_pages: .space 8            # pages in the reservation
heap_top:   .space 8            # 1 + the highest page in use
heap_hwm:   .space 8            # pages at or above this index were never used (still zero)
page_hint:  .space 8            # no free page below this index
ms_ptr:     .space 8            # mark stack top
ms_overflow: .space 8           # 1 when a push found the mark stack full
fl_heads:   .space 8 * NCLASSES * 2     # free list per (class, noscan), ascending addresses
.ifdef GC_STRESS_BUILD
    .globl gc_stress
gc_stress:  .space 8            # collect before every Nth allocation (VO_GC_STRESS, default 64)
gc_tick:    .space 8
.endif

    .section .rodata
    .align 2
class_size:
    .word 16, 32, 48, 64, 96, 128, 192, 256, 384, 512, 768, 1024
    .word 1536, 2048, 3072, 4096, 6144, 8192, 12288, 16384
class_of_16:                    # class for sizes up to 1024, indexed by ceil(size / 16)
    .byte 0,0,1,2,3,4,4,5,5,6,6,6,6,7,7,7,7,8,8,8,8,8,8,8,8,9,9,9,9,9,9,9,9,10,10,10,10,10,10,10,10
    .byte 10,10,10,10,10,10,10,10,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11,11
.ifdef GC_STRESS_BUILD
s_stress_env:   .string "VO_GC_STRESS="
s_selfcheck:    .string "error[T0003]: internal error: heap self-check failed\n"
.endif

    .text
# void gc_init(void)
# Reserves the heap for gc_limit bytes of live data (once per process) and empties it.
    .globl gc_init
gc_init:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    la t0, gc_inited
    ld t1, 0(t0)
    bnez t1, 1f
    li t1, 1
    sd t1, 0(t0)
    la t0, gc_limit
    ld s0, 0(t0)
    li t1, GC_HEADROOM
    add s0, s0, t1              # reservation: limit + 64 MiB
    srli t1, s0, PAGE_SHIFT
    la t0, heap_pages
    sd t1, 0(t0)
    li a0, ADDR_HEAP
    mv a1, s0
    li a2, MAP_NORESERVE
    call map_fixed
    srli a1, s0, PAGE_SHIFT - 3 # 8 bytes per page, rounded up to whole pages
    li t0, PAGE_SIZE - 1
    add a1, a1, t0
    li t0, -PAGE_SIZE
    and a1, a1, t0
    li a0, ADDR_PTAB
    li a2, MAP_NORESERVE
    call map_fixed
    li a0, ADDR_MARKSTACK
    li a1, MARKSTACK_SIZE
    li a2, MAP_NORESERVE
    call map_fixed
.ifdef GC_STRESS_BUILD
    call read_stress_env
.endif
1:
    call gc_reset
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret

# void stack_scrub(void)
# Zeroes the dead native stack below sp, so stale words left by earlier tests cannot keep objects
# alive (and so memory reports do not depend on test order). Stops after 8 KiB of zeros: below the
# deepest point any earlier code reached, the stack was never written.
# ponytail: a run of 1,024 zero words inside the old frames would stop it early; a recorded
# lowest sp would be exact.
    .globl stack_scrub
stack_scrub:
    addi t0, sp, -8
    li t1, 0                    # zero words in a row
    li t2, 1024
    li t3, ADDR_STACK           # the stack's own bottom
1:
    bltu t0, t3, 9f
    ld t4, 0(t0)
    beqz t4, 2f
    sd zero, 0(t0)
    li t1, 0
    j 3f
2:
    addi t1, t1, 1
    bgeu t1, t2, 9f
3:
    addi t0, t0, -8
    j 1b
9:
    ret

# void gc_reset(void)
# Empties the heap: every page free, counters zero (before each test, research §3).
    .globl gc_reset
gc_reset:
    addi sp, sp, -16
    sd ra, 8(sp)
.ifdef GC_STRESS_BUILD
    li a0, ADDR_HEAP            # stale pointers into the old heap now read poison
    la t0, heap_top
    ld a1, 0(t0)
    slli a1, a1, PAGE_SHIFT
    add a1, a0, a1
    call poison_range
.endif
    la t0, heap_top
    ld t1, 0(t0)
    sd zero, 0(t0)
    li t2, ADDR_PTAB
1:
    beqz t1, 2f
    sd zero, 0(t2)
    addi t2, t2, 8
    addi t1, t1, -1
    j 1b
2:
    la t0, fl_heads
    li t1, NCLASSES * 2
3:
    sd zero, 0(t0)
    addi t0, t0, 8
    addi t1, t1, -1
    bnez t1, 3b
    la t0, page_hint
    sd zero, 0(t0)
    la t0, gc_since
    sd zero, 0(t0)
    la t0, gc_live
    sd zero, 0(t0)
    la t0, gc_peak
    sd zero, 0(t0)
    la t0, gc_total
    sd zero, 0(t0)
    la t0, gc_count
    sd zero, 0(t0)
.ifdef GC_STRESS_BUILD
    la t0, gc_tick
    sd zero, 0(t0)
.endif
    call set_trigger
    ld ra, 8(sp)
    addi sp, sp, 16
    ret

# void set_trigger(void)
# gc_trigger = min(max(GC_MIN_TRIGGER, live), limit - live): live data may double between
# collections, but a collection always comes before live data could pass the limit (research §3).
set_trigger:
    la t0, gc_live
    ld t0, 0(t0)
    li t1, GC_MIN_TRIGGER
    bgeu t1, t0, 1f
    mv t1, t0
1:
    la t2, gc_limit
    ld t2, 0(t2)
    sub t2, t2, t0
    bgez t2, 2f
    li t2, 0
2:
    bgeu t2, t1, 3f
    mv t1, t2
3:
    la t0, gc_trigger
    sd t1, 0(t0)
    ret

# void *gc_alloc(long size, int kind)
# A zeroed heap block of at least size bytes (header included) with its header written. Strings
# and OBJ_BYTES go to no-scan pages.
    .globl gc_alloc
gc_alloc:
.ifdef GC_STRESS_BUILD
    tail gc_alloc_slow          # the stress build counts every allocation
.else
    li t0, 1024
    bgtu a0, t0, 9f
    addi t0, a0, 15
    srli t0, t0, 4
    la t1, class_of_16
    add t1, t1, t0
    lbu t0, 0(t1)               # class
    addi t1, a1, -OBJ_STR
    seqz t1, t1
    addi t2, a1, -OBJ_BYTES
    seqz t2, t2
    or t1, t1, t2               # noscan
    slli t3, t0, 1
    or t3, t3, t1
    slli t3, t3, 3
    la t1, fl_heads
    add t3, t1, t3              # &free list head
    ld t4, 0(t3)
    beqz t4, 9f
    la t1, class_size
    slli t0, t0, 2
    add t1, t1, t0
    lwu t0, 0(t1)               # slot size
    la t1, gc_since
    ld t2, 0(t1)
    add t2, t2, t0
    la t1, gc_trigger
    ld t1, 0(t1)
    bgeu t2, t1, 9f             # time to collect
    la t1, gc_since
    sd t2, 0(t1)
    ld t2, 8(t4)                # pop
    sd t2, 0(t3)
    add t0, t4, t0
    addi t1, t4, 8
1:
    sd zero, 0(t1)
    addi t1, t1, 8
    bltu t1, t0, 1b
    slli t1, a1, 32
    or t1, t1, a0
    sd t1, 0(t4)
    mv a0, t4
    ret
9:
    tail gc_alloc_slow
.endif

# void *gc_alloc_slow(long size, int kind)
# gc_alloc when the fast path cannot serve: collections, new pages, large objects. It saves no
# argument or temporary registers: their stale contents would become roots and make retention
# depend on unrelated earlier code (spec 003 FR-009).
    .globl gc_alloc_slow
gc_alloc_slow:
    addi sp, sp, -64
    sd ra, 56(sp)
    sd s0, 48(sp)
    sd s1, 40(sp)
    sd s2, 32(sp)
    sd s3, 24(sp)
    sd s4, 16(sp)
    sd zero, 8(sp)
    sd zero, 0(sp)
    mv s0, a0                   # size as asked (written to the header)
    mv s1, a1                   # kind
    srli t0, s0, 31
    bnez t0, 90f                # no header can describe 2 GiB or more
.ifdef GC_STRESS_BUILD
    la t0, gc_stress
    ld t1, 0(t0)
    beqz t1, 1f
    la t0, gc_tick
    ld t2, 0(t0)
    addi t2, t2, 1
    sd t2, 0(t0)
    bltu t2, t1, 1f
    sd zero, 0(t0)
    li a0, 0
    call collect_for
1:
.endif
    addi t1, s1, -OBJ_STR
    seqz t1, t1
    addi t2, s1, -OBJ_BYTES
    seqz t2, t2
    or s2, t1, t2               # noscan
    li t0, MAX_SMALL
    bgtu s0, t0, 50f
    li s3, 0                    # class: the first that fits
    la t0, class_size
2:
    slli t1, s3, 2
    add t1, t0, t1
    lwu s4, 0(t1)
    bgeu s4, s0, 3f
    addi s3, s3, 1
    j 2b
3:
    la t0, gc_since
    ld t1, 0(t0)
    add t1, t1, s4
    la t0, gc_trigger
    ld t0, 0(t0)
    bltu t1, t0, 4f
    mv a0, s4
    call collect_for
4:
    slli t0, s3, 1
    or t0, t0, s2
    slli t0, t0, 3
    la t1, fl_heads
    add t0, t1, t0
    ld t1, 0(t0)
    bnez t1, 5f
    mv a0, s3
    mv a1, s2
    call new_small_page
    bnez a0, 4b
    mv a0, s4                   # no free page left in the reservation
    call collect_for
    mv a0, s3
    mv a1, s2
    call new_small_page
    bnez a0, 4b
    j 90f
5:
    ld t2, 8(t1)                # pop
    sd t2, 0(t0)
    sd t1, 0(sp)
    mv a0, t1
    mv a1, s4
    call zero_block
    ld a0, 0(sp)
    mv a1, s4
    j 60f
50:                             # large: a run of whole pages
    li t0, PAGE_SIZE - 1
    add s3, s0, t0
    srli s3, s3, PAGE_SHIFT     # pages
    slli s4, s3, PAGE_SHIFT     # bytes
    la t0, gc_since
    ld t1, 0(t0)
    add t1, t1, s4
    la t0, gc_trigger
    ld t0, 0(t0)
    bltu t1, t0, 51f
    mv a0, s4
    call collect_for
51:
    mv a0, s3
    mv a1, s2
    call take_run
    bnez a0, 52f
    mv a0, s4
    call collect_for
    mv a0, s3
    mv a1, s2
    call take_run
    beqz a0, 90f
52:
    mv a1, s4
60:                             # a0 = zeroed block, a1 = its size: account, header
    la t0, gc_since
    ld t1, 0(t0)
    add t1, t1, a1
    sd t1, 0(t0)
    slli t1, s1, 32
    or t1, t1, s0
    sd t1, 0(a0)
    j 99f
90:
    mv a0, s0
    call gc_out_of_memory
99:
    ld ra, 56(sp)
    ld s0, 48(sp)
    ld s1, 40(sp)
    ld s2, 32(sp)
    ld s3, 24(sp)
    ld s4, 16(sp)
    addi sp, sp, 64
    ret

# void zero_block(char *p, long n)
# Zeroes n bytes (a multiple of 8), skipping the part of the heap that was never used.
zero_block:
    add t0, a0, a1
    la t1, heap_hwm
    ld t1, 0(t1)
    slli t1, t1, PAGE_SHIFT
    li t2, ADDR_HEAP
    add t1, t2, t1              # first never-used address
    bgeu t0, t1, 1f
    mv t1, t0
1:
    bgeu a0, t1, 2f
    sd zero, 0(a0)
    addi a0, a0, 8
    j 1b
2:
    ret

# void collect_for(long need)
# Collects; if the live data plus need would pass the limit, collects once more (to drop what
# stale words kept alive), then reports out-of-memory (R0006).
collect_for:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    mv s0, a0
    call gc_collect
    la t0, gc_live
    ld t0, 0(t0)
    add t0, t0, s0
    la t1, gc_limit
    ld t1, 0(t1)
    bleu t0, t1, 9f
    call gc_collect
    la t0, gc_live
    ld t0, 0(t0)
    add t0, t0, s0
    la t1, gc_limit
    ld t1, 0(t1)
    bleu t0, t1, 9f
    mv a0, s0
    call gc_out_of_memory
9:
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret

# void gc_out_of_memory(long need)
# The live data (plus the pending request) passes the limit: R0006 through the interpreter.
gc_out_of_memory:
    addi sp, sp, -16
    sd ra, 8(sp)
    la t0, gc_live
    ld t0, 0(t0)
    add a0, a0, t0
    call rt_oom_live
    ld ra, 8(sp)
    addi sp, sp, 16
    ret

# long first_free_page(void)
# The lowest free page index (heap_top when every page below it is in use).
first_free_page:
    la t0, page_hint
    ld a0, 0(t0)
    la t1, heap_top
    ld t1, 0(t1)
    li t2, ADDR_PTAB
1:
    bgeu a0, t1, 2f
    slli t3, a0, 3
    add t3, t2, t3
    lbu t3, PT_KIND(t3)
    beqz t3, 2f
    addi a0, a0, 1
    j 1b
2:
    sd a0, 0(t0)
    ret

# void use_pages(long first, long n)
# Pages first..first+n are now in use: moves heap_top and heap_hwm past them.
use_pages:
    add t0, a0, a1
    la t1, heap_top
    ld t2, 0(t1)
    bgeu t2, t0, 1f
    sd t0, 0(t1)
1:
    la t1, heap_hwm
    ld t2, 0(t1)
    bgeu t2, t0, 2f
    sd t0, 0(t1)
2:
    ret

# void *new_small_page(int cls, int noscan)
# Takes the lowest free page for size class cls, threads its slots onto the class's free list in
# ascending order and returns the list head, or 0 when the reservation is full.
new_small_page:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    sd s2, 0(sp)
    mv s0, a0
    mv s1, a1
    call first_free_page
    mv s2, a0
    la t0, heap_pages
    ld t0, 0(t0)
    li a0, 0
    bgeu s2, t0, 9f
    mv a0, s2
    li a1, 1
    call use_pages
    addi t0, s2, 1
    la t1, page_hint
    sd t0, 0(t1)
    li t0, ADDR_PTAB
    slli t1, s2, 3
    add t0, t0, t1
    li t1, PG_SMALL
    sb t1, PT_KIND(t0)
    sb s0, PT_CLASS(t0)
    sb s1, PT_NOSCAN(t0)
    sw zero, PT_RUN(t0)
    la t0, class_size
    slli t1, s0, 2
    add t0, t0, t1
    lwu t1, 0(t0)               # slot size
    li t2, PAGE_SIZE
    divu t2, t2, t1             # slots
    mul t2, t2, t1
    slli t3, s2, PAGE_SHIFT
    li t4, ADDR_HEAP
    add t3, t4, t3              # page start
    add t2, t3, t2              # past the last slot
    li t4, OBJ_FREE
    slli t4, t4, 32             # header of a free slot
    li a0, 0
1:
    sub t2, t2, t1
    sd t4, 0(t2)
    sd a0, 8(t2)
    mv a0, t2
    bne t2, t3, 1b
    slli t0, s0, 1
    or t0, t0, s1
    slli t0, t0, 3
    la t1, fl_heads
    add t0, t1, t0
    sd a0, 0(t0)
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    ld s2, 0(sp)
    addi sp, sp, 32
    ret

# void *take_run(long n, int noscan)
# First fit: the lowest run of n free pages (growing past heap_top when needed), set up as one
# large object. Returns its start, or 0 when the reservation has no such run.
take_run:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    sd s2, 0(sp)
    mv s0, a0
    mv s1, a1
    call first_free_page
    mv s2, a0                   # candidate start
    la t5, heap_top
    ld t5, 0(t5)
    li t6, ADDR_PTAB
1:
    li t0, 0                    # free pages counted from s2
2:
    bgeu t0, s0, 5f             # found n
    add t1, s2, t0
    bgeu t1, t5, 5f             # past heap_top: everything above is free
    slli t2, t1, 3
    add t2, t6, t2
    lbu t2, PT_KIND(t2)
    bnez t2, 3f
    addi t0, t0, 1
    j 2b
3:
    addi s2, t1, 1              # restart after the used page
    j 1b
5:
    add t0, s2, s0
    la t1, heap_pages
    ld t1, 0(t1)
    li a0, 0
    bgtu t0, t1, 9f
    slli a0, s2, PAGE_SHIFT     # zero it while heap_hwm still tells which pages were never used
    li t0, ADDR_HEAP
    add a0, t0, a0
    slli a1, s0, PAGE_SHIFT
    call zero_block
    mv a0, s2
    mv a1, s0
    call use_pages
    li t0, ADDR_PTAB
    slli t1, s2, 3
    add t0, t0, t1
    li t1, PG_LARGE
    sb t1, PT_KIND(t0)
    sb zero, PT_CLASS(t0)
    sb s1, PT_NOSCAN(t0)
    sw s0, PT_RUN(t0)
    li t1, 1
    li t2, PG_TAIL
6:
    bgeu t1, s0, 7f
    addi t0, t0, 8
    sb t2, PT_KIND(t0)
    sb s1, PT_NOSCAN(t0)
    sw t1, PT_RUN(t0)           # back to the head
    addi t1, t1, 1
    j 6b
7:
    slli a0, s2, PAGE_SHIFT
    li t0, ADDR_HEAP
    add a0, t0, a0
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    ld s2, 0(sp)
    addi sp, sp, 32
    ret

# void *gc_find(long addr)
# The start of the allocated heap object that contains addr (interior pointers included), or 0.
    .globl gc_find
gc_find:
    li t0, ADDR_HEAP
    bltu a0, t0, 8f
    sub t1, a0, t0
    srli t2, t1, PAGE_SHIFT     # page
    la t3, heap_top
    ld t3, 0(t3)
    bgeu t2, t3, 8f
    li t3, ADDR_PTAB
    slli t4, t2, 3
    add t3, t3, t4
    lbu t4, PT_KIND(t3)
    li t0, PG_SMALL
    beq t4, t0, 1f
    li t0, PG_LARGE
    beq t4, t0, 3f
    li t0, PG_TAIL
    bne t4, t0, 8f
    lwu t4, PT_RUN(t3)
    sub t2, t2, t4              # the head page
3:
    slli t2, t2, PAGE_SHIFT
    li t0, ADDR_HEAP
    add a0, t0, t2
    ret
1:
    lbu t4, PT_CLASS(t3)
    la t0, class_size
    slli t4, t4, 2
    add t0, t0, t4
    lwu t4, 0(t0)               # slot size
    li t0, PAGE_SIZE - 1
    and t1, t1, t0              # offset in the page
    divu t0, t1, t4
    mul t0, t0, t4              # slot offset
    add t1, t0, t4
    li t3, PAGE_SIZE
    bgtu t1, t3, 8f             # in the page's unused tail
    slli t2, t2, PAGE_SHIFT
    add t2, t2, t0
    li t0, ADDR_HEAP
    add a0, t0, t2
    lwu t0, 4(a0)
    li t1, OBJ_FREE
    beq t0, t1, 8f
    ret
8:
    li a0, 0
    ret

# int page_noscan(void *block)
page_noscan:
    li t0, ADDR_HEAP
    sub t0, a0, t0
    srli t0, t0, PAGE_SHIFT
    slli t0, t0, 3
    li t1, ADDR_PTAB
    add t0, t1, t0
    lbu a0, PT_NOSCAN(t0)
    ret

# void mark_ptr(long word)
# If word points into an unmarked heap object, marks it and queues it for scanning.
mark_ptr:
    addi sp, sp, -16
    sd ra, 8(sp)
    sd s0, 0(sp)
    call gc_find
    beqz a0, 9f
    lwu t0, 4(a0)
    li t1, OBJ_MARK
    and t2, t0, t1
    bnez t2, 9f
    or t0, t0, t1
    sw t0, 4(a0)
    mv s0, a0
    call page_noscan
    bnez a0, 9f
    la t0, ms_ptr
    ld t1, 0(t0)
    li t2, ADDR_MARKSTACK + MARKSTACK_SIZE
    bgeu t1, t2, 8f
    sd s0, 0(t1)
    addi t1, t1, 8
    sd t1, 0(t0)
    j 9f
8:
    # ponytail: on overflow the object stays marked but unscanned; rescan_marked catches up with
    # a full heap walk. Needs more than 8M queued objects at once.
    la t0, ms_overflow
    li t1, 1
    sd t1, 0(t0)
9:
    ld ra, 8(sp)
    ld s0, 0(sp)
    addi sp, sp, 16
    ret

# void mark_range(long *lo, long *hi)
# Treats every aligned word in [lo, hi) as a possible pointer.
mark_range:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    mv s1, a1
    li s2, ADDR_HEAP
    la t0, heap_top
    ld s3, 0(t0)
    slli s3, s3, PAGE_SHIFT
    add s3, s2, s3              # end of the used heap
1:
    addi t0, s0, 8
    bgtu t0, s1, 9f
    ld a0, 0(s0)
    addi s0, s0, 8
    bltu a0, s2, 1b
    bgeu a0, s3, 1b
    call mark_ptr
    j 1b
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# void scan_obj(void *block)
# Marks what an object's payload points to. The FAULT builds skip one kind each (US6).
scan_obj:
    lwu a1, 0(a0)               # size from the header
    li t0, MAX_SMALL
    bleu a1, t0, 1f
    li t0, ADDR_HEAP            # a large object: no more than its run
    sub t0, a0, t0
    srli t0, t0, PAGE_SHIFT
    slli t0, t0, 3
    li t1, ADDR_PTAB
    add t0, t1, t0
    lwu t0, PT_RUN(t0)
    slli t0, t0, PAGE_SHIFT
    bleu a1, t0, 1f
    mv a1, t0
1:
.ifdef GC_FAULT_LISTBUF
    lwu t0, 4(a0)
    li t1, OBJ_LIST | OBJ_MARK
    bne t0, t1, 2f
    li a1, L_DATA               # the element buffer is never reached
2:
.endif
.ifdef GC_FAULT_MAPARR
    lwu t0, 4(a0)
    li t1, OBJ_MAP | OBJ_MARK
    bne t0, t1, 3f
    li a1, M_INDEX
3:
.endif
.ifdef GC_FAULT_RECORD
    lwu t0, 4(a0)
    li t1, OBJ_RECORD | OBJ_MARK
    bne t0, t1, 4f
    li a1, R_FIELDS
4:
.endif
.ifdef GC_FAULT_VARIANT
    lwu t0, 4(a0)
    li t1, OBJ_VARIANT | OBJ_MARK
    bne t0, t1, 5f
    li a1, V_PAY
5:
.endif
.ifdef GC_FAULT_CLOSURE
    lwu t0, 4(a0)
    li t1, OBJ_CLOSURE | OBJ_MARK
    bne t0, t1, 6f
    li a1, CL_CAPS
6:
.endif
    add a1, a0, a1
    addi a0, a0, 8
    tail mark_range

# void drain(void)
# Scans queued objects until the mark stack is empty.
drain:
    addi sp, sp, -16
    sd ra, 8(sp)
1:
    la t6, rt_budget          # POLL 1: --timeout budget
    ld t5, 0(t6)
    addi t5, t5, -1
    sd t5, 0(t6)
    bgtz t5, 1000f
    call rt_poll
1000:
    la t0, ms_ptr
    ld t1, 0(t0)
    li t2, ADDR_MARKSTACK
    bleu t1, t2, 9f
    addi t1, t1, -8
    sd t1, 0(t0)
    ld a0, 0(t1)
    call scan_obj
    j 1b
9:
    ld ra, 8(sp)
    addi sp, sp, 16
    ret

# void rescan_marked(void)
# After a mark-stack overflow: scans every marked object on a scanned page again.
rescan_marked:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    li s0, 0                    # page
1:
    la t0, heap_top
    ld t0, 0(t0)
    bgeu s0, t0, 9f
    li t0, ADDR_PTAB
    slli t1, s0, 3
    add s3, t0, t1              # entry
    lbu t0, PT_NOSCAN(s3)
    bnez t0, 8f
    slli s1, s0, PAGE_SHIFT
    li t0, ADDR_HEAP
    add s1, t0, s1              # page start
    lbu t0, PT_KIND(s3)
    li t1, PG_LARGE
    bne t0, t1, 2f
    lwu t0, 4(s1)
    li t1, OBJ_MARK
    and t0, t0, t1
    beqz t0, 8f
    mv a0, s1
    call scan_obj
    j 8f
2:
    li t1, PG_SMALL
    bne t0, t1, 8f
    lbu t0, PT_CLASS(s3)
    la t1, class_size
    slli t0, t0, 2
    add t1, t1, t0
    lwu s2, 0(t1)               # slot size
    li t0, PAGE_SIZE
    divu t0, t0, s2
    mul t0, t0, s2
    add s3, s1, t0              # past the last slot
3:
    bgeu s1, s3, 8f
    lwu t0, 4(s1)
    li t1, OBJ_MARK
    and t0, t0, t1
    beqz t0, 4f
    mv a0, s1
    call scan_obj
4:
    add s1, s1, s2
    j 3b
8:
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

# void gc_collect(void)
# One full mark-sweep. Spills every s register into its own frame first, so values that live
# only in registers are roots too.
    .globl gc_collect
gc_collect:
    addi sp, sp, -112
    sd ra, 104(sp)
    sd s0, 96(sp)
    sd s1, 88(sp)
    sd s2, 80(sp)
    sd s3, 72(sp)
    sd s4, 64(sp)
    sd s5, 56(sp)
    sd s6, 48(sp)
    sd s7, 40(sp)
    sd s8, 32(sp)
    sd s9, 24(sp)
    sd s10, 16(sp)
    sd s11, 8(sp)
    call sample_peak
    la t0, ms_ptr
    li t1, ADDR_MARKSTACK
    sd t1, 0(t0)
    la t0, ms_overflow
    sd zero, 0(t0)
.ifndef GC_FAULT_NATSTACK
    mv a0, sp
    li a1, ADDR_STACK + STACK_SIZE
    call mark_range
.endif
.ifndef GC_FAULT_VSTACK
    la t0, vbase
    ld a0, 0(t0)
    la t0, vsp
    ld a1, 0(t0)
    call mark_range
.endif
.ifndef GC_FAULT_GLOBALS
    la a0, __DATA_BEGIN__
    la a1, _end
    call mark_range
.endif
.ifndef GC_FAULT_PERM
    li a0, ADDR_PERM
    la t0, arena_ptr
    ld a1, 0(t0)
    call mark_range
.endif
1:
    call drain
    la t0, ms_overflow
    ld t1, 0(t0)
    beqz t1, 2f
    sd zero, 0(t0)
    call rescan_marked
    j 1b
2:
    call sweep
    la t0, gc_since
    ld t1, 0(t0)
    sd zero, 0(t0)
    la t0, gc_total
    ld t2, 0(t0)
    add t2, t2, t1
    sd t2, 0(t0)
    la t0, gc_count
    ld t1, 0(t0)
    addi t1, t1, 1
    sd t1, 0(t0)
    call set_trigger
.ifdef GC_STRESS_BUILD
    call gc_selfcheck
.endif
    ld ra, 104(sp)
    ld s0, 96(sp)
    ld s1, 88(sp)
    ld s2, 80(sp)
    ld s3, 72(sp)
    ld s4, 64(sp)
    ld s5, 56(sp)
    ld s6, 48(sp)
    ld s7, 40(sp)
    ld s8, 32(sp)
    ld s9, 24(sp)
    ld s10, 16(sp)
    ld s11, 8(sp)
    addi sp, sp, 112
    ret

# void sample_peak(void)
# gc_peak = max(gc_peak, gc_live + gc_since): called at each collection and at the end.
    .globl sample_peak
sample_peak:
    la t0, gc_live
    ld t0, 0(t0)
    la t1, gc_since
    ld t1, 0(t1)
    add t0, t0, t1
    la t1, gc_peak
    ld t2, 0(t1)
    bgeu t2, t0, 1f
    sd t0, 0(t1)
1:
    ret

# void sweep(void)
# Frees every unmarked object and clears the marks. Pages are walked from the top down and slots
# from the end of each page, so the rebuilt free lists come out in ascending address order.
sweep:
    addi sp, sp, -80
    sd ra, 72(sp)
    sd s0, 64(sp)
    sd s1, 56(sp)
    sd s2, 48(sp)
    sd s3, 40(sp)
    sd s4, 32(sp)
    sd s5, 24(sp)
    sd s6, 16(sp)
    la t0, fl_heads
    li t1, NCLASSES * 2
1:
    sd zero, 0(t0)
    addi t0, t0, 8
    addi t1, t1, -1
    bnez t1, 1b
    li s5, 0                    # live bytes
    la t0, heap_top
    ld s0, 0(t0)                # page + 1
    mv s6, s0                   # lowest free page seen
2:
    beqz s0, 80f
    la t6, rt_budget          # POLL 16: --timeout budget
    ld t5, 0(t6)
    addi t5, t5, -16
    sd t5, 0(t6)
    bgtz t5, 1000f
    call rt_poll
1000:
    addi s0, s0, -1
    li t0, ADDR_PTAB
    slli t1, s0, 3
    add s1, t0, t1              # entry
    slli s2, s0, PAGE_SHIFT
    li t0, ADDR_HEAP
    add s2, t0, s2              # page start
    lbu t0, PT_KIND(s1)
    li t1, PG_SMALL
    beq t0, t1, 10f
    li t1, PG_LARGE
    beq t0, t1, 30f
    bnez t0, 2b                 # a tail: handled with its head
    mv s6, s0
    j 2b
10:                             # small page: first pass frees and counts
    lbu t0, PT_CLASS(s1)
    la t1, class_size
    slli t0, t0, 2
    add t1, t1, t0
    lwu s3, 0(t1)               # slot size
    li t0, PAGE_SIZE
    divu t0, t0, s3
    mul t0, t0, s3
    add s4, s2, t0              # past the last slot
    mv a0, s2
    li a2, 0                    # live slots
11:
    bgeu a0, s4, 15f
    lwu t0, 4(a0)
    li t1, OBJ_FREE
    beq t0, t1, 14f
    li t1, OBJ_MARK
    and t2, t0, t1
    beqz t2, 12f
    xor t0, t0, t1
    sw t0, 4(a0)
    addi a2, a2, 1
    j 14f
12:
.ifdef GC_STRESS_BUILD
    sd a0, 0(sp)                # scratch slots: keep a0 and a2 around the call
    sd a2, 8(sp)
    addi a0, a0, 8
    add a1, a0, s3
    addi a1, a1, -8
    call poison_range
    ld a0, 0(sp)
    ld a2, 8(sp)
.endif
    li t0, OBJ_FREE
    slli t0, t0, 32
    sd t0, 0(a0)
14:
    add a0, a0, s3
    j 11b
15:
    bnez a2, 16f
    sb zero, PT_KIND(s1)        # nothing alive: the page is free again
    mv s6, s0
    j 2b
16:
    mul t0, a2, s3
    add s5, s5, t0
    lbu t0, PT_CLASS(s1)        # second pass, from the end: push free slots
    slli t0, t0, 1
    lbu t1, PT_NOSCAN(s1)
    or t0, t0, t1
    slli t0, t0, 3
    la t1, fl_heads
    add t0, t1, t0
    ld t1, 0(t0)
    li t3, OBJ_FREE
17:
    sub s4, s4, s3
    lwu t2, 4(s4)
    bne t2, t3, 18f
    sd t1, 8(s4)
    mv t1, s4
18:
    bne s4, s2, 17b
    sd t1, 0(t0)
    j 2b
30:                             # large object
    lwu t0, 4(s2)
    li t1, OBJ_MARK
    and t2, t0, t1
    lwu s3, PT_RUN(s1)
    beqz t2, 31f
    xor t0, t0, t1
    sw t0, 4(s2)
    slli t0, s3, PAGE_SHIFT
    add s5, s5, t0
    j 2b
31:
.ifdef GC_STRESS_BUILD
    mv a0, s2
    slli a1, s3, PAGE_SHIFT
    add a1, s2, a1
    call poison_range
.endif
    mv t0, s1                   # free the whole run
    mv t1, s3
32:
    sd zero, 0(t0)
    addi t0, t0, 8
    addi t1, t1, -1
    bnez t1, 32b
    mv s6, s0
    j 2b
80:
    la t0, gc_live
    sd s5, 0(t0)
    la t0, page_hint
    sd s6, 0(t0)
    la t0, heap_top             # drop free pages from the top
    ld t1, 0(t0)
    li t2, ADDR_PTAB
81:
    beqz t1, 82f
    addi t3, t1, -1
    slli t3, t3, 3
    add t3, t2, t3
    lbu t3, PT_KIND(t3)
    bnez t3, 82f
    addi t1, t1, -1
    j 81b
82:
    sd t1, 0(t0)
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

.ifdef GC_STRESS_BUILD
    .section .rodata
s_reclaimed:    .string "error[T0003]: internal error: a reclaimed value was used\n"
    .text
# void *gc_check(void *obj, int kind)
# Stress build: obj must be a live heap object of this kind; objects outside the heap are
# permanent and pass. Otherwise stops with T0003 (contracts/error-codes-v3.md). Returns obj.
    .globl gc_check
gc_check:
    li t0, ADDR_HEAP
    bltu a0, t0, 9f
    la t1, heap_pages
    ld t1, 0(t1)
    slli t1, t1, PAGE_SHIFT
    add t1, t0, t1
    bgeu a0, t1, 9f
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    mv s0, a0
    mv s1, a1
    call gc_find
    bne a0, s0, 8f
    lwu t0, 4(s0)
    bne t0, s1, 8f
    mv a0, s0
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
9:
    ret
8:
    li a0, 2
    la a1, s_reclaimed
    call out_cstr
    li a0, EXIT_INTERNAL
    call vo_exit
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void poison_range(long *lo, long *hi)
# Fills reclaimed memory with 0xdeadbeefdeadbeef, so a use after reclaim reads nonsense.
poison_range:
    li t0, 0xdeadbeefdeadbeef
1:
    bgeu a0, a1, 2f
    sd t0, 0(a0)
    addi a0, a0, 8
    j 1b
2:
    ret

# void read_stress_env(void)
# gc_stress = N from VO_GC_STRESS=N in the environment, default 64.
read_stress_env:
    addi sp, sp, -32
    sd ra, 24(sp)
    sd s0, 16(sp)
    sd s1, 8(sp)
    la t0, gc_stress
    li t1, 64
    sd t1, 0(t0)
    la t0, envp
    ld s0, 0(t0)
1:
    ld s1, 0(s0)
    beqz s1, 9f
    addi s0, s0, 8
    mv a0, s1
    la a1, s_stress_env
    li a2, 13
    call mem_eq
    beqz a0, 1b
    addi s1, s1, 13
    li t1, 0
2:
    lbu t0, 0(s1)
    addi t0, t0, -'0'
    li t2, 9
    bgtu t0, t2, 3f
    li t2, 10
    mul t1, t1, t2
    add t1, t1, t0
    addi s1, s1, 1
    j 2b
3:
    beqz t1, 9f
    la t0, gc_stress
    sd t1, 0(t0)
9:
    ld ra, 24(sp)
    ld s0, 16(sp)
    ld s1, 8(sp)
    addi sp, sp, 32
    ret

# void gc_selfcheck(void)
# For every allocated object, gc_find of its first, middle and last byte must give its start.
gc_selfcheck:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    li s0, 0
1:
    la t0, heap_top
    ld t0, 0(t0)
    bgeu s0, t0, 9f
    li t0, ADDR_PTAB
    slli t1, s0, 3
    add t0, t0, t1
    lbu t1, PT_KIND(t0)
    slli s1, s0, PAGE_SHIFT
    li t2, ADDR_HEAP
    add s1, t2, s1              # page start
    li t2, PG_LARGE
    bne t1, t2, 2f
    li s2, PAGE_SIZE            # a large object: its first page is enough
    mv s3, s1
    addi s3, s3, 8
    j 4f
2:
    li t2, PG_SMALL
    bne t1, t2, 8f
    lbu t1, PT_CLASS(t0)
    la t2, class_size
    slli t1, t1, 2
    add t2, t2, t1
    lwu s2, 0(t2)               # slot size
    li t0, PAGE_SIZE
    divu t0, t0, s2
    mul t0, t0, s2
    add s3, s1, t0              # past the last slot
4:
    bgeu s1, s3, 8f
    lwu t0, 4(s1)
    li t1, OBJ_FREE
    beq t0, t1, 7f
    mv a0, s1
    call gc_find
    bne a0, s1, 90f
    lwu t0, 0(s1)
    srli t0, t0, 1
    add a0, s1, t0
    call gc_find
    bne a0, s1, 90f
    lwu t0, 0(s1)
    add a0, s1, t0
    addi a0, a0, -1
    call gc_find
    bne a0, s1, 90f
7:
    add s1, s1, s2
    j 4b
8:
    addi s0, s0, 1
    j 1b
90:
    li a0, 2
    la a1, s_selfcheck
    call out_cstr
    li a0, EXIT_INTERNAL
    call vo_exit
9:
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret
.endif

    .section .rodata
s_units:    .string "B\0\0\0KiB\0MiB\0GiB"
s_mem_json: .string "{\"v\":1,\"kind\":\"memory\",\"command\":\""
s_mem_test: .string ",\"test\":"
s_mem_peak: .string ",\"peak_live\":"
s_mem_alloc: .string ",\"allocated\":"
s_mem_count: .string ",\"collections\":"
s_mem_limit: .string ",\"limit\":"
s_mem_end:  .string "}\n"
s_mem_h1:   .string "memory: peak live "
s_mem_h2:   .string ", allocated "
s_mem_h3:   .string " collections (limit "
s_mem_h4:   .string ")\n"
s_comma_sp: .string ", "
    .text

# void size_into(void *sb, long bytes)
# Appends a size as B, KiB, MiB or GiB with one decimal (rounded), which is left out when it is 0.
    .globl size_into
size_into:
    li a2, 0
    j size_into_r

# void size_into_up(void *sb, long bytes)
# size_into, rounding up: a size just over a limit never prints as the limit itself.
    .globl size_into_up
size_into_up:
    li a2, 1

# void size_into_r(void *sb, long bytes, int up)
size_into_r:
    addi sp, sp, -48
    sd ra, 40(sp)
    sd s0, 32(sp)
    sd s1, 24(sp)
    sd s2, 16(sp)
    sd s3, 8(sp)
    mv s0, a0
    mv s1, a1
    mv s3, a2
    li s2, 0                    # unit index: the unit is 1 << (10 * s2)
1:
    li t1, 3
    beq s2, t1, 2f
    li t2, 10
    mul t2, s2, t2
    li t1, 1
    sll t1, t1, t2
    slli t1, t1, 10             # the next unit
    bltu s1, t1, 2f
    addi s2, s2, 1
    j 1b
2:
    li t2, 10
    mul t2, s2, t2
    li t1, 1
    sll t1, t1, t2              # unit
    li t2, 10
    mul t0, s1, t2
    srli t2, t1, 1              # round to nearest, or up
    beqz s3, 4f
    addi t2, t1, -1
4:
    add t0, t0, t2
    divu t0, t0, t1             # tenths
    li t2, 10
    divu a1, t0, t2
    remu t0, t0, t2
    sd t0, 0(sp)
    mv a0, s0
    call int_to_dec
    ld t0, 0(sp)
    beqz t0, 3f
    mv a0, s0
    li a1, '.'
    call sb_byte
    ld a1, 0(sp)
    addi a1, a1, '0'
    mv a0, s0
    call sb_byte
3:
    mv a0, s0
    li a1, ' '
    call sb_byte
    la a1, s_units
    slli t0, s2, 2
    add a1, a1, t0
    mv a0, s0
    call sb_cstr
    ld ra, 40(sp)
    ld s0, 32(sp)
    ld s1, 24(sp)
    ld s2, 16(sp)
    ld s3, 8(sp)
    addi sp, sp, 48
    ret

# void gc_report(int fd, char *command, void *test)
# With --mem-stats: the memory report of a run or a test (contracts/cli-v3.md), as a JSON record
# with --json, else one human line. test is the test's name (a str), or 0 for a run.
    .globl gc_report
gc_report:
    la t0, opt_mem_stats
    ld t0, 0(t0)
    bnez t0, 1f
    ret
1:
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
    mv s2, a2
    call sample_peak
    la t0, gc_peak              # the numbers, before the report allocates anything
    ld s4, 0(t0)
    la t0, gc_total
    ld s5, 0(t0)
    la t0, gc_since
    ld t1, 0(t0)
    add s5, s5, t1
    la t0, gc_count
    ld s6, 0(t0)
    call sb_new
    mv s3, a0
    la t0, opt_json
    ld t0, 0(t0)
    beqz t0, 50f
    la a1, s_mem_json
    call sb_cstr
    mv a0, s3
    mv a1, s1
    call sb_cstr
    mv a0, s3
    li a1, '"'
    call sb_byte
    beqz s2, 2f
    mv a0, s3
    la a1, s_mem_test
    call sb_cstr
    mv a0, s3
    mv a1, s2
    call json_str_into
2:
    mv a0, s3
    la a1, s_mem_peak
    call sb_cstr
    mv a0, s3
    mv a1, s4
    call int_to_dec
    mv a0, s3
    la a1, s_mem_alloc
    call sb_cstr
    mv a0, s3
    mv a1, s5
    call int_to_dec
    mv a0, s3
    la a1, s_mem_count
    call sb_cstr
    mv a0, s3
    mv a1, s6
    call int_to_dec
    mv a0, s3
    la a1, s_mem_limit
    call sb_cstr
    la t0, gc_limit
    ld a1, 0(t0)
    mv a0, s3
    call int_to_dec
    mv a0, s3
    la a1, s_mem_end
    call sb_cstr
    j 90f
50:
    la a1, s_mem_h1
    call sb_cstr
    mv a0, s3
    mv a1, s4
    call size_into
    mv a0, s3
    la a1, s_mem_h2
    call sb_cstr
    mv a0, s3
    mv a1, s5
    call size_into
    mv a0, s3
    la a1, s_comma_sp
    call sb_cstr
    mv a0, s3
    mv a1, s6
    call int_to_dec
    mv a0, s3
    la a1, s_mem_h3
    call sb_cstr
    la t0, gc_limit
    ld a1, 0(t0)
    mv a0, s3
    call size_into
    mv a0, s3
    la a1, s_mem_h4
    call sb_cstr
90:
    mv a0, s0
    ld a1, SB_PTR(s3)
    ld a2, SB_LEN(s3)
    call out_bytes
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

