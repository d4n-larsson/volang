# Results: Module Imports

## Setup

- T003 baseline (commit `6a6e86e`, x86-64): import lines in `examples/*/*.vo` 200, in `std/*.vo` 83, at
  most 48 in one file (`examples/notes-api/api.vo`; also `examples/secure-notes/notes.vo`). `vo check`,
  best of 5: `examples/notes-api/main.vo` 8 ms, `examples/secure-notes/main.vo` 15 ms, the 8,000-item
  program of `tests/perf.sh` 159 ms.

## SC-001

Met. After migration (`tests/migrate_058.py apply`, `tools/vo_fix.vo --only E0215`): import lines in
`examples/*/*.vo` 52 (from 200), in `std/*.vo` 20 (from 83): 72 in all, 75% fewer. At most 8 in one file
(`std/web.vo`); the examples at most 7 (`examples/notes-api/api.vo`, `examples/secure-notes/notes.vo`, from
48). Every example's tests give the same results before and after (expenses 5, helpdesk 5, job-queue 3,
notes-api 10, rates 4, release-check 3, secure-notes 8, status-board 4, url-check 2 passed).

## SC-002

Met on x86-64 (ARM64 and RISC-V: see Verification). E0215's fix rewrote every program in one round:
`std/` (7 files), `packages/postgres` (8), the examples (9 programs), `tools/vo_fix.vo`, the benchmark
tasks and fixtures (195 fixes), 610 golden-case sources (`tests/migrate_058.py apply`), the reference, the
catalog and the guide. `tests/migrate_058.py compare` ran the 573 migrated cases against their 0.3 expected
output: 532 equal after normalizing positions and module names; the 41 others were read one by one: 31
step cases differ in hashes, positions and the import line only; the rest test import mistakes and were
rewritten for the module form (`check/std_internal`, `math/name_clash`, `const/files_dup`,
`const/files_missing`, `multi/not_found`), or found defects fixed in this spec (below). `tests/migrate.py`
(61 of 61, now 4 rounds with E0215) and `tests/migrate_056.py` (11 programs, 0 problems) pass. Every
example's tests give the same results before and after.

Defects the migration found and fixed: a qualified type as a type argument (`workers.Worker[jobs.Job, ...]`)
read as an effect; a statement starting with `bytes.` (the keyword module); E0509 instead of E0209 for a
module's private effect set; W0506's fix needing an import; a module's file named by its absolute path in
`vo doc`; a package file checked as an entry getting E0215 under a second path (the fix applied twice);
uses of a module whose import failed reported again.

## SC-003

Met. New codes E0215 (`tests/cases/imports_v03/`: single, mixed, shadow, renamed_056, text_output and a
15-program corpus) and W0203 (`imports/unused`); widened E0201 (`imports/bare_use`, `no_item`,
`module_value`, `builtin_qualified`, `const/files_missing`), E0202 (`imports/clash_modules`, `clash_item`,
`repeat`), E0203 (`imports/shadow_local`), E0204 (`imports/missing_module`, `missing_bare`), E0205
(`imports/bad_path`), E0209 (`imports/private_item`, `check/std_internal`, `effect_sets/import/private`),
E0214 (`library/qualified_change`). Each has its catalog entry or second paragraph; `tests/catalog.py`
161 sections, 0 problems; `tests/apply_fixes.py` 257 of 257 fixes resolve their diagnostic.

## SC-004

Met. Every `vo query`, `vo edit`, `vo doc` and `vo lsp` case gives the same answers after migration, up to
positions, qualified names and the `import` role of 0.3 imports (`tests/cases/query`, `edit`, `doc`, 43
step cases; `tests/lsp.py` 10 groups, 0 failures, with hover and definition on a module name added).

## SC-005

Met: ~14,987 tokens of 15,000 before and after (`tests/ref_examples.py`). The import examples got shorter
(7 import lines became 5) and paid for the sentence on module names and locals; nothing moved to the
site.

## SC-006

Met. `vo check`, 150 runs alternating 0.3.0 (a build of HEAD) and 0.4.0, fastest of each:
`examples/notes-api` 6.43 ms and 6.27 ms (-2.5%), `examples/secure-notes` 7.38 ms and 7.55 ms (+2.3%); the
8,000-item program of `tests/perf.sh` 153 ms and 149 ms (-2.8%). A first, shorter measurement gave +10.6%
for notes-api; alternating runs showed it was noise (medians 11.95 and 12.05 ms).

## Verification

- x86-64: `make test` passed (golden cases, step cases and every script of the target), after the
  migration and every fix above. The scripts outside `make test` that embed vo programs (bytes_scale,
  format_speed, fs_scale, https_scale, math_speed, regex_speed, serve_endurance, sleep_check, sqlite_speed,
  tasks_determinism, tls_fuzz, web_speed) were run once each and pass.
- ARM64 (`make ARCH=aarch64 test`, qemu): passed.
- RISC-V 64 (`make ARCH=riscv64 test`, qemu): one doc example failed its 10 s run limit: std.crypto's
  password example (three PBKDF2 runs of 600,000 rounds). The C code is the same; the larger binary moved
  `sha256_compress` across a 4 KiB page (offset 3476, 780 bytes), and qemu does not chain translated code
  across pages, so that loop runs about twice as slow under emulation (0.19 s and 0.32 s for 60,000 hashes
  with the 0.3.0 and 0.4.0 builds; native x86-64 is unchanged: 0.52 s and 0.51 s for one hash_password).
  `tests/doc_examples.py` now gives programs 100 s under emulation, as other scripts scale their limits
  there (SLOW). With that, `make ARCH=riscv64 test` passed.
- `tests/fuzz.py --iterations 1000 --seed 7`: 0 failures. `tests/catalog.py`: 161 sections, 0 problems.
  `tests/apply_fixes.py`: 257 of 257. `tests/grammars.py`: 7 checks, 0 failures.
- `make gate` was not run (it is run only when asked).

## Deviations

- **No `NF_QUAL` flag** (data-model): node flags are 16 bits and only one was free. A qualified name is
  recognized by the `.` in its text, which no other name contains.
- **E0203 for a local named like a module** reports at the declaration and at each use, each with the
  rename (the mechanism spec 056 built for builtins), instead of one edit over the function. The E0215
  fix still renames such locals in its single edit.
- **The import line is not a reference** of the items used through it: `vo query refs` lists no
  `import` role for module imports (research R8 listed one). Logging every exported item at the import
  made `vo edit rename` rename the module name in the import line.
- **E0215 only in program files.** A std or package file in the 0.3 form still checks: a program cannot
  change a pinned package, so its author migrates it. `std/` and `packages/postgres` were migrated.
- **Library files were migrated in a copy**: `vo check` of a copy outside `std/` (where E0215 applies),
  the package in a copy with a `postgres -> .` link so its own imports resolve; then copied back.
- **Programs inside test scripts and step files** (and package programs that need their manifest) were
  migrated by text (`tests/migrate_058.py text|py`), checked by running them; `tests/postgres.py` puts its
  per-item programs into the module form at run time with the same function.
- **The W0506 fix** for the only field of a log record is now `[]` (spec 056 lets it take its type from
  the parameter): the old fix inserted `new list[Field]` and an import, which the module form cannot
  write as one edit in the right place.
- **Messages of an import that finds no file** name the module's file (`money/format.vo`), and a std
  path of three parts is read as the 0.3 form (std modules are flat).
