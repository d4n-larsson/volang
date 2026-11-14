# Data Model: Query, Structural Editing, Trace and Replay

In-memory records added by spec 006, and the existing structures they build on. Offsets are bytes; every record is allocated in the permanent arena (the checker runs with `gc_on == 0`) unless noted.

## Reference log entry (`RL_*`, 40 bytes) — R1

| Field | Offset | Meaning |
|---|---|---|
| `RL_KEY_A` | 0 | declaration node (items), declaring name node (locals, parameters, type parameters), record/variant decl (fields, cases), 0 (builtins, unresolved names) |
| `RL_KEY_B` | 8 | field or case index, builtin id, the name string (unresolved), else -1 |
| `RL_NODE` | 16 | the node whose span is the reference (a name node, or the import's last-segment `N_NAME`) |
| `RL_ITEM` | 24 | enclosing function/test/property item node, or 0 |
| `RL_ROLE` | 32 | `RR_DECL 0`, `RR_CALL 1`, `RR_VALUE 2`, `RR_TYPE 3`, `RR_IMPORT 4`, `RR_USE 5` |

- Kept in a growable vec `q_log`; appended only while `q_collect` is 1.
- Two entries refer to the same target exactly when `(KEY_A, KEY_B)` are equal.
- Validation: every logged node has `N_FILE` of a program file and `N_START < N_END`.

## Query state (globals)

| Global | Meaning |
|---|---|
| `q_collect` | 1 while `query`/`impact`/`edit` check a program |
| `q_log` | the reference log |
| `q_pos_file`, `q_pos_off` | position target of `type-at` (file id, byte offset), or -1 |
| `q_best` | innermost expression node containing the position, recorded by `check_expr` |
| `ck_item` | the item being checked (set in `check_items` and for each test/property) |

## Target (`TG_*`) — R2

Parsed from the command line: `TG_FORM` (name / member / position), `TG_NAME`, `TG_MEMBER`, `TG_FILE`, `TG_OFF`. Resolved to a `(KEY_A, KEY_B)` pair, or T0004/T0005.

## Change (`CH_*`, 40 bytes) — R7, R8, R9, R10

| Field | Offset | Meaning |
|---|---|---|
| `CH_FILE` | 0 | file id (current generation) |
| `CH_START` | 8 | byte offset |
| `CH_END` | 16 | byte offset (`== START` for an insertion) |
| `CH_OLD` | 24 | str: the replaced text |
| `CH_NEW` | 32 | str: the replacement |

- Sorted by file, then offset; changes never overlap (duplicates from the same node logged twice are dropped).
- The effects record file is an ordinary file in this list.
- The original text of every changed file is kept until all files are written, so a failed rename can be undone (R10).
- **Mapping back:** an offset `o` in the edited text maps to `o - Σ(len(NEW) - len(OLD))` over the changes of that file that end at or before it.

## Overlay and file generations — R7

- `FILE_GEN` (new field of the file table entry): the generation the entry belongs to. `file_find` matches only entries of `cur_gen`.
- Overlay map: path str → text, consulted by `load_file` before the disk.
- `check_reset()`: clears the per-program checker state so that a second `check_program` gives the same result as a fresh process.
- Lifecycle of an edit: check (gen 0, log on) → build changes → apply to texts → `check_reset`, `cur_gen = 1`, check from the overlay (log on) → accept or refuse → write (all or none).

## Diff (`DF_*`) — R5

- Per file: `DF_PATH` (new-side path, prefix dropped), `DF_FILE` (program file id or -1), `DF_HUNKS` (vec).
- Per hunk: new-side `start`, `count`, and a vec of lines `{op: ' ' | '+' | '-', text}`.
- Derived per program file: a sorted list of changed new-side line numbers (each `+` line, and the line where each run of `-` lines was).
- Impact record: `{name, role, file, start, via}` (contracts/reports.md).

## Trace state — R11, R12

| Global | Meaning |
|---|---|
| `tr_on` | 1 when `--trace` was given (read by `c_emit` to pick `vm_ops_trace`) |
| `tr_fd`, `tr_buf` (64 KiB), `tr_len` | output |
| `tr_limit`, `tr_written`, `tr_omitted` | the limit and the counters for `end` |
| `tr_depth` | current call depth |

New instruction `OP_TRACE_ENTER` (emitted first in each function's code only when tracing). `vm_ops_trace` = `vm_ops` with `op_ret`, `op_try` and `OP_TRACE_ENTER` replaced by tracing handlers.

## Frame values — R13

- Built only while reporting a runtime error, as rendered text: per frame, a vec of `{name, type str, value str}`.
- Source: parameters (`N_B(decl)`: `N_A` name, `N_E` slot, `N_TYPE`), `N_LET` (`N_A`, `N_E`, `N_TYPE`), `N_FOR` (`N_E`, element type), `N_ARM` (`N_B` name, `N_E` slot, **new:** `N_TYPE` = binding type).
- Visible at position `p` in a function: parameters; and each binding whose declaring statement ends before `p` and whose enclosing block contains `p` (for `for` and arm bindings: whose body contains `p`). Unnamed slots (`_`, hidden counters, `ret`, exit slots) are skipped.
- `rt_reporting` (new global): 1 while the report is built; disables `rt_poll` and timeouts.

## Rendering cap — R14

`render_cap` (global, bytes; 0 = no cap). Set to 200 for traces and frame values, 0 elsewhere. When the builder's length passes the cap, rendering stops; the result is cut at a UTF-8 boundary at most 200 bytes long and `...` is appended.

## Recording state — R15, R16

| Global | Meaning |
|---|---|
| `rcd_mode` | 0 off, 1 recording, 2 replaying |
| `rcd_fd`, `rcd_buf` (64 KiB), `rcd_len` | recording output |
| `rp_events`, `rp_next` | replay: parsed events (vec) and the index of the next one |
| `rp_header` | replay: program, fingerprint, seed, clock, args |

The prefixes `rcd_` and `rp_` keep these apart from `src/effects_record.s`, whose exported symbols use `rec_`.

Event (`EV_*`, 40 bytes): `EV_KIND` (one of the six input kinds `read_line`, `read_all`, `read_file`, `write_file`, `env`, `now_ms`, or `end`), `EV_NAME` (path or variable, or 0), `EV_RESULT` (ok / err / denied), `EV_VALUE` (str or int), `EV_BYTES` (flag).

State transitions of a replayed input call: take event `rp_next` → kind and name match → return its value, `rp_next++`; otherwise R0014. Running past the last event → R0014.
