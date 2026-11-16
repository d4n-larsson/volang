---

description: "Task list for vo-lang spec 015: documentation site"
---

# Tasks: Documentation Site

**Input**: Design documents from `specs/015-docs-site/`

**Prerequisites**: plan.md, spec.md, research.md (R1-R10), data-model.md, contracts/site-map.md,
contracts/examples.md, contracts/agents.md, quickstart.md

**Tests**: REQUIRED (constitution IV, FR-007/FR-008): the example checker exists and fails on a broken
example before any guide page is written; every page is written against it.

**Organization**: setup and the shared checker, then one phase per user story (US1-US4 are P1, US5-US7
P2, US8 P3), then the gate.

## Format: `[ID] [P?] [Story] Description`

## Conventions

- Hand-written pages live in `site/docs/`; generated pages are written by `tools/site.py` into
  `build/site-src/` and never committed (FR-010).
- Every vo block follows contracts/examples.md; syntax shapes that are not programs are `text` blocks.
- Every page starts with `# Title` and one summary sentence (data-model.md, Page); the guide never states
  a rule differently from `docs/reference.md` and links to the reference section that states it.
- `make test` after every task that touches checked content.

---

## Phase 1: Setup

- [X] T001 `specs/015-docs-site/results.md` (SC table, verification table); in `docs/roadmap.md` renumber
  concurrency to spec 016 and add 015 (documentation site) as the next spec; confirm `build/` is ignored.
- [X] T002 The site frame: `site/requirements.txt` (pinned MkDocs, Material for MkDocs,
  mkdocs-redirects, Pygments); `site/mkdocs.yml` (site name, repo URL `d4n-larsson/volang`, `edit_uri`,
  `docs_dir: ../build/site-src`, `site_dir: ../build/site`, Material with `font: false`, light and dark
  palettes, navigation tabs, sections, footer, `toc.integrate` off, search plugin, redirects plugin,
  `strict` link and anchor validation, no analytics); `Makefile` targets `site` (create
  `build/site-venv` once, run `tools/site.py`, `mkdocs build`, `tools/site.py --agents`), `site-serve`,
  `site-check`.

---

## Phase 2: Foundational (blocks every content story)

- [X] T003 `tests/doc_examples.py`: the example checker factored out of `tests/ref_examples.py` (fences
  `vo` + `output` / `error CODE` / nothing, `# file:` multi-file programs, `vo-test`, `vo-fmt`, `console`),
  plus a ```` ```stdin ```` block before the expected output; `tests/ref_examples.py` uses it with
  unchanged results.
- [X] T004 `tests/site_examples.py`: runs the checker over every `site/docs/**/*.md` page and every
  `examples/*/README.md`, reporting `page:line` and the difference; added to the `make test` list in
  `Makefile`. A fixture page with one deliberately wrong output (`tests/site_examples_fixtures/broken.md`,
  checked with `--self-test`) proves it fails and names the page and line.
- [X] T005 `tools/site.py` (assemble): copy `site/docs/` into `build/site-src/`; write the version
  (`vo version`, `git rev-parse --short HEAD`) into a generated `build/site-src/version.json` read by
  `site/overrides/partials/footer.html`; write `build/mkdocs.gen.yml` = `site/mkdocs.yml` plus the full
  navigation (hand-written sections from `site/docs/`, generated sections from T015-T018).
- [X] T006 [P] `site/pygments_vo/` (lexer package with `pyproject.toml`, installed into the site venv by
  `make site`): keywords from the reference, built-in types (`list`, `map`, `result`, `ordered_map`,
  `ordered_set`, `priority_queue`, `deque`), builtins, strings, numbers, `#` comments, effects in `uses`,
  `@spec`; a check that every `vo` block in `docs/reference.md` highlights with no error tokens.
- [X] T007 `site/docs/index.md`: what vo is, who it is for, the three ways in (Getting started, the
  language guide, the reference), a checked first example.

**Checkpoint**: `make test` passes with the new checker; `make site` builds the home page.

---

## Phase 3: User Story 1 - From nothing to a running, tested program (Priority: P1) 🎯 MVP

**Goal**: FR-001. **Independent Test**: a reader follows the three pages on a clean machine; every
shown output matches.

- [X] T008 [US1] `site/docs/start/install.md` (prerequisites, `make`, cross builds, `bin/vo version`),
  `site/docs/start/first-program.md` (a program, `vo run`, `vo check --json`, `vo fmt`),
  `site/docs/start/tests-and-effects.md` (a `test`, a property, a `uses` clause with a scope, the missing
  effect as an ```` ```error E0501 ```` block and its fix, `vo effects --update`), every example checked.
- [X] T009 [US1] `tests/site_examples.py --start`: runs the `sh` blocks of `start/install.md` in a fresh
  clone of the current commit (`git clone`, `make`, `bin/vo version`) and compares the shown output; run
  by `make site-check`.

---

## Phase 4: User Story 2 - Learning a feature in depth (Priority: P1)

**Goal**: FR-002, the chapters of contracts/site-map.md. **Independent Test**: every chapter's examples
pass; every reference section is linked from a chapter.

- [X] T010 [P] [US2] Chapters `site/docs/guide/basics.md`, `types.md`, `records-and-variants.md`,
  `functions.md`.
- [X] T011 [P] [US2] Chapters `site/docs/guide/generics.md`, `results-and-errors.md`, `lists-and-maps.md`,
  `collections.md`.
- [X] T012 [P] [US2] Chapters `site/docs/guide/effects.md`, `tests.md`, `properties.md`, `contracts.md`,
  `holes.md`.
- [X] T013 [P] [US2] Chapters `site/docs/guide/modules-and-packages.md`, `standard-library.md`,
  `determinism.md`.
- [X] T014 [US2] Coverage in `tests/site_examples.py --coverage`: every `##` section of
  `docs/reference.md` is linked from at least one chapter; every error code a chapter mentions links to
  its catalog page; each chapter has the parts the contract lists (purpose, rules, examples, edge cases,
  error codes).

---

## Phase 5: User Story 3 - Looking up exact facts (Priority: P1)

**Goal**: FR-004, FR-005, FR-006. **Independent Test**: the generated pages equal the toolchain's own
output at the built commit.

- [X] T015 [US3] `tools/site.py`: `reference/<section>.md` from `docs/reference.md` split at `##`, text
  unchanged except links rewritten (repository files to `https://github.com/d4n-larsson/volang/blob/main/`,
  sections to site addresses); `reference/index.md`.
- [X] T016 [US3] `tools/site.py`: `errors/<CODE>.md` per `##` entry of `docs/errors.md` and
  `errors/index.md` (a table by code range: code, name, one-line meaning); the set of pages checked equal
  to the codes `vo explain --json` knows (fail otherwise).
- [X] T017 [US3] `tools/site.py`: `std/<module>.md` from `vo doc --json` (module description, one anchor
  per item with signature, effects, description, example) and `std/index.md`; the set checked equal to
  the modules `vo doc --json` lists.
- [X] T018 [US3] `tools/site.py`: `examples/<name>.md` (README, then each source file in a vo block marked
  as shown, not re-checked: the examples are checked by `make test`), `security/index.md`,
  `security/review-1.md`, `roadmap.md`, `specs.md` (number, title, status from `results.md`, repository
  link).

---

## Phase 6: User Story 4 - Documentation that cannot drift (Priority: P1)

**Goal**: FR-007-FR-010. **Independent Test**: deliberate breaks fail the right check with the right
message.

- [X] T019 [US4] `make site-check`: `mkdocs build --strict` with link and anchor validation (FR-009); the
  redirect map in `site/mkdocs.yml` (empty to start, rule documented in `site/README.md`); demonstrate
  three deliberate breaks in a scratch copy (a guide example's output, a link to a removed page, an
  error code removed from the catalog) and record each failure message in results.

---

## Phase 7: User Story 5 - Agents read the documentation (Priority: P2)

**Goal**: FR-011, contracts/agents.md. **Independent Test**: every `llms.txt` link resolves to a
Markdown page.

- [X] T020 [US5] `tools/site.py --agents`: a `.md` copy of every page next to its HTML (links rewritten to
  `.md`), `llms.txt` (summary with the version, sections in navigation order, one line per page with its
  summary sentence), `llms-full.txt` (version line, then every page in navigation order with an address
  comment); `tests/site_agents_files.py` (run by `make site-check`) checks that every page is listed once
  and every link resolves.

---

## Phase 8: User Story 6 - The toolchain guide (Priority: P2)

**Goal**: FR-003. **Independent Test**: every command page's options match the toolchain; every session
is checked.

- [X] T021 [P] [US6] Pages `site/docs/tools/check.md`, `run.md` (with `--trace`, `--record`, `--timeout`,
  `--max-memory`, `--mem-stats`, `--seed`, `--clock-ms`), `test.md`, `fmt.md`, `effects.md`, `explain.md`,
  `doc.md`, `reference.md`: synopsis, an Options table, text and JSON output, exit statuses, a checked
  `console` session.
- [X] T022 [P] [US6] Pages `site/docs/tools/query.md`, `impact.md`, `edit.md`, `replay.md`, `package.md`,
  `version.md`, `json-output.md` (every record kind, the schema version rules), `exit-statuses.md`.
- [X] T023 [US6] `tests/site_examples.py --options`: every option the toolchain's usage message lists
  appears in some command page's Options table, and every option a page documents is accepted by that
  command (no "Unknown option").

---

## Phase 9: User Story 7 - Finding things (Priority: P2)

**Goal**: FR-012-FR-015. **Independent Test**: SC-005 lookups; contrast; reading without JavaScript.

- [X] T024 [US7] `tests/site_search.py` (run by `make site-check`): 30+ fixed lookups (features, codes,
  library items, commands) against `build/site/search/search_index.json`, titles and headings ranked
  above text, the expected page in the first three for at least 90%; page titles and summaries adjusted
  where a lookup misses.
- [X] T025 [P] [US7] `tests/site_contrast.py` (run by `make site-check`): the palette's text, link and code
  colors against their backgrounds, WCAG 2.1 AA (4.5:1) in light and dark; a check that every built HTML
  page carries its content and navigation without scripts (no content injected by JavaScript); no
  external URLs in `<script>`, `<link>` or `@import`.

---

## Phase 10: User Story 8 - Publishing and previewing (Priority: P3)

**Goal**: FR-016, FR-017. **Independent Test**: the workflow passes on a branch push in dry-run form;
`make site` and `make site-serve` work locally.

- [X] T026 [US8] `.github/workflows/pages.yml`: on push to `main` and manual dispatch; build vo (`make`),
  the docs checks (`tests/site_examples.py`, `ref_examples.py`, `catalog.py`, `doc_conformance.py`),
  `make site`, `make site-check`; upload with `actions/upload-pages-artifact` and deploy with
  `actions/deploy-pages` (pinned versions, `pages: write`, `id-token: write`); the one-time "Pages →
  Source: GitHub Actions" step in `site/README.md` and the quickstart (not performed by this feature).

---

## Phase 11: Polish & Cross-Cutting Concerns

- [X] T027 `tests/site_agents.py` (SC-004): the isolated agent protocol (`claude -p` with no tools, an
  empty working directory, no user settings) given only the Getting started pages, 3 runs, each asked to
  build a program with a test and an effect; results recorded when the user asks for the run.
- [X] T028 Docs and gate: `README.md` (the site's address and how to build it), `docs/roadmap.md` (015
  status); `make test`, `make site`, `make site-check`, the full gate (`make gate`; the toolchain is
  unchanged unless a guide revealed a fix, FR-018); fill `results.md` (SC-001 to SC-007, the deliberate
  breaks, any toolchain or reference fixes the guides revealed, deviations).

---

## Dependencies & Execution Order

- Setup (T001-T002) → Foundational (T003-T007: the checker before any page) → US1 → US2 and US3 in
  parallel → US4 (needs pages to break) → US5, US6, US7 in any order → US8 → Polish.
- Guide chapters (T010-T013) and toolchain pages (T021-T022) are independent files: parallel.

## Parallel Example

T010, T011, T012 and T013 (four groups of chapters); T021 and T022 (two groups of command pages); T006
alongside T003-T005.

## Implementation Strategy

MVP: the frame, the checker, Getting started and the generated reference, catalog and library pages
(US1, US3) — a useful site on day one. Then the language guide (US2), the drift checks (US4), agents
(US5), the toolchain guide (US6), search quality (US7) and publishing (US8).
