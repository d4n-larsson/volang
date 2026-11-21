---

description: "Task list for vo-lang spec 038: The Toolchain in the Editor (vo lsp)"
---

# Tasks: The Toolchain in the Editor (`vo lsp`)

**Input**: Design documents from `specs/038-vo-lsp/`

**Prerequisites**: plan.md, spec.md, research.md (R1–R12), data-model.md, contracts/lsp.md, quickstart.md

**Tests**: REQUIRED (constitution IV). Each story's cases land in `tests/lsp.py` first and fail before
its code.

**Organization**:
- Setup: wiring, the test client, the results file.
- Foundational: the shared protocol code out of `src/mcp.c`; the command, framing, lifecycle, open
  documents, the copy with overlays and capture, positions.
- US1 (P1) diagnostics and quick fixes; US2 (P1) hover, definition, references; US5 (P1) scheduling,
  limits, robustness; US3 (P2) rename and format; US6 (P2) editors and docs; US4 (P3) document symbols.
- Polish: stress cases, long session, timing, three platforms, results.

## Format: `[ID] [P?] [Story] Description`

## Conventions

- **Everything protocol-visible** (capabilities, methods, error codes, rules, limits):
  contracts/lsp.md. Request-to-command mapping: research R5.
- **The test client**: `tests/lsp.py` starts `bin/vo lsp` with a temporary copy of a small multi-file
  project as the workspace, speaks `Content-Length` framing, keeps the notifications it receives, and
  compares with `bin/vo ... --json` run on a copy of the project holding the same texts on disk. Output:
  `FAIL <case>: ...` per failure and a one-line summary; exit 1 on a failure.
- **No static array over 64 KiB** in any new or moved code (research R8).
- **Unchanged programs**: no existing golden case may change except those that print the list of
  commands, which gain `lsp`.
- **Reference budget**: `python3 tests/ref_examples.py` after the reference change; at most +50 tokens
  (from about 14,803).
- **Between edits**: `make quick AREA=lsp` (T001); `make test` once, through `tools/progress.py run
  test`; the gate only when asked. Never two suites at once.

---

## Phase 1: Setup

- [X] T001 `tools/quick.sh`: area `lsp` (the `cli`, `query`, `edit` cases; scripts `lsp.py`, `mcp.py`);
  files `src/lsp.c`, `src/jsonrpc.c`, `tests/lsp.py`; `src/mcp.c` also selects it. `Makefile`:
  `tests/lsp.py` in the `make test` script list
- [X] T002 `tests/lsp.py`: the client of Conventions: `Server(workspace)`, `request(method, params)`,
  `notify`, `open(path, text)`, `change(path, text)`, `close(path)`, `diagnostics(path, timeout)` (waits
  for the next publish for that file), `direct(args, cwd)`; the fixture: `main.vo` importing
  `lib/shapes.vo` (a record with documented fields, a variant, a documented exported function, a
  function with a fixable error kept in a string for later use), a stand-alone `alone.vo`, and a line
  with non-ASCII text before a diagnostic; position conversion helpers for both encodings; options
  `--session N` and `--time` reserved
- [X] T003 `specs/038-vo-lsp/results.md`: the skeleton (SC-001 to SC-006, three platforms, the gate line)

---

## Phase 2: Foundational (blocks every story)

- [X] T004 `src/jsonrpc.c` (new) and `src/vo.h`: move from `src/mcp.c`, with a `jr_` prefix: the in-place
  reader (`js_ws`, `js_skip`, `js_member`, `js_is`, `js_get`, string decoding into a caller's buffer),
  the buffered writer on a descriptor (`w_raw`, `w`, `w_int`, `w_json_string` with UTF-8 replacement,
  flush with the host-gone flag), `utf8_seq`, `fmt_int`; and `jr_run(fn, arg, input, input_len, out,
  limit, deadline_ms, on_input)`: fork, own process group, die with the parent, pipes on 0/1/2 (and an
  optional fourth for notes to the server), collect output into caller-given buffers with caps, a
  deadline, a callback when the server's stdin is readable (returns "stop it"), kill and reap; returns
  the status and why it ended. `src/mcp.c` uses them. `python3 tests/mcp.py` passes unchanged
- [X] T005 `tests/lsp.py`: lifecycle cases: `initialize` answers the contract's capabilities exactly
  (`positionEncoding` `utf-16` by default, `utf-8` when offered) and `serverInfo`; a request before
  `initialize` is `-32002`; `shutdown` then `exit` gives status 0, `exit` alone 1, end of input 1; an
  unknown request `-32601`, an unknown notification nothing; `tests/cases/cli/lsp_args.cmd` (`lsp x`):
  T0001. All fail
- [X] T006 `src/main.c`, `src/vo.h`: `lsp` in `cmd_names` with `CMD_LSP`, dispatched like `mcp`
  (`argc != 2` is T0001 "`lsp` takes no arguments: it serves the Language Server Protocol on standard
  input and output."); `lsp` in the two messages that list commands
- [X] T007 `src/lsp.c`: framing (headers to the blank line, `Content-Length`, a body mapped for the
  message and unmapped after it, 64 MiB limit, malformed headers end the session with status 1);
  dispatch; `initialize` (workspace folder from `workspaceFolders[0].uri` or `rootUri`; `utf-8` when
  `general.positionEncodings` lists it), `initialized`, `shutdown`, `exit`; error answers. T005 passes
- [X] T008 `src/lsp.c`: open documents per data-model.md: a table grown by remapping; `didOpen`,
  `didChange` (the last full `text` in `contentChanges`), `didClose`, `didSave`; `file://` addresses to
  paths with percent-decoding (others ignored); each text in its own mapping, unmapped on replacement
  or close
- [X] T009 `src/sys.c`, `src/main.c`, `src/effects_record.c`, `src/vo.h`: the capture hooks of research
  R4: `out_capture` (output for descriptors 1 and 2 appended to a buffer), `lsp_finish` called by
  `vo_exit` before the flush, `write_capture` in `write_file_safely` and in the function that writes
  `vo.effects` (the text kept, nothing written). No behaviour changes when the hooks are unset: `make
  quick AREA=cli` and the `fmt`, `edit`, `effects` cases pass
- [X] T010 `src/lsp.c`: the copy: `in_copy(entry, words...)`: change directory to the entry's directory,
  `file_add` every open document under it with the path form the loader builds (verify the forms and
  the notes of research R3 with a two-file program whose imported file is open and differs from the
  disk), set the hooks, call `vo_main`; `lsp_finish` hands the captured JSON lines to the request's
  translator and writes framed messages to descriptor 1
- [X] T011 `src/lsp.c` and `tests/lsp.py`: positions per research R6: establish the lexer's column unit
  with a non-ASCII case and record it in the contract; `to_protocol(file, line, col)` and
  `to_toolchain(file, line, character)` for both encodings, `\r\n` lines, clamping past the end; the
  Python twins in the test; a table of mixed-width lines agrees in both directions

**Checkpoint**: a copy can run `check --json` on unsaved text and hand its records to a translator.

---

## Phase 3: User Story 1 - Mistakes shown as you type, with their fixes (P1) 🎯 MVP

**Independent Test**: `python3 tests/lsp.py` diagnostics and quick-fix cases.

- [X] T012 [US1] `tests/lsp.py`: diagnostics equal `vo check --json` (range converted, severity, code,
  message, `codeDescription.href`, `source`) for: the fixture as saved; an unsaved change that adds an
  error; one that removes it (an empty list); the non-ASCII line in both encodings; a syntax error
  (only syntax diagnostics, then the rest when fixed); a warning; `alone.vo` as its own program; a file
  that exists only in the editor. Fails
- [X] T013 [US1] `tests/lsp.py`: multi-file: with `main.vo` and `lib/shapes.vo` open, a change in
  `lib/shapes.vo` that breaks `main.vo` publishes for `main.vo`; a mistake in the unopened
  `lib/shapes.vo` is published for it and cleared when fixed on disk and `main.vo` is saved; closing a
  document clears its diagnostics. Fails
- [X] T014 [US1] `src/lsp.c`: the entry of a document per research R7 (candidates from the nearest
  `main.vo` upward within the workspace; the copy reports on its notes pipe whether the document was
  loaded; the result kept with the document); the diagnostics run: `check --json ENTRY` in a copy,
  `diagnostic` records grouped by file into `textDocument/publishDiagnostics` (with the document's
  `version` for open files), empty lists for open files of the program without any, and the published
  set per entry to clear files that no longer have any. Triggered directly on open, change, save and
  close for now (scheduling is T022). T012 and T013 pass
- [X] T015 [US1] `tests/lsp.py`: quick fixes: for every diagnostic with fixes in the fixture, the
  `codeAction` answer at its range has one action per fix (title, kind `quickfix`, the diagnostic), and
  applying its edit to the editor's text equals applying the toolchain's fix; a range with no fixable
  diagnostic and a stale range give `[]`. Fails
- [X] T016 [US1] `src/lsp.c`: `textDocument/codeAction`: the check again in a copy; fixes of diagnostics
  whose range touches the asked range → `CodeAction[]` with `edit.changes`. T015 passes

**Checkpoint**: an editor shows vo's diagnostics and applies its fixes.

---

## Phase 4: User Story 2 - Know what a name is, and go to it (P1)

**Independent Test**: `python3 tests/lsp.py` hover, definition and references cases.

- [X] T017 [US2] `tests/lsp.py`: for a list of positions (a call of the documented function from another
  file, a record field, a variant case, a local, a builtin, a standard-library function, white space, a
  comment): hover equals the `type_at` record's type, signature and description (as Markdown per R5)
  and range; definition equals `declaration` (none for the builtin and the library item); references
  equal `vo query refs` with and without the declaration; all three again after an unsaved edit that
  moves the lines. Fails
- [X] T018 [US2] `src/lsp.c`: `textDocument/hover`, `definition` (both from `query --json type-at`) and
  `references` (`query --json refs ENTRY FILE:L:C`), translated per research R5; a `toolchain_error`
  gives `null` or `[]`. T017 passes

---

## Phase 5: User Story 5 - A session that survives everything (P1)

**Independent Test**: `python3 tests/lsp.py` robustness cases.

- [X] T019 [US5] `tests/lsp.py`: a burst of 20 changes 5 ms apart publishes diagnostics for the last
  text only (versions never go backwards, and the last publish has the last version); a hover sent
  during the pause is answered for the current text without waiting for the check. Fails
- [X] T020 [US5] `tests/lsp.py`: robustness, each followed by a hover that must succeed: a document of
  random bytes; 5,000 nested parentheses; positions far past the end; requests for an unopened file, a
  missing file and a non-`file` address; a body that is not JSON (`-32700`); a batch (`-32600`);
  parameters of the wrong shape (`-32602`); a frame without `Content-Length`; `$/cancelRequest` for a
  running request (`-32800`) and for an unknown id (ignored); a copy over its deadline (`-32803`, by a
  test hook `VO_LSP_TEST_SLOW=1` that makes one named request sleep, if no natural slow input is found);
  the client closing its end mid-request (the server ends, no process left in the workspace). Fails
  where not yet true
- [X] T021 [US5] `tests/lsp.py`: no file is written and no program run: after the whole test, every file
  of the workspace has its starting bytes and time stamps, and a `main` that would create a file has not
  created it. Fails only if violated
- [X] T022 [US5] `src/lsp.c`: scheduling per research R7: the 150 ms pause (the wait for input with a
  timeout), entries of all open documents with the changed one first, a change during a check kills its
  copy, requests answered in order between checks; the 5 s deadline; `$/cancelRequest` through
  `jr_run`'s input callback (later messages stay queued in the frame reader); one `window/showMessage`
  per session when a check is cut off. T019 and T020 pass

---

## Phase 6: User Story 3 - Change code safely: rename and format (P2)

**Independent Test**: `python3 tests/lsp.py` rename and format cases.

- [X] T023 [US3] `tests/lsp.py`: `prepareRename` on a function, a field, a local (their ranges), on a
  builtin and on white space (`null`); `rename` of an exported function used in two files: the edits
  applied to the editor's texts equal the files `vo edit rename` writes in a copy of the project,
  `vo.effects` included; with an unsaved edit in one file; a refused rename (an existing name; a wrong
  spelling) is `-32803` with the toolchain's message; `formatting` of a non-canonical buffer applied
  equals `vo fmt` on a copy, `[]` for a canonical one and for one with a syntax error. Fails
- [X] T024 [US3] `src/lsp.c`: `textDocument/prepareRename`, `rename` (`edit --json --dry-run rename`),
  `formatting` (`fmt --json FILE` with the write captured; one `TextEdit` over the lines that differ).
  T023 passes

---

## Phase 7: User Story 6 - It starts by itself, and is documented (P2)

- [X] T025 [P] [US6] `editors/vscode/extension.js` and `package.json` per research R10 (`main`,
  `activationEvents: ["onLanguage:vo"]`, setting `vo.path`, dependency `vscode-languageclient`, its
  lock file); `editors/vscode/.vscodeignore` not needed (nothing is packaged); `tests/grammars.py`'s
  check of the folder still passes; `.gitignore`: `/editors/vscode/node_modules/`
- [X] T026 [P] [US6] `site/docs/tools/lsp.md` (new) per research R11 and `site/docs/tools/editors.md`:
  the start lines for VS Code (`npm install` in the folder), Neovim, Helix, and Zed as research R10
  decides after reading Zed's documentation; `tools/site.py` lists the page
- [X] T027 [US6] `docs/reference.md` (the command list and one bullet; `python3 tests/ref_examples.py`
  under 15,000), `docs/roadmap.md`, `README.md`, `CHANGELOG.md`, `docs/review-2026-11/README.md`;
  re-accept the golden cases that print the command list after reading each diff

---

## Phase 8: User Story 4 - The outline of a file (P3)

- [X] T028 [US4] `tests/lsp.py`: `documentSymbol` of `lib/shapes.vo` lists its items in order with the
  kinds of research R5, names, ranges inside the file, records and variants with their members as
  children, a test with its text as the name; a file with a syntax error in its last item lists the
  earlier ones. Fails
- [X] T029 [US4] `src/lsp.c`: `textDocument/documentSymbol`: in a copy, `load_file` and a walk over the
  file's top-level nodes. T028 passes

---

## Phase 9: Polish

- [X] T030 `python3 tests/mcp.py`; `tests/run.sh --stress tests/cases/run/oom_small.vo` and
  `tests/cases/map_keys` (no new static buffers: compare `size build/vo` bss with the commit before);
  `make site-check` once; `make test` once through `tools/progress.py run test`
- [X] T031 `tests/lsp.py --session 5000` (SC-004) and `--time` (SC-003: a 2,000-line file in a 20-file
  program generated by the test); record
- [X] T032 `VO_BIN=build-aarch64/vo` and `VO_BIN=build-riscv64/vo python3 tests/lsp.py`; record
- [ ] T033 SC-005 by hand in an editor if one is available; otherwise recorded as not done with the
  steps of quickstart.md left for the user
- [X] T034 `specs/038-vo-lsp/results.md` completed; spec status "Implemented (gate not run)"

---

## Dependencies

- Setup → Foundational → US1. US2, US3 and US4 need only Foundational. US5's scheduling builds on US1's
  diagnostics run. US6 needs the features it documents. Polish last.
- Within a story: the test task before the code task.

## Parallel opportunities

- T025 and T026 (different files, no C). The C work is in two files and is sequential.

## Implementation strategy

MVP is Foundational plus US1: diagnostics and fixes on unsaved text. US2 adds reading, US5 makes it
safe to leave running, US3 adds the two edits, US6 lets editors start it, US4 is the outline.
