# Implementation Plan: Documentation Site

**Branch**: `015-docs-site` | **Date**: 2026-11-16 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/015-docs-site/spec.md`

## Summary

A documentation site for vo on GitHub Pages (research R1-R10): MkDocs with the Material theme builds
hand-written guides (Getting started, the language guide, the toolchain guide) together with pages
generated at build time from the toolchain's own sources (the reference, the error catalog, the standard
library, the examples, the security model, the roadmap, a specs index). Every vo example on a guide page
uses the reference's example conventions and is checked by `make test`; generated pages are already
checked by the existing scripts. Agents get `llms.txt`, a Markdown copy of every page and a full-text
file. A GitHub Actions workflow publishes on every push to `main` when all checks pass.

## Technical Context

**Language/Version**: Markdown (pages), Python 3 (build and check scripts, standard library only for the
checks), YAML (site and workflow configuration); vo for the examples.

**Primary Dependencies**: MkDocs and Material for MkDocs (pinned, in a virtual environment, for the site
build only), Pygments (a vo lexer), `mkdocs-redirects`; GitHub Actions' Pages actions (pinned).

**Storage**: N/A (static files; generated sources in `build/site-src/`, the site in `build/site/`).

**Testing**: `tests/site_examples.py` (every vo block on guide pages and example READMEs; command options)
in `make test`; `make site-check`: `mkdocs build --strict` (links and anchors), `tests/site_search.py`
(30+ lookups), `tests/site_contrast.py`; `tests/site_agents.py` for SC-004 when asked.

**Target Platform**: browsers (desktop and mobile, no JavaScript needed to read) and agents fetching
Markdown; built on Linux (locally and on GitHub's runners).

**Project Type**: documentation site for a programming language, generated from its repository.

**Performance Goals**: full site build under 2 minutes locally (SC-006); publish within 15 minutes of a
push.

**Constraints**: constitution Principle V (the reference, catalog and library docs stay the single
sources; the site renders them); no third-party scripts, fonts or trackers (FR-015); the toolchain and
the embedded reference unchanged except for fixes the guides reveal (FR-018).

**Scale/Scope**: about 35 hand-written pages (Getting started 3, language guide 16, toolchain guide 15,
home 1), about 150 generated pages (reference sections ~20, error codes ~100, library modules 6,
examples 4, others), ~400 lines of Python, one workflow file, one lexer.

## Constitution Check

| Principle | Assessment |
|---|---|
| I. Agent-first design | **PASS.** No language change. The site serves agents first-class (`llms.txt`, Markdown per page, one full-text file) and uses the same example conventions as the reference. |
| II. Machine-readable feedback | **PASS.** No new codes or output; the toolchain's JSON output is documented on its own page. |
| III. Determinism | **PASS.** Examples fix their seed and clock where output would vary; the site build is reproducible from a commit. |
| IV. Test-first through the CLI | **PASS.** Every guide example is checked in `make test`; the docs check is written before the guides it checks and fails on a deliberately broken example. |
| V. Single source of truth, minimal surface | **PASS.** The reference, catalog and library pages are generated from their sources and never edited by hand; the guides explain and link, and where they disagree with the reference the reference wins. The 15,000-token budget applies to the embedded reference only. |
| Implementation constraints | **PASS with a note.** Developer scripts (Python) and the site's build dependencies are not part of the shipped binary. The site build needs MkDocs and Material from PyPI (Complexity Tracking). |

Re-checked after design (research, data model, contracts): no change.

## Project Structure

### Documentation (this feature)

```text
specs/015-docs-site/
├── spec.md, plan.md, research.md, data-model.md, quickstart.md
├── contracts/site-map.md       # the sections, pages and addresses
├── contracts/examples.md       # the code block conventions every page follows
├── contracts/agents.md         # llms.txt, Markdown copies, llms-full.txt
├── checklists/requirements.md
└── tasks.md
```

### Source Code (repository root)

```text
site/mkdocs.yml                 # navigation, theme (no external fonts), search, redirects, strict links
site/requirements.txt           # pinned MkDocs, Material, mkdocs-redirects
site/pygments_vo/               # the vo lexer (installed into the site venv)
site/overrides/                 # footer: version and commit
site/docs/index.md              # home: what vo is, where to start
site/docs/start/                # getting started: install, first program, tests and effects
site/docs/guide/                # language guide: one chapter per feature area
site/docs/tools/                # toolchain guide: one page per command, JSON output
tools/site.py                   # assemble build/site-src (generated pages, version), agent pages
tests/doc_examples.py           # the example checker shared with ref_examples.py
tests/site_examples.py          # make test: guide and example-README blocks, command options
tests/site_search.py, tests/site_contrast.py, tests/site_agents.py
.github/workflows/pages.yml     # build, check, publish on push to main
Makefile                        # site, site-serve, site-check
README.md, docs/roadmap.md      # the site's address; concurrency renumbered to spec 016
```

**Structure Decision**: hand-written pages under `site/docs/`; everything else on the site is generated
from the sources the toolchain already uses, into `build/` (never committed).

## Complexity Tracking

| Item | Why needed | Simpler alternative rejected because |
|---|---|---|
| MkDocs and Material from PyPI for the site build | Search without a server, navigation, TOC, dark mode, redirects and link validation come built in | A hand-written generator would rebuild all of them; the dependency is pinned, used only by `make site` and the workflow, never by the toolchain or `make test` |
