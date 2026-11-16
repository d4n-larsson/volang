# Data Model: Documentation Site

## Page

| Field | Meaning |
|---|---|
| address | the URL path (`/guide/effects/`); its Markdown copy is the same path with `.md` (`/guide/effects.md`) |
| title | the page's first heading; used by navigation, search and `llms.txt` |
| summary | the first sentence after the title; used by `llms.txt` |
| section | Home, Getting started, Language guide, Toolchain guide, Reference, Error catalog, Standard library, Examples, Security, Roadmap, Specs |
| kind | `guide` (hand-written in `site/docs/`) or `generated` (assembled by `tools/site.py` from a source) |
| source | the repository file it comes from (the "edit this page" link points there; for generated pages, to the source, e.g. `docs/errors.md`) |
| examples | its code blocks (see Example) |

**Rules:** every page has a title and a summary sentence; generated pages are never in git; a moved page
keeps a redirect from its old address (FR-009).

## Example

| Field | Meaning |
|---|---|
| kind | from the fence: `vo` (program), `vo-test`, `vo-fmt`, `console`, or a non-vo block (`text`, `sh`, `json`) |
| files | consecutive `vo` blocks starting with `# file: NAME.vo` form one program |
| stdin | an optional `stdin` block before the expected result |
| expected | the next block: `output` (run, compare stdout exactly), `error CODE` (that code is produced), or none (checks clean) |
| page, line | where it is, for failure reports |

**Rules:** every `vo`, `vo-test`, `vo-fmt` and `console` block is checked (SC-001); syntax shapes that are
not programs are `text` blocks; output that could vary uses `--seed`/`--clock-ms`.

## Build

| Field | Meaning |
|---|---|
| version | `vo version` and the commit, shown on every page and at the top of `llms-full.txt` |
| sources | `site/docs/`, `docs/`, `vo doc --json`, `examples/`, `specs/` |
| outputs | `build/site-src/` (assembled Markdown), `build/site/` (HTML, Markdown copies, search index, `llms.txt`, `llms-full.txt`) |
