# Results: Filesystem and Processes

## Success criteria

| SC | Status | Evidence |
|---|---|---|
| SC-001 | met | `tests/fs_scale.py`: a program walks a tree of 1,000 files in 350 directories (`walk`), reads each file and writes a summary; 20 runs give identical stdout, stderr, exit status and summary file (0.05 s per run). The same program prints the same summary on x86-64, ARM64 and RISC-V. The fixture-tree step cases (`tests/cases/fs_steps/`) pass on all three platforms. |
| SC-002 | met | `examples/release-check` runs three git checks, inspects their exit codes and output, and writes a report. It is 73 lines without its tests; an equivalent Python script is 49, a ratio of 1.49 (limit 1.5). |
| SC-003 | met | Every operation refuses paths and programs outside its grant: E0503 for literals (`fs/literal_outside`, `fs/rename_literals`, `process/literal_outside`) and R0010 at run time (`fs_steps/outside`, `fs_steps/changes_outside`, `process/exact_text`, the scope directory itself in `fs_steps/delete` and `fs_steps/rename`). Link-swap race: `tests/security/fs_race.py` flips a directory for a link to outside while `append_file`, `make_dir`, `rename`, `delete` and `list_dir` run in a loop; 300 runs, nothing outside the scope listed, created, changed or deleted (part of `make security-check`). |
| SC-004 | met | `tests/fs_scale.py`: 100,000 entries listed in 0.33-0.35 s (limit 1 s); 1,000 runs of `true` in 1.06-1.60 s (1.1-1.6 ms per run: one fork and one exec; limit 5 s); 0 processes left; a waiting program has 4 open descriptors before and after 20 rounds of listings and runs. |
| SC-005 | met | `fs_steps/record_replay` records info, append, make_dir, rename, delete (an err), a listing and a run, then removes the tree and the helpers: `vo replay` gives identical output with nothing created. `tests/cross_replay.py`: 6 platform pairs, 19 cases each (the replay cases, the task recording and this one), all identical. `examples/release-check/recordings/demo.jsonl` replays without git. |
| SC-006 | met | `process_steps/review_process` (a function now runs `make` too: E0504 "`release` gains process("make")" with the call) and `fs_steps/review_delete` (a function that only appended now deletes: E0504 "gains file.delete"); `fs_steps/review_read_only` and `process_steps/effects_listing` show `vo effects` lists exactly the grant. |
| SC-007 | met | Reference: about 11,687 tokens, +294 (limit +500, budget 15,000). Every existing golden and step case unchanged except E0509's message (Behavior changes); the full gate passes on x86-64, `make test` on ARM64 and RISC-V. |

## Verification

| Check | Result |
|---|---|
| `make test` (x86-64) | passed: golden 582, step 210 + 189 + 35 repeated, catalog 105 sections, reference 36 examples, site 44 pages / 187 examples, doc conformance (92 library items, 92 examples), fuzzers, examples (release-check tests and replay), diff-engines 1,000 (0 disagreements) |
| `make gate` (x86-64) | passed: lint; test; ubtrap 582; diff-asm 2,000 (0 disagreements); tasks determinism (20 runs); files and processes determinism (new: 20 runs of the 14 golden and 33 step cases, all identical); gc-check (stress golden 582, stress diff-engines 1,000: 0 disagreements); speed geometric mean 0.875 (limit 1.10) |
| `make instrumented` | 582 of 582 under ASan and UBSan; the fs, process and security step cases (49) also pass with `VO_BIN=build-instrumented/vo` |
| `make security-check` | passed: `fs_race.py` 300 runs, 0 escapes; the spec 013 races (1,000 runs each), package race and server attacks unchanged |
| ARM64, RISC-V | `make ARCH=aarch64 test` and `make ARCH=riscv64 test` (qemu): golden 582, step 210 + 189 + 35, catalog, reference, site, doc conformance, fuzzers, diff-engines 1,000 (0 disagreements) on both |
| `tests/cross_replay.py` | 6 platform pairs, 19 cases each, all identical |
| `tests/fs_scale.py` | as SC-001 and SC-004 |

## Behavior changes

- **Two effects** in the canonical order: console, input, file.read, file.write, **file.delete**, net,
  clock, random, env, serve, **process**. The bit values behind them were renumbered; nothing stores
  them, so no file or output changes. E0509's message lists the eleven effects, and the existing
  `effects/unknown_effect` case was updated.
- **New code** E0512 `process_unscoped`; E0506 has a program-scope message; E0503, E0501, E0504, R0007,
  R0009, R0010 and R0014 have new triggers (contracts/codes.md).
- **Two library modules**, `std.fs` and `std.process`, over two std-only primitives (`__fs`, `__run`);
  `vo doc` lists them (`doc/all` cases updated).
- **Recording schema 1** gains seven event kinds (`info`, `list_dir`, `append_file`, `make_dir`,
  `delete`, `rename`, `run`); earlier recordings replay as before.
- **`docs/security.md`**: the boundary table, S2, S3 (39 system calls; processes only for tasks and
  `process`) and the known limits (granted programs are not confined, escaped grandchildren); the stale
  spec 016 lines (S9, "one request at a time") are corrected.
- **Test tooling**: `tests/steps.py` gains `helpers` and `touch`; `tests/process_helpers/` holds the
  helper scripts; `tests/doc_conformance.py` scopes `process` and knows the new order; the gate runs the
  new cases 20 times.

## Deviations

- **Absolute program paths** are allowed in `process` scopes (FR-011, amended in planning).
- **`__run` takes three arguments** (`argv`, `input`, `env`, with the program as `argv[0]`): builtins
  get at most three argument values.
- **Deleting and renaming do not follow the last path component**, so deleting a link inside the scope
  that points outside is allowed and removes the link (found by `fs_steps/delete`). The research had
  every path resolved like `read_file`'s; for `file.delete` that refused such links.
- **A leading `./` is dropped only while a `/` remains**: `"./git"` (a file in the start directory)
  stays apart from `"git"` (looked up through `PATH`). Found by `process/literal_outside`.
- **The child closes every descriptor above 2** (`close_range`, with a loop as fallback), not only
  relying on close-on-exec: `vo` can inherit descriptors from whoever started it without the flag.
  Found by `process_steps/fds`. The error pipe moves to 3 first, so a failed exec still reports its
  errno (found by `process_steps/not_executable`).
- **`rename` refuses moving a directory into itself** with its own message ("cannot move a directory
  into itself") instead of the kernel's EINVAL, which otherwise reads as "not supported on this file
  system".
- **No `fchdir`**: `vo` never changes its working directory, so the child already starts in the start
  directory (`SYS_fchdir` is defined but unused).
