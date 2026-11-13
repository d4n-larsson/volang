---

description: "Task list for vo-lang core language and toolchain"
---

# Tasks: vo-lang Core Language and Toolchain

**Input**: Design documents from `specs/001-vo-lang-core/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/ (cli.md, output-schema.md, grammar.md, error-codes.md), quickstart.md

**Tests**: REQUIRED. Constitution v1.1.0 Principle IV says every behavior change lands first as a golden case under `tests/cases/` that fails before the implementation exists. Every test task in a phase comes before that phase's implementation tasks.

**Organization**: Tasks are grouped by user story. vo-lang is a compiler pipeline, so US2–US6 build on the lexer, parser, checker and interpreter that US1 delivers. Each story is still validated on its own by its golden cases.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies on incomplete tasks)
- **[Story]**: The user story this task belongs to (US1–US6)

## Conventions for every assembly task

- Target: RV64IMAFD, lp64d ABI, GNU as syntax. The SESI riscv64 hook checks every edit. Fix every report, or state in your reply why a report is a false positive. Follow `.claude/skills/sesi-riscv64/SKILL.md`.
- Every function starts with a C prototype comment followed by its `.globl` label. The SESI parser only understands C type words, so prototypes use only `void`, `int`, `long`, `unsigned long`, `char *` and `void *`. Put the vo-level types in a trailing note, e.g. `# void *fmt_file(void *file)  -- str* fmt_file(node* file)`. Signatures written elsewhere in this file with types like `node*`, `str*`, `type*` or `sb` describe meaning only. Shared constants (token kinds, node kinds, type kinds, object offsets, exit statuses) live in `src/defs.inc`, which each `.s` file pulls in with `.include "defs.inc"`.
- No libc. Syscalls go through the wrappers in `src/sys.s`. Memory comes from `alloc` in `src/sys.s` and is never freed.
- Until T059–T060 write `docs/reference.md`, contracts/ is the language definition, and implementing it is not a language change. From T061 on, any task that changes syntax, builtins, CLI, schema or codes updates `docs/reference.md`, `docs/errors.md` and contracts/ in the same task (Principle V).
- Layouts: data-model.md. Syntax: contracts/grammar.md. Codes: contracts/error-codes.md. Output: contracts/output-schema.md. CLI behavior: contracts/cli.md.

## Golden-case conventions (implemented by T004, used by every test task)

- A file `tests/cases/<dir>/<name>.vo` is a case only if at least one sibling file `<name>.cmd`, `<name>.out`, `<name>.err` or `<name>.code` exists. Other `.vo` files are helpers, such as imported modules or `*.expected.vo`.
- A `<name>.cmd` with no `<name>.vo` beside it is also a case, for commands that take no source file (`version`, `explain`, `reference`).
- A `<name>.skip-out` sibling skips the stdout comparison.
- `<name>.cmd` holds a single line of `vo` arguments. `{}` expands to the case's absolute path and `{dir}` to the case directory's absolute path. Commands run from a fresh `build/tmp/`, so relative paths in `.cmd` refer to that directory. If there is no `.cmd`, the default depends on `<dir>`:
  - `lex`, `parse`, `check`: `check --json {}`
  - `run`, `multi`: `run {}`
  - `test`: `test --json {}`
  - `fmt`: the fmt procedure in T004
- `<name>.in` is fed to stdin. Without it, stdin is `/dev/null`. A `<name>.hold-stdin` sibling instead connects stdin to a pipe that stays open and never writes (`tail -f /dev/null |`); this is for stdin-timeout cases. `<name>.out` and `<name>.err` give the expected stdout and stderr; a missing file means the output must be empty. `<name>.code` gives the expected exit status (default 0).
- Before comparison the runner normalizes `"ms":<digits>` to `"ms":0` and `(<digits>ms)` to `(0ms)`. It also strips the case directory's absolute path prefix (`<dir>/`) from stdout and stderr, so expected files contain paths relative to the case directory.

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Build system, wrapper, and test runner

- [X] T001 Create the directory layout from plan.md: `src/`, `docs/`, `bin/`, `bench/tasks/`, `build/` (generated), and `tests/cases/{cli,lex,parse,check,run,test,fmt,multi}/`. Add `.gitkeep` to each empty `tests/cases/*` directory.
- [X] T002 Create `Makefile` with these targets:
  - `all`: assemble every `src/*.s` with `riscv64-linux-gnu-as -march=rv64imafd -mabi=lp64d -g -I src -o build/<name>.o`, then link with `riscv64-linux-gnu-ld -static -e _start -o build/vo build/*.o`. `docs/*.md` are prerequisites of `build/data.o`.
  - `check-tools`: fail with the hint `sudo apt install binutils-riscv64-linux-gnu qemu-user` if `riscv64-linux-gnu-as` or `qemu-riscv64` is missing.
  - `lint`: run `/home/dan/dev/priv/sesi/scripts/sesi lint` on every `src/*.s` and fail on any report.
  - `test`: depends on `all`. It runs, in order:
    - `tests/run.sh`
    - `tests/run.sh --repeat 3 tests/cases/run` (determinism)
    - `python3 tests/fuzz.py --iterations 200 --seed 1`
    - `python3 tests/apply_fixes.py`
    - `python3 tests/catalog.py`
    - `python3 tests/ref_examples.py`
    - `python3 tests/fmt_corpus.py`

    Skip a script that doesn't exist yet with a notice. This target is the constitution's quality gate.
  - `clean`.
- [X] T003 [P] Create `bin/vo`, a POSIX sh script, executable. It resolves the repo root from its own path. When `uname -m` is `riscv64` it runs `exec "$root/build/vo" "$@"`, otherwise `exec qemu-riscv64 "$root/build/vo" "$@"`.
- [X] T004 [P] Create `tests/run.sh`, a POSIX sh golden runner that implements the golden-case conventions above.
  - Usage: `tests/run.sh [--repeat N] [PATH...]`, with PATH defaulting to `tests/cases`.
  - For each case, run `bin/vo` with the case's arguments from inside `build/tmp/` (fresh per case), feed `.in` to stdin, capture stdout, stderr and exit status, normalize them, and compare each with its expectation. On mismatch print `FAIL <case>` plus a unified diff, otherwise print `PASS <case>`.
  - **fmt procedure:** copy the case to `build/tmp/case.vo`, run `vo fmt build/tmp/case.vo`, compare the file with `<name>.expected.vo`, then require `vo fmt --check build/tmp/case.vo` to exit 0.
  - **`--repeat N`:** run each case N times and fail if any run differs byte-wise from the first (SC-006).
  - Print a final `passed/failed` count. Exit 1 if anything failed.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Process startup, memory, text, the diagnostic engine, the CLI shell and the lexer. Every story uses all of these.

**⚠️ CRITICAL**: No user story work can begin until this phase is complete.

### Tests (write first, must fail)

- [X] T005 [P] Create CLI golden cases in `tests/cases/cli/`:
  - `version` (`.cmd` = `version`; `.out` = `vo 0.1.0 schema 1`)
  - `unknown_command` (`.cmd` = `frobnicate`; `.code` = 64; `.out` names T0001 and lists the commands, since toolchain errors go to the report stream per contracts/cli.md)
  - `bad_flag` (`.cmd` = `check --nope {}`; code 64)
  - `missing_file` (`.cmd` = `check --json does/not/exist.vo`; code 66; stdout is a `toolchain_error` T0002 record with `path`, then a `summary`)
  - `version_json` (`.cmd` = `version --json`; `.out` = `{"v":1,"kind":"version","version":"0.1.0","schema":1}`)
  - `unknown_command_json` (`.cmd` = `frobnicate --json`; code 64; stdout is a `toolchain_error` T0001 record)
- [X] T006 [P] Create lexer golden cases in `tests/cases/lex/`, each otherwise valid vo per contracts/grammar.md, so the cases stay valid once the parser exists:
  - `invalid_char.vo` (a `$` → E0101 with a removal fix)
  - `trailing_comment.vo` (`let x = 1; # hi` → E0101)
  - `unterminated_string.vo` (E0102 with an insert-`"` fix)
  - `bad_escape.vo` (`"\q"` → E0103)
  - `invalid_utf8.vo` (byte `0xFF` inside a string → E0104 at that byte; create it with `printf`)
  - `int_too_big.vo` (`9223372036854775808` → E0105)
  - `float_too_long.vo` (16 significant digits → E0105)
  - `empty.vo` and `only_comments.vo` (zero diagnostics; summary `"errors":0`)
  - `columns.vo` (a string containing `é` before an error on the same line; the expected `col` counts code points, not bytes)

### Implementation

- [X] T007 Create `src/defs.inc` with `.equ` constants:
  - exit statuses from contracts/cli.md (0, 1, 64, 65, 66, 70, 124, 125)
  - Linux RISC-V syscall numbers: read 63, write 64, openat 56, close 57, exit_group 94, mmap 222, munmap 215, clock_gettime 113, getdents64 61, renameat 38, fstat 80, ppoll 73
  - token kinds for every keyword, operator and punctuator in contracts/grammar.md, plus IDENT, TYPE_IDENT, INT, FLOAT, STRING, COMMENT, EOF
  - the Token layout `kind:u16 | flags:u16 | start:u32 | end:u32` (12 bytes)
  - the heap object header `size:u32 | kind:u32`
  - the str layout `len:u32, bytes[len]`

  AST node kinds and type kinds get added by later tasks.
- [X] T008 Implement process startup and syscall wrappers in `src/sys.s`:
  - **`_start`**: read argc/argv from the initial stack; `mmap` a 64 MiB stack (research §9), move `sp` to its 16-byte-aligned top, and store `base + 1 MiB` in global `stack_floor`; call `int vo_main(int argc, char **argv)`, then flush output and `exit_group` with its result.
  - **Wrappers**: `sys_read`, `sys_write`, `sys_openat`, `sys_close`, `sys_exit`, `sys_mmap`, `sys_clock_ms` (CLOCK_MONOTONIC, returns ms), `sys_getdents64`, `sys_renameat`, and `long sys_wait_stdin(long ms)`. That last one calls `ppoll` on fd 0 and returns 1 if readable, 0 on timeout; `ms` < 0 means wait forever.
  - **Buffered output**: `out_bytes(int fd, char *p, long n)`, `out_cstr`, and `out_flush`, with 64 KiB buffers for fd 1 and fd 2. Flush before exit and before any blocking read of stdin.
  - **`read_whole_file`**: `long read_whole_file(char *path, char **out)` returns the length or a negative errno, reading in 64 KiB steps into an arena buffer.
- [X] T009 Implement the arena allocator in `src/sys.s`: `void *alloc(long size)` returns 8-byte-aligned zeroed memory from a bump pointer over `mmap`'d 16 MiB chunks. Larger requests get their own chunk. There is a hard cap of 512 MiB in total. When the cap is hit, call the function pointer stored in global `oom_hook`; the default hook prints a T0003 internal error and exits with 125. Add the comment `# ponytail: bump arena, never frees; long allocation-heavy runs hit the 512 MiB cap (R0006); upgrade: copying GC using the size:u32 object headers`.
- [X] T010 [P] Implement text utilities in `src/text.s`:
  - `str_new(ptr,len)` building a str object, `str_eq`, `str_cmp` (byte-wise)
  - a growable byte builder: `sb_new`, `sb_byte`, `sb_bytes`, `sb_str`, `sb_finish` (returns a str)
  - `long utf8_validate(char *p, long n)`, returning −1 or the offset of the first invalid byte
  - `long cp_count(char *p, long n)`, counting code points
  - `json_escape_into(sb, char *p, long n)`, which escapes `"`, `\`, and control chars (`\n`, `\t`, `\u00XX`)
- [X] T011 [P] Implement integer↔text conversion in `src/num.s`:
  - `void int_to_dec(sb, long v)`, handling `INT64_MIN` correctly
  - `long dec_to_int(char *p, long n, long *out)`, returning 0 on success, 1 on overflow, 2 on a malformed number; accepts an optional leading `-`

  Float conversion is added in T038.
- [X] T012 Create `docs/errors.md` with one section for every code in contracts/error-codes.md (E, W, R and T codes). Each section opens with `## <CODE> <name>` followed by a one-line summary and a line `Fixable: yes` or `Fixable: no`, copied from the "Fix?" column of contracts/error-codes.md (yes when that column names a fix). It then contains `### Example` and `### Fixed` subsections with `TODO` placeholders; T053–T054 fill these in. Also create `docs/reference.md` containing only `# vo-lang Reference` plus a TODO line (filled in by T059 and T060).
- [X] T013 Create `src/data.s`. It embeds the two docs with `.incbin` and exports their address and length: `ref_md`/`ref_md_len` for `docs/reference.md`, and `errors_md`/`errors_md_len` for `docs/errors.md`. Add `int catalog_name(char *code, char **name, long *len)`, which looks up the `## <CODE> <name>` heading in `errors_md`. Error names therefore come from the catalog, never from a second table.
- [X] T014 Implement the diagnostic engine in `src/diag.s`, using the layouts in data-model.md.
  - **File table**: `file_add(path, text, len) -> id`. It builds the line-start table. `pos_of(file, offset, *line, *col)` gives a 1-based line and a col counted in code points.
  - **Recording**: `diag_add(code, severity, file, start, end, message) -> diag*`, plus `diag_set_expected_found`, `diag_add_fix(d, file, start, end, replacement)` (drop a fix that overlaps a fix already on the diagnostic), and `diag_add_related(d, file, start, end, message)`.
  - **Emitting**: `diag_emit_all(int json, int fd)` sorts by `(file, start)`. It then drops any fix whose range overlaps or touches a fix already emitted by an earlier diagnostic; an insertion at the boundary of another fix's range counts as touching. Finally it caps output at 50 diagnostics, counting the rest as `truncated`. It renders the human format or JSONL exactly as in contracts/output-schema.md. Every record includes `text` (the source covered by the span) and `name` (via `catalog_name`).
  - **Summary**: `summary_emit(json, fd, command, files)` writes the summary line.
  - **Counters**: `diag_error_count()`.
- [X] T015 Implement the CLI shell in `src/main.s` (`vo_main`), following contracts/cli.md.
  - **Arguments**: parse the command and the flags `--json`, `--timeout SEC` (default 10; 0 = none), `--filter TEXT`, `--check`, and `--` (arguments after it go to the program), into a global options struct.
  - **Paths**: expand directory PATHs into every `*.vo` file beneath them, recursively, sorted byte-wise (`sys_getdents64`).
  - **Errors**: usage errors emit T0001 and exit 64. An unreadable input emits T0002 and exits 66. Both are `toolchain_error` records (contracts/output-schema.md), written as JSONL when `--json` appears anywhere in argv and in human form otherwise. They go to stdout, or to stderr for `run`.
  - **Commands**: implement `version`, printing `vo 0.1.0 schema 1`, or the `version` record under `--json`. `check` reads each file, registers it with `file_add`, and runs only the lexer for now (T016), then emits diagnostics and the summary and exits 65 if there were errors. `run`, `test`, `fmt`, `explain` and `reference` exit 125 with T0003 "not implemented" until their stories land.
- [X] T016 Implement the lexer in `src/lex.s`: `tokens* lex(file_id)` produces an arena array of Tokens following the lexical rules in contracts/grammar.md.
  - **Comments**: a whole-line `#` comment becomes a COMMENT token. A `#` after code on the same line is E0101.
  - **Identifiers**: `[a-z][a-z0-9_]*` is IDENT, `[A-Z][A-Za-z0-9]*` is TYPE_IDENT, a lone `_` is its own token, and keywords come from a table.
  - **Numbers**: an INT literal above 2^63−1 is E0105. A FLOAT is `[0-9]+ "." [0-9]+`; more than 15 significant digits is E0105.
  - **Strings**: single-line `"…"` with only `\n \t \\ \"` as escapes. Other escapes are E0103; an unterminated string is E0102, with a fix inserting `"` at the end of the line.
  - **Bad characters**: any other character is E0101 with a removal fix.
  - **UTF-8**: run `utf8_validate` over the whole file first; an invalid byte is E0104 at its offset.
  - **Recovery**: the lexer always reaches EOF, emitting an error token and continuing after each bad character.
- [X] T017 Create `tests/fuzz.py` (SC-007). Usage: `--iterations N --seed S`. Each iteration picks a corpus file from `tests/cases/**/*.vo`, applies 1–5 random mutations (delete, duplicate or swap bytes; delete or duplicate a token-sized chunk; insert random bytes, including invalid UTF-8), and runs `bin/vo check --json` with a 5 s hard limit. It also runs `bin/vo run --timeout 2` once `run` is implemented, detected by `bin/vo run` on `tests/cases/run/hello.vo` not exiting 125. The script is created in Phase 2 against the lexer and grows stronger as the pipeline lands. It asserts:
  - the exit status is in {0–63, 65, 70, 124}; never 125, never a signal, never killed by the hard limit
  - every stdout line of `check --json` parses as JSON with `"v":1`

  It saves failing inputs to `build/fuzz-failures/`.

**Checkpoint**: `make && tests/run.sh tests/cases/cli tests/cases/lex && python3 tests/fuzz.py --iterations 200 --seed 1` passes.

---

## Phase 3: User Story 1 — Write and run a program (Priority: P1) 🎯 MVP

**Goal**: `vo run file.vo` runs every v1 construct and builtin. Program output, exit statuses and structured runtime errors all work.

**Independent Test**: `printf '1\n2\n39\n' | bin/vo run /tmp/sum.vo` prints `42` (quickstart.md US1), and `tests/run.sh tests/cases/run` passes.

### Tests for User Story 1 (write first, must fail)

- [X] T018 [P] [US1] Create language-feature run cases in `tests/cases/run/` (`.vo` + `.out`, plus `.in`/`.cmd`/`.code` where noted):
  - `hello.vo`
  - `arith.vo` (int and float `+ - * / %`, precedence, unary `-`, truncating `/`)
  - `strings.vo` (`+`, `len`, `slice`, `find`, `split`, `join`, `trim`, `chars` on a non-ASCII string)
  - `control.vo` (if / else if / else, while, `for i in 0..5`, `for x in list`, break, continue)
  - `functions.vo` (recursion: fib 20, early return)
  - `records.vo` (`new Point { x: 1, y: 2 }`, field read, `set p.x = …`)
  - `variants.vo` (a variant with payload and payload-less cases; exhaustive match)
  - `results.vo` (`ok`/`err`, `try` propagation, match on result, `ok()` for `result[void]`)
  - `lists_maps.vo` (push, pop on empty → err, index, `set xs[i]`, `new map[str, int]`, `set m[k]`, has, remove, keys in insertion order, stable sort of int/float/str)
  - `equality.vo` (structural `==` on lists/records/variants; `nan != nan`)
  - `conversions.vo` (`str` of int/float/bool printing `3.14`, `1.0`, `0.1`, `-0.5`, `100000000000000.0`; `int`, `float`; `parse_int`/`parse_float` ok and err)
  - `io.vo` with `.in` (`read_line` loop to eof, then `read_all`)
  - `args.vo` with `.cmd` = `run {} -- a b`
  - `files.vo` (`write_file("out.txt", …)`, then `read_file` it back; reading a missing file → err)
  - `exit_status.vo` (`exit(3)`; `.code` = 3)
  - `main_err.vo` (main returns `err("boom")`; `.err` = `error: boom`; code 1)
  - `run_ignores_tests.vo` (a file with `test` blocks; `run` executes only main)
- [X] T019 [P] [US1] Create runtime-error cases in `tests/cases/run/`. Each expects a human report on stderr (`.err`) with code, message, location and a `trace:` list, and `.code` = 70 unless noted:
  - `int_overflow_add.vo` and `int_overflow_mul.vo` (R0001; mul case: `3037000500 * 3037000500`)
  - `int_div_zero.vo` (R0002; also covers `%` by 0 in `int_rem_zero.vo`)
  - `int_div_min.vo` (`MIN / -1` → R0001, building MIN as `-9223372036854775807 - 1`)
  - `index_oob.vo` (R0003)
  - `key_missing.vo` (R0004)
  - `stack.vo` (unbounded recursion → R0005)
  - `oom.vo` (doubling a string in a loop → R0006)
  - `invalid_arg.vo` (`int(0.0/0.0)` → R0007)
  - `slice_utf8.vo` (R0007)
  - `exit_range.vo` (`exit(64)` → R0008)
  - `timeout.vo` (`.cmd` = `run --timeout 1 {}`, `while true {}` → R0009, code 124)
  - `timeout_stdin.vo` (`.cmd` = `run --timeout 1 {}`, `.hold-stdin`; `read_line()` blocks on an open, silent stdin → R0009, code 124)
  - `div_zero_json.vo` (`.cmd` = `run --json {}`; `.err` = the exact runtime_error JSONL record from contracts/output-schema.md, trace innermost first)
- [X] T020 [P] [US1] Create minimal check cases in `tests/cases/check/` for errors US1 must already detect (first error only is fine at this stage):
  - `type_mismatch.vo` (E0301 with expected/found)
  - `undefined_name.vo` (E0201)
  - `missing_main.vo` (`.cmd` = `run {}` → E0408, code 65)
  - `non_exhaustive.vo` (E0401 listing missing cases)
  - `unhandled_result.vo` (E0404)
  - `assign_let.vo` (E0208)
  - `shadowed.vo` (E0203 with `related` at the outer definition)
  - `arg_count.vo` (E0302)
  - `not_callable.vo` (E0303)
  - `missing_field.vo` (E0305 listing the missing fields)
  - `cannot_infer.vo` (`let r = ok(1);` → E0307)
  - `invalid_operands.vo` (`"a" * 2` → E0308)
  - `map_key.vo` (`map[bool, int]` → E0309)
  - `missing_return.vo` (E0403)
  - `try_outside.vo` (E0406)
  - `break_outside.vo` (E0407)
  - `bad_main.vo` (`.cmd` = `run {}`; `fn main(x: int)` → E0409, code 65)
  - `warnings.vo` (an unused local → W0201 and a statement after `return` → W0401; summary `"warnings":2`, code 0)

### Implementation for User Story 1

- [X] T021 [US1] Add AST node kinds and type kinds to `src/defs.inc`:
  - the 24-byte node header `kind:u16 | flags:u16 | type:ptr(8) | file:u16 | pad:u16 | start:u32 | end:u32`
  - node-specific field offsets for every grammar production in contracts/grammar.md
  - type kinds `INT FLOAT BOOL STR VOID LIST MAP RESULT RECORD VARIANT ERROR`
  - typed operator kinds used after checking (`ADD_INT`, `ADD_FLOAT`, `CONCAT`, `SUB_INT`, … `EQ_DEEP`, `LT_INT`, `LT_FLOAT`, `LT_STR`, …)
- [X] T022 [US1] Implement parser infrastructure and declarations in `src/parse.s`:
  - `node* parse_file(file_id, tokens*)`
  - token cursor helpers `peek`, `advance` and `expect(kind)`, where a mismatch emits E0110 with expected/found and stops parsing this file (recovery comes in T048)
  - `import` lines (`import a.b.name;`)
  - `fn` with params and `-> type`
  - `record` with `field: type;` lines
  - `variant` with `case;` / `case(type);` lines
  - `test "name" { … }`
  - types (`int float bool str`, TYPE_IDENT, `list[T]`, `map[K, V]`, `result[T|void]`)
  - COMMENT tokens become COMMENT nodes in the item list
  - child lists use `count:u32, items:ptr[]`
- [X] T023 [US1] Implement statement parsing in `src/parse.s`: `let`/`var` (with an optional `: type`; `let _ = …`), `set place = expr;` (place = ident with `.field`/`[expr]` suffixes), `if … else if … else`, `while`, `for x in expr [.. expr]`, `match expr { case name[(binding|_)] block … }`, `return [expr];`, `break;`, `continue;`, and call statements (`ident(...)`). Every statement kind is chosen from its first token (FR-002). COMMENT tokens become COMMENT statement nodes.
- [X] T024 [US1] Implement expression parsing in `src/parse.s` with the precedence from contracts/grammar.md: `or` < `and` < `not` < cmp (non-associative; a second comparison operator is E0110) < `+ -` < `* / %` < unary `-`/`try` < postfix `.field`/`[i]`. Primaries are int, float, string (decode escapes into a str), `true`/`false`, ident, call, the conversion calls `str(e)`, `int(e)` and `float(e)` (these names are keywords, contracts/grammar.md `primary`), `Type.case`, `Type.case(expr)`, `( expr )` (no paren node is kept), non-empty `[a, b]`, `new Type { f: e, … }`, `new list[T]`, and `new map[K, V]`. Check the stack floor on entry to the recursive routines; past it emit E0115.
- [X] T025 [US1] Implement type interning and declaration collection in `src/check.s`.
  - **Interning**: `type* type_intern(kind, a, b)`, so equal types are the same pointer. Add predefined `t_int`, `t_float`, `t_bool`, `t_str`, `t_void` and `t_error`.
  - **Declarations**: collect top-level `fn`, `record` and `variant` items into the file scope as Symbols (`name | kind:u8 | flags:u8 | decl | type | slot:u32`). A duplicate is E0202, with `related` pointing at the first definition.
  - **Naming**: violations are E0207, with a fix giving the converted name.
  - **Declared types**: resolve them; an unknown type name is E0201, and a map key other than `int`/`str` is E0309.
- [X] T026 [US1] Implement scopes and name resolution in `src/check.s`.
  - **Scopes**: block scopes are arena-allocated linked lists. Defining a name that already resolves in any enclosing scope is E0203, with `related` pointing at the outer definition.
  - **Identifiers**: each identifier node gets its Symbol pointer, and each local or parameter gets a frame `slot` index. Store the per-function `local_count` on the FN node.
  - **Errors**: an undefined name is E0201 (fixes come in T051). Unused locals are W0201.
- [X] T027 [US1] Implement expression typing in `src/check.s`. It must allow no implicit conversions (FR-005).
  - **Arithmetic**: both operands must have the same type, int or float. `+` on two str becomes CONCAT. `%` is int only. Rewrite each operator node to its typed kind (T021).
  - **Logic**: `and`, `or` and `not` take bool.
  - **Comparison**: `==`/`!=` become EQ_DEEP on any type. `<` and the other orderings work on int, float and str.
  - **Errors**: a mismatch is E0301 with expected/found; invalid operands are E0308.
  - **Calls**: user functions are checked for argument count (E0302) and argument types. Calling a non-function is E0303.
  - **Records**: field access and literals check for unknown fields (E0304) and missing fields (E0305, listing them).
  - **Lists**: a list literal's element type is the first element's type. An empty `[]` is E0306.
  - **Expected types**: `ok(v)`/`ok()`/`err(msg)` and `new …` take their type from the declared return type or annotation; when there is none, E0307.
  - **Variant cases**: `Type.case(x)` is checked against the case's payload type.
- [X] T028 [US1] Add the builtin signature table to `src/check.s`, covering every builtin in contracts/grammar.md §Builtins with its exact signature. Encode the polymorphic builtins (`len`, `slice`, `str`, `push`, `pop`, `sort`, `has`, `remove`, `keys`, `expect_eq`) as small per-builtin type rules. Record a builtin id on each CALL node. `expect`/`expect_eq` outside a `test` block are E0410.
- [X] T029 [US1] Implement statement and control-flow checks in `src/check.s`:
  - `set` on a `let` is E0208, with a fix replacing `let` with `var` at the declaration
  - `if`/`while` conditions must be bool
  - `for x in` needs a list; `a..b` needs int bounds
  - `match` works only on variant or result values: a missing case is E0401 (message lists the missing cases), a duplicate is E0402, and the payload binding is typed
  - `try` outside a result-returning fn is E0406
  - a call returning a result used as a statement, or bound to `_`, is E0404
  - any other non-void call used as a statement is E0405
  - a path that can fall off the end of a function with a return type is E0403
  - `break`/`continue` outside a loop is E0407
  - for `run`, a missing `main` is E0408; a `main` that isn't `fn main()` or `fn main() -> result[void]` is E0409
  - statements after `return`/`break`/`continue` are W0401
- [X] T030 [US1] Implement the core of the evaluator in `src/interp.s`.
  - **Frames**: `fn:ptr | call_node:ptr | parent:ptr | slot[local_count]`, allocated on the native stack. The global `cur_frame` gives the trace.
  - **Calls**: `long eval_call(fn_node, args)`. Statement execution returns a status: NORMAL, BREAK, CONTINUE or RETURN, with the value in a global.
  - **Statements**: let, var, set (local/field/index/map), if, while, `for` over a range or a list, return, break, continue, and the call statement.
  - **Step counter**: increment at every loop iteration and call. Every 65,536 steps compare `sys_clock_ms()` with the global `deadline_ms` (0 = none); past it raise R0009.
  - **Stack**: check `sp` against `stack_floor` on entry to `eval_expr`/`eval_call`; past it raise R0005.
  - Add `# ponytail: tree-walking evaluator, ~50-100x slower than native; upgrade: bytecode VM behind the same checker`.
- [X] T031 [US1] Implement expression evaluation in `src/interp.s`. Every value is an 8-byte slot in one register (data-model.md).
  - **Checked int ops, inline (research §3)**:
    - add: `add t,a,b`; overflow if `(t<a) != (b<0)`
    - sub: the same rule mirrored
    - mul: `mul` and `mulh`; overflow if `hi != lo>>63`
    - div/rem: check `b==0` first (R0002), then `a==MIN && b==-1` (R0001), then `div`/`rem`
    - unary `-MIN` is R0001
  - **Floats**: arithmetic goes through `fmv.d.x`/`fmv.x.d` with `fadd.d` and the other D-extension instructions.
  - **Comparisons**: int, float and str.
  - **Equality**: EQ_DEEP compares recursively using the node's static type. Floats compare with `feq.d`, so `nan != nan`.
  - **Logic**: `and`/`or` short-circuit.
  - **Other expressions**: CONCAT, literals, local loads, field and index reads.
- [X] T032 [US1] Implement records, variants and results in `src/interp.s`:
  - `new Type { … }` allocates a record (`slot[field_count]` in declaration order, after the object header)
  - a variant or result is `tag:u32, pad:u32, payload slot`; result tag 0 = `ok`, 1 = `err` with a str payload
  - `Type.case(x)` constructs a variant value
  - `match` dispatches on the tag and binds the payload to its slot
  - `try e` either yields the ok payload, or stores the err value as the current function's return value and unwinds with status RETURN
- [X] T033 [US1] Implement runtime error reporting in `src/interp.s` with `void rt_raise(code, message, node)`. It builds the report `code | name | message | file, start, end | trace: [{function, file, start}]`, innermost frame first, by walking `cur_frame` and taking each frame's `call_node` span. In `run` it writes the report to stderr: human format with a `trace:` list and one `  at function (file:line:col)` line per frame, or JSONL per contracts/output-schema.md under `--json`. Then it flushes and exits 70, or 124 for R0009. If the global `recover_point` is set, it longjmps there instead; T065 uses this for tests. Set `oom_hook` (T009) to raise R0006.
- [X] T034 [P] [US1] Implement the I/O builtins in `src/rt_io.s`:
  - `print`, `write`, `eprint`
  - stdin reads (`read_line`, `read_all`): before each `sys_read` of fd 0, call `sys_wait_stdin(deadline_ms - now)`, or -1 when there is no deadline; on timeout raise R0009 (FR-020)
  - `read_line`: flushes stdout before reading; strips the trailing `\n`; returns `err("eof")` at EOF
  - `read_all`
  - `read_file` / `write_file`: return `err(<message>)` on failure
  - `args`: the arguments after `--`
  - `exit`: codes 0–63, otherwise R0008; flush before exiting
  - `now_ms`
- [X] T035 [P] [US1] Implement the string builtins in `src/rt_str.s`:
  - `len` (bytes)
  - `slice(s, a, b)`: half-open; R0003 when out of range; R0007 when a boundary splits a UTF-8 sequence
  - `find` (first byte offset, or −1)
  - `split`: an empty separator is R0007
  - `join`
  - `trim` (ASCII whitespace)
  - `chars` (one str per code point)
  - `str(int|bool)`: float goes through T038
  - `parse_int`, via `dec_to_int`; returns `err` with a message
- [X] T036 [P] [US1] Implement the list builtins in `src/rt_list.s`. The list layout is `len:u32, cap:u32, data:ptr → slot[cap]`.
  - **Basics**: `new list[T]`, list literals, `push` (doubling growth by copying), `pop` (`err("empty")` on an empty list), index read and `set xs[i]` (both R0003 when out of bounds), `len`, `slice(xs, a, b)`.
  - **`sort`**: a stable merge sort for int, float and str, using a scratch array from the arena.
- [X] T037 [P] [US1] Implement maps in `src/rt_map.s`. The layout is `count:u32, cap:u32, index:ptr → u32[cap*2], entries:ptr → {key slot, value slot, hash:u32, live:u32}[cap]`.
  - **Hashing**: FNV-1a with a fixed seed over the int's 8 bytes or the str's bytes. Open addressing with linear probing indexes into the entries array, which is kept in insertion order.
  - **Operations**: `new map[K, V]`, `m[k]` (R0004 when missing), `set m[k] = v`, `has`, `remove` (marks the entry not live and clears its index slot as a tombstone), `keys` (live entries in insertion order), and `len`.
  - **Growth**: rebuild when entries are full or the tombstone load passes 50%.
- [X] T038 [US1] Implement float↔text conversion in `src/num.s` (research §11):
  - **`float_to_text`**: `nan`, `inf` and `-inf` print as words. Otherwise scale to 15 significant digits, round to an int64, emit the digits with a decimal point, and trim trailing zeros, keeping at least one digit after the point (`1.0`, `3.14`, `0.1`). Values outside the fixed range print in plain decimal digits; there is no exponent form.
  - **`text_to_float`**: at most 15 significant digits, no exponent. Divide the exact int64 mantissa by an exact power of ten (`fdiv.d`).
  - **Wiring**: use these for float literals in `src/lex.s`/`src/parse.s`, for `str(float)`, and for `parse_float`.
  - Add `# ponytail: 15 significant digits, not shortest round-trip; upgrade: Ryu port`.
- [X] T039 [US1] Wire the builtins into `src/interp.s`: dispatch each CALL node's builtin id (T028) to its `src/rt_*.s` function, passing the node so runtime errors carry the right span. `expect`/`expect_eq` remain stubs until T064.
- [X] T040 [US1] Wire the pipeline into `src/main.s`:
  - `check`: lex → parse → check for each file, then emit diagnostics and the summary; exit 65 if there are errors, else 0
  - `run FILE`: lex → parse → check with run mode, so `main` is required. Any error goes to stderr in human or JSONL format, then the summary, and exits 65. Otherwise set `deadline_ms` from `--timeout` and evaluate `main`. A normal return exits 0. `main` returning `err(msg)` prints `error: <msg>` to stderr and exits 1.
- [X] T041 [US1] Make `tests/run.sh tests/cases/run tests/cases/check` pass. Fix defects in `src/*.s` until every T018–T020 case passes, then run `make lint` and resolve every report.

**Checkpoint**: US1 is fully functional. The quickstart US1 scenario prints `42`, and all `run/` cases pass, including `--repeat 10`.

---

## Phase 4: User Story 2 — Fix mistakes from machine-readable diagnostics (Priority: P1)

**Goal**: One pass reports every independent error, with fixes and related locations. `vo explain` works for every code.

**Independent Test**: `bin/vo check --json tests/cases/check/three_errors.vo` gives 3 ordered diagnostics, a summary with `"errors":3`, and exit 65. `python3 tests/apply_fixes.py` passes.

### Tests for User Story 2 (write first, must fail)

- [X] T042 [P] [US2] Create multi-error and recovery cases in `tests/cases/check/`:
  - `three_errors.vo` (a type mismatch, an undefined name and a missing `;` in three different functions; all three reported, ordered by position)
  - `cascade.vo` (`let x: int = "a"; let y = x + 1; let z = undefined_thing + 1;` reports exactly two diagnostics: one E0301 and one E0201, with nothing derived from either)
  - `flood.vo` (60 independent E0201 errors; 50 records, then a summary with `"truncated":10`)
  - `unclosed_block.vo` (E0111 with `related` pointing at the opening `{` and a fix inserting `}`)
  - `nesting.vo` (200,000 nested parentheses generated into the file, deep enough to exceed the 64 MiB parser stack → E0115, no crash)
  - `chained_compare.vo` (`a < b < c` → E0110 with expected/found)
  - `unexpected_token.vo` (`fn f() -> { }` → E0110 expecting a type)
- [X] T043 [P] [US2] Create fix-suggestion cases in `tests/cases/check/`, where each `.out` contains the full JSONL including `fixes`:
  - `typo_name.vo` (`totl` → `total`, `related` = the definition)
  - `typo_field.vo` (E0304 with a similar-field fix)
  - `missing_semicolon.vo` (E0112 insertion at the end of the previous token)
  - `trailing_comma.vo` (E0113 removal)
  - `assign_without_set.vo` (`x = 1;` → E0114 with a fix to `set x = 1;`)
  - `str_wrap.vo` (`print(total)` with int `total` → E0301, fix `str(total)`)
  - `int_to_float.vo` (E0301, fix `float(…)`)
  - `let_to_var.vo` (E0208 replacing `let` with `var`)
  - `non_exhaustive_fix.vo` (E0401 inserting the missing `case name(_) { }` arms)
  - `unhandled_result_fix.vo` (E0404 with a fix inserting `try ` inside a result-returning fn)
  - `unused_value.vo` (E0405 with a fix inserting `let _ = `)
  - `naming.vo` (E0207 with `myVar` → `my_var`)
  - `empty_list.vo` (E0306 with `[]` → `new list[int]` from the annotation)
  - `duplicate_case.vo` (E0402 with a removal fix)
  - `overlapping_fixes.vo` (`let x: str = 5` followed by a newline and another statement: E0301 with fix `str(5)` and E0112 whose `;` insertion touches that fix's range, so the E0112 record has `"fixes":[]`)
- [X] T044 [P] [US2] Create explain cases in `tests/cases/cli/`:
  - `explain_e0401` (`.cmd` = `explain E0401`; `.out` = that exact section of `docs/errors.md`, written after T054)
  - `explain_r0002`
  - `explain_unknown` (`explain E9999` → T0001, code 64)
  - `explain_json` (`.cmd` = `explain --json E0401` → one `explain` record with `code`, `name`, `fixable` and `text`)
- [X] T045 [P] [US2] Create `tests/apply_fixes.py` (SC-003). For every case under `tests/cases/check` and `tests/cases/parse` whose expected `.out` contains a diagnostic with a non-empty `fixes`:
  - apply that diagnostic's first fix to a temporary copy, translating line/col in code points to a byte offset, and confirm the source at the span equals `text`
  - re-run `bin/vo check --json` and assert the same code no longer appears at that position
  - print each failure; exit 1 if any fix fails or if fewer than 95% of fixes resolve their error
  - **Fix coverage (SC-003)**: across every diagnostic in the expected outputs of `tests/cases/{lex,parse,check,multi}` (the error corpus), count the diagnostics whose code is `Fixable: yes` in `docs/errors.md` and how many of those carry a fix. Print the overall share of diagnostics with a fix (for information) and the share among fixable codes. Exit 1 if the share among fixable codes is below 80%.
- [X] T046 [P] [US2] Create `tests/catalog.py` (FR-024, Principle IV):
  - every `## CODE name` section in `docs/errors.md` has a filled `### Example` and `### Fixed`
  - running `vo check --json` on the Example produces that code, or for R codes, `vo run` on it produces that code. Write each Example to a temp dir; an Example made of several `# file: name.vo`-headed blocks becomes several files there, and the block containing `fn main` (or the first block) is the entry.
  - the Fixed version checks and runs clean
  - every E, W and R code appears in at least one expected `.out`/`.err` under `tests/cases/`; so do T0001 and T0002. T0003 (internal error) is exempt because it cannot be triggered by design.
  - catalog headings and `Fixable:` lines match contracts/error-codes.md one to one

  Exit 1 on any violation.

### Implementation for User Story 2

- [X] T047 [US2] Add the syntax-error fixes to `src/parse.s`:
  - an expected `;` that is missing (the next token on a later line, or any token that cannot continue the statement) emits E0112 with an insertion fix right after the previous token, then continues as if the `;` were present
  - a `,` directly before `)`, `]` or `}` is E0113 with a removal fix
  - a statement starting with an identifier that is not a call is E0114; the shape `ident = expr;` gets the fix `set ident = expr;`
- [X] T048 [US2] Add error recovery to `src/parse.s`. On a syntax error, emit the diagnostic, create an ERROR node, and resynchronize to the next `;` (consumed), a `}` at the current block depth, or the next item keyword (`fn record variant test import`) at depth 0, then keep parsing. Track the stack of open delimiters: at EOF with any still open, emit E0111 for the innermost one, with `related` = the opening token (message "opened here") and a fix inserting the closer at EOF. Report all independent syntax errors from one parse.
- [X] T049 [US2] Suppress cascades in `src/check.s`. Any node that failed to parse or check gets type `t_error`. `t_error` is compatible with every type and produces no further diagnostic (E0301/E0302/E0308/E0401…). An undefined name binds a placeholder Symbol of type `t_error`, so later uses of the same name are not reported again. Check every function, even after errors in earlier ones.
- [X] T050 [US2] Implement `int levenshtein(char *a, long na, char *b, long nb, int max)` in `src/diag.s`, using an arena row buffer and returning max+1 early once the distance exceeds max.
- [X] T051 [US2] Add suggestion fixes to `src/check.s`:
  - **E0201**: the closest in-scope name with distance ≤ 2 becomes a replacement fix plus a `related` "defined here".
  - **E0304**: the same matching against the record's fields.
  - **E0301 with expected str and found int/float**: a fix wrapping the expression in `str(…)`.
  - **E0301 with expected float and found int**: wrap in `float(…)`.
  - **E0401**: one insertion fix before the closing `}` of the match containing every missing `case name(_) { }` (or `case name { }`) arm.
  - **E0404**: insert `try ` when the enclosing fn returns a result.
  - **E0405**: insert `let _ = `.
  - **E0402**: remove the duplicate arm.
  - **E0306**: replace `[]` with `new list[T]` when the annotation is known.
  - **E0207**: the converted name.
- [X] T052 [US2] Implement `vo explain CODE` in `src/main.s`. Validate the format `[EWRT][0-9]{4}`. Find the `## CODE ` heading in `errors_md` and print from it up to, but not including, the next `## ` heading or EOF. An unknown or malformed code is T0001, exit 64. Under `--json`, emit one `explain` record (`code`, `name`, `fixable`, `text` = the section).
- [X] T053 [P] [US2] Fill in `docs/errors.md` sections E0101–E0208. For each: one explanatory paragraph, a minimal `### Example` (vo code that triggers exactly this code) and a `### Fixed` version that checks clean. For multi-file codes (E0204, E0205, E0206), write the Example as several ```` ```vo ```` blocks, each starting with a `# file: name.vo` comment (T046 executes them as one program).
- [X] T054 [US2] Fill in `docs/errors.md` sections E0301–E0410, W0201, W0401, R0001–R0009 and T0001–T0003 in the same format. R examples must fail at runtime with that code; T codes need no executable Example.
- [X] T055 [US2] Make `tests/run.sh tests/cases/check tests/cases/parse tests/cases/cli` and `python3 tests/apply_fixes.py` pass. Fix `src/parse.s`, `src/check.s` and `src/diag.s` as needed, then run `make lint`.

**Checkpoint**: US1 and US2 work: complete diagnostics with fixes, and `explain` for every code.

---

## Phase 5: User Story 3 — Learn the language from one compact reference (Priority: P1)

**Goal**: `vo reference` prints a complete, self-checking reference of at most 15,000 tokens, and every example in it runs.

**Independent Test**: `python3 tests/ref_examples.py` passes, which includes the token budget and the byte-identity of `vo reference` with the doc. The SC-001 agent run is done in T080.

### Tests for User Story 3 (write first, must fail)

- [X] T056 [P] [US3] Create `tests/ref_examples.py` (FR-030, SC-002):
  - assert `bin/vo reference` stdout is byte-identical to `docs/reference.md`
  - extract every ```` ```vo ```` block and handle it by what follows it (blank lines allowed in between):
    - an ```` ```output ```` block: write the code to a temp file, run `bin/vo run`, and compare stdout
    - an ```` ```error E#### ```` block: run `bin/vo check --json` and assert that code appears
    - anything else: the block must pass `vo check`
  - **Multi-file examples**: consecutive `vo` blocks that each start with `# file: name.vo` are written into one temp dir. The block containing `fn main` is the entry, and the expectation after the last block applies.
  - **`vo-test` blocks**: run `bin/vo test` on the block and compare against the following ```` ```output ```` block (human format, `ms` normalized as in T004).
  - **`vo-fmt` blocks**: format the block and require the result to equal the following ```` ```vo ```` block.
  - assert the doc has at least 1 example per top-level `##` section
  - assert `words × 1.3 ≤ 15000` (the agreed token proxy for SC-002)
  - print each failure; exit 1 on any failure
- [X] T057 [P] [US3] Create the golden case `tests/cases/cli/reference_cmd.cmd` = `reference`, with `.code` = 0 and an empty `reference_cmd.skip-out`. The case asserts only the exit status; byte identity with `docs/reference.md` is checked by T056.

### Implementation for User Story 3

- [X] T058 [US3] Implement `vo reference` in `src/main.s`: write `ref_md`/`ref_md_len` to stdout unchanged and exit 0. Under `--json`, emit one `reference` record whose `text` is the document. Add the case `tests/cases/cli/reference_json.cmd` = `reference --json` with `.skip-out`; T056 additionally asserts that the record's `text` equals the doc.
- [X] T059 [US3] Write `docs/reference.md` part 1, taught to a reader who knows no vo. It covers:
  - a 5-line "how to work with vo" preface: the check/run/test/fmt/explain loop, `--json`, and the advice to apply the first fix
  - lexical rules
  - types
  - declarations (fn, record, variant, test, import)
  - statements, with the first-token rule stated explicitly
  - expressions and precedence
  - results, `try` and `match`

  Each section gives 1–3 minimal ```` ```vo ```` examples with ```` ```output ```` blocks, plus at least one ```` ```error E#### ```` counter-example showing the most likely mistake an agent carries over from other languages (e.g. `x = 1;`, `[]`, `if (x) …` is fine but `x == 1 == true` is not).
- [X] T060 [US3] Write `docs/reference.md` part 2:
  - collections (list and map semantics, reference behavior, insertion-ordered `keys`)
  - the builtins table with every signature from contracts/grammar.md §Builtins
  - tests (`test`, `expect`, `expect_eq`)
  - multiple files and imports
  - the CLI commands and flags from contracts/cli.md
  - exit statuses
  - the JSONL record shapes from contracts/output-schema.md (diagnostic, runtime_error, test, summary: field lists, one example each)
  - "every error has a code: run `vo explain CODE`"

  Keep the whole document within the token budget.
- [X] T061 [US3] Rebuild (the docs are embedded via `.incbin`) and make `python3 tests/ref_examples.py`, `python3 tests/catalog.py` and `tests/run.sh tests/cases/cli` pass. Fix the docs where they are wrong and the toolchain where it is wrong, checking which against contracts/.

**Checkpoint**: US1, US2 and US3 are complete. This is the P1 MVP: an agent can learn vo from `vo reference`, write a program, fix it from diagnostics, and run it.

---

## Phase 6: User Story 4 — Verify behavior with built-in tests (Priority: P2)

**Goal**: `vo test` runs `test` blocks with structured results, filtering and per-test timeouts.

**Independent Test**: `bin/vo test --json tests/cases/test/two_pass_one_fail.vo` prints 2 `pass` records, 1 `fail` record with `expected`/`actual`, and exits 1.

### Tests for User Story 4 (write first, must fail)

- [X] T062 [P] [US4] Create test-runner cases in `tests/cases/test/`:
  - `two_pass_one_fail.vo` (code 1; JSONL with a failure carrying `expected`/`actual` and the assertion span)
  - `all_pass.vo` (code 0; also a human-format variant `all_pass_human.vo` with `.cmd` = `test {}`, showing `PASS name (0ms)` lines and the summary line)
  - `values.vo` (`expect_eq` failures on a str, a list, a record and a variant; values rendered in vo literal syntax, e.g. `"abc"`, `[1, 2]`, `new Point { x: 1, y: 2 }`, `Shape.circle(1.0)`)
  - `filter.vo` (`.cmd` = `test --json --filter parse {}`; only matching tests run)
  - `infinite.vo` (`.cmd` = `test --json --timeout 1 {}`; one test loops forever → `timeout`, and the tests after it still run and pass)
  - `runtime_error.vo` (a test dividing by zero → status `error` with failure `code` R0002, and the next test still runs)
  - `duplicate_name.vo` (E0202, code 65)
  - `dir/` containing two files with tests, plus the command-only case `dir_run.cmd` = `test --json {dir}/dir`, so results from both files come in sorted path order. The `dir/*.vo` files have no expectation siblings, so they are helpers, not cases.
  - `expect_outside.vo` (E0410)
  - `no_tests.vo` (summary with 0 tests, code 0)

### Implementation for User Story 4

- [X] T063 [US4] Implement a value renderer in `src/rt_render.s`: `void render_value(sb, long value, type*)` writes vo literal syntax for every type. str is quoted with escapes, list is `[a, b]`, map is `{k: v}` in insertion order (a display form; maps have no literal syntax, contracts/output-schema.md), a record is `new T { f: v, … }`, a variant is `T.case(v)` or `T.case`, a result is `ok(v)`/`err("m")`, and a float uses T038.
- [X] T064 [US4] Implement `expect(cond)` and `expect_eq(actual, expected)` in `src/rt_test.s`, replacing the T039 stubs. On failure, record `{file, start, end, message, expected?, actual?}` (rendered with T063) in the global `test_failure`, then longjmp to `recover_point` (T033).
- [X] T065 [US4] Implement the test runner in `src/testrun.s`. After a clean check, collect the TEST items in each file in source order, and keep those whose name contains `--filter`. For each test:
  - save a recovery point (`sp`, `s0`–`s11`, return address) in `recover_point`
  - set `deadline_ms = now + timeout`
  - evaluate the block in a fresh frame
  - classify the result: `pass`, `fail` (`test_failure` set), `error` (a runtime error, its R code in `failure.code`), or `timeout` (R0009)
  - record `ms`
  - emit the `test` record (contracts/output-schema.md) or a human `PASS`/`FAIL`/`ERROR`/`TIMEOUT name (Nms)` line followed by a failure block

  Clear `recover_point` after the last test. Finish with a summary that adds `passed`, `failed`, `errored` and `timed_out`.
- [X] T066 [US4] Wire `vo test PATH...` in `src/main.s`: expand the paths (T015), check each file (any error → diagnostics, exit 65), run the tests, and exit 1 if any test is not `pass`, else 0. The test program's own stdout and stderr pass through.
- [X] T067 [US4] Make `tests/run.sh tests/cases/test` pass, and re-run `tests/run.sh` in full. Then run `make lint`.

**Checkpoint**: US4 works independently. `vo test` gives agents an objective "done" signal.

---

## Phase 7: User Story 5 — One canonical form for every program (Priority: P2)

**Goal**: `vo fmt` produces exactly one canonical text form, is idempotent, preserves comments, and never changes behavior.

**Independent Test**: `tests/run.sh tests/cases/fmt` and `python3 tests/fmt_corpus.py` pass.

### Tests for User Story 5 (write first, must fail)

- [X] T068 [P] [US5] Create fmt cases in `tests/cases/fmt/`, each a case file with a `.code` = 0 marker and a `.expected.vo`:
  - `messy.vo` (bad indentation, extra spaces, blank lines inside blocks, redundant parentheses, imports out of order)
  - `comments.vo` (comments between items, inside blocks and before `}`)
  - `all_constructs.vo` (every grammar production)
  - `strings_numbers.vo` (escapes kept canonical; `1.50` → `1.5`)

  The canonical style is:
  - 4-space indent and `{` on the same line; `} else {` and `} else if … {`
  - one statement per line
  - no blank lines inside blocks
  - exactly one blank line between top-level items
  - imports first, sorted byte-wise, with no blank lines between them
  - single spaces around binary operators, after `,` and after `:` in params, fields and inits
  - parentheses only where precedence requires them
  - a newline at EOF
- [X] T069 [P] [US5] Create `tests/cases/fmt/check_mode.vo` with `.cmd` = `fmt --check --json {}` on an unformatted file. Expect code 1, a `fmt` record with `"changed":true` and a summary, and the file left unchanged. Also create `tests/cases/fmt/broken.vo` (a syntax error; `.cmd` = `fmt {}`; code 65; diagnostics on stdout; file unchanged).
- [X] T070 [P] [US5] Create `tests/fmt_corpus.py` (SC-005). For every `.vo` under `tests/cases/` and `bench/` that passes `vo check`:
  - copy it to a temp dir and format it twice; the two results must be byte-identical
  - when the file is a `run` case, run both the original and the formatted copy with the case's `.in`/`.cmd`, and require identical stdout, stderr and exit status

  Exit 1 on any difference.

### Implementation for User Story 5

- [X] T071 [US5] Implement the declaration and statement printer in `src/fmt.s` with `str* fmt_file(node* file)`:
  - imports sorted and grouped first
  - items separated by exactly one blank line
  - COMMENT nodes printed on their own line at the current indent
  - types, fn, record, variant and test printed in canonical style (T068)
  - every statement kind printed, with blocks at 4-space indentation
- [X] T072 [US5] Implement the expression printer in `src/fmt.s`. Emit parentheses only when a child's precedence is lower than its context requires, or when an equal-precedence child sits on the right of a left-associative operator. Strings are re-escaped from their decoded value (only `\n \t \\ \"`). Ints print in decimal and floats via `float_to_text` (T038). Print `new` forms, list literals and calls with `, ` separators.
- [X] T073 [US5] Wire `vo fmt [--check] [--json] PATH...` in `src/main.s`. Parse each file; on any syntax error, emit the diagnostics, leave every file unchanged, and exit 65. Otherwise compare `fmt_file` output with the file. Without `--check`, write changed files safely: write `<path>.vo-fmt.tmp`, then `sys_renameat` it over the original, so a crash can never truncate a source file. Emit one `fmt` record per file (`changed`), then a summary with `changed`. With `--check`, exit 1 if any file would change.
- [X] T074 [US5] Make `tests/run.sh tests/cases/fmt` and `python3 tests/fmt_corpus.py` pass, then run `make lint`.

**Checkpoint**: US5 works. Formatting is canonical, idempotent and safe.

---

## Phase 8: User Story 6 — Programs across multiple files (Priority: P3)

**Goal**: Explicit one-name imports resolve relative to the importing file, with cycle detection and missing-import fixes.

**Independent Test**: `bin/vo run tests/cases/multi/app/main.vo` prints the expected output. `bin/vo check --json tests/cases/multi/cycle/a.vo` reports E0206 with the full chain.

### Tests for User Story 6 (write first, must fail)

- [X] T075 [P] [US6] Create multi-file cases under `tests/cases/multi/`:
  - `app/main.vo` + `.out`: imports `geom.Point`, `geom.area` and `text.util.shout` from `app/geom.vo` and `app/text/util.vo`
  - `cycle/a.vo` + `.out` + `.code` 65 (`.cmd` = `check --json {}`): `a` imports from `b`, which imports from `a`; E0206 with `related` listing a → b → a
  - `missing_import/main.vo` (`.cmd` = `check --json {}`): uses `helper()`, which is defined in `missing_import/helpers.vo` but not imported; E0204 with a fix inserting `import helpers.helper;` after the existing imports (or at line 1)
  - `not_found/main.vo`: imports `nope.thing` → E0205; also imports `geom2.missing` from an existing file that lacks that item → E0205 naming the file
  - `error_in_import/main.vo`: the imported file has a type error, and the diagnostic is reported with that file's path

### Implementation for User Story 6

- [X] T076 [US6] Implement the program loader in `src/check.s`, called from `src/main.s`. Starting from the entry file, resolve each `import a.b.name;` to `<dir of importing file>/a/b.vo`; `..` cannot occur because the grammar has no such token. Read, register (`file_add`), lex and parse each file exactly once, keyed by its normalized path. Detect cycles with a DFS stack: E0206 at the import that closes the cycle, with one `related` entry per hop. A missing file or missing item is E0205. Bind each imported item's Symbol into the importing file's scope; a duplicate against a local item is E0202. Check every loaded file, then run the entry `main`.
- [X] T077 [US6] Implement missing-import suggestions in `src/check.s`. When E0201 finds no near-match in scope, scan the `*.vo` files under the importing file's directory (`sys_getdents64`, recursive, sorted), lex and parse each file's top-level item names only, and look for an exact match. If found, emit E0204 instead of E0201, with a fix inserting `import <dotted path>.<name>;\n` after the last import line (or at offset 0).
- [X] T078 [US6] Make `tests/run.sh tests/cases/multi` and `python3 tests/catalog.py` pass (E0204–E0206 now have golden cases), then run `make lint`.

**Checkpoint**: All six user stories work independently.

---

## Phase 9: Polish & Cross-Cutting Concerns

**Purpose**: Full-scale determinism and fuzz runs, performance and large inputs, and the agent benchmark (SC-001, SC-004, SC-006, SC-007, SC-008). The fuzzer itself was created in Phase 2.

- [X] T079 [P] Create `bench/tasks/` with 50 tasks, five groups of 10: text processing, numeric, data structures, parsing, simulation/algorithms. Each `bench/tasks/NN-name/` holds `task.md` (a plain-language task statement that never mentions vo syntax) and `cases/*.in` + `cases/*.out` (at least 3 stdin/stdout pairs). Tasks must be solvable in under 150 lines in either vo or Python.
- [X] T080 [P] Create `bench/run_agent.sh --lang vo|python [--model M]`. For each task:
  - start a headless agent (`claude -p`). The context is the task, plus `bin/vo reference` output for vo or nothing for Python.
  - ask for a single solution file
  - run it against `cases/`, giving up to 3 attempts in total; after a failed attempt, feed back `vo check --json`/`vo run` output (or the Python traceback) together with the failing case diff
  - append `{"task", "lang", "attempts", "solved"}` to `bench/results/<lang>.jsonl`

  Also create `bench/summarize.py`, which reports the first-attempt rate and the rate within 3 attempts (SC-001: ≥70% / ≥95%) and the mean attempts for vo vs Python (SC-008).
- [X] T081 [P] Create `tests/perf.sh` (SC-004). It generates a 1,000-line valid program into `build/tmp/perf.vo`: 50 functions, records, a variant, matches and loops, with `main` calling everything once. It times `bin/vo check` + `bin/vo run` under qemu-riscv64, prints the durations, and fails if their sum is 1 s or more. It also covers the large-input edge case: it generates `build/tmp/big.vo` (10,000 lines) and `build/tmp/wide.vo` (one statement containing a 10,000-character string literal), and requires `vo check` to exit 0 and `vo run` to produce the expected output for both.
- [X] T082 Run `tests/run.sh --repeat 100 tests/cases/run` (SC-006) and `python3 tests/fuzz.py --iterations 5000 --seed 1`. Fix every failure in `src/*.s`, and add a golden case for each fuzz failure under the matching `tests/cases/<dir>/`.
- [X] T083 Verify the deliberate shortcuts are marked: `grep -n 'ponytail:' src/sys.s src/interp.s src/num.s` must show the three markers from T009, T030 and T038 (plan.md Complexity Tracking). Also run `make lint` over all of `src/` with zero unexplained reports.
- [X] T084 Run every quickstart.md scenario verbatim, plus `make test`, `tests/perf.sh` and `bench/run_agent.sh --lang vo` and `--lang python`, then `bench/summarize.py`. Record the results in `specs/001-vo-lang-core/results.md`: test counts, the fuzz iterations, perf timings, and the SC-001/SC-008 numbers. For any SC that is not met, list the gap and the proposed follow-up.

---

## Dependencies & Execution Order

### Phase dependencies

- **Setup (Phase 1)**: none.
- **Foundational (Phase 2)**: needs Setup. Blocks all stories.
- **US1 (Phase 3)**: needs Foundational. It delivers the parser, checker and evaluator that every later story builds on.
- **US2 (Phase 4)**: needs US1 (it extends the parser and checker).
- **US3 (Phase 5)**: needs US1. T059 and T060 can be drafted while US2 is in progress. T061 needs US2 (catalog content) and should come after US4–US6 if the reference documents them; re-run T061 at the end, under T084.
- **US4 (Phase 6)**: needs US1 (T033 `recover_point`).
- **US5 (Phase 7)**: needs US1's parser only (T022–T024) plus T038.
- **US6 (Phase 8)**: needs US1. T077 needs T051 (US2) for the E0201 path.
- **Polish (Phase 9)**: T079–T081 can start once US1 is done. T082–T084 need all stories.

### Within each phase

- Golden cases first, and they must fail.
- `src/defs.inc` additions → parser → checker → evaluator → runtime → wiring.
- Tasks that edit the same `.s` file run in ID order.

### Story-to-story order once US1 is done

US2 → US3 for the P1 MVP. US4, US5 and US6 can then run in parallel, since they touch mostly different files: `testrun.s`, `fmt.s`, and loader code in `check.s`. US6's `check.s` work must not overlap US2's `check.s` tasks.

## Parallel Examples

```text
# Foundational, after T007:
T010 src/text.s   |  T011 src/num.s   |  T012 docs/errors.md + docs/reference.md
# US1 tests:
T018 run feature cases  |  T019 runtime-error cases  |  T020 check cases
# US1 runtime, after T021–T033:
T034 src/rt_io.s  |  T035 src/rt_str.s  |  T036 src/rt_list.s  |  T037 src/rt_map.s
# US2 tests:
T042 | T043 | T044 | T045 | T046
# Catalog content:
T053 E01–E02 sections, then T054 E03–E04/W/R/T sections (same file, sequential)
# After US1–US2:
US4 (T062–T067)  |  US5 (T068–T074)  |  US6 (T075–T078)
# Polish:
T079 bench tasks  |  T080 agent harness  |  T081 perf
```

## Implementation Strategy

### MVP first (P1: US1 → US2 → US3)

1. Phases 1–2: `vo version`, `vo check` on lexical errors.
2. Phase 3 (US1): **stop and validate** with the quickstart US1 scenario. vo can now run real programs.
3. Phase 4 (US2): complete diagnostics with fixes and `explain`.
4. Phase 5 (US3): the reference. **Stop and validate**: run the agent benchmark early (T079–T080) on a 10-task subset to catch reference or diagnostic gaps before building P2/P3.

### Incremental delivery

5. US4 (`vo test`), then US5 (`vo fmt`), then US6 (imports). Each ends at a checkpoint where `make test` is green.
6. Phase 9: the fuzz, determinism, performance and benchmark gates. Results go in `results.md`.

## Notes

- Every task leaves `make` building. Golden cases for later tasks may fail until their implementation lands; that is expected.
- The runtime is split across `src/rt_io.s`, `rt_str.s`, `rt_list.s`, `rt_map.s`, `rt_render.s` and `rt_test.s`. The Makefile assembles every `src/*.s`.
- When a contract and the implementation disagree, fix the implementation unless the contract is wrong. In that case update the contract, `docs/reference.md` and `docs/errors.md` in the same change (Principle V).
