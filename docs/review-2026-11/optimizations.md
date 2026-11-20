# Optimizing what exists

Measured with the native x86-64 `build/vo` (built 2026-11-20) with `--timeout 0`, against Python 3.12.3
and Node, one run each on a desktop machine. Treat the figures as ratios, not as a benchmark suite.

## Measurements

| Program | vo | Python | Node | Notes |
|---|---|---|---|---|
| `fib(32)`, recursive | 0.10 s | 0.20 s | | |
| `fib(38)`, recursive | 1.66 s | 3.64 s | 0.44 s | |
| 20 million float additions in a `while` loop | 0.11 s | 1.79 s | | |
| 1M map inserts and lookups, 300k string-key counts, 1M records, sort 1M ints, `map` and `fold` | 0.34 s, 114 MB | 1.17 s, 310 MB | | |
| Build and walk binary trees (262k long-lived nodes, 40 short-lived trees) | 0.12 s, 52 MB | 0.18 s, 27 MB | | |
| 10M calls of a closure | 0.13 s | | | |
| 1M `str(i)` pushed and joined | 0.06 s | 0.11 s | | |
| 200,000 × `set s = s + "x";` | **4.94 s**, 24 GiB allocated, 3,116 collections | 0.73 s at module level, 0.006 s in a function | | finding 2 |
| 200,000 × `remove(m, i)` | **about 14 s** | 0.02 s | | finding 1 |
| Build, parse, write, replace and split 3.5 MB of JSON | **1.53 s, 358 MB** | 0.15 s, 59 MB | | finding 4 |
| The same with 13 MB of JSON | **R0006** at the 512 MiB limit | 0.53 s, 204 MB | | finding 4 |
| `vo check`, 1,000 records and functions (13k lines) | 0.05 s | | | |
| `vo check`, 2,000 (26k lines) | 0.20 s | | | finding 3 |
| `vo check`, 8,000 (104k lines) | **6.84 s** | | | finding 3 |
| `vo fmt --check`, 52k lines | 0.03 s | | | |
| Start-up, an empty `main` | 2.5 ms | 14 ms | | 100 runs each |

The VM, the collector, the hash map's insert and lookup, sorting and start-up are all in good shape. The
problems are four cliffs and some memory density.

## Findings, in order

### 1. `remove` on a map is linear in the map's capacity

`src/rt_map.c:139`: after finding the entry, `map_remove` scans the whole index for the slot that points
at it. The `ponytail:` note there already names the fix: probe from `me->hash`, which the entry stores.

- Measured: 200,000 inserts then 200,000 removes take about 14 s.
- Any cache, work queue or "seen" set kept in a `map` hits this.
- Fix: a few lines. Add a golden case with a time bound next to `tests/perf.sh`.
- While there, check that tombstones are dropped when the index is rebuilt, so a map that is filled and
  emptied over and over does not grow. (Not verified in this review.)

### 2. Building a string with `+` in a loop is quadratic

Each `s + x` allocates a new string and copies `s`.

- Measured: 200,000 single-character appends take 4.94 s, allocate 24 GiB in total and run 3,116
  collections. CPython takes 0.73 s at module level and 6 ms inside a function, where it appends in place.
- `join` is fast (a million parts in 0.06 s), but agents and people write the loop first.
- `format` has the same shape inside: `src/compile.c:642` notes one concatenation per piece.

Options, cheapest first:

1. **A warning with a fix.** W07xx: "`set s = s + ...` in a loop builds the string again each time;
   push the parts on a list and `join`". This fits vo: the checker teaches the one right way. It does not
   help `+` chains outside the simple pattern.
2. **`format` and `+` chains in one allocation.** Compile `a + b + c + d` and a `format` call to one
   instruction that measures all pieces, allocates once and copies once. Removes the quadratic term inside
   one expression.
3. **Lazy concatenation.** `+` on strings above some length makes a small node (left, right, length)
   and the bytes are produced once, on the first read that needs them. The loop becomes linear, strings
   stay immutable, and nothing in the language changes. It costs a kind check wherever the runtime reads
   string bytes, so it is the largest change of the three.

Do 1 and 2 now. Do 3 only if the benchmark runs show agents still writing the loop.

### 3. Checking is quadratic in the number of top-level items

`src/check.c:196`: `scope_find` walks one linked list of every symbol in scope, innermost first, so a
reference to a top-level function costs a walk over all top-level items.

- Measured: 13k lines 0.05 s, 26k lines 0.20 s, 104k lines 6.84 s. Time quadruples when size doubles.
- Functions alone show it (8,000 one-line functions: 0.72 s; 4,000: 0.09 s), so name lookup is the main
  term. The full program is slower than the sum of its parts, so there is at least one more quadratic
  pass: the next suspects are W0704 (repeated statements) and record lookup. Profile after the first fix.
- It matters beyond big programs: `vo edit` checks twice, `vo test --mutate` recompiles per mutant, and a
  language server would check on every keystroke.

Fix: keep the linked list for locals (scopes are small and it gives shadowing checks for free) and put
file-level names in a hash table keyed by name. The runtime already has a string hash for `map`.

### 4. Text is processed through `chars()`, at 40 bytes per character

A string cannot be indexed, so every parser written in vo starts with `chars(s)`: a list of
one-character strings. `std/json.vo` does this, and so will every agent-written tokenizer.

- Measured: `chars` of a 3 MB string holds 121 MiB live (an 8-byte list slot plus a heap string per
  character).
- Measured: parsing 3.5 MB of JSON peaks at 348 MiB and is about 10× slower than Python end to end;
  13 MB stops with R0006 at the default limit. A JSON API response of that size is ordinary.

Fixes:

1. **Intern one-byte strings.** Keep 128 permanent `Str` objects for ASCII and return them from `chars`,
   `slice` of length 1 and `chr`. No allocation for ASCII text, no language change, and `==` on them can
   compare pointers first. Brings the list to 8 bytes per character.
2. **`byte_at(s, i) -> int`** as a builtin (and `code_at` if code points are wanted). Parsers then walk
   the string in place with integers and never build the list. One new builtin; `ord(slice(s, i, i + 1))`
   already expresses it, slowly.
3. **Native `parse_json` and `write_json`.** The note at the top of `std/json.vo` already plans a
   byte-level primitive. JSON is the main data format of the programs vo targets (`std.web`,
   `from_json`), so it should cost what it costs in Python: C speed.

### 5. Values that need no allocation are allocated

- Measured: a list of a million `T.leaf` (a case with no value) holds 38.6 MiB: each is its own heap
  object of about 24 to 32 bytes. A million `result[int]` values cost about the same.
- Fix for cases without a value: one permanent object per case, or an immediate. The compiler knows
  every variant's layout, so `match` needs no change beyond reading the tag from that object.
- `result[T]` is on every fallible call. `ok(v)` is allocated to be matched and dropped in the next
  instruction. A fused "call, then branch on ok" path for `try f(...)` and `match f(...)`, which returns
  the tag and the value in two slots, removes the allocation for the common case. Measure first: the
  allocator is fast and the trees benchmark is already ahead of Python.

### 6. The collector does more work than the types require

From the code (`src/gc.c:15`, roadmap 003): object contents are scanned conservatively, and the whole
permanent arena is scanned at every collection.

- vo is statically typed and has no `any`, so the compiler knows which words of every record, variant,
  list and frame are pointers. Precise layouts per kind (a pointer bitmap per record type, "elements are
  pointers" per list) remove false retention and let lists of `int` and `float` be skipped entirely.
- A remembered set for the permanent arena removes the fixed cost per collection. That fixed cost is what
  makes finding 2 so bad: 3,116 collections each rescan it.
- A generational or bump-allocated nursery is the next step after precise layouts, and not before:
  it needs them. The current collector is good enough for agent-sized programs, so this is third in line
  behind the cliffs.

### 7. The VM: where the next 2× is

The dispatch loop is direct-threaded with fused compare-and-branch, constant operands and a fast call
path. It beats CPython everywhere measured. Against a JIT it is 4× behind on calls.

Cheap steps, in order:

1. **16-byte instructions.** `Instr` is 32 bytes (`vm.c:14` notes it): handler, two operands, a word and
   the source node. Moving the node to a side table indexed by instruction number halves the code's
   cache footprint and costs nothing on the hot path, since the node is read only for errors, ticks and
   builtins.
2. **Frame set-up.** `vm_push` zeroes every slot and runs the effect, generic and closure logic on each
   call. The fast path for plain calls exists; extend it to closures without type arguments (`map`,
   `filter`, `sort_by` callbacks all go through the slow path).
3. **Builtins on the general path.** Roadmap 004 says builtin-heavy programs gained least. Give the
   most used ones (`get_or`, `slice`, `str`, `find`, map index and assign with a string key) their own
   instructions, as `len`, `push` and `has` have.
4. **Superinstructions from profiles.** Field read then arithmetic, list index then compare, and
   `for` over a list with a field read are the usual wins.

Native code is "out of scope for now" in the roadmap, and that is right while the cliffs above exist.
When it comes up, note that vo's bytecode is typed and has no dynamic dispatch, which makes it an
unusually easy source for ahead-of-time compilation. The cheapest route is a template (copy-and-patch)
compiler over the existing handlers, which keeps one static binary and the three architectures.
Determinism survives as long as step counting stays.

### 8. Iteration copies

From the reference: "a `for` loop over a collection copies it first (all of it, even with `break`)", and
a map is iterated through `keys(m)`, which builds a list.

- Fix: iterate in place and stop with a runtime error if the collection changes during the loop (a
  version counter in the header). That is also a better answer for agents than silently iterating over
  a stale copy.
- Add `for k, v in m` (see [language.md](language.md#7-iteration)), which removes the second lookup per
  entry as well.

### 9. The server

From the code, not measured: `Connection: close` on every response (`serve.c:5`, `std/http.vo:4`), IPv4
only, 64 connections, one request per client connection, and a 16 MiB response cap (`net.c:20`).

- Keep-alive is the first thing a load test will show, on both the server and the client: every request
  pays a TCP handshake, and an HTTPS client request pays a full TLS handshake.
- Load-test `serve_workers` with a standard tool and put the numbers in `specs/028-*/results.md` next
  to a Go or Node baseline. A language that ships a web package will be judged on that table.

### 10. Float text is not round-trip

`src/num.c:222`: floats print with 15 significant digits, not the shortest text that reads back as the
same value.

- Measured: `str(0.1 + 0.2)` is `"0.3"` while `0.1 + 0.2 == 0.3` is `false`, and
  `parse_float(str(x)) == x` is `false` for that `x`.
- This is a correctness problem more than a speed one: a failing `expect_eq` on floats can show two
  identical texts, and data written as text does not read back.
- Fix: the Ryu port the note plans (shortest round-trip, also faster). Float literals are limited to 15
  digits for the same reason and can then take 17.

## What not to touch

- Start-up time and the single static binary. Both are better than every mainstream runtime.
- Fork-based tasks. They are slow to start compared with threads, but they are why the VM and collector
  stay single-threaded and deterministic. Parallel computation needs a different design
  ([language.md](language.md#8-concurrency-beyond-concurrent_map)), not a faster fork.
- The formatter and parser: 52k lines in 30 ms.
