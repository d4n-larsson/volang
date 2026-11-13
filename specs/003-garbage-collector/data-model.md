# Data Model: vo-lang Memory Reclamation

Layouts are in bytes. This extends `specs/002-effects-functions-generics/data-model.md`.

## Address space (fixed mappings, research §3)

All bases are below 2^38, so the map also fits Sv39 RV64 hardware, whose user space ends at
`0x3f_ffff_ffff`. (The plan had `0x7c00_…`, which only exists with 47-bit user space.)

| Region | Base (virtual) | Size | Notes |
|---|---|---|---|
| Collected heap | `0x10_0000_0000` | reserved `limit + 64 MiB`, `MAP_NORESERVE` | 64 KiB pages; touched only when used |
| Page table | `0x21_0000_0000` | 8 bytes per heap page, reserved | One entry per page (below) |
| Mark stack | `0x22_0000_0000` | reserved 64 MiB | 8M entries; a full rescan covers overflow |
| Value stack | `0x23_0000_0000` | 32 MiB | Its own region; only `[vbase, vsp)` is scanned |
| Permanent arena | `0x24_0000_0000` | grows in 16 MiB chunks, contiguous | Checker data, interned types, caches; scanned up to its bump pointer |
| Native stack | `0x30_0000_0000` | 64 MiB | Scanned from the collector's `sp` to the top |

Mapping uses `MAP_FIXED_NOREPLACE`. If a region cannot be mapped, the toolchain stops with
`error[T0003]: cannot map memory at 0x…` (exit 125). The out-of-memory report needs no reserve:
it switches the collector off and allocates from the permanent arena.

## Heap pages

```
page table entry (8 bytes per 64 KiB page):
  kind    u8    0 free, 1 small, 2 large head, 3 large tail
  class   u8    size-class index (small pages)
  noscan  u8    1: the page holds strings or bytes, never scanned
  dirty   u8    (unused; heap_hwm tells which pages were never touched)
  run     u32   head: pages in the run; tail: pages back to the head
```

- **Size classes:** 16, 32, 48, 64, 96, 128, 192, 256, 384, 512, 768, 1024, 1536, 2048, 3072, 4096, 6144, 8192, 12288 and 16384 bytes (20 classes). Sizes up to 1 KiB find their class through a 65-entry table. An object needing more than 16384 bytes is large and takes `ceil(size / 64 KiB)` whole pages, first fit.
- **Slots start at the page start;** pages have no header.
- **Free and marked state live in each object's header** (no per-page bitmaps):
  - a free slot has kind `OBJ_FREE` and links to the next free slot through its first payload word;
  - the mark bit is `OBJ_MARK` (bit 31 of the kind word), set only during a collection.
- **Free lists:** one per (class, noscan), rebuilt in ascending address order by every sweep.
- **Finding the object for an address:**
  1. In the used heap? Look up the page's entry.
  2. A small page: `slot = offset / class_size`. It is an object unless its kind is `OBJ_FREE`.
  3. A large tail: go back `run` pages to the head. A large head: the object starts at the page.
- **Zeroing:** a block is zeroed when allocated, except pages at or above `heap_hwm`, which were never used and are still zero.

## Objects

The header is unchanged, so every existing runtime offset stays valid:

```
object: size:u32 | kind:u32 | payload...
```

**One header per block. `gc_alloc(size, flags)` returns the start of the block, and the block's header is the object's header:**
- **Typed objects:** `alloc_obj(kind, payload)` calls `gc_alloc(payload + 8, flags)` directly and writes `size` and `kind` into that header, exactly as it does today. The collector never adds a second header.
- **Header-less buffers:** a plain `alloc(n)` while a program runs (list data, map arrays, `vec` arrays, string-builder storage via `alloc_bytes`) calls `gc_alloc(n + 8, flags)`, writes a header with kind `OBJ_RAW` or `OBJ_BYTES`, and returns the block start + 8. Its callers see plain memory, as before.
- **Boundaries:** the collector finds object boundaries from the page table and slot size, never from the header's `size` field.

- **New kinds:**
  - `OBJ_RAW` (existing, 6): a scanned buffer, such as list data, map index and entries, or `vec` data allocated while a program runs.
  - `OBJ_BYTES` (8): a no-scan buffer, such as string-builder storage.
  - `OBJ_FREE` (15): a reclaimed slot. Its payload is poisoned (`0xdeadbeefdeadbeef` words) in the stress build.
  - `OBJ_MARK` (bit 31 of the kind word): marked, only during a collection.
- **No scan:** `OBJ_STR` and `OBJ_BYTES` are allocated on no-scan pages. Every other kind is scanned word by word from the payload start to `size`.

## Roots (research §4)

| Root | Range |
|---|---|
| `s0`–`s11`, `ra` | spilled by `gc_collect` into its frame |
| Native stack | `gc_collect`'s `sp` up to the stack top |
| Value stack | `vbase` up to `vsp` |
| Globals | `__DATA_BEGIN__` up to `_end` (defined by the default linker script) |
| Permanent arena | `ADDR_PERM` up to its bump pointer (the chunks are contiguous) |

**Stale words and test order.** Conservative roots can keep an object alive through a stale word.
To keep each test's memory report independent of the tests before it (FR-009):
- `gc_start` zeroes the dead native stack below `sp` (until 8 KiB of zeros) and resets the step
  counter, so deadline checks happen at the same points;
- the test runner clears its result globals after each report;
- the allocator saves no argument or temporary registers. Their stale contents would otherwise
  become roots, and this was the actual cause of an order dependence found by
  `tests/gc_bounds.py`.

## Collector state (`gc.s` .bss)

| Field | Meaning |
|---|---|
| `gc_on` | 1 while a program runs (allocation goes to the heap) |
| `gc_perm` | perm-bracket depth (> 0: allocate from the permanent arena) |
| `gc_since` | bytes allocated since the last collection |
| `heap_top` / `heap_hwm` / `page_hint` | 1 + the highest page in use / never-used pages start here / no free page below |
| `gc_trigger` | `min(max(8 MiB, live_after_last), gc_limit − live_after_last)`, never less than one minimum-size allocation |
| `gc_live` | live bytes after the last collection |
| `gc_peak` | the highest `gc_live` seen, plus allocation since that collection (the reported peak) |
| `gc_total` | total bytes allocated |
| `gc_count` | collections so far |
| `gc_limit` | live-data limit in bytes (`--max-memory`, default 512 MiB) |
| `gc_stress` | stress build only: N from `VO_GC_STRESS` (default 64) |

## Run-time memo (research §6)

- **Table:** an open-addressing hash in the permanent arena. Entries are `{key_a, key_b, kind, value}`, and the table grows at 50% load.
- **Kinds:**
  - `TARGS`: `(bindings, caller_targs)` → the interned type-argument vector.
  - `CONCRETE`: `(type, targs)` → the concrete type.
  - `ESUBST`: `(set, targs)` → the effect set.
- **Interned type-argument vectors:** a table of vecs compared element by element (types are already interned), so equal argument lists are one pointer.

## Work budget (research §7)

`rt_budget` (a global counter, reset to 65536 when it runs out) is decremented by long runtime loops. When it reaches 0, `rt_poll` checks the deadline exactly as `tick` does.

## Memory report (FR-007)

- **Values:** `peak_live` (bytes), `allocated` (bytes), `collections` and `limit`, per run or per test.
- **Human form:** `memory: peak live 3.2 MiB, allocated 1.4 GiB, 212 collections (limit 512 MiB)`.
- **JSON:** the `memory` record in contracts/output-schema-v3.md.
