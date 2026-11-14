# Research: Query, Structural Editing, Trace and Replay

Decisions for spec 006. Facts about the existing code come from a survey of `src/` (file:line references are to the spec 005 tree, commit `99b5d3f`).

## Queries

### R1: References are logged by the checker, not found by a separate walk

- **Decision:** a new reference log, filled while the checker resolves names, when a global flag `q_collect` is set (only by `query`, `impact` and `edit`). Each entry is `{key_a, key_b, node, item, role}`:
  - **Top-level items** (functions, records, variants): key = the declaration node (`SY_DECL`). Imported copies of a symbol share `SY_DECL` with the original (loader.s:466-473), so a reference through an import and a reference in the defining file have the same key.
  - **Parameters, locals, type parameters:** key = the declaring name node (`SY_NODE`). Lambda captures map back to the outer symbol (`CP_OUTER`).
  - **Fields:** key = (record decl, field index), from `N_FIELD` (`N_TYPE(N_A)` → `TY_DECL`, index `N_C`), `N_FINIT` (index `N_C`, decl = the literal's `N_C`), method-style field calls (`CV_FIELD`), and the field's declaration.
  - **Variant cases:** key = (variant decl, case index), from `N_VCASE` (`N_D`, `N_E`), match arms (`N_D`, variant from the scrutinee's type) and the case declaration.
  - **Builtins:** key = (0, builtin id), so `callers print` works.
  - `role` is one of `declaration`, `call`, `value` (`NF_FNVAL`), `type` (a type annotation), `import` (the last segment of an import line, `N_B` of `N_IMPORT`) or `use`.
  - `item` is the enclosing function, test or property (a new checker global `ck_item`, set where each item is checked).
- **Where:** the existing hook `rename_use(sym, node)` (check.s:3066) is already called at every resolved use of a symbol (check_expr.s:1228, 1544, 2260; check.s:890). Logging goes next to it. Fields and cases have no symbols, so their resolution sites (`member_index` callers, check_expr.s:1637, 2333, 2490; check.s:1979-2010) log directly. Declarations log in `sym_new` / `new_local` and where record fields and variant cases are declared.
- **Rationale:** the checker already resolves every name exactly once, with shadowing, imports, generics and contract clauses (which hang off FX records, not the item's children) handled. A second walker would have to reproduce all of that, and would drift from it. With the flag clear, `check`, `run` and `test` pay one load and branch per resolved name at check time, and nothing at run time.
- **Alternatives:**
  - A generic AST walker after the check: there is none (formatter, checker and compiler each walk their own way). It would miss FX-side children and need a resolution pass of its own.
  - Storing back-pointers on every node: most uses already point forward (`N_IDENT.N_B`, `N_CALL.N_C`), but fields, arms and type names don't. Logging is fewer changes.

### R2: Name forms and resolution of a target

- **Decision:** a target is `name`, `Record.field`, `Variant.case` or `FILE:LINE:COL`.
  - `name` matches top-level items of every file of the program. Two files may each define a private `helper`; then the target is ambiguous (T0005), listing both declarations, and the position form disambiguates.
  - `Record.field` / `Variant.case` look the type up by name the same way, then the member by name.
  - `FILE:LINE:COL` converts to a byte offset (new `pos_offset(file, line, col)`: `line_start` plus a code-point walk, the inverse of `pos_col`, diag.s:242). The target is the innermost logged entry (declaration or reference) whose span contains the offset.
  - Not found: T0004, with the closest existing name (the existing edit-distance suggestion used by E0201) when one is within distance 2.
- **Rationale:** the roadmap's examples (`callers parse_line`, `type-at main.vo:12:5`) and the shapes agents already see in diagnostics. One position syntax covers locals, parameters and type parameters, which have no global name.

### R3: Every query takes the entry file first

- **Decision:** `vo query callers ENTRY NAME`, `vo query refs ENTRY TARGET`, `vo query type-at ENTRY FILE:LINE:COL`, `vo impact ENTRY --diff FILE`. `ENTRY` is the program's entry file, as for `check` and `run`.
- **Rationale:** a position in `io.vo` can belong to several programs. One fixed argument order (Principle I: one form) instead of a shorthand where the position's file is the entry. The roadmap's `vo query type-at main.vo:12:5` becomes `vo query type-at main.vo main.vo:12:5`.

### R4: `type-at` uses the checker's types, logged at the same time

- **Decision:** with a position target set (`q_pos_file`, `q_pos_off`), `check_expr` records the innermost expression node whose span contains the position, when it writes `N_TYPE` (check_expr.s:2711). Declaring name nodes, which have no `N_TYPE`, come from the reference log (type from `SY_TYPE`, or the param's `N_TYPE`, or the field's type node). The answer is the smaller of the two spans. For a name, it adds the declaration's span.
- **Rationale:** scopes are rewound after the check (check.s:545), so nothing can be asked afterwards; recording during the one check is the cheapest exact answer.

### R5: `impact` reads a unified diff and follows the reference log backwards

- **Decision:**
  - **Parsing:** `--- a/X`, `+++ b/Y` and `@@ -a,b +c,d @@` lines; other header lines (`diff --git`, `index`) are skipped. `/dev/null` marks added or deleted files. `-` from stdin.
  - **File matching:** after dropping one leading `a/` or `b/` component, a diff path matches a program file when one path is a suffix of the other at a `/` boundary (diffs are made from the repository root, programs are named from the working directory). A diff file matching no program file is reported (`ignored`) and skipped; matching two is T0007.
  - **Applies check:** every context and `+` line of each hunk must equal the current file's line at its new-side position; otherwise T0007 (the diff is not of the current files).
  - **Changed items:** a function, test or property is `changed` when a `+` line, or the position of a `-` line, falls inside its span in the current file; `added` when its whole span is `+` lines. A `-` line of the form `fn NAME(` (canonical layout) whose `NAME` is no longer defined in that file gives `removed NAME`.
  - **Affected:** reverse reachability over the logged `call` and `value` references, from the changed and removed set. Tests and properties among them are listed separately as `test`. Call sites of a removed function are unresolved names in the current program; the checker's undefined-name path logs them under the name (key 0), so they can be reported as the removed function's callers.
- **Rationale:** the spec's assumption: vo runs no version control, and every agent setup can produce `git diff` or `diff -u`. Call-based impact is what the spec asks for (Assumptions).
- **Alternative:** two program versions (`vo impact OLD_DIR NEW_ENTRY`): needs two programs loaded at once, which the file table cannot do without the overlay of R7 anyway, and agents rarely keep an old copy.

### R6: Queries on programs with errors

- **Decision:** if a file has a hard syntax error, nothing is checked (loader.s:547), so a query reports the syntax diagnostics and exits 65 with no answers. Otherwise every name the checker resolves is logged, so answers cover the parts that check; the summary gives `errors` and the text output ends with `note: the program has N errors; answers cover the parts that check`.

## Structural edits

### R7: Verification re-checks the edited text in the same process (overlay)

- **Decision:** an edit is computed as a list of `{file, start, end, old, new}` changes, applied to copies of the file texts (an overlay), and the program is checked a second time from the overlay before anything is written.
  - **Overlay:** `load_file` consults an overlay map (path → text) before reading the disk. Files get new ids (a generation counter on the file table; `file_find` only matches the current generation), so the first check's nodes stay valid for mapping results back.
  - **Reset:** a new `check_reset` clears the checker's per-program globals (scope chain, `ck_*`, `all_edges`, the generic memo, the effects state, the reference log, diagnostics) between the two checks. A golden self-test re-checks an unchanged program and requires identical diagnostics, which catches any state the reset misses.
  - **Accept rule (FR-009, FR-010):**
    - the multiset of `(code, file)` of error diagnostics after the edit equals the one before (for a clean program: no errors after);
    - for a rename, the reference log of the new check, for the target at its new position, has exactly the edited sites (so nothing new resolves to the target and nothing that did now resolves elsewhere);
    - for add-field, the record has the new field and no `E0305`/`E0304` remains for it.
  - **Refusal:** T0006 with the reason. New diagnostics are listed with their code and message, and spans mapped back to the current text through the change list (each change shifts later offsets by `len(new) - len(old)`). A clash names the existing declaration in `related`.
- **Rationale:** the checker is the only complete judge of "this still means the same". vo has no shadowing and no overloading (Principle I), so any capture by a rename shows up as E0202 (duplicate) or E0203 (shadowing) in the second check, and the reference-set comparison catches the rest.
- **Alternatives:**
  - Spawning a second `vo check` on a temporary copy: needs fork/execve of the running binary, which under `qemu-user` is not reliable (`/proc/self/exe`, binfmt), and copying a program tree.
  - Pure static rules (scan scopes for clashes): duplicates checker logic and misses cases (fields, cases, generics).

### R8: Rename changes exactly the logged sites, plus the effects record

- **Decision:** the change list is every logged entry of the target: declaration, uses, calls, values, type annotations and import names (the import's `N_B` node spans only the last segment). For a function, test or property in `vo.effects`, the record's `path:name` line is changed too (the record is registered with `file_add`, effects_record.s:129, so its lines have spans). Strings and comments are never touched because they are never logged.
- **Refused targets:** builtins, `main`, a new name that is not an identifier, a keyword, a builtin name, or the same as the old name.
- Rename of a file or an import path is out of scope (the spec's edits are renames of declarations).

### R9: add-field text placement

- **Decision:** `vo edit add-field ENTRY RECORD FIELD: TYPE = DEFAULT`. The arguments after `RECORD` are joined with single spaces and parsed with the parser's field and expression rules, so `discount: int = 0` works unquoted.
  - **Declaration:** a new line `FIELD: TYPE;` after the last field's line, with that line's indentation (4 spaces for an empty record).
  - **Literals:** `, FIELD: DEFAULT` right after the last field initialiser (canonical literals are on one line); if the closing `}` is on a later line than the last initialiser, `,` then a new line with the last initialiser's indentation. An empty literal gets `FIELD: DEFAULT`.
  - The default is pasted as written at every literal. Names in it that are not visible there fail the overlay check (E0201) and refuse the edit; the default's type must match the field's (E0301 in the overlay).
- **Rationale:** canonical form (`vo fmt`) keeps the result formatted without reformatting other lines.

### R10: Writing all files or none

- **Decision:** each changed file is written to `<path>.vo-edit.tmp`; when all temp files are written, each is renamed over its target. If a temp write fails, the temp files are removed and nothing is renamed (T0006, "cannot write"). If a rename fails, every file already renamed gets its original text back (the originals are in memory from the first check, and are written the same way, temp file then rename), the remaining temp files are removed, and T0006 reports the failure (FR-012).
- The existing `write_file_safely` (main.s:1469) ignores errors; the edit path uses a checked variant.

## Traces

### R11: A second handler table, chosen at compile time

- **Decision:** `c_emit` copies handler addresses from `vm_ops` into each instruction (compile.s:609-613). With `--trace`, it copies from `vm_ops_trace` instead, which differs only in the call and return handlers:
  - `op_trace_enter`: a new instruction emitted at the start of every function's code, only when tracing. It writes the `call` record from the new frame (`FR_FN`, `FR_CALL`, parameter slots), so every way into a function is covered: `op_call`, `op_call_fast`, `op_call_generic`, `op_call_value`, and native entries through `call_fn`/`call_value` (builtins such as `map`, `run_main`, the test runner).
  - Tracing versions of `op_ret` and `op_try` write the `return` record, then continue in the normal handler.
  - The tracing handlers live in a new file `src/trace.s`, linked after the runtime. `vm_ops_trace` is in `.rodata`. Nothing in `vm.s`'s hot code moves (the page offsets pinned in spec 005 stay valid).
- **Rationale:** runs without `--trace` execute exactly the same instructions as today (FR-016). Spec 005 showed that the VM's speed under qemu depends on code placement, so no code goes into the hot handlers.
- **GC:** the return value is only in `a0` at return; it is spilled to an s register or the frame before rendering allocates (the collector scans s registers, not a/t registers).

### R12: Trace records, limit and buffer

- **Decision:**
  - Records are `call`, `return`, `test` (in `vo test`, before each test's calls), `error` (the runtime error that ended the run or test) and `end` (always last). See contracts/trace.md.
  - Each record counts towards the limit, `--trace-limit N` (default 100,000). After the limit, records are counted but not written; `end` gives `records` and `omitted`.
  - Output goes through a new 64 KiB buffer for the trace fd, flushed when full and from a hook in `vo_exit` (sys.s:66), which every way of ending a run goes through.
  - Values are rendered with the shortened rendering of R14.
- **Rationale:** depth plus call/return pairs are enough to rebuild the call tree; no timings keeps traces deterministic (FR-014).

## Local values in runtime errors

### R13: Locals are rebuilt from the AST at the frame's position

- **Decision:** nothing maps slots to names at run time, but the AST stays alive and every local has its own slot per function (check.s:986). At a runtime error, for each frame shown (the existing 10-frame limit):
  - **Position:** the error node for the innermost frame; the child frame's `FR_CALL` node for the others.
  - **Locals:** a walk of the function's body that descends only into blocks containing the position, collecting parameters (`N_B(decl)`: name, slot `N_E`, type `N_TYPE`), `let` (`N_A`, `N_E`, `N_TYPE`), `for` variables and match-arm bindings visible there. A `let` counts only when its statement ends before the position, so a local whose initialiser is still running is not shown (its slot is still zero).
  - **Arm binding types:** not stored today (only in `SY_TYPE`); the checker writes the type into the `N_ARM` node's `N_TYPE`.
  - **Generics:** rendering resolves type parameters through `cur_frame`'s `FR_TARGS` (types.s:1230), so `cur_frame` is set to each frame while its values are rendered.
- **Reporting:** JSON adds `locals: [{name, type, value}]` to each trace entry; text adds one line under each `at` line: `      n: int = 3, xs: list[int] = [1, 2]`. In `vo test`, an `error` failure gains the same `trace` array (captured in `rt_raise` before the recovery jump).
- **Safety during the report:** rendering spends `rt_budget` and may call `rt_poll`, which can raise R0009 (rt_render.s:85). A flag set while the error report is built turns polling and timeouts off. After R0006 the existing 64 MiB headroom covers the shortened renderings.

### R14: Shortened rendering

- **Decision:** a new `render_cap` global for `render_value`: when the builder passes the cap (200 bytes), list, map, record and string rendering stop early, and the result is cut at a UTF-8 boundary and ends with `...`. Function values render as their type in angle brackets (`<fn(int) -> int>`) instead of nothing (rt_render.s label 99). `expect_eq` and counterexamples keep the uncapped rendering (cap 0 = none), so their output is unchanged.
- **Rationale:** a 1,000,000-element list must not take seconds to show in an error report; 200 bytes is enough to recognise a value.

## Recording and replay

### R15: Interception at the eight input builtins

- **Decision:** every outside input reaches the program through one of eight functions, each called from one stub of `builtin_call` (rt_call.s:13): `rt_read_line`, `rt_read_all`, `rt_read_file`, `rt_write_file`, `rt_args`, `rt_env`, `rt_now_ms`, `rt_random_int`. Recording and replay hook those, not the syscall wrappers, which the loader, the effects record and timeouts share.
  - **Recording:** each call appends an event with what the program received (contracts/recording.md). `random_int` needs no events: the chosen seed (from `--seed` or the clock, rt_effects.s:102) goes in the header. `args` go in the header.
  - **Replay:** each call takes the next event, checks its kind and name (path, variable), and returns the recorded value without touching the outside. `write_file` performs no write. The path check `rt_check_path` (which looks at the filesystem through `readlinkat`) is skipped in replay; its recorded outcome (`denied`) is replayed as the same R0010.
  - A mismatch (other kind, other path or variable, or no events left) is R0014 `replay_mismatch`, naming what the program asked for and what was recorded.
- **Rationale:** the program's view of the world is exactly these eight calls, so recording them is complete and replay is exact by Principle III.

### R16: Recording format and fingerprint

- **Decision:** JSON lines (schema 1, like every other output): a `recording` header, then one record per event, then `end` with the exit status. Text values are JSON strings; when a value is not valid UTF-8, its record has `"bytes":true` and each `\u00XX` escape stands for one byte. The fingerprint is FNV-1a 64 over each program file's path and text, in load order.
  - `vo replay` reads the recording with a small JSON-lines reader that accepts exactly what the writer produces (objects of strings, integers, booleans, `null` and arrays of strings). Anything else is T0008.
  - The recording is written through its own buffer and completed from the `vo_exit` hook, so it ends with `end` however the run stops (FR-019). A kill by signal leaves it without `end`; replay then reports T0008.
- **Timeouts:** a recorded run that timed out replays to a timeout too (exit 124), but how far it gets depends on the machine's speed, so its output may differ in length. Exactness is promised only for runs that did not time out.
- **Program changed:** replay loads the entry file named in the header (relative to the working directory, as recorded). If the fingerprint differs, it prints `note: the program differs from the recorded one` (a `replay` record with `"changed":true` in JSON) on stderr before running, and runs.

### R17: Options and safety

- **Decision:**
  - A trace or recording file that cannot be created, or whose writes fail (a full disk), is T0009 `cannot_write_output` (exit 74). A write failure is reported once, at exit; the exit status becomes 74 unless the run already ended with a non-zero status.
  - `--trace FILE` and `--record FILE` are refused (T0001) when FILE is one of the program's source files, or (for `run`) when both name the same file. A program that later writes the trace file through `write_file` gets R0010 only if its grant does not cover it; the toolchain does not try to predict writes.
  - Writing a trace or a recording needs no effect: it is the toolchain's output, like `--mem-stats`.
  - `vo replay` takes `--json`, `--trace`, `--trace-limit`, `--timeout`, `--mem-stats`, `--max-memory`. `--seed`, `--clock-ms` and `-- ARGS` are T0001 there (they come from the recording).
  - Recordings can contain secrets. The reference says so in one sentence.

## Measurement

### R18: The speed baseline moves to spec 005

- **Decision:** `make baseline` builds `build-base/vo` from `BASE_REV := 99b5d3f` (spec 005) in a worktree, replacing `baseline-004`. `tests/speed.py --baseline build-base/vo` is the FR-016/SC-006 check, with the same alternated-run method as spec 005.

### R19: The refactor stand-in

- **Decision:** `tests/refactor_eval/rNN-*/` holds `edits.txt` (one edit per line, `rename TARGET NEW` or `add-field RECORD FIELD: TYPE = DEFAULT`) and `refs.txt` (a query, then the expected `file:line:col role` lines). `tests/refactor_eval.py`:
  1. copies the task's starter to a temporary directory and records the starter's output on every visible and hidden input;
  2. applies each edit with `vo edit` (alone, on a fresh copy) and requires: exit 0, `vo check` clean, byte-identical output on every input;
  3. applies each rename by whole-word text replacement of the old name over all the program's files (for `Record.field`, the field name; for a position, the local's name), then checks and runs, and counts the results that fail to check or change behaviour;
  4. runs each query in `refs.txt` and compares with the expected lines; for the same names, counts whole-word text-search hits that are not in the expected list.
  - add-field has no text-replacement counterpart (a text edit that only adds the declaration line always fails E0305), so it is measured for `vo edit` only.
- The edit scripts are written from the task statements before `vo edit` exists, and are committed before the first run (spec Assumptions).
