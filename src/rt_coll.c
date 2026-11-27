/* Spec 014: the std.collections types (research R5): OrderedMap and OrderedSet on an AVL tree,
 * PriorityQueue on a binary heap whose ties leave in push order, Deque on a ring buffer. All four are
 * one object kind (Coll, the count at offset 8 so OPC_LEN reads it like a list's length). Tree code is
 * iterative with an explicit path bounded by the AVL height: no native recursion over the data. */
#include "vo.h"

typedef struct CNode {
    W key;
    W val;
    struct CNode *left;
    struct CNode *right;
    long height;
} CNode;

#define PATH_MAX_DEPTH 128 /* the AVL height of 2^63 nodes is about 91 */

typedef union {
    long i;
    double d;
} Bits;

/* ---- order ---- */

static Coll *coll_active[32]; /* collections whose operation is calling a comparison function */
static long coll_nactive;

/* A raise unwinds every operation in progress (rt_raise). */
void coll_reset_active(void)
{
    coll_nactive = 0;
}

/* R0007 when c is being ordered (a comparison function changing its own collection). */
static void coll_check_change(Coll *c)
{
    for (long i = 0; i < coll_nactive; i++)
        if (coll_active[i] == c)
            rt_raise_c("R0007", "The collection changed while it was being ordered (by its comparison function).",
                       (Node *)cur_node);
}

/* -1, 0 or 1: a against b in the built-in order of t (int, float, str, bytes, bool; records field by field;
 * variants by case, then payload). */
long value_cmp(long a, long b, Type *t)
{
    t = rt_concrete(t);
    switch (t->kind) {
    case TY_FLOAT: {
        Bits x = {.i = a}, y = {.i = b};
        return x.d < y.d ? -1 : x.d > y.d ? 1 : 0;
    }
    case TY_STR:
    case TY_BYTES: { /* spec 021 */
        int r = str_cmp((Str *)a, (Str *)b);
        return r < 0 ? -1 : r > 0;
    }
    case TY_RECORD: return record_cmp(a, b, t);
    case TY_VARIANT: { /* spec 056: by case (declaration order), then the payload */
        long ta = (long)(uint32_t)((Variant *)a)->tag, tb = (long)(uint32_t)((Variant *)b)->tag;
        if (ta != tb) return ta < tb ? -1 : 1;
        Node *pt = member_at(t->decl.n->b.v, ta)->b.n; /* payload type node */
        if (!pt) return 0;
        return value_cmp(((Variant *)a)->pay.i, ((Variant *)b)->pay.i, field_type_in(t, pt->type));
    }
    default: return a < b ? -1 : a > b; /* int, bool */
    }
}

/* Records of ordered fields: field by field in declaration order (spec 014, FR-012). */
long record_cmp(long a, long b, Type *t)
{
    POLL(1);
    check_stack((Node *)cur_node); /* nested records */
    Vec *fields = t->decl.n->b.v;
    for (long i = 0;; i++) {
        Node *m = member_at(fields, i);
        if (!m) return 0;
        long r = value_cmp(((Record *)a)->fields[i].i, ((Record *)b)->fields[i].i, field_type_in(t, m->b.n->type));
        if (r) return r;
    }
}

/* The collection's order: its comparison function (a negative, zero or positive int), else the
 * built-in order of its key or element type. */
static long ccmp(Coll *c, long a, long b)
{
    if (!c->cmp) return value_cmp(a, b, c->type->a.t);
    if (coll_nactive >= 32) rt_stack_full((Node *)cur_node); /* every level is guarded, or none run */
    coll_active[coll_nactive++] = c;
    long vals[2] = {a, b};
    long r = call_value(c->cmp, (Node *)cur_node, vals);
    coll_nactive--;
    return r < 0 ? -1 : r > 0;
}

/* ---- the object ---- */

Coll *coll_new(Type *type, Closure *cmp)
{
    GC_ROOT(&type);
    GC_ROOT(&cmp);
    Coll *c = alloc_obj(OBJ_COLL, COLL_SIZE);
    c->count = 0;
    c->kind = type->kind;
    c->type = type;
    c->cmp = cmp;
    c->root = 0;
    c->data = 0;
    c->cap = 0;
    c->head = 0;
    c->seq = 0;
    GC_UNROOT(2);
    return c;
}

/* ---- AVL tree (OrderedMap, OrderedSet) ---- */

static long ht(CNode *n)
{
    return n ? n->height : 0;
}

static void fix_height(CNode *n)
{
    long l = ht(n->left), r = ht(n->right);
    n->height = (l > r ? l : r) + 1;
}

static CNode *rot_right(CNode *n)
{
    CNode *l = n->left;
    n->left = l->right;
    l->right = n;
    fix_height(n);
    fix_height(l);
    return l;
}

static CNode *rot_left(CNode *n)
{
    CNode *r = n->right;
    n->right = r->left;
    r->left = n;
    fix_height(n);
    fix_height(r);
    return r;
}

static CNode *rebalance(CNode *n)
{
    fix_height(n);
    long b = ht(n->left) - ht(n->right);
    if (b > 1) {
        if (ht(n->left->left) < ht(n->left->right)) n->left = rot_left(n->left);
        return rot_right(n);
    }
    if (b < -1) {
        if (ht(n->right->right) < ht(n->right->left)) n->right = rot_right(n->right);
        return rot_left(n);
    }
    return n;
}

#ifdef GC_STRESS_BUILD
/* The stress build checks every structure after every change (T0003 on a violation): AVL heights and
 * balance, the node count, ascending keys (built-in order only: no extra calls of vo code), and the
 * heap order. */
static void coll_bad(char *what)
{
    out_cstr(2, "error[T0003]: internal error: a collection's invariant broke: ");
    out_cstr(2, what);
    out_cstr(2, "\n");
    vo_exit(EXIT_INTERNAL);
}

static long verify_tree(Coll *c, CNode *n, long *count, CNode **prev)
{
    if (!n) return 0;
    long l = verify_tree(c, n->left, count, prev);
    if (*prev && !c->cmp && value_cmp((*prev)->key.i, n->key.i, c->type->a.t) >= 0) coll_bad("keys out of order");
    *prev = n;
    (*count)++;
    long r = verify_tree(c, n->right, count, prev);
    if (n->height != (l > r ? l : r) + 1) coll_bad("a wrong height");
    if (l - r > 1 || r - l > 1) coll_bad("unbalanced");
    return n->height;
}

static void coll_verify(Coll *c)
{
    if (c->kind == TY_OMAP || c->kind == TY_OSET) {
        long count = 0;
        CNode *prev = 0;
        verify_tree(c, c->root, &count, &prev);
        if (count != c->count) coll_bad("a wrong size");
    } else if (c->kind == TY_PQUEUE && !c->cmp) {
        for (long i = 1; i < c->count; i++) {
            long p = (i - 1) / 2, r = value_cmp(c->data[2 * p].i, c->data[2 * i].i, c->type->a.t);
            if (r > 0 || (!r && c->data[2 * p + 1].i > c->data[2 * i + 1].i)) coll_bad("heap order");
        }
    }
}
#else
#define coll_verify(c) ((void)0)
#endif

/* The node holding key, or 0. */
static CNode *tree_find(Coll *c, long key)
{
    CNode *n = c->root;
    while (n) {
        POLL(1); /* --timeout sees work done here (spec 014: a timeout never interrupts a change) */
        long r = ccmp(c, key, n->key.i);
        if (!r) return n;
        n = r < 0 ? n->left : n->right;
    }
    return 0;
}

/* Inserts key (with val), or replaces the value of an equal key. */
static void tree_put(Coll *c, long key, long val)
{
    coll_check_change(c);
    POLL(16); /* before any change */
    CNode **path[PATH_MAX_DEPTH];
    long d = 0;
    CNode **link = (CNode **)&c->root;
    while (*link && d < PATH_MAX_DEPTH) {
        long r = ccmp(c, key, (*link)->key.i);
        if (!r) {
            (*link)->val.i = val;
            return;
        }
        path[d++] = link;
        link = r < 0 ? &(*link)->left : &(*link)->right;
    }
    if (*link) return; /* (an inconsistent comparison function went deeper than any AVL tree) */
    GC_ROOT(&c);
    CNode *n = alloc(sizeof(CNode)); /* key and val are the caller's (rooted) values */
    GC_UNROOT(1);
    n->key.i = key;
    n->val.i = val;
    n->left = n->right = 0;
    n->height = 1;
    *link = n;
    c->count++;
    while (d > 0) {
        CNode **l = path[--d];
        *l = rebalance(*l);
    }
    coll_verify(c);
}

/* Removes key; nothing when it is missing. */
static void tree_remove(Coll *c, long key)
{
    coll_check_change(c);
    POLL(16); /* before any change */
    CNode **path[PATH_MAX_DEPTH];
    long d = 0;
    CNode **link = (CNode **)&c->root;
    while (*link && d < PATH_MAX_DEPTH - 1) {
        long r = ccmp(c, key, (*link)->key.i);
        if (!r) break;
        path[d++] = link;
        link = r < 0 ? &(*link)->left : &(*link)->right;
    }
    if (!*link || d >= PATH_MAX_DEPTH - 1) return;
    CNode *n = *link;
    if (!n->left) {
        *link = n->right;
    } else if (!n->right) {
        *link = n->left;
    } else { /* two children: the successor's key and value move here */
        path[d++] = link;
        CNode **s = &n->right;
        while ((*s)->left && d < PATH_MAX_DEPTH) {
            path[d++] = s;
            s = &(*s)->left;
        }
        CNode *succ = *s;
        n->key = succ->key;
        n->val = succ->val;
        *s = succ->right;
    }
    c->count--;
    while (d > 0) {
        CNode **l = path[--d];
        *l = rebalance(*l);
    }
    coll_verify(c);
}

/* The smallest key at or above x (dir 1), the largest at or below (dir -1); with strict, above or
 * below only; 0 when none. */
static CNode *tree_neighbor(Coll *c, long x, int dir, int strict)
{
    CNode *n = c->root, *best = 0;
    while (n) {
        POLL(1);
        long r = ccmp(c, n->key.i, x);
        if (!r && !strict) return n;
        if (dir > 0 ? r > 0 : r < 0) { /* n is on the wanted side of x: a candidate */
            best = n;
            n = dir > 0 ? n->left : n->right;
        } else {
            n = dir > 0 ? n->right : n->left;
        }
    }
    return best;
}

static CNode *tree_end(Coll *c, int last)
{
    CNode *n = c->root;
    while (n && (last ? n->right : n->left)) n = last ? n->right : n->left;
    return n;
}

/* Keys (which 0) or values (1) in key order, those between lo and hi when bounded. */
static List *tree_list(Coll *c, int which, int bounded, long lo, long hi)
{
    GC_ROOT(&c);
    List *out = list_new(c->count ? c->count : 1);
    GC_ROOT(&out);
    CNode *stack[PATH_MAX_DEPTH];
    long d = 0;
    CNode *n = c->root;
    while (n || d) {
        while (n && d < PATH_MAX_DEPTH) {
            if (bounded && ccmp(c, n->key.i, lo) < 0) { /* everything left of n is below lo */
                n = n->right;
                continue;
            }
            stack[d++] = n;
            n = n->left;
        }
        if (!d) break;
        POLL(1); /* a loop's snapshot costs time like the loop */
        n = stack[--d];
        if (bounded && ccmp(c, n->key.i, hi) > 0) break; /* in order: the rest is above hi */
        list_push(out, which ? n->val.i : n->key.i);
        n = n->right;
    }
    GC_UNROOT(2);
    return out;
}

/* ---- binary heap (PriorityQueue): (item, sequence) pairs; ties by sequence ---- */

static int heap_less(Coll *c, long i, long j)
{
    W *d = c->data;
    long r = ccmp(c, d[2 * i].i, d[2 * j].i);
    if (r) return r < 0;
    return d[2 * i + 1].i < d[2 * j + 1].i;
}

static void heap_swap(Coll *c, long i, long j)
{
    W *d = c->data;
    W a = d[2 * i], s = d[2 * i + 1];
    d[2 * i] = d[2 * j];
    d[2 * i + 1] = d[2 * j + 1];
    d[2 * j] = a;
    d[2 * j + 1] = s;
}

/* Room for one more element (heap: 2 words each; deque: 1, capacity a power of two). */
static void coll_grow(Coll *c, long words)
{
    if (c->count < c->cap) return;
    long cap = c->cap ? c->cap * 2 : 8;
    GC_ROOT(&c);
    W *data = alloc(cap * words * 8);
    GC_UNROOT(1);
    if (c->kind == TY_DEQUE) { /* unwrap the ring */
        for (long i = 0; i < c->count; i++) data[i] = c->data[(c->head + i) & (c->cap - 1)];
        c->head = 0;
    } else if (c->count) {
        mem_copy8((long *)data, (long *)c->data, c->count * words * 8);
    }
    c->data = data;
    c->cap = cap;
}

static void heap_push(Coll *c, long x)
{
    coll_check_change(c);
    POLL(16); /* before any change */
    GC_ROOT(&x);
    coll_grow(c, 2);
    GC_UNROOT(1);
    long i = c->count++;
    c->data[2 * i].i = x;
    c->data[2 * i + 1].i = c->seq++;
    while (i > 0 && heap_less(c, i, (i - 1) / 2)) {
        heap_swap(c, i, (i - 1) / 2);
        i = (i - 1) / 2;
    }
    coll_verify(c);
}

/* Removes and returns the smallest item (count > 0). */
static long heap_pop(Coll *c)
{
    coll_check_change(c);
    POLL(16); /* before any change */
    long top = c->data[0].i;
    long n = --c->count;
    c->data[0] = c->data[2 * n];
    c->data[1] = c->data[2 * n + 1];
    for (long i = 0;;) {
        long l = 2 * i + 1, r = l + 1, m = i;
        if (l < n && heap_less(c, l, m)) m = l;
        if (r < n && heap_less(c, r, m)) m = r;
        if (m == i) break;
        heap_swap(c, i, m);
        i = m;
    }
    coll_verify(c);
    return top;
}

/* ---- snapshots, copies ---- */

/* An independent collection with the same order and contents. */
Coll *coll_copy(Coll *c)
{
    GC_ROOT(&c);
    Coll *d = coll_new(c->type, c->cmp);
    GC_ROOT(&d);
    if (c->kind == TY_OMAP || c->kind == TY_OSET) {
        List *ks = tree_list(c, 0, 0, 0, 0);
        GC_ROOT(&ks);
        List *vs = tree_list(c, 1, 0, 0, 0);
        GC_ROOT(&vs);
        for (long i = 0; i < ks->len; i++) tree_put(d, ks->data[i].i, vs->data[i].i);
        GC_UNROOT(2);
    } else if (c->count) {
        long words = c->kind == TY_PQUEUE ? 2 : 1;
        W *data = alloc(c->cap * words * 8);
        mem_copy8((long *)data, (long *)c->data, c->cap * words * 8);
        d->data = data;
        d->cap = c->cap;
        d->head = c->head;
        d->count = c->count;
        d->seq = c->seq;
    }
    GC_UNROOT(2);
    return d;
}

/* The elements in iteration order as a list: keys (map), elements in order (set), pop order
 * (queue; heap order when reporting, so no comparison function runs inside a report), front to back
 * (deque). Loops, rendering and equality work on this copy, never on the collection itself. */
List *coll_snapshot(Coll *c)
{
    if (c->kind == TY_OMAP || c->kind == TY_OSET) return tree_list(c, 0, 0, 0, 0);
    GC_ROOT(&c);
    List *out = list_new(c->count ? c->count : 1);
    GC_ROOT(&out);
    if (c->kind == TY_DEQUE) {
        POLL(c->count);
        for (long i = 0; i < c->count; i++) list_push(out, c->data[(c->head + i) & (c->cap - 1)].i);
    } else if (rt_reporting && c->cmp) {
        for (long i = 0; i < c->count; i++) list_push(out, c->data[2 * i].i);
    } else {
        Coll *t = coll_copy(c);
        GC_ROOT(&t);
        while (t->count) list_push(out, heap_pop(t));
        GC_UNROOT(1);
    }
    GC_UNROOT(2);
    return out;
}

/* The values of a map, in key order. */
List *coll_values(Coll *c)
{
    return tree_list(c, 1, 0, 0, 0);
}

/* ---- the builtins on collections (rt_call.c, vm.c) ---- */

long coll_has(Coll *c, long key)
{
    return tree_find(c, key) != 0;
}

void coll_remove(Coll *c, long key)
{
    tree_remove(c, key);
}

long coll_get_or(Coll *c, long key, long dflt)
{
    CNode *n = tree_find(c, key);
    return n ? n->val.i : dflt;
}

void coll_push(Coll *c, long x)
{
    if (c->kind == TY_OSET) {
        tree_put(c, x, 0);
    } else if (c->kind == TY_PQUEUE) {
        heap_push(c, x);
    } else { /* deque: at the back */
        GC_ROOT(&x);
        coll_grow(c, 1);
        GC_UNROOT(1);
        c->data[(c->head + c->count) & (c->cap - 1)].i = x;
        c->count++;
    }
}

/* pop: the smallest (queue) or the back (deque); err("empty"). */
Variant *coll_pop(Coll *c)
{
    if (!c->count) return make_err("empty");
    if (c->kind == TY_PQUEUE) return make_result(0, heap_pop(c));
    c->count--;
    return make_result(0, c->data[(c->head + c->count) & (c->cap - 1)].i);
}

/* c[k]: a map's value (R0004 when missing) or a deque's element by position (R0003). */
long coll_index(Coll *c, long k, Node *node)
{
    if (c->kind == TY_DEQUE) {
        if (k < 0 || k >= c->count) rt_index_error(node, k, c->count);
        return c->data[(c->head + k) & (c->cap - 1)].i;
    }
    CNode *n = tree_find(c, k);
    if (!n) rt_key_error(node, k, c->type->a.t);
    return n->val.i;
}

void coll_set_index(Coll *c, long k, long v, Node *node)
{
    if (c->kind == TY_DEQUE) {
        if (k < 0 || k >= c->count) rt_index_error(node, k, c->count);
        c->data[(c->head + k) & (c->cap - 1)].i = v;
        return;
    }
    tree_put(c, k, v);
}

/* ---- the library operations: __coll(OP, a, b, c) from std/collections.vo ---- */

enum {
    CO_NEW_BY = 2, /* (cmp): an empty collection of the expected type ordered by cmp (`new` creates the others) */
    CO_FIRST,   /* (c) -> result */
    CO_LAST,
    CO_AT_OR_ABOVE, /* (c, x) -> result */
    CO_AT_OR_BELOW,
    CO_BETWEEN, /* (c, lo, hi) -> list */
    CO_UNION,   /* (a, b) -> set */
    CO_INTERSECTION,
    CO_DIFFERENCE,
    CO_PEEK,       /* (q) -> result */
    CO_PUSH_FRONT, /* (d, x) */
    CO_POP_FRONT,  /* (d) -> result */
    CO_FRONT,
    CO_BACK,
    CO_COPY,
    CO_AFTER,  /* (c, x) -> result: the smallest key strictly above x */
    CO_BEFORE, /* the largest strictly below */
};

static Variant *found(CNode *n, char *none)
{
    return n ? make_result(0, n->key.i) : make_err(none);
}

long coll_op(Node *call, long a0, long a1, long a2)
{
    long op = ((Node *)call->b.v->data[0].p)->a.i;
    Coll *c = (Coll *)a0;
    switch (op) {
    case CO_NEW_BY: return (long)coll_new(rt_concrete(call->type), (Closure *)a0);
    case CO_FIRST:
    case CO_LAST: return (long)(c->count ? found(tree_end(c, op == CO_LAST), "empty") : make_err("empty"));
    case CO_AT_OR_ABOVE: return (long)found(tree_neighbor(c, a1, 1, 0), "none");
    case CO_AT_OR_BELOW: return (long)found(tree_neighbor(c, a1, -1, 0), "none");
    case CO_AFTER: return (long)found(tree_neighbor(c, a1, 1, 1), "none");
    case CO_BEFORE: return (long)found(tree_neighbor(c, a1, -1, 1), "none");
    case CO_BETWEEN: return (long)tree_list(c, 0, 1, a1, a2);
    case CO_UNION:
    case CO_INTERSECTION:
    case CO_DIFFERENCE: {
        Coll *b = (Coll *)a1;
        Coll *out = coll_new(c->type, c->cmp);
        GC_ROOT(&out);
        List *xs = tree_list(c, 0, 0, 0, 0);
        GC_ROOT(&xs);
        for (long i = 0; i < xs->len; i++) {
            int in_b = op != CO_UNION && tree_find(b, xs->data[i].i);
            if (op == CO_UNION || (op == CO_INTERSECTION) == in_b) tree_put(out, xs->data[i].i, 0);
        }
        if (op == CO_UNION) {
            List *ys = tree_list(b, 0, 0, 0, 0);
            GC_ROOT(&ys);
            for (long i = 0; i < ys->len; i++) tree_put(out, ys->data[i].i, 0);
            GC_UNROOT(1);
        }
        GC_UNROOT(2);
        return (long)out;
    }
    case CO_PEEK: {
        if (!c->count) return (long)make_err("empty");
        return (long)make_result(0, c->data[0].i);
    }
    case CO_PUSH_FRONT:
        GC_ROOT(&c);
        GC_ROOT(&a1);
        coll_grow(c, 1);
        GC_UNROOT(2);
        c->head = (c->head - 1) & (c->cap - 1);
        c->data[c->head].i = a1;
        c->count++;
        return 0;
    case CO_POP_FRONT: {
        if (!c->count) return (long)make_err("empty");
        long x = c->data[c->head].i;
        c->head = (c->head + 1) & (c->cap - 1);
        c->count--;
        return (long)make_result(0, x);
    }
    case CO_FRONT:
    case CO_BACK: {
        if (!c->count) return (long)make_err("empty");
        long i = op == CO_FRONT ? 0 : c->count - 1;
        return (long)make_result(0, c->data[(c->head + i) & (c->cap - 1)].i);
    }
    case CO_COPY: return (long)coll_copy(c);
    default: return 0;
    }
}

/* ---- equality (values_equal) ---- */

/* Same contents in iteration order (and, for maps, equal values); comparison functions are not
 * compared. t is the collection type. */
int coll_equal(Coll *a, Coll *b, Type *t)
{
    if (a == b) return 1;
    if (a->count != b->count) return 0;
    GC_ROOT(&a);
    GC_ROOT(&b);
    List *xa = coll_snapshot(a);
    GC_ROOT(&xa);
    List *xb = coll_snapshot(b);
    GC_ROOT(&xb);
    int eq = xa->len == xb->len;
    for (long i = 0; eq && i < xa->len; i++) eq = values_equal(xa->data[i].i, xb->data[i].i, t->a.t);
    if (eq && t->kind == TY_OMAP) {
        List *va = coll_values(a);
        GC_ROOT(&va);
        List *vb = coll_values(b);
        for (long i = 0; eq && i < va->len; i++) eq = values_equal(va->data[i].i, vb->data[i].i, t->b.t);
        GC_UNROOT(1);
    }
    GC_UNROOT(4);
    return eq;
}
