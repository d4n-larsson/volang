/* The builtins that take functions (contracts/grammar-v2.md "New builtins"; ported from
 * src/rt_func.s, spec 008). Each calls its function value through call_value; the list is read
 * afresh on every step and never again after the callback (which may shrink it or replace its
 * buffer), so a callback that changes the list cannot make the loop read past its end.
 * Callbacks run program code, which allocates: what the loops hold is rooted (research R4). */
#include "vo.h"

List *hof_map(List *list, void *fn, Node *call)
{
    GC_ROOT(&list);
    GC_ROOT(&fn);
    List *out = list_new(list->len);
    GC_ROOT(&out);
    for (long i = 0; i < list->len; i++) list_push(out, call_value(fn, call, &list->data[i].i));
    GC_UNROOT(3);
    return out;
}

/* Spec 032: map_err(r, f): an ok as it is (values have no identity), err(e) as err(f(e)). */
Variant *hof_map_err(Variant *r, void *fn, Node *call)
{
    if (!(uint32_t)r->tag) return r;
    long e = call_value(fn, call, &r->pay.i);
    return make_result(1, e);
}

List *hof_filter(List *list, void *fn, Node *call)
{
    GC_ROOT(&list);
    GC_ROOT(&fn);
    List *out = list_new(0);
    GC_ROOT(&out);
    long x = 0; /* the element, read before the callback: the callback may shrink the list */
    GC_ROOT(&x);
    for (long i = 0; i < list->len; i++) {
        x = list->data[i].i;
        if (call_value(fn, call, &x)) list_push(out, x);
    }
    GC_UNROOT(4);
    return out;
}

long hof_fold(List *list, long init, void *fn, Node *call)
{
    long args[2]; /* the two argument values: the accumulator and the element */
    GC_ROOT(&list);
    GC_ROOT(&fn);
    GC_ROOT(&args[0]);
    GC_ROOT(&args[1]);
    long acc = init;
    for (long i = 0; i < list->len; i++) {
        args[0] = acc;
        args[1] = list->data[i].i;
        acc = call_value(fn, call, args);
        args[0] = acc;
    }
    GC_UNROOT(4);
    return acc;
}

/* any: stops at the first true; all (all = 1): stops at the first false. */
int hof_any(List *list, void *fn, Node *call, int all)
{
    GC_ROOT(&list);
    GC_ROOT(&fn);
    int result = all; /* none decided it: any is false, all is true */
    for (long i = 0; i < list->len; i++) {
        long r = call_value(fn, call, &list->data[i].i);
        if (r == !all) {
            result = (int)r;
            break;
        }
    }
    GC_UNROOT(2);
    return result;
}

/* Merges the sorted position runs from[lo, mid) and from[mid, hi) into to[lo, hi), by key; on
 * equal keys the left run first (stable). */
void hof_merge(long lo, long mid, long hi, long *keys, long *from, long *to, int kind)
{
    long i = lo, j = mid;
    for (long k = lo; k < hi; k++) {
        int right; /* right < left? then right, else left */
        if (i >= mid)
            right = 1;
        else if (j >= hi)
            right = 0;
        else
            right = elem_less(keys[from[j]], keys[from[i]], kind);
        to[k] = right ? from[j++] : from[i++];
    }
}

/* Stable, in place: each key is computed once, then a merge sort of the positions by key. The
 * elements are taken from a snapshot made before any key is computed; a key function that changes
 * the list (its length or its buffer) is R0007 (spec 013, C4: it made sort_by read and write past
 * the list's buffer). */
void hof_sort_by(List *list, void *fn, Node *call)
{
    /* the key's type: the function argument's return type */
    Type *key_type = rt_concrete(((Node *)call->b.v->data[1].p)->type->b.t);
    int kind = (int)key_type->kind;
    long n = list->len;
    W *before = list->data;
    GC_ROOT(&list);
    GC_ROOT(&fn);
    long *block = alloc(n * 32); /* keys, positions, scratch and the snapshot: one root (deep re-entry) */
    GC_ROOT(&block);
    long *keys = block, *pos = block + n, *scratch = block + 2 * n, *elems = block + 3 * n;
    for (long i = 0; i < n; i++) elems[i] = list->data[i].i;
    for (long i = 0; i < n; i++) {
        pos[i] = i;
        keys[i] = call_value(fn, call, &elems[i]);
    }
    if (list->len != n || list->data != before)
        rt_raise_c("R0007", "The list changed while `sort_by` computed its keys: do not change it in the key function.",
                   call);
    elem_record_type = key_type; /* spec 014: record keys field by field; set after the key function ran (it may sort) */
    for (long width = 1; width < n; width *= 2) {
        for (long left = 0; left < n;) {
            long mid = left + width < n ? left + width : n;
            long end = left + 2 * width < n ? left + 2 * width : n;
            hof_merge(left, mid, end, keys, pos, scratch, kind); /* [left, mid) and [mid, end) */
            left = end;
        }
        long *t = pos; /* the merged run is in scratch now */
        pos = scratch;
        scratch = t;
    }
    W *data = list->data; /* permute the elements (the same buffer, n long: checked above) */
    for (long i = 0; i < n; i++) data[i].i = elems[pos[i]];
    GC_UNROOT(3);
}
