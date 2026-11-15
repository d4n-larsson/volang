/* The builtins that take functions (contracts/grammar-v2.md "New builtins"; ported from
 * src/rt_func.s, spec 008). Each calls its function value through call_value; the list is read
 * afresh on every step, so a callback that changes the list cannot make the loop read past its end.
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

List *hof_filter(List *list, void *fn, Node *call)
{
    GC_ROOT(&list);
    GC_ROOT(&fn);
    List *out = list_new(0);
    GC_ROOT(&out);
    for (long i = 0; i < list->len; i++)
        if (call_value(fn, call, &list->data[i].i)) list_push(out, list->data[i].i);
    GC_UNROOT(3);
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

/* Stable, in place: each key is computed once, then a merge sort of the positions by key. */
void hof_sort_by(List *list, void *fn, Node *call)
{
    /* the key's type: the function argument's return type */
    int kind = (int)((Node *)call->b.v->data[1].p)->type->b.t->kind;
    long n = list->len;
    GC_ROOT(&list);
    GC_ROOT(&fn);
    long *keys = alloc(n * 8);
    GC_ROOT(&keys);
    long *pos = alloc(n * 8); /* positions */
    GC_ROOT(&pos);
    long *scratch = alloc(n * 8);
    GC_ROOT(&scratch);
    for (long i = 0; i < n; i++) {
        pos[i] = i;
        keys[i] = call_value(fn, call, &list->data[i].i);
    }
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
    scratch = alloc(n * 8); /* permute the elements */
    W *data = list->data;
    for (long i = 0; i < n; i++) scratch[i] = data[pos[i]].i;
    for (long i = 0; i < n; i++) data[i].i = scratch[i];
    GC_UNROOT(5);
}
