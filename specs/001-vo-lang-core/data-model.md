# Data Model: vo-lang Core

The toolchain is written in assembly, so the data model is mostly memory layout. Sizes are in bytes. Pointers and value slots are 8 bytes (RV64), and all records are 8-byte aligned. Positions are stored as byte offsets and converted to 1-based line/column (counted in code points) only when a report is printed. Each source file keeps a line-start table for that conversion.

## Source File

| Field | Size | Notes |
|---|---|---|
| id | 4 | index into the program's file table |
| path | 8 | → str, path relative to the program root |
| text | 8 | → bytes (whole file, read once) |
| len | 4 | |
| lines | 8 | → u32[] of line-start offsets, built by the lexer |
| ast | 8 | → File node |

Validation: the contents must be valid UTF-8 (E0104 at the first bad byte otherwise), and identifiers must be ASCII.

## Token

`kind:u16 | flags:u16 | start:u32 | end:u32 | pad:u32`, 16 bytes (padded so tokens index with a shift). Comment lines are kept as `COMMENT` tokens so the formatter can see them.

## AST Node

Every node is 64 bytes: a 24-byte header followed by five generic 8-byte fields (A–E) whose meaning depends on the kind (see `src/defs.inc`):

| Field | Size | Notes |
|---|---|---|
| kind | 2 | e.g. `FN`, `LET`, `SET`, `IF`, `CALL`, `ADD_INT` … |
| flags | 2 | mutable, has-else, … |
| type | 8 | → Type, filled in by the checker (`<error>` after a failure) |
| file | 2+2 | (+2 bytes padding) |
| start, end | 4+4 | byte offsets |

Child lists use the layout `count:u32, items:ptr[]`. The checker resolves names: every identifier node gets a pointer to its Symbol, and locals also get a frame slot index. At runtime, the interpreter never looks a name up by string.

`COMMENT` nodes sit in block statement lists and in the file's item list.

## Type

Types are interned, so two types are equal exactly when their pointers are equal.

| Kind | Params |
|---|---|
| `INT FLOAT BOOL STR VOID` | — |
| `LIST` | elem |
| `MAP` | key (`INT` or `STR` only), value |
| `RESULT` | value (may be `VOID`) |
| `RECORD` / `VARIANT` | → declaring node |
| `ERROR` | — (compatible with everything; suppresses cascading errors) |

## Symbol

`name:str | kind:u8 (fn, record, variant, case, local, param, import) | flags:u8 (mutable, used) | decl:ptr | type:ptr | slot:u32`

Scopes are arena-allocated linked lists. The checker rejects a definition whose name already resolves in any enclosing scope (E0203, no shadowing).

## Runtime Values

Every value is an 8-byte slot with no tag, which is exactly one integer register:

| Type | Slot contents |
|---|---|
| int | i64 |
| float | f64 bit pattern (`fmv.d.x` into an FPR for arithmetic) |
| bool | 0 or 1 |
| str, list, map, record, variant, result | 64-bit pointer |

The heap is a bump arena. Every object starts with `size:u32 | kind:u32`, which is reserved for a later copying GC.

| Object | Layout after the header |
|---|---|
| str | `len:u32, bytes[len]` (immutable) |
| list | `len:u32, cap:u32, data:ptr → slot[cap]` |
| map | `count:u32, cap:u32, index:ptr → u32[cap*2], entries:ptr → {key slot, value slot, hash:u32, live:u32}[cap]` (iterated in insertion order) |
| record | `slot[field_count]` in declaration order |
| variant / result | `tag:u32, pad:u32, payload slot` (result: tag 0 = `ok`, 1 = `err` with a str payload) |

## Call Frame

`fn:ptr | call_node:ptr (for the trace) | parent:ptr | slot[local_count]`

Frames live on a separate 32 MiB value stack (native recursion depth is checked separately). Walking the `parent` chain produces the runtime trace.

## Diagnostic

| Field | Notes |
|---|---|
| code | e.g. `E0301`; an index into the catalog |
| severity | `error` or `warning` |
| file, start, end | span |
| message | one sentence |
| expected, found | optional strings |
| fixes | list of Fix |
| related | list of `{file, start, end, message}` (e.g. the opening delimiter, a previous definition) |

**Fix**: `file, start, end, replacement`. A replacement over an empty span is an insertion. Overlapping fixes are dropped, and only the first is kept.

Lifecycle: collected → sorted by (file, start) → capped at 50 → rendered once (human or JSONL).

## Runtime Error Report

`code (R-code) | kind | message | file, start, end | trace: [{function, file, start}]` (innermost frame first). Produced at most once per run. Stops execution with status 70 (or 124 for a timeout).

## Test Case and Test Result

**Test Case**: a `TEST` node `{name:str, body:block}`. Duplicate names in one file are rejected (E0202).

**Test Result**: `name | status | file, start | ms | message? | expected? | actual?`. Each test runs in fresh frames. Mutations a test makes to its own values are not visible to other tests, because tests share no globals: the language has no global variables.

Status transitions: `pending → running → pass | fail (expectation failed) | error (runtime error) | timeout`.

## Error Catalog Entry

Each entry is a section of `docs/errors.md`:

```
## E0301 type_mismatch
<one-paragraph explanation>
### Example
<vo code that triggers it>
### Fixed
<corrected vo code>
```

A code, once published, is never removed or reused (FR-024). A retired code keeps its section, marked `(retired)`.

## Program Pipeline States

`read → lexed → parsed → checked → (run: running → exited | runtime_error | timeout) | (test: per-test results) | (fmt: rendered)`

Each stage stops the pipeline if it produced any error-severity diagnostic. Every stage after `parsed` requires a parse with zero errors. `fmt` needs only `parsed`.
