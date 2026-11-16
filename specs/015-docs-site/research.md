# Research: Documentation Site

## R1: Site generator: MkDocs with the Material theme

- **Decision:** MkDocs with Material for MkDocs, versions pinned in `site/requirements.txt`, installed in a
  virtual environment (`build/site-venv`) by `make site`, and by the publishing workflow. Configuration:
  `theme.font: false` (no Google Fonts), no analytics, the built-in search plugin (a prebuilt index
  searched in the browser: no server), navigation tabs and sections, a table of contents per page,
  "edit this page" links to the repository, light and dark palettes, `mkdocs-redirects` for moved pages,
  and `mkdocs build --strict` with link and anchor validation, so a broken internal link fails the build
  (FR-009).
- **Rationale:** everything the spec asks of the frame (search without a server, navigation, TOC, edit
  links, dark mode, redirects, link validation, readable without JavaScript) is built in; the project's
  tooling is already Python; code is highlighted at build time by Pygments, so it works without
  JavaScript (FR-014).
- **Alternatives:** a hand-written generator on Python-Markdown (would rebuild search, navigation and
  theming); mdBook (good, single binary, but highlighting runs in the browser and a vo grammar would be a
  JavaScript file); Jekyll, GitHub's default (weaker search and navigation, Ruby); Hugo (templates to
  write). If Material's maintenance stops, its configuration is the format its successors read; the
  content is plain Markdown either way.

## R2: Layout: hand-written sources, generated pages outside git

- **Decision:**
  - `site/mkdocs.yml`, `site/requirements.txt`, `site/overrides/` (footer with the version), and
    `site/docs/` with the hand-written pages: `index.md`, `start/` (getting started), `guide/` (the
    language guide), `tools/` (the toolchain guide).
  - `tools/site.py` (Python standard library only) assembles `build/site-src/`: a copy of `site/docs/`
    plus every generated page (R3), and stamps the version; `mkdocs build` reads `build/site-src/`
    and writes `build/site/`.
  - Generated pages are never in git, so they cannot be edited by hand (FR-010).
- **Rationale:** sources stay where they are today (`docs/`, `std/`, `examples/`, `specs/`); the site is
  a view of them plus the guides.

## R3: Generated pages

- **Decision:** `tools/site.py` writes:
  - **Reference:** `docs/reference.md` split at its `##` sections into `reference/<section>.md`, text
    unchanged except relative links rewritten (FR-004). An index page lists the sections.
  - **Error catalog:** `docs/errors.md` split into `errors/<CODE>.md` (one page per code: name, meaning,
    fixable, example, fixed) and an index table by code range; the set of pages is the set of `##`
    entries, which `tests/catalog.py` already keeps equal to the toolchain's codes (FR-010).
  - **Standard library:** from `vo doc --json`, one page per module, with an anchor per item (signature,
    effects, description, example), in the toolchain's words.
  - **Examples:** each `examples/*/` as a page: its README, then each source file in a vo code block.
  - **Security, roadmap, specs:** `docs/security.md`, `docs/security-review-1.md`, `docs/roadmap.md`
    copied; a specs index from each `specs/*/spec.md` title and its `results.md` status line.
  - **Version:** `vo version` and the commit (`git rev-parse --short HEAD`) on every page (FR-006).
- **Rationale:** the content generated pages show is already checked against the toolchain by existing
  scripts (`ref_examples.py`, `catalog.py`, `doc_conformance.py`); the site does not repeat those checks.

## R4: Checking the examples on hand-written pages

- **Decision:** the guide pages use the reference's conventions, which `tests/ref_examples.py` already
  checks: a ```` ```vo ```` block followed by ```` ```output ```` (run and compare), by ```` ```error
  E#### ```` (produces that code), or by nothing (checks clean); consecutive blocks starting with `#
  file: name.vo` form one program; ```` ```vo-test ````, ```` ```vo-fmt ````, ```` ```console ````.
  Syntax shapes that are not programs use ```` ```text ````, never ```` ```vo ````. The checker's code is
  factored into `tests/doc_examples.py` (a module both scripts use), and `tests/site_examples.py` runs it
  over every page in `site/docs/` and every example README: part of `make test` (FR-008), standard
  library only, no site build needed. A failure names the page, the line and the block.
- **Data and input:** an example's files are `# file:` blocks; stdin is a ```` ```stdin ```` block before
  the output; examples needing a service are not used (guides show `std.http` against the local test
  server only in the toolchain guide's console sessions, checked by the step harness).
- **Command options (US6):** each command page has an "Options" table; `tests/site_examples.py` checks that
  every option the toolchain's usage message lists appears on a command page and that every option a
  command page documents is accepted by that command.
- **Rationale:** one convention for the reference and the guides; agents and people learn one format.

## R5: vo highlighting

- **Decision:** a small Pygments lexer for vo (`site/pygments_vo/`, installed into the site's virtual
  environment), with the reference's keyword list, builtins, type names (lowercase built-ins included),
  strings, numbers, comments, effects in `uses` clauses and `@spec` annotations. `output`, `error`,
  `console` and `text` blocks render as plain text.
- **Rationale:** highlighting at build time works without JavaScript; the IntelliJ plugin's token rules
  are the model.

## R6: Pages for agents

- **Decision:** after `mkdocs build`, `tools/site.py --agents` writes into `build/site/`:
  - a Markdown copy of every page next to its HTML (`guide/effects.md` beside `guide/effects/`), the
    assembled source text without navigation;
  - `llms.txt` in the llmstxt.org format: `# vo`, a one-paragraph summary, then sections (Getting
    started, Language guide, Toolchain guide, Reference, Error catalog, Standard library, Examples) with
    `- [Title](url.md): summary` lines, the summary being each page's first sentence;
  - `llms-full.txt`: the version line, then every page's Markdown in navigation order.
- **Rationale:** FR-011, with no plugin needed.

## R7: Search quality

- **Decision:** the built-in search. `tests/site_search.py` checks SC-005 on the built index
  (`build/site/search/search_index.json`) with a fixed list of 30+ queries and their expected pages,
  ranking titles and headings above body text as the search does; it runs with `make site-check`, not
  `make test` (it needs the built site). Pages are written so their titles and first lines name what
  they cover.
- **Rationale:** a deterministic, offline check of the index the browser searches; the in-browser ranking
  is the same index with the same fields boosted.

## R8: Publishing

- **Decision:** `.github/workflows/pages.yml`, on push to `main` and on manual dispatch: build vo (`make`),
  `make test` subset for the docs (`tests/site_examples.py`, `ref_examples.py`, `catalog.py`,
  `doc_conformance.py`), `make site` (assemble, `mkdocs build --strict`, agent pages), upload the
  artifact, deploy with GitHub's Pages actions (pinned versions). Publishing happens only if every step
  passes (FR-016). Enabling Pages ("Source: GitHub Actions" in the repository settings) is a one-time
  maintainer step, documented in the quickstart; this feature does not change repository settings.
- **Local:** `make site` builds `build/site/`; `make site-serve` previews it (FR-017). The first run
  creates the virtual environment (needs the network once); later builds work offline.

## R9: Accessibility

- **Decision:** Material's default light and dark palettes with a contrast check of the chosen primary
  and accent colors against WCAG 2.1 AA (`tests/site_contrast.py` over the palette's CSS values), and a
  keyboard walk-through of the navigation, search and code blocks recorded in results (SC-007).

## R10: Getting started measured with agents

- **Decision:** SC-004 runs the isolated protocol (`claude -p` with no tools, an empty working directory,
  no user settings) three times with only the Getting started page, scripted in `tests/site_agents.py`;
  results recorded, not gated (it costs agent time; run when the user asks).
