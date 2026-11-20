# Language features

What the language lacks compared with Go, Rust, TypeScript, Python and Kotlin, ordered by how many real
programs each gap forces into a workaround. Syntax sketches are suggestions in vo's style, not designs.

Two rules were applied to every item:

- **It must remove a workaround, not add a second spelling.** vo's value is that an agent can hold the
  whole language. The reference is about 10,700 words against a 15,000-token budget, so every feature
  also has to earn its paragraph.
- **It must keep effects, determinism and the diagnostics-with-fixes loop intact.**

## Summary

| # | Feature | Today's workaround | Conflicts with a stated decision? |
|---|---|---|---|
| 1 | Typed errors | error kinds as message prefixes | no |
| 2 | Constants | functions that return a literal | no |
| 3 | Tuples and structural map keys | one-off records; string keys built by hand | no (on the roadmap) |
| 4 | Reading strings in place; raw strings | `chars(s)`; `\\d`, `\"` | no |
| 5 | Bitwise operations, hex literals | none: hashing and binary formats cannot be written | no |
| 6 | Exact numbers | integer cents by hand | no |
| 7 | Iteration | `keys(m)` then `m[k]`; manual index counters | no |
| 8 | Concurrency beyond `concurrent_map` | none | partly ("not done on purpose" in 016) |
| 9 | Streaming | whole files and bodies in memory | no |
| 10 | Variant cases with fields; `match` on values | a record per case; `if`/`else if` chains | mild (one way) |
| 11 | Interfaces | records of functions; built-in constraints | **yes** (out of scope in the roadmap) |
| 12 | Imports | one line per name | mild (one way) |
| 13 | Contracts | no `old()`, run-time only | no (on the roadmap) |

## 1. Typed errors

`result[T]` always carries a `str`. That was right for small programs and is the largest thing in the
way of large ones:

- A caller cannot tell "not found" from "permission denied" without comparing message text.
- `std.web` already works around it: `err("not_found: detail")` answers 404, by prefix. That is a
  stringly-typed protocol inside a language whose point is that the checker knows everything.
- `match` exhaustiveness, vo's best feature for agents, stops at the error.

Sketch:

```text
variant LoadError {
    missing(str);
    malformed(str);
}

fn load(path: str) -> result[Config, LoadError] uses file.read("conf/") { ... }
```

- `result[T]` stays and means `result[T, str]`, so no existing program changes.
- `try` passes the error on when the function's error type is the same. To convert, one explicit form:
  `try e else f` is tempting but is a second way; a plain builtin `map_err(r, f)` is enough.
- `main` and tests keep `result[void]`; a typed error reaching `main` prints in vo syntax.
- `std.web` then maps a variant to statuses in one checked table, and `App.errors` goes away.

An `option[T]` falls out of the same work. Generic variants exist, so it is a library type
(`std.option`) plus changing `pop`, `env`, `first_match` and friends to return it over a major version.
Today `pop` returns `err("empty")` and `env` returns `err("not set")`: absence dressed up as failure.

## 2. Constants

"There are no global variables or constants." No globals is right. No constants means limits, table
sizes, default paths and messages are repeated literals or zero-argument functions.

```text
const max_items = 100;
export const default_port = "8080";
```

- Literals and pure expressions of other constants only, evaluated by the checker.
- Usable in effect scopes: `uses file.write(out_dir)`. Today a scope and the path used under it are two
  string literals that must agree, and E0503 is the only thing connecting them.
- Usable in annotations: `@size(1, max_items)`.

This is the cheapest item on the page.

## 3. Tuples and structural map keys

Both are already on the roadmap ("Tuples, more map keys"). The case for doing them together:

- `==` is structural on every type, so hashing can be too. `map[K, V]` with `K` any type made of
  scalars, strings, records and variants of those needs no new concept, only a hash walk and a
  constraint like the existing `T: ordered` (say `T: key`). Mutating a record while it is a key is the
  same documented hazard `ordered_map` already has.
- Without it, programs build keys by hand: `str(x) + "," + str(y)`. That is slow, and a wrong separator
  is a real bug class.
- Tuples: `(int, str)` as a type, `(a, b)` as a value, `let (a, b) = f();` to take one apart. The uses
  are returning two things and compound keys. Without them every such function declares a record.
  A reasonable alternative that adds nothing to the language is to say no to tuples and yes to keys:
  records already cover multiple returns, verbosely.

## 4. Strings

- **Read in place.** `byte_at(s, i)` (see [optimizations](optimizations.md#4-text-is-processed-through-chars-at-40-bytes-per-character)) and
  `for c in s` over code points without building a list.
- **Raw and multi-line strings.** SQL, regular expressions, JSON fixtures and HTML are the strings vo
  programs contain most, and all four fight the escape rules: the reference itself says "write `\\d` for
  `\d`", and the JSON example needs `"{\"count\": 7}"`. One raw form, for example
  `` `...` `` with no escapes and newlines allowed, covers all of them. `regex(...)` and `sql(...)`
  are checked literals already, so they keep working.
- **Unicode.** `upper` and `lower` are ASCII only (`upper("åäö")` is `"åäö"`), `len` counts bytes, and
  there is no character count other than `len(chars(s))`. For a language that guarantees valid UTF-8,
  add `std.text` functions for code-point length, case mapping for at least Latin, Greek and Cyrillic,
  and comparison without case.
- **Exponent literals**: `1e9`, `1.5e-3`. Their absence only causes E0105-style round trips.

String interpolation is deliberately absent and `format` covers it. Leave it.

## 5. Bitwise operations and hex literals

"No bitwise operators", and no hex literals. A program cannot compute a checksum or a hash, read a
binary header, decode a varint, or test a flag. `bytes` exists since spec 021, so this is the missing half.

To keep the operator table as it is, make them builtins in a module:

```text
import std.bits.and;
import std.bits.shift_left;
```

`and`, `or`, `xor`, `not`, `shift_left`, `shift_right` (arithmetic and logical) on `int`, plus wrapping
`add`/`mul` for hashes, since ordinary overflow is R0001. Add `0xFF` and `0b1010` literals and `_`
separators (`1_000_000`): three lexer rules, each with an obvious fix for today's E01xx.

## 6. Exact numbers

- **Decimal.** The expenses example carries money as integer cents and formats it by hand
  (`money(cents)`). Business software is the natural home of a language built for reviewable,
  agent-written code, and it needs a `decimal` with exact arithmetic, rounding modes and `format`
  support. A library type over `int` (value and scale) is enough to start.
- **Float text that reads back**: see [optimizations](optimizations.md#10-float-text-is-not-round-trip).
- **Checked conversions exist; sized integers do not.** `std.bytes` plus the bit builtins above cover
  reading `u16`/`u32` fields without adding integer types. Do not add sized integers to the language.

## 7. Iteration

Each line below replaces a pattern that appears throughout `examples/` and `std/`:

```text
for k, v in m { ... }          today: for k in keys(m) { let v = m[k]; ... }
for i, x in xs { ... }         today: a var counter, or for i in 0..len(xs) with xs[i]
for i in 0..n by 2 { ... }     today: a while loop
```

Also worth having as builtins or in a `std.list`: `zip`, `reverse`, `contains`, `index_of`, `first`, `last`, `sum`, `min_of`/`max_of`, `group_by`,
`sort_by` with a descending flag (the README's own example sorts descending by negating the key, which
does not work for strings).

## 8. Concurrency beyond `concurrent_map`

`concurrent_map` is a good primitive: structured, ordered, deterministic. It covers "do these N things at
once". It does not cover:

- **Something that runs for the life of the program** next to the main work: a queue consumer, a
  periodic job, a server that also polls.
- **Events arriving over time**: server-sent events, WebSockets, a subscription. (The roadmap's "Events"
  row.)
- **Parallel computation.** Fork-per-task copies the heap and sends results back as text, so it only
  pays for I/O.
- **Cancellation and deadlines** for one call, other than `--timeout` for the whole run.

A direction that keeps what vo has: workers that share nothing and exchange messages. The fork model
already gives isolation, and `serve_workers` already has the shape (a setup per worker, then messages).
Generalize it: `worker(setup, handle)` returns a typed handle with `send` and `receive`, messages limited
to the types `concurrent_map` results allow, delivery order recorded so replay stays exact. That is
Erlang's model, not Go's, and it fits effects: a worker has a grant like a test has.

Threads with shared memory would cost the deterministic collector and are not worth it.

## 9. Streaming

Everything is whole: `read_file` returns the file, `get` returns the body (capped at 16 MiB in
`net.c`), `run` returns the program's output after it ends. A log file of a gigabyte, a large download
or a long-running child process cannot be handled.

Handles would need closing, and vo has no destructors or `defer`. Callbacks avoid that and fit effect
parameters:

```text
fn each_line[E: effects](path: str, f: fn(str) -> result[void] uses E) -> result[void] uses file.read, E
```

with the same shape for chunks of `bytes`, HTTP bodies in both directions, and a child program's
output. The file is opened and closed inside the call, record and replay see a sequence of chunks, and
the grant is checked once.

## 10. Variant cases with fields, and `match` on values

- "A case carries at most one value (use a record to carry several)." So a variant with five
  multi-value cases needs five extra records whose only job is to be payloads, as `Shape`/`Size` shows in
  the reference. Allow fields in a case: `rect { w: float; h: float; }`, built as
  `Shape.rect { w: 2.0, h: 1.5 }` and bound whole in `case rect(r)`. No positional payloads, so there
  is still one way.
- "`match` works on variant and `result` values only." Dispatch on a command name or status code is an
  `if`/`else if` chain where a forgotten case is silent. Allow `match` on `int` and `str` literals with a
  required `else` arm. It needs one new keyword use and removes a class of bugs W-codes cannot catch.
- Enum-like variants need `str` and parse helpers by hand. Derive `name_of(v)` and a `from_name` for
  variants whose cases carry no values, as `to_json` is derived.

## 11. Interfaces

The roadmap puts user-defined interfaces out of scope: "generics with `ordered` and effect parameters
cover the cases seen so far". That holds for the programs written so far. It is the first thing a team
moving a real codebase will miss, in three places:

- A function that accepts "anything that can be stored" or "any notifier" takes a record of functions
  today. That works and is honest about what it is, but each new operation changes every construction
  site.
- The built-in constraints (`ordered`, `json`, effect sets) are a closed list. Each new library that
  needs one (hashing, text rendering, SQL row mapping) needs a change in the checker.
- Testing with a substitute for a dependency is done by passing functions, one parameter per operation.

If it is done, the smallest version that fits vo: an `interface` is a named set of function signatures;
a type satisfies it when a function of each name exists for it; generic parameters name it (`T: Store`);
dispatch is static, like the existing generics. No inheritance, no methods syntax, no dynamic objects.
The record-of-functions pattern stays for the dynamic case.

Recommendation: keep it out until typed errors, keys and constants are in, then decide from what the
`voledger` experiment and the benchmark runs show agents reaching for.

## 12. Imports

One `import` per name is the most repeated line in every file: `examples/expenses/report.vo` opens with
nine. It is greppable and unambiguous, and that is worth keeping. Two changes that keep both properties:

- `import std.json.{Json, as_array, int_field};` formatted one name per line by `vo fmt` when long.
- Or a module import with qualified use: `import std.json;` then `json.parse(...)`. This also solves
  name clashes (T0005 for a name declared in two files) without renaming.

Pick one. The second ages better as programs grow.

## 13. Contracts

- `old(expr)` in `ensures` (on the roadmap). Without it a contract cannot say "the list grew by one".
- Contracts are checked at run time only. Because conditions are pure and types are simple, a cheap
  static pass can already report a `requires` broken by literal arguments at check time, like E0503
  does for scopes.
- A release mode that skips `ensures` (keeping `requires`) for speed, stated in `vo version` output so
  a recording says how it was made.

## Things to keep saying no to

- Exceptions, `null`, implicit conversions, overloading, default arguments, operator overloading,
  inheritance, macros, string interpolation, truthiness. Each is a second way to do something or a way
  to hide what code does.
- Shared-memory threads and `async`/`await`. Both split the ecosystem and the second colors every
  function; effects already color them once.
- A foreign function interface into the process. One C call voids the effect boundary, the determinism
  and the three-platform guarantee. Reach outside through `std.process` (with the planned confinement)
  and through protocols.
