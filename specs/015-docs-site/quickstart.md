# Quickstart: Validating the Documentation Site

Prerequisites: the toolchain builds (`make`); Python 3 with `venv`; the network once, to install the
site's pinned dependencies into `build/site-venv`.

## 1. Examples are checked (every build, no site needed)

```sh
make test                          # includes tests/site_examples.py
python3 tests/site_examples.py     # every vo block on guide pages and example READMEs, command options
```

Expected: 0 failures. Break one example on purpose (change an expected output line): the check fails
and names the page and line.

## 2. Build and preview the site

```sh
make site          # assemble build/site-src, mkdocs build --strict, agent pages -> build/site
make site-serve    # preview at http://127.0.0.1:8000/
make site-check    # strict links and anchors, search lookups, contrast
```

Expected: under 2 minutes; 0 broken links; at least 90% of the search lookups find the right page in
the first three; `build/site/llms.txt`, `build/site/llms-full.txt` and a `.md` copy per page exist; the
footer shows `vo version` and the commit.

## 3. Generated pages match the toolchain

```sh
ls build/site/errors/ | wc -l      # one directory per code, plus the index
python3 tests/catalog.py && python3 tests/doc_conformance.py && python3 tests/ref_examples.py
```

## 4. Publishing (maintainer, one time)

In the repository's settings, Pages → Source: GitHub Actions. Then every push to `main` runs
`.github/workflows/pages.yml`: build, checks, site, deploy to `https://d4n-larsson.github.io/volang/`.

## 5. Agents (when asked)

```sh
python3 tests/site_agents.py       # SC-004: 3 isolated runs given only the Getting started page
```
