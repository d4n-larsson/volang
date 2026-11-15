/* The collected heap (spec 003): non-moving mark-sweep over size-classed 64 KiB pages (ported from
 * src/gc.s, spec 008).
 *
 * Regions (vo.h, data-model.md): pages at ADDR_HEAP, one 8-byte page-table entry per page at
 * ADDR_PTAB, the mark stack at ADDR_MARKSTACK. All fixed, so collection is deterministic.
 *
 * Objects keep the runtime's header (size:u32 | kind:u32); gc_alloc writes it. A free small slot has
 * kind OBJ_FREE and links to the next free slot through its first payload word. The mark bit is
 * OBJ_MARK in the kind word, set only during a collection.
 *
 * Roots (research R4): the value stack, the C root stack (GC_ROOT), the globals (vo_data_begin ..
 * _end: the platform file starts .data, the linker script ends .bss) and the permanent arena. No native stack or
 * register is scanned, so retention is the same on every platform. Every word that points into an
 * allocated object (interior pointers too) keeps it alive.
 * ponytail: object contents are scanned conservatively and can retain garbage that a stale word
 * happens to point at; deterministic thanks to the fixed map. Precise layouts per kind would fix it.
 * ponytail: the whole permanent arena is scanned at every collection (a few MiB for typical
 * programs); a remembered set of perm objects written at run time would avoid it. */
#include "vo.h"

typedef struct PageEntry {
    uint8_t kind; /* PG_* */
    uint8_t cls;
    uint8_t noscan;
    uint8_t dirty;
    uint32_t run; /* a large object's page count; a tail page's distance to its head */
} PageEntry;
CHECK_OFFSET(PageEntry, kind, PT_KIND);
CHECK_OFFSET(PageEntry, cls, PT_CLASS);
CHECK_OFFSET(PageEntry, noscan, PT_NOSCAN);
CHECK_OFFSET(PageEntry, run, PT_RUN);

#define PTAB ((PageEntry *)ADDR_PTAB)
#define HDR_SIZE(p) (((uint32_t *)(p))[0])
#define HDR_KIND(p) (((uint32_t *)(p))[1])
#define PAGE_START(pg) (ADDR_HEAP + ((unsigned long)(pg) << PAGE_SHIFT))
#define PAGE_OF(p) (((unsigned long)(p) - ADDR_HEAP) >> PAGE_SHIFT)
#define FREE_HDR ((unsigned long)OBJ_FREE << 32)

long gc_on;    /* 1 while a program runs: plain allocations go to the heap */
long gc_perm;  /* perm-bracket depth: > 0 sends allocations to the permanent arena */
long gc_limit = GC_DEFAULT_LIMIT; /* live-data limit in bytes (--max-memory) */
long gc_since; /* bytes allocated since the last collection */
static unsigned long gc_trigger; /* collect when gc_since would reach this */
long gc_live;  /* live bytes after the last collection */
long gc_peak;  /* highest gc_live + gc_since seen (the reported peak) */
long gc_total; /* bytes allocated before the last collection (report: + gc_since) */
long gc_count; /* collections so far */
long gc_roots_n; /* entries on the C root stack at ADDR_ROOTS (research R4) */
static long gc_inited;
static unsigned long heap_pages; /* pages in the reservation */
static unsigned long heap_top;   /* 1 + the highest page in use */
static unsigned long heap_hwm;   /* pages at or above this index were never used (still zero) */
static unsigned long page_hint;  /* no free page below this index */
static unsigned long *ms_ptr;    /* mark stack top */
static long ms_overflow;         /* 1 when a push found the mark stack full */
static unsigned long *fl_heads[NCLASSES * 2]; /* free list per (class, noscan), ascending addresses */
#ifdef GC_STRESS_BUILD
long gc_stress; /* collect before every Nth allocation (VO_GC_STRESS, default 64) */
static unsigned long gc_tick;
#endif

extern char vo_data_begin[], _end[];

static const uint32_t class_size[NCLASSES] = {
    16, 32, 48, 64, 96, 128, 192, 256, 384, 512, 768, 1024,
    1536, 2048, 3072, 4096, 6144, 8192, 12288, 16384,
};
#ifndef GC_STRESS_BUILD /* (the stress build allocates through gc_alloc_slow only) */
static const uint8_t class_of_16[65] = { /* class for sizes up to 1024, indexed by ceil(size / 16) */
    0, 0, 1, 2, 3, 4, 4, 5, 5, 6, 6, 6, 6, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 9, 9, 9,
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
    11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11, 11,
};
#endif

static void internal_error(char *what)
{
    out_cstr(2, what);
    vo_exit(EXIT_INTERNAL);
}

#ifdef GC_STRESS_BUILD
/* Fills reclaimed memory with 0xdeadbeefdeadbeef, so a use after reclaim reads nonsense. */
static void poison_range(unsigned long *lo, unsigned long *hi)
{
    while (lo < hi) *lo++ = 0xdeadbeefdeadbeefUL;
}

/* gc_stress = N from VO_GC_STRESS=N in the environment, default 64. */
static void read_stress_env(void)
{
    gc_stress = 64;
    for (char **e = (char **)envp; *e; e++) {
        if (!mem_eq(*e, "VO_GC_STRESS=", 13)) continue;
        long n = 0;
        for (char *p = *e + 13; (unsigned)(*p - '0') <= 9; p++) n = n * 10 + (*p - '0');
        if (n) gc_stress = n;
        return;
    }
}
#endif

/* gc_trigger = min(max(GC_MIN_TRIGGER, live), limit - live): live data may double between
 * collections, but a collection always comes before live data could pass the limit (research §3). */
static void set_trigger(void)
{
    unsigned long t = (unsigned long)gc_live > GC_MIN_TRIGGER ? (unsigned long)gc_live : GC_MIN_TRIGGER;
    long room = gc_limit - gc_live;
    if (room < 0) room = 0;
    gc_trigger = (unsigned long)room < t ? (unsigned long)room : t;
}

/* Empties the heap: every page free, counters zero (before each test, research §3). */
void gc_reset(void)
{
#ifdef GC_STRESS_BUILD
    poison_range((unsigned long *)ADDR_HEAP, (unsigned long *)PAGE_START(heap_top)); /* stale pointers read poison */
#endif
    for (unsigned long i = 0; i < heap_top; i++) *(unsigned long *)&PTAB[i] = 0;
    heap_top = 0;
    for (int i = 0; i < NCLASSES * 2; i++) fl_heads[i] = 0;
    page_hint = 0;
    gc_since = 0;
    gc_live = 0;
    gc_peak = 0;
    gc_total = 0;
    gc_count = 0;
#ifdef GC_STRESS_BUILD
    gc_tick = 0;
#endif
    set_trigger();
}

/* Reserves the heap for gc_limit bytes of live data (once per process) and empties it. */
void gc_init(void)
{
    if (!gc_inited) {
        gc_inited = 1;
        long bytes = gc_limit + GC_HEADROOM; /* reservation: limit + 64 MiB */
        heap_pages = (unsigned long)bytes >> PAGE_SHIFT;
        map_fixed(ADDR_HEAP, bytes, MAP_NORESERVE);
        long ptab = (long)((((unsigned long)bytes >> (PAGE_SHIFT - 3)) + PAGE_SIZE - 1) & -(unsigned long)PAGE_SIZE);
        map_fixed(ADDR_PTAB, ptab, MAP_NORESERVE); /* 8 bytes per page, rounded up to whole pages */
        map_fixed(ADDR_MARKSTACK, MARKSTACK_SIZE, MAP_NORESERVE);
#ifdef GC_STRESS_BUILD
        read_stress_env();
#endif
    }
    gc_reset();
}

/* gc_peak = max(gc_peak, gc_live + gc_since): called at each collection and at the end. */
void sample_peak(void)
{
    unsigned long now = (unsigned long)gc_live + (unsigned long)gc_since;
    if ((unsigned long)gc_peak < now) gc_peak = (long)now;
}

/* Zeroes n bytes (a multiple of 8), skipping the part of the heap that was never used. */
static void zero_block(unsigned long *p, unsigned long n)
{
    unsigned long *end = (unsigned long *)((char *)p + n);
    unsigned long *fresh = (unsigned long *)PAGE_START(heap_hwm); /* first never-used address */
    if (end < fresh) fresh = end;
    while (p < fresh) *p++ = 0;
}

/* The live data (plus the pending request) passes the limit: R0006 through the interpreter. */
static void gc_out_of_memory(long need)
{
    rt_oom_live(need + gc_live);
}

/* Collects; if the live data plus need would pass the limit, collects once more, then reports
 * out-of-memory (R0006). */
static void collect_for(unsigned long need)
{
    gc_collect();
    if ((unsigned long)gc_live + need <= (unsigned long)gc_limit) return;
    gc_collect();
    if ((unsigned long)gc_live + need <= (unsigned long)gc_limit) return;
    gc_out_of_memory((long)need);
}

/* The lowest free page index (heap_top when every page below it is in use). */
static unsigned long first_free_page(void)
{
    unsigned long i = page_hint;
    while (i < heap_top && PTAB[i].kind) i++;
    page_hint = i;
    return i;
}

/* Pages first..first+n are now in use: moves heap_top and heap_hwm past them. */
static void use_pages(unsigned long first, unsigned long n)
{
    unsigned long end = first + n;
    if (heap_top < end) heap_top = end;
    if (heap_hwm < end) heap_hwm = end;
}

/* Takes the lowest free page for size class cls, threads its slots onto the class's free list in
 * ascending order and returns the list head, or 0 when the reservation is full. */
static unsigned long *new_small_page(int cls, int noscan)
{
    unsigned long pg = first_free_page();
    if (pg >= heap_pages) return 0;
    use_pages(pg, 1);
    page_hint = pg + 1;
    PageEntry *e = &PTAB[pg];
    e->kind = PG_SMALL;
    e->cls = (uint8_t)cls;
    e->noscan = (uint8_t)noscan;
    e->run = 0;
    unsigned long ss = class_size[cls];
    char *start = (char *)PAGE_START(pg);
    char *p = start + PAGE_SIZE / ss * ss; /* past the last slot */
    unsigned long *head = 0;
    do {
        p -= ss;
        ((unsigned long *)p)[0] = FREE_HDR;
        ((unsigned long *)p)[1] = (unsigned long)head;
        head = (unsigned long *)p;
    } while (p != start);
    fl_heads[cls * 2 + noscan] = head;
    return head;
}

/* First fit: the lowest run of n free pages (growing past heap_top when needed), set up as one
 * large object. Returns its start, or 0 when the reservation has no such run. */
static unsigned long *take_run(unsigned long n, int noscan)
{
    unsigned long s = first_free_page(), top = heap_top;
    for (unsigned long c = 0; c < n;) {
        unsigned long t = s + c;
        if (t >= top) break; /* past heap_top: everything above is free */
        if (PTAB[t].kind) { /* restart after the used page */
            s = t + 1;
            c = 0;
        } else {
            c++;
        }
    }
    if (s + n > heap_pages) return 0;
    unsigned long *p = (unsigned long *)PAGE_START(s);
    zero_block(p, n << PAGE_SHIFT); /* while heap_hwm still tells which pages were never used */
    use_pages(s, n);
    PageEntry *e = &PTAB[s];
    e->kind = PG_LARGE;
    e->cls = 0;
    e->noscan = (uint8_t)noscan;
    e->run = (uint32_t)n;
    for (unsigned long i = 1; i < n; i++) {
        e[i].kind = PG_TAIL;
        e[i].noscan = (uint8_t)noscan;
        e[i].run = (uint32_t)i; /* back to the head */
    }
    return p;
}

/* gc_alloc when the fast path cannot serve: collections, new pages, large objects. */
void *gc_alloc_slow(long size, int kind)
{
    if ((unsigned long)size >> 31) { /* no header can describe 2 GiB or more */
        gc_out_of_memory(size);
        return 0;
    }
#ifdef GC_STRESS_BUILD
    if (gc_stress && ++gc_tick >= (unsigned long)gc_stress) {
        gc_tick = 0;
        collect_for(0);
    }
#endif
    int noscan = kind == OBJ_STR || kind == OBJ_BYTES;
    unsigned long *p, bytes;
    if (size <= MAX_SMALL) {
        int cls = 0; /* the first class that fits */
        while (class_size[cls] < (unsigned long)size) cls++;
        bytes = class_size[cls];
        if ((unsigned long)gc_since + bytes >= gc_trigger) collect_for(bytes);
        unsigned long **head = &fl_heads[cls * 2 + noscan];
        while (!*head) {
            if (new_small_page(cls, noscan)) continue;
            collect_for(bytes); /* no free page left in the reservation */
            if (!new_small_page(cls, noscan)) {
                gc_out_of_memory(size);
                return 0;
            }
        }
        p = *head;
        *head = (unsigned long *)p[1]; /* pop */
        zero_block(p, bytes);
    } else { /* large: a run of whole pages */
        unsigned long pages = ((unsigned long)size + PAGE_SIZE - 1) >> PAGE_SHIFT;
        bytes = pages << PAGE_SHIFT;
        if ((unsigned long)gc_since + bytes >= gc_trigger) collect_for(bytes);
        p = take_run(pages, noscan);
        if (!p) {
            collect_for(bytes);
            p = take_run(pages, noscan);
            if (!p) {
                gc_out_of_memory(size);
                return 0;
            }
        }
    }
    gc_since += (long)bytes;
    p[0] = (unsigned long)kind << 32 | (unsigned long)size;
    return p;
}

/* A zeroed heap block of at least size bytes (header included) with its header written. Strings and
 * OBJ_BYTES go to no-scan pages. */
void *gc_alloc(long size, int kind)
{
#ifdef GC_STRESS_BUILD
    return gc_alloc_slow(size, kind); /* the stress build counts every allocation */
#else
    if ((unsigned long)size > 1024) return gc_alloc_slow(size, kind);
    int cls = class_of_16[(size + 15) >> 4];
    unsigned long **head = &fl_heads[cls * 2 + (kind == OBJ_STR || kind == OBJ_BYTES)];
    unsigned long *p = *head;
    if (!p) return gc_alloc_slow(size, kind);
    unsigned long ss = class_size[cls], since = (unsigned long)gc_since + ss;
    if (since >= gc_trigger) return gc_alloc_slow(size, kind); /* time to collect */
    gc_since = (long)since;
    *head = (unsigned long *)p[1]; /* pop */
    for (unsigned long *q = p + 1, *end = (unsigned long *)((char *)p + ss); q < end; q++) *q = 0;
    p[0] = (unsigned long)kind << 32 | (unsigned long)size;
    return p;
#endif
}

/* The start of the allocated heap object that contains addr (interior pointers included), or 0. */
void *gc_find(long addr)
{
    if ((unsigned long)addr < ADDR_HEAP) return 0;
    unsigned long off = (unsigned long)addr - ADDR_HEAP, pg = off >> PAGE_SHIFT;
    if (pg >= heap_top) return 0;
    PageEntry *e = &PTAB[pg];
    switch (e->kind) {
    case PG_SMALL: {
        unsigned long ss = class_size[e->cls];
        unsigned long slot = (off & (PAGE_SIZE - 1)) / ss * ss;
        if (slot + ss > PAGE_SIZE) return 0; /* in the page's unused tail */
        char *obj = (char *)PAGE_START(pg) + slot;
        if (HDR_KIND(obj) == OBJ_FREE) return 0;
        return obj;
    }
    case PG_TAIL:
        return (void *)PAGE_START(pg - e->run); /* the head page */
    case PG_LARGE:
        return (void *)PAGE_START(pg);
    default:
        return 0;
    }
}

/* If word points into an unmarked heap object, marks it and queues it for scanning. */
static void mark_ptr(unsigned long word)
{
    char *obj = gc_find((long)word);
    if (!obj) return;
    uint32_t k = HDR_KIND(obj);
    if (k & OBJ_MARK) return;
    HDR_KIND(obj) = k | (uint32_t)OBJ_MARK;
    if (PTAB[PAGE_OF(obj)].noscan) return;
    if (ms_ptr >= (unsigned long *)(ADDR_MARKSTACK + MARKSTACK_SIZE)) {
        /* ponytail: on overflow the object stays marked but unscanned; rescan_marked catches up
           with a full heap walk. Needs more than 8M queued objects at once. */
        ms_overflow = 1;
        return;
    }
    *ms_ptr++ = (unsigned long)obj;
}

/* Treats every aligned word in [lo, hi) as a possible pointer. */
static void mark_range(unsigned long *lo, unsigned long *hi)
{
    unsigned long end = PAGE_START(heap_top); /* end of the used heap */
    for (; lo + 1 <= hi; lo++) {
        unsigned long w = *lo;
        if (w >= ADDR_HEAP && w < end) mark_ptr(w);
    }
}

/* Marks what an object's payload points to. The FAULT builds skip one kind each (spec 003 US6). */
static void scan_obj(char *obj)
{
    unsigned long size = HDR_SIZE(obj);
    if (size > MAX_SMALL) { /* a large object: no more than its run */
        unsigned long run = (unsigned long)PTAB[PAGE_OF(obj)].run << PAGE_SHIFT;
        if (size > run) size = run;
    }
#ifdef GC_FAULT_LISTBUF
    if (HDR_KIND(obj) == (OBJ_LIST | OBJ_MARK)) size = L_DATA; /* the element buffer is never reached */
#endif
#ifdef GC_FAULT_MAPARR
    if (HDR_KIND(obj) == (OBJ_MAP | OBJ_MARK)) size = M_INDEX;
#endif
#ifdef GC_FAULT_RECORD
    if (HDR_KIND(obj) == (OBJ_RECORD | OBJ_MARK)) size = R_FIELDS;
#endif
#ifdef GC_FAULT_VARIANT
    if (HDR_KIND(obj) == (OBJ_VARIANT | OBJ_MARK)) size = V_PAY;
#endif
#ifdef GC_FAULT_CLOSURE
    if (HDR_KIND(obj) == (OBJ_CLOSURE | OBJ_MARK)) size = CL_CAPS;
#endif
    mark_range((unsigned long *)(obj + 8), (unsigned long *)(obj + size));
}

/* Scans queued objects until the mark stack is empty. */
static void drain(void)
{
    for (;;) {
        POLL(1); /* --timeout budget */
        if (ms_ptr <= (unsigned long *)ADDR_MARKSTACK) return;
        scan_obj((char *)*--ms_ptr);
    }
}

/* After a mark-stack overflow: scans every marked object on a scanned page again. */
static void rescan_marked(void)
{
    for (unsigned long pg = 0; pg < heap_top; pg++) {
        PageEntry *e = &PTAB[pg];
        if (e->noscan) continue;
        char *p = (char *)PAGE_START(pg);
        if (e->kind == PG_LARGE) {
            if (HDR_KIND(p) & OBJ_MARK) scan_obj(p);
        } else if (e->kind == PG_SMALL) {
            unsigned long ss = class_size[e->cls];
            for (char *end = p + PAGE_SIZE / ss * ss; p < end; p += ss)
                if (HDR_KIND(p) & OBJ_MARK) scan_obj(p);
        }
    }
}

/* Frees every unmarked object and clears the marks. Pages are walked from the top down and slots
 * from the end of each page, so the rebuilt free lists come out in ascending address order. */
static void sweep(void)
{
    for (int i = 0; i < NCLASSES * 2; i++) fl_heads[i] = 0;
    unsigned long live = 0, pg = heap_top, lowest_free = heap_top;
    while (pg) {
        POLL(16); /* --timeout budget */
        pg--;
        PageEntry *e = &PTAB[pg];
        char *start = (char *)PAGE_START(pg);
        if (e->kind == PG_SMALL) { /* first pass frees and counts */
            unsigned long ss = class_size[e->cls], alive = 0;
            char *end = start + PAGE_SIZE / ss * ss; /* past the last slot */
            for (char *p = start; p < end; p += ss) {
                uint32_t k = HDR_KIND(p);
                if (k == OBJ_FREE) continue;
                if (k & OBJ_MARK) {
                    HDR_KIND(p) = k ^ (uint32_t)OBJ_MARK;
                    alive++;
                    continue;
                }
#ifdef GC_STRESS_BUILD
                poison_range((unsigned long *)(p + 8), (unsigned long *)(p + ss));
#endif
                *(unsigned long *)p = FREE_HDR;
            }
            if (!alive) { /* nothing alive: the page is free again */
                e->kind = PG_FREE;
                lowest_free = pg;
                continue;
            }
            live += alive * ss;
            unsigned long **head = &fl_heads[e->cls * 2 + e->noscan], *h = *head;
            char *p = end; /* second pass, from the end: push free slots */
            do {
                p -= ss;
                if (HDR_KIND(p) == OBJ_FREE) {
                    ((unsigned long *)p)[1] = (unsigned long)h;
                    h = (unsigned long *)p;
                }
            } while (p != start);
            *head = h;
        } else if (e->kind == PG_LARGE) {
            uint32_t k = HDR_KIND(start);
            unsigned long run = e->run;
            if (k & OBJ_MARK) {
                HDR_KIND(start) = k ^ (uint32_t)OBJ_MARK;
                live += run << PAGE_SHIFT;
                continue;
            }
#ifdef GC_STRESS_BUILD
            poison_range((unsigned long *)start, (unsigned long *)(start + (run << PAGE_SHIFT)));
#endif
            for (unsigned long i = 0; i < run; i++) *(unsigned long *)&e[i] = 0; /* free the whole run */
            lowest_free = pg;
        } else if (e->kind == PG_FREE) {
            lowest_free = pg;
        } /* a tail: handled with its head */
    }
    gc_live = (long)live;
    page_hint = lowest_free;
    while (heap_top && !PTAB[heap_top - 1].kind) heap_top--; /* drop free pages from the top */
}

#ifdef GC_STRESS_BUILD
/* Stress build: obj must be a live heap object of this kind; objects outside the heap are permanent
 * and pass. Otherwise stops with T0003 (contracts/error-codes-v3.md). Returns obj. */
void *gc_check(void *obj, int kind)
{
    if ((unsigned long)obj < ADDR_HEAP || (unsigned long)obj >= PAGE_START(heap_pages)) return obj;
    if (gc_find((long)obj) != obj || HDR_KIND(obj) != (uint32_t)kind)
        internal_error("error[T0003]: internal error: a reclaimed value was used\n");
    return obj;
}

/* For every allocated object, gc_find of its first, middle and last byte must give its start. */
static void gc_selfcheck(void)
{
    for (unsigned long pg = 0; pg < heap_top; pg++) {
        PageEntry *e = &PTAB[pg];
        char *p = (char *)PAGE_START(pg), *end;
        unsigned long ss;
        if (e->kind == PG_LARGE) { /* a large object: its first page is enough */
            ss = PAGE_SIZE;
            end = p + 8;
        } else if (e->kind == PG_SMALL) {
            ss = class_size[e->cls];
            end = p + PAGE_SIZE / ss * ss;
        } else {
            continue;
        }
        for (; p < end; p += ss) {
            if (HDR_KIND(p) == OBJ_FREE) continue;
            if (gc_find((long)p) != p || gc_find((long)(p + (HDR_SIZE(p) >> 1))) != p ||
                gc_find((long)(p + HDR_SIZE(p) - 1)) != p)
                internal_error("error[T0003]: internal error: heap self-check failed\n");
        }
    }
}
#endif

/* One full mark-sweep. */
void gc_collect(void)
{
    sample_peak();
    ms_ptr = (unsigned long *)ADDR_MARKSTACK;
    ms_overflow = 0;
#ifndef GC_FAULT_VSTACK
    mark_range((unsigned long *)vbase, (unsigned long *)vsp);
#endif
#ifndef GC_FAULT_GLOBALS
    mark_range((unsigned long *)vo_data_begin, (unsigned long *)_end);
#endif
#ifndef GC_FAULT_PERM
    mark_range((unsigned long *)ADDR_PERM, (unsigned long *)arena_ptr);
#endif
#ifndef GC_FAULT_ROOTS
    for (long i = 0; i < gc_roots_n; i++) { /* each entry is the address of a C local holding a reference */
        unsigned long *r = gc_roots[i];
        mark_range(r, r + 1);
    }
#endif
    for (;;) {
        drain();
        if (!ms_overflow) break;
        ms_overflow = 0;
        rescan_marked();
    }
    sweep();
    gc_total += gc_since;
    gc_since = 0;
    gc_count++;
    set_trigger();
#ifdef GC_STRESS_BUILD
    gc_selfcheck();
#endif
}

/* GC_ROOT found the C root stack full (ROOTS_CAP, or ROOTS_HWM_LIMIT in stress builds): T0003. */
void gc_roots_overflow(void)
{
    internal_error("error[T0003]: internal error: the C root stack is full\n");
    __builtin_unreachable();
}

/* Stress builds: T0003 unless the C root stack is empty when a run or a test body returns normally
 * (every GC_ROOT matched by GC_UNROOT). Other builds: nothing. */
void gc_roots_check(void)
{
#ifdef GC_STRESS_BUILD
    if (gc_roots_n) internal_error("error[T0003]: internal error: the C root stack is not empty after a run\n");
#endif
}

/* Appends a size as B, KiB, MiB or GiB with one decimal (rounded, or up), which is left out when it
 * is 0. */
static void size_into_r(SB *sb, unsigned long bytes, int up)
{
    static const char units[4][4] = {"B", "KiB", "MiB", "GiB"};
    int u = 0; /* the unit is 1 << (10 * u) */
    while (u != 3 && bytes >= (1UL << (10 * u)) << 10) u++;
    unsigned long unit = 1UL << (10 * u);
    unsigned long tenths = (bytes * 10 + (up ? unit - 1 : unit >> 1)) / unit;
    int_to_dec(sb, (long)(tenths / 10));
    if (tenths % 10) {
        sb_byte(sb, '.');
        sb_byte(sb, (int)('0' + tenths % 10));
    }
    sb_byte(sb, ' ');
    sb_cstr(sb, (char *)units[u]);
}

void size_into(SB *sb, long bytes)
{
    size_into_r(sb, (unsigned long)bytes, 0);
}

/* size_into, rounding up: a size just over a limit never prints as the limit itself. */
void size_into_up(SB *sb, long bytes)
{
    size_into_r(sb, (unsigned long)bytes, 1);
}

/* With --mem-stats: the memory report of a run or a test (contracts/cli-v3.md), as a JSON record
 * with --json, else one human line. test is the test's name (a str), or 0 for a run. */
void gc_report(int fd, char *command, void *test)
{
    if (!opt_mem_stats) return;
    sample_peak();
    long peak = gc_peak, allocated = gc_total + gc_since, count = gc_count; /* before the report allocates */
    GC_ROOT(&test);
    SB *sb = sb_new();
    GC_ROOT(&sb);
    if (opt_json) {
        sb_cstr(sb, "{\"v\":1,\"kind\":\"memory\",\"command\":\"");
        sb_cstr(sb, command);
        sb_byte(sb, '"');
        if (test) {
            sb_cstr(sb, ",\"test\":");
            json_str_into(sb, test);
        }
        sb_cstr(sb, ",\"peak_live\":");
        int_to_dec(sb, peak);
        sb_cstr(sb, ",\"allocated\":");
        int_to_dec(sb, allocated);
        sb_cstr(sb, ",\"collections\":");
        int_to_dec(sb, count);
        sb_cstr(sb, ",\"limit\":");
        int_to_dec(sb, gc_limit);
        sb_cstr(sb, "}\n");
    } else {
        sb_cstr(sb, "memory: peak live ");
        size_into(sb, peak);
        sb_cstr(sb, ", allocated ");
        size_into(sb, allocated);
        sb_cstr(sb, ", ");
        int_to_dec(sb, count);
        sb_cstr(sb, " collections (limit ");
        size_into(sb, gc_limit);
        sb_cstr(sb, ")\n");
    }
    out_bytes(fd, sb->ptr, sb->len);
    GC_UNROOT(2);
}
