# Implementation Plan: Editor Grammars

**Branch**: `037-editor-grammars` (work on `main`, as for specs 001–036) | **Date**: 2026-11-21 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/037-editor-grammars/spec.md`

## Summary

A new directory `editors/` with three grammars written by hand, none generated from another: a TextMate
grammar inside a VS Code extension folder (R2), a tree-sitter grammar with its generated parser and
queries (R3), and Vim syntax files (R6). One check, `tests/grammars.py`, ties them to the language: the
word lists of every grammar (and of the site's Pygments lexer) must equal the lists in the reference and
the toolchain (R1); the tree-sitter grammar must parse every `.vo` file the toolchain parses (R4); the
TextMate grammar must agree with the tree-sitter tokens on those files (R5). The toolchain is not
changed. The node tools are development dependencies of `editors/` only (R7). No colours are chosen
anywhere new; the site's are already Nord (R9).

| Part | Files | What |
|---|---|---|
| TextMate + VS Code | `editors/vscode/` (`package.json`, `language-configuration.json`, `syntaxes/vo.tmLanguage.json`) | R2 |
| tree-sitter | `editors/tree-sitter-vo/` (`grammar.js`, `tree-sitter.json`, `src/` generated, `queries/*.scm`, `test/highlight/`) | R3 |
| Vim | `editors/vim/` (`syntax/vo.vim`, `ftdetect/vo.vim`, `ftplugin/vo.vim`) | R6 |
| Zed, Helix, Neovim | `editors/zed/` (extension manifest and language config), snippets on the site page | R8 |
| Tools | `editors/package.json`, `editors/package-lock.json`, `editors/check.js` | R7 |
| Check | `tests/grammars.py` (in `make test`), `Makefile` (`make grammars` regenerates), `.github/workflows/gate.yml` (`npm ci`) | R1, R4, R5 |
| Site lexer | `site/pygments_vo/pygments_vo.py` (its lists join the check; the effects list is out of date) | R1 |
| Docs | `site/docs/tools/editors.md` (new), `tools/site.py`, `.gitattributes`, `docs/roadmap.md`, `README.md`, `CHANGELOG.md`, `docs/review-2026-11/README.md` | R8, R10 |

## Technical Context

**Language/Version**: JSON (TextMate), JavaScript (tree-sitter grammar DSL; the generated parser is C),
Scheme queries, Vim script; Python and a small node script for the check.

**Primary Dependencies**: development only, in `editors/package.json`, pinned by a lock file:
`tree-sitter-cli`, `vscode-textmate`, `vscode-oniguruma`. None is needed to build or test the toolchain.

**Storage**: none.

**Testing**: `tests/grammars.py`: word lists always; corpus checks when `editors/node_modules` exists,
else one "skip" line each. `make quick AREA=grammars` between edits; `make test` once; the gate only when
asked.

**Target Platform**: the grammars are data; the check runs on the development machine and in CI.

**Project Type**: compiler toolchain with editor support files.

**Performance Goals**: a 5,000-line file in under a second per grammar (SC-004); the corpus check (about
1,500 files) in under a minute.

**Constraints**: the toolchain binary, the reference and every existing test case unchanged (FR-013);
`make test` passes without node (FR-010); the generated parser committed and verified (FR-009); parser
ABI 14, which every current release of Neovim, Helix and Zed loads.

**Scale/Scope**: about 400 lines of grammar JSON, 350 of `grammar.js`, 120 of queries, 80 of Vim script,
250 of checks; the generated parser is large (tens of thousands of lines) and never edited.

## Constitution Check

| Principle | Check | Status |
|---|---|---|
| I. Agent-first | Nothing in the language or toolchain changes. People who review what agents write get readable code. | Pass |
| II. Machine-readable feedback | Not affected. The check's failures name the word and the grammar. | Pass |
| III. Determinism | Not affected. The generated parser is reproducible from `grammar.js` with the pinned generator, and the check proves it. | Pass |
| IV. Test-first | The check is written first: word lists (failing for the site lexer's stale effects today), then the corpus checks, which fail until each grammar exists. | Pass |
| V. Single source of truth, minimal surface | The lists live in the reference and the toolchain; grammars are compared with them, never the other way. No toolchain command is added for this. | Pass |
| Vendored code | The generated parser includes tree-sitter's small runtime header (MIT), produced by the generator; recorded in `editors/tree-sitter-vo/README.md`. No third-party code enters the toolchain. | Pass |

Re-checked after Phase 1: unchanged.

## Project Structure

### Documentation (this feature)

```text
specs/037-editor-grammars/
├── plan.md
├── research.md          # R1–R10
├── data-model.md        # token classes and their names per format
├── quickstart.md
├── contracts/
│   └── grammars.md      # files, scopes and captures, the check's rules
└── tasks.md             # /speckit-tasks
```

### Source Code (repository root)

```text
editors/
├── README.md
├── package.json, package-lock.json, check.js     # development tools and the node side of the check
├── vscode/                                       # the extension folder
├── tree-sitter-vo/                               # grammar.js, src/ (generated), queries/, test/
├── vim/
└── zed/
tests/grammars.py
site/docs/tools/editors.md, .gitattributes
```

**Structure Decision**: everything under `editors/`, one folder per consumer, so each can be copied,
linked or referenced by path on its own.

## Order of work

1. `tests/grammars.py` with the word-list check; fix the site lexer's lists.
2. The tree-sitter grammar until the whole corpus parses; queries and highlight tests; commit the parser.
3. The TextMate grammar until it agrees with the tree-sitter tokens; the VS Code folder.
4. Vim files; Zed folder; the site page; CI; `.gitattributes`.
5. `make test`; the drift experiment (SC-003); timing; editors by hand where present; results.

## Complexity Tracking

No constitution violations.
