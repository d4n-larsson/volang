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

/* The entry index of the key, or -1. */
long map_find(Map *map, long key, Type *key_type)
{
    long kind = key_type->kind;
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
    long hash = map_hash(key, (int)key_type->kind);
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
