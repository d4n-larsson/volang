/* Runtime maps: open addressing (linear probing) over an insertion-ordered entries array
 * (ported from src/rt_map.s, spec 008).
 * header | count | cap | index (u32[cap*2]: 0 empty, 0xFFFFFFFF removed, else entry+1) | entries | used
 * Hashing is FNV-1a with a fixed seed, so iteration order and behavior are deterministic (FR-009). */
#include "vo.h"

#define TOMB 0xFFFFFFFFu

Map *map_new(void)
{
    Map *m = alloc_obj(OBJ_MAP, MAP_SIZE);
    GC_ROOT(&m);
    map_alloc(m, 8);
    GC_UNROOT(1);
    return m;
}

/* Fresh, empty index and entries for cap entries. */
void map_alloc(Map *map, long cap)
{
    map->cap = cap;
    map->count = 0;
    map->used = 0;
    GC_ROOT(&map);
    map->index = alloc(cap * 8); /* u32[cap*2] */
    map->entries = alloc(cap * ME_SIZE);
    GC_UNROOT(1);
}

long map_hash(long key, int key_kind)
{
    unsigned long h = 0xcbf29ce484222325UL; /* FNV-1a 64 offset basis */
    const unsigned long prime = 0x100000001b3UL;
    if (key_kind == TY_STR) {
        Str *s = (Str *)key;
        for (long i = 0; i < s->len; i++) h = (h ^ (unsigned char)s->data[i]) * prime;
    } else { /* int: its 8 bytes */
        unsigned long k = (unsigned long)key;
        for (int i = 0; i < 8; i++, k >>= 8) h = (h ^ (k & 0xff)) * prime;
    }
    return (long)((h ^ (h >> 32)) & 0xffffffffUL); /* keep 32 bits (stored as u32) */
}

/* Spec 033: FNV-1a over a key's structure (research R4): ints 8 bytes low first, text and bytes their
 * length then their bytes, records field by field, variants their tag then their value. Equal keys
 * (values_equal) hash equally; nothing depends on memory layout, so every platform agrees. */
static unsigned long fnv_word(unsigned long h, unsigned long k)
{
    for (int i = 0; i < 8; i++, k >>= 8) h = (h ^ (k & 0xff)) * 0x100000001b3UL;
    return h;
}

static unsigned long fnv_key(unsigned long h, long key, Type *type)
{
    check_stack((Node *)cur_node); /* deep keys are R0005, as for == */
    type = rt_concrete(type);
    switch (type->kind) {
    case TY_STR:
    case TY_BYTES: {
        Str *s = (Str *)key;
        h = fnv_word(h, (unsigned long)s->len);
        for (long i = 0; i < s->len; i++) h = (h ^ (unsigned char)s->data[i]) * 0x100000001b3UL;
        return h;
    }
    case TY_RECORD: {
        Vec *fields = type->decl.n->b.v;
        for (long i = 0;; i++) {
            Node *m = member_at(fields, i);
            if (!m) return h;
            h = fnv_key(h, ((Record *)key)->fields[i].i, field_type_in(type, m->b.n->type));
        }
    }
    case TY_VARIANT: {
        long tag = (long)(uint32_t)((Variant *)key)->tag;
        h = fnv_word(h, (unsigned long)tag);
        Node *pt = member_at(type->decl.n->b.v, tag)->b.n; /* payload type node */
        return pt ? fnv_key(h, ((Variant *)key)->pay.i, field_type_in(type, pt->type)) : h;
    }
    default: return fnv_word(h, (unsigned long)key); /* int, bool */
    }
}

/* The 32-bit hash of a key of type t: int, bool and str as map_hash, others structurally. */
static long key_hash(long key, Type *t)
{
    if ((unsigned long)t->kind > TY_VOID) t = rt_concrete(t); /* type parameters: the frame's arguments */
    if ((unsigned long)t->kind <= TY_VOID) return map_hash(key, (int)t->kind); /* int, bool, str */
    unsigned long h = fnv_key(0xcbf29ce484222325UL, key, t);
    return (long)((h ^ (h >> 32)) & 0xffffffffUL);
}

/* map_find for keys that are not int, bool or str: hashed and compared structurally (out of line, so
 * the int and str loop below stays as it was). */
__attribute__((noinline)) static long map_find_structural(Map *map, long key, Type *key_type)
{
    key_type = rt_concrete(key_type); /* type parameters: the running frame's arguments */
    if ((unsigned long)key_type->kind <= TY_VOID) return map_find(map, key, key_type); /* a K bound to int or str */
    unsigned long hash = (unsigned long)key_hash(key, key_type);
    unsigned long mask = (unsigned long)map->cap * 2 - 1;
    for (unsigned long at = hash & mask;; at = (at + 1) & mask) {
        uint32_t slot = map->index[at];
        if (!slot) return -1;
        if (slot == TOMB) continue;
        long e = (long)slot - 1;
        MapEntry *me = &map->entries[e];
        if (me->hash != hash) continue;
        if (values_equal(me->key.i, key, key_type)) return e;
    }
}

/* The entry index of the key, or -1. */
long map_find(Map *map, long key, Type *key_type)
{
    long kind = key_type->kind;
    if ((unsigned long)kind > TY_VOID) return map_find_structural(map, key, key_type); /* spec 033 */
    unsigned long hash = (unsigned long)map_hash(key, (int)kind);
    unsigned long mask = (unsigned long)map->cap * 2 - 1; /* index mask */
    for (unsigned long at = hash & mask;; at = (at + 1) & mask) {
        uint32_t slot = map->index[at];
        if (!slot) return -1;
        if (slot == TOMB) continue;
        long e = (long)slot - 1;
        MapEntry *me = &map->entries[e];
        if (me->hash != hash) continue;
        if (kind == TY_STR ? str_eq(me->key.s, (Str *)key) : me->key.i == key) return e;
    }
}

/* Puts entry+1 into the first free index slot of its probe sequence. */
void map_index_insert(Map *map, long entry, long hash)
{
    unsigned long mask = (unsigned long)map->cap * 2 - 1;
    unsigned long at = (unsigned long)hash & mask;
    while (map->index[at] && map->index[at] != TOMB) at = (at + 1) & mask;
    map->index[at] = (uint32_t)(entry + 1);
}

/* Rebuilds with room for twice the live entries, dropping removed ones (keeps insertion order). The
 * capacity follows the live count, so a small map with heavy churn stays small (spec 003). */
void map_grow(Map *map, Type *key_type)
{
    (void)key_type;
    MapEntry *old = map->entries; /* old entries */
    long used = map->used;
    long cap = 8; /* the smallest power of two, at least 8, that holds twice the live count */
    while (cap < map->count * 2) cap *= 2;
    GC_ROOT(&map);
    GC_ROOT(&old);
    map_alloc(map, cap);
    for (long i = 0;; i++) {
        POLL(1);
        if (i >= used) break;
        MapEntry *o = &old[i];
        if (!o->live) continue;
        long e = map->used; /* append */
        MapEntry *me = &map->entries[e];
        me->key = o->key;
        me->val = o->val;
        me->hash = o->hash;
        me->live = 1;
        map->used = e + 1;
        map->count++;
        map_index_insert(map, e, o->hash);
    }
    GC_UNROOT(2);
}

/* ponytail: a record changed while it is a key keeps its old hash and is not detected (spec 033 R7,
 * documented as for ordered_map); copying keys on insert would detect it at one allocation per insert. */
void map_set(Map *map, long key, long value, Type *key_type)
{
#ifdef GC_STRESS_BUILD
    gc_check(map, OBJ_MAP); /* stress build: a live OBJ_MAP (spec 003 FR-011) */
#endif
    long e = map_find(map, key, key_type);
    if (e >= 0) { /* replace the value, keep the position */
        map->entries[e].val.i = value;
        return;
    }
    if (map->used >= map->cap) {
        GC_ROOT(&map);
        GC_ROOT(&key);
        GC_ROOT(&value);
        map_grow(map, key_type);
        GC_UNROOT(3);
    }
    long hash = key_hash(key, key_type); /* (spec 033: structural for keys other than int, bool, str) */
    e = map->used;
    MapEntry *me = &map->entries[e];
    me->key.i = key;
    me->val.i = value;
    me->hash = (uint32_t)hash;
    me->live = 1;
    map->used = e + 1;
    map->count++;
    map_index_insert(map, e, hash); /* the new entry's index */
}

/* Does nothing when the key is missing. */
void map_remove(Map *map, long key, Type *key_type)
{
    long e = map_find(map, key, key_type);
    if (e < 0) return;
    MapEntry *me = &map->entries[e];
    me->live = 0;
    me->key.i = 0; /* let the collector reclaim them */
    me->val.i = 0;
    map->count--;
    /* The index slot holding entry+1 is on the probe sequence of its hash (map_find just walked it). */
    unsigned long mask = (unsigned long)map->cap * 2 - 1;
    unsigned long at = me->hash & mask;
    while (map->index[at] != (uint32_t)(e + 1)) at = (at + 1) & mask;
    map->index[at] = TOMB;
}

/* The live keys in insertion order. */
List *map_keys(Map *map)
{
    GC_ROOT(&map);
    List *keys = list_new(map->count);
    GC_ROOT(&keys);
    for (long i = 0; i < map->used; i++)
        if (map->entries[i].live) list_push(keys, map->entries[i].key.i);
    GC_UNROOT(2);
    return keys;
}
