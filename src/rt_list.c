/* Runtime lists: header | len | cap | data (slot[cap]). Growth copies (the arena never frees).
 * Ported from src/rt_list.s (spec 008); the fixed page offset that pinned list_sort's and
 * list_push's loops under qemu-user is gone (results.md records the speed before and after). */
#include "vo.h"

typedef union {
    long i;
    double d;
} Bits;

List *list_new(long cap)
{
    if (!cap) cap = 4;
    List *l = alloc_obj(OBJ_LIST, LIST_SIZE);
    l->len = 0;
    l->cap = cap;
    GC_ROOT(&l);
    l->data = alloc(cap * 8);
    GC_UNROOT(1);
    return l;
}

void list_push(List *list, long value)
{
#ifdef GC_STRESS_BUILD
    gc_check(list, OBJ_LIST); /* stress build: a live OBJ_LIST (spec 003 FR-011) */
#endif
    if (list->len >= list->cap) {
        long cap = list->cap ? list->cap * 2 : 4;
        GC_ROOT(&list);
        GC_ROOT(&value);
        W *grown = alloc(cap * 8);
        GC_UNROOT(2);
        W *old = list->data;
        list->data = grown;
        mem_copy8((long *)grown, (long *)old, list->len * 8);
        list->cap = cap;
    }
    list->data[list->len++].i = value;
}

/* Copies a whole number of 8-byte words. */
void mem_copy8(long *dst, long *src, long bytes)
{
    for (; bytes > 0; bytes -= 8) *dst++ = *src++;
}

Variant *make_result(int tag, long payload)
{
    GC_ROOT(&payload);
    Variant *v = alloc_obj(OBJ_VARIANT, VARIANT_SIZE);
    GC_UNROOT(1);
    v->tag = (long)(uint32_t)tag;
    v->pay.i = payload;
    return v;
}

Variant *make_err(char *message)
{
    return make_result(1, (long)str_from_cstr(message));
}

/* result[T]: ok(last element) or err("empty"). */
Variant *list_pop(List *list)
{
    if (!list->len) return make_err("empty");
    list->len--;
    return make_result(0, list->data[list->len].i);
}

/* Elements a..b (half-open) as a new list; R0003 when out of range. */
List *list_slice(List *list, long a, long b, Node *node)
{
    if (a < 0 || a > list->len) rt_index_error(node, a, list->len);
    if (b < a || b > list->len) rt_index_error(node, b, list->len);
    GC_ROOT(&list);
    List *r = list_new(b - a);
    GC_UNROOT(1);
    r->len = b - a;
    mem_copy8((long *)r->data, (long *)(list->data + a), (b - a) * 8);
    return r;
}

/* Spec 014: the record type elem_less compares when kind is TY_RECORD, set by the caller (sort, min,
 * max, sort_by). ponytail: a global, safe because record comparison runs no vo code (no nesting). */
Type *elem_record_type;

/* a < b for sortable element kinds (int, float, str; records field by field). */
int elem_less(long a, long b, int kind)
{
    if (kind == TY_RECORD) return value_cmp(a, b, elem_record_type) < 0;
    if (kind == TY_FLOAT) {
        Bits x = {.i = a}, y = {.i = b};
        return x.d < y.d;
    }
    if (kind == TY_STR || kind == TY_BYTES) return str_cmp((Str *)a, (Str *)b) < 0;
    return a < b;
}

/* Stable bottom-up merge sort into a scratch array. */
void list_sort(List *list, int kind)
{
    long n = list->len;
    W *src = list->data;
    if (n < 2) return;
    GC_ROOT(&list);
    GC_ROOT(&src);
    W *dst = alloc(n * 8);
    GC_ROOT(&dst);
    for (long width = 1; width < n; width *= 2) {
        for (long left = 0; left < n; left += 2 * width) {
            long mid = left + width < n ? left + width : n;
            long end = left + 2 * width < n ? left + 2 * width : n;
            long i = left, j = mid, k = left;
            for (;;) {
                POLL(1);
                if (k >= end) break;
                int take_right; /* take right only when right < left (keeps the sort stable) */
                if (i >= mid)
                    take_right = 1;
                else if (j >= end)
                    take_right = 0;
                else if (kind == TY_INT) /* ints: compared here (spec 004: elem_less's int case, inline) */
                    take_right = src[j].i < src[i].i;
                else
                    take_right = elem_less(src[j].i, src[i].i, kind);
                dst[k++] = take_right ? src[j++] : src[i++];
            }
        }
        W *t = src; /* swap src and dst */
        src = dst;
        dst = t;
    }
    list->data = src; /* the sorted data is in src */
    list->cap = n;
    GC_UNROOT(3);
}
