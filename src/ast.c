/* Growable pointer vectors and AST node construction (ported from src/ast.s, spec 008 step 1). */
#include "vo.h"

long cur_file; /* file id stamped on new nodes */

Vec *vec_new(void)
{
    Vec *v = alloc(VEC_SIZE);
    GC_ROOT(&v); /* vecs are used at run time too (property tests, type interning) */
    v->data = alloc(64);
    GC_UNROOT(1);
    v->cap = 8;
    v->n = 0;
    return v;
}

void vec_push(Vec *v, void *item)
{
    if (v->n >= v->cap) {
        long cap = v->cap * 2;
        GC_ROOT(&v);
        GC_ROOT(&item);
        W *data = alloc(cap * 8);
        GC_UNROOT(2);
        mem_copy((char *)data, (char *)v->data, v->n * 8);
        v->data = data;
        v->cap = cap;
    }
    v->data[v->n++].p = item;
}

void *vec_get(Vec *v, long i)
{
    return v->data[i].p;
}

long vec_len(Vec *v)
{
    return v ? v->n : 0;
}

Node *node_new(int kind, long start, long end)
{
    Node *n = alloc(NODE_SIZE);
    n->kind = (uint16_t)kind;
    n->file = (uint32_t)cur_file;
    n->start = (uint32_t)start;
    n->end = (uint32_t)end;
    return n;
}
