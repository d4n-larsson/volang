---

description: "Task list for vo-lang spec 037: Editor Grammars"
---

# Tasks: Editor Grammars

**Input**: Design documents from `specs/037-editor-grammars/`

**Prerequisites**: plan.md, spec.md, research.md (R1–R10), data-model.md, contracts/grammars.md,
quickstart.md

**Tests**: REQUIRED (constitution IV). Each check lands in `tests/grammars.py` first and fails (or
skips, for a grammar that does not exist yet: then it must fail under `VO_GRAMMARS=require`) before the
grammar it checks.

**Organization**:
- Setup: the tools, the check's skeleton, wiring.
- Foundational: the word lists from the reference and the toolchain (US3's core, needed by every
  grammar).
- US2 (P1) the tree-sitter grammar comes before US1 (P1) the TextMate grammar, because US1's agreement
  check reads the tree-sitter tokens; US3 (P1) the remaining drift checks and CI; US4 (P3) Vim, the
  editor folders and the site page.
- Polish: the drift experiment, timing, `make test`, results.

## Format: `[ID] [P?] [Story] Description`

## Conventions

- **Scopes, captures, groups**: data-model.md's table; file paths and the check's rules and messages:
  contracts/grammars.md.
- **The language**: `src/parse.c` (rules and `tok_spell`), `src/lex.c`, the lexical and grammar sections
  of `docs/reference.md`. The toolchain, the reference and existing test cases are not changed (FR-013).
- **Corpus**: every `*.vo` under `tests/cases`, `examples`, `std`; "parses" = `bin/vo fmt --check FILE`
  exits 0 or 1.
- **Tools**: `cd editors && npm ci` once; everything node-side runs from `editors/`.
- **Between edits**: `make quick AREA=grammars` (T003); `make test` once, through
  `tools/progress.py run test`; the gate only when asked. Never two suites at once.

---

## Phase 1: Setup

- [X] T001 `editors/package.json` (private; devDependencies `tree-sitter-cli`, `vscode-textmate`,
  `vscode-oniguruma`, exact versions) and `editors/package-lock.json` by `npm install`; `.gitignore`:
  `/editors/node_modules/` and the tree-sitter build outputs (`/editors/tree-sitter-vo/build/`,
  `*.so`, `*.wasm` there)
- [X] T002 `tests/grammars.py`: the skeleton per contracts/grammars.md: named checks, each returning
  pass, fail (with lines) or skip (with the reason); `VO_GRAMMARS=require` turns a skip into a failure;
  the last line `grammars: N checks, S skipped, F failures`; exit 1 on a failure
- [X] T003 `Makefile`: `tests/grammars.py` in the `make test` script list; a target `grammars`
  (`tree-sitter generate --abi 14` in `editors/tree-sitter-vo`, then the hash of `grammar.js` into
  `src/grammar.sha256`); `tools/quick.sh`: area `grammars` (script `grammars.py`; files under
  `editors/`, `tests/grammars.py`, `site/pygments_vo/pygments_vo.py`)
- [X] T004 `specs/037-editor-grammars/results.md`: the skeleton (SC-001 to SC-006, the gate line)

---

## Phase 2: Foundational (blocks every story)

- [X] T005 `tests/grammars.py`: `language_lists()` per research R1: keywords from the "Keywords" line of
  `docs/reference.md`, compared first with the keyword entries of `tok_spell` in `src/parse.c` (a
  difference between the two is itself a failure naming the word); builtin types (the type keywords
  plus the collection types of the reference); contextual words; effects from the first column of the
  reference's effects table; literals. Print the lists with `--lists`
- [X] T006 `tests/grammars.py`: the `word lists` check with one extractor per grammar file (a file that
  does not exist yet is "missing", a failure under require and a skip otherwise): the Pygments lexer's
  tuples now; it fails on the stale `EFFECTS`. Fix `site/pygments_vo/pygments_vo.py` (effects, and any
  other list the check reports) and keep `tests/site_lexer.py` passing (`make site-check` at the end
  of the spec, once)

**Checkpoint**: `python3 tests/grammars.py` passes with the one existing highlighter in the check.

---

## Phase 3: User Story 2 - Zed, Neovim and Helix: the tree-sitter grammar (P1)

**Goal**: a grammar that parses everything the toolchain parses, with highlight queries.

**Independent Test**: the `tree-sitter corpus` and `highlight tests` checks pass.

- [X] T007 [US2] `tests/grammars.py`: the `tree-sitter corpus` check per research R4: split the corpus
  with `bin/vo fmt --check` (in parallel, cached per run), then `tree-sitter parse --quiet` over the
  "parses" set in batches: any `ERROR` or `MISSING` is a failure naming the file and position (first 20
  shown, then a count); the "rejected" set must only end (a 10 s limit per batch). Skips without
  `editors/node_modules`. Fails now (no grammar)
- [X] T008 [US2] `editors/tree-sitter-vo/grammar.js` and `tree-sitter.json`: the lexical part (comment as
  extra, `identifier` and `type_identifier` as separate tokens, `integer`, `float`, `string` with
  `escape_sequence`, booleans, the keyword and type lists as arrays at the top of the file in the form
  T006's extractor reads) and the top level: imports, exports, `const`, `fn` (generics with constraints,
  parameters, result, `uses_clause` with scoped effects, `requires`/`ensures`), `record`, `variant`,
  `test`, `prop`, annotations
- [X] T009 [US2] `editors/tree-sitter-vo/grammar.js`: types (builtin, generic `name[...]`, function types,
  `result[T, E]`), statements (`let var set if else while for in match case return break continue try`,
  call statements, blocks) and expressions with the toolchain's precedence (`or`, `and`, `not`,
  comparison, range `..`, additive, multiplicative, unary `-`, postfix call, index, field; `new` records,
  lists and maps; `match` patterns; lambdas or function values if the language has them; holes). Follow
  `src/parse.c` function by function
- [X] T010 [US2] Iterate T008–T009 against T007 until the "parses" set has no error node; `make grammars`;
  commit `src/parser.c`, `src/grammar.json`, `src/node-types.json`, `src/tree_sitter/`,
  `src/grammar.sha256`. Record in results.md any construct found only through the corpus
- [X] T011 [US2] `editors/tree-sitter-vo/test/highlight/*.vo`: one small file per group (declarations,
  types and effects, statements, literals and comments, annotations) with the expected capture under
  each token, per data-model.md; the `highlight tests` check in `tests/grammars.py` (`tree-sitter
  test`). Fails
- [X] T012 [US2] `editors/tree-sitter-vo/queries/highlights.scm` with the captures of research R3;
  `locals.scm`; `indents.scm`; `injections.scm` (comments only). T011 passes

**Checkpoint**: `npx tree-sitter highlight` colours an example; the corpus is clean.

---

## Phase 4: User Story 1 - VS Code and GitHub: the TextMate grammar (P1)

**Goal**: a line-oriented TextMate grammar and the VS Code folder.

**Independent Test**: the `TextMate agreement` check passes.

- [X] T013 [US1] `editors/check.js` and the `TextMate agreement` check in `tests/grammars.py` per
  research R5: for each file of the "parses" set, tokenize with `vscode-textmate` and read the
  tree-sitter leaves (the CLI's output with positions); for every leaf of a checked class in
  data-model.md, the TextMate scope at that position must start with the class's prefix; no `invalid.`
  scope in such a file. Output: file, line, column, the token, expected and actual scope (first 20,
  then a count). Fails (no grammar)
- [X] T014 [US1] `editors/vscode/syntaxes/vo.tmLanguage.json` per research R2: scope `source.vo`; a
  repository of patterns (comment, string with escapes and `invalid.illegal.escape.vo`, float before
  integer with `..` not taken as a decimal point, booleans, annotation, `fn` with the declared name,
  `record`/`variant`/`import`/`const`/`test`/`prop` with their names, the `uses` clause up to `{`, `;`,
  `requires` or `ensures` on its line, keyword groups, builtin types, `PascalCase` type names, calls,
  operators, punctuation); no pattern spans a line. The word alternations in the form T006's extractor
  reads
- [X] T015 [US1] Iterate T014 against T013 until the agreement check passes on the whole "parses" set
- [X] T016 [P] [US1] `editors/vscode/package.json` (name `vo`, language id `vo`, extension `.vo`, the
  grammar, no code, licence MIT) and `language-configuration.json` (line comment `#`; brackets `{}`,
  `[]`, `()`; auto-closing and surrounding pairs including `"`; `increaseIndentPattern` after an opening
  bracket at the end of a line, `decreaseIndentPattern` for a closing one); the check validates both as
  JSON and that the paths they name exist
- [X] T017 [US1] FR-005 cases in `editors/check.js`: a file with an unclosed string on one line and a
  half-typed `fn` on another: with TextMate the next line's tokens have their normal scopes; with
  tree-sitter the items after the broken one are still `function_definition` nodes

**Checkpoint**: both grammars agree on every token of every file that parses.

---

## Phase 5: User Story 3 - The grammars cannot drift (P1)

**Independent Test**: quickstart.md "The drift experiment".

- [X] T018 [US3] `tests/grammars.py`: extractors for `grammar.js` and `vo.tmLanguage.json` in the
  `word lists` check (they were "missing" until now); the `grammar hash` check (`src/grammar.sha256`
  against `grammar.js`, no tools needed); the `generated parser` check (regenerate into a temporary
  directory with ABI 14 and compare every generated file byte for byte; the message says `make grammars`)
- [X] T019 [US3] `.github/workflows/gate.yml`: `actions/setup-node` with the version of
  `editors/package.json`'s `engines`, `npm ci` in `editors/`, and `VO_GRAMMARS=require` in the
  environment of the test step; the same in `.github/workflows/release.yml`'s test step
- [X] T020 [US3] Verify the skip path: with `editors/node_modules` moved aside, `python3
  tests/grammars.py` exits 0 and names each skipped check; with `VO_GRAMMARS=require` it exits 1.
  Record both outputs in results.md (SC-006)

---

## Phase 6: User Story 4 - Vim, and where to find all of it (P3)

- [X] T021 [P] [US4] `editors/vim/syntax/vo.vim`, `ftdetect/vo.vim`, `ftplugin/vo.vim` per research R6;
  its extractor in the `word lists` check; the `Vim` check (when `vim` is installed: load the three
  files on an example with `-es`, fail on any message)
- [X] T022 [P] [US4] `editors/zed/extension.toml` (the grammar's repository, a revision placeholder the
  site page explains, `path = "editors/tree-sitter-vo"`), `languages/vo/config.toml` (name, `path_suffixes
  = ["vo"]`, `line_comments = ["# "]`, brackets) and `languages/vo/*.scm` with Zed's capture names; the
  check validates the TOML and that the queries compile against the grammar (`tree-sitter query`)
- [X] T023 [US4] `site/docs/tools/editors.md` per research R8: steps for VS Code, Neovim, Helix, Zed and
  Vim with exact snippets, and the paragraph on GitHub; `tools/site.py` lists it; `editors/README.md`
  (the folders, regenerating, the licence of the generated runtime header); `.gitattributes`
  (`*.vo linguist-language=Vo`, and the generated parser as `linguist-generated`)
- [X] T024 [P] [US4] `docs/roadmap.md`, `README.md` (one line), `CHANGELOG.md` (Unreleased),
  `docs/review-2026-11/README.md`

---

## Phase 7: Polish

- [X] T025 The drift experiment (SC-003): a word added to the reference's Keywords line in a scratch
  copy (the check takes `--reference FILE`); the failure names the word and every grammar file. Record
- [X] T026 SC-004: a 5,000-line vo file (the largest example repeated) through `tree-sitter parse` and
  through the TextMate tokenizer, each timed; record
- [X] T027 `make site-check` once; `make test` once through `tools/progress.py run test`
- [ ] T028 SC-005: Vim by eye here; VS Code, Zed, Neovim and Helix recorded as not verified by hand,
  with the steps left for the user
- [X] T029 `specs/037-editor-grammars/results.md` completed; spec status "Implemented (gate not run)"

---

## Dependencies

- Setup → Foundational → US2 → US1 (its check reads tree-sitter tokens) → US3. US4's Vim task needs
  only Foundational; its Zed task needs US2; the site page needs all.
- Within a story: the check before the grammar.

## Parallel opportunities

- T016 beside T014–T015; T021, T022 and T024 beside each other.

## Implementation strategy

The first usable result is US2 (three editors and the proof that a grammar matches the language). US1
adds VS Code on top of that proof. US3 makes both permanent. US4 is small and last.
