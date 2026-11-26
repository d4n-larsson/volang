---

description: "Task list for vo-lang spec 061: Package Manager"
---

# Tasks: Package Manager (Versions and Dependencies of Dependencies)

**Input**: Design documents from `specs/061-package-manager/`

**Prerequisites**: plan.md, spec.md, research.md (R1–R10), data-model.md, contracts/package-command.md,
contracts/manifest.md, contracts/codes.md, quickstart.md

**Tests**: REQUIRED (constitution IV). Each task's cases land first and fail before its code.

**Organization**:
- Setup: test wiring, results file, catalog listing, baselines.
- Foundational: the git reader and the manifest's version fields; nothing else works without them. It
  ends with a usable increment: one versioned package added and restored, no dependency graph.
- US1 (P1) add with dependencies; US2 (P1) selection rules and conflicts; US3 (P1) check, repair,
  restore; US4 (P2) outdated and update; US5 (P2) packages with their own manifest; US6 (P3) remove with
  pruning; US7 (P3) documentation.
- Polish: version, measurements, three platforms.

## Format: `[ID] [P?] [Story] Description`

## Conventions

- **Step cases**: a folder `tests/cases/packages/<name>/` with `steps`, the program, `test-ca.pem`
  (copied from `tests/cases/packages/fetch/`), `repos/` and `expected/` (format: `tests/steps.py`
  header). The first line is `server git a.test [OPTION...]` (T002). Fetching steps carry `--ca-file
  test-ca.pem` and `https://127.0.0.1:{port}/NAME`. Accept with `python3 tests/steps.py --accept PATH`
  only after reading the diff, and never before the catalog entry of a case's codes exists (the
  diagnostic `name` comes from the catalog).
- **Repositories**: `repos/NAME/VERSION/...` is the content of NAME at the tag `vVERSION` (a folder named
  `1.2.0-plain` gives the tag `1.2.0` without `v`); a `vo.packages` there uses `{port}` for the host.
  Hashes in those manifests are irrelevant unless a task says so (only E0710 reads them).
- **Catalog**: each new code gets its entry in `docs/errors.md` (with a `# file: vo.packages` example)
  and its row in `contracts/codes.md`, in the task that implements it. Messages: `contracts/codes.md`.
- **Every failure leaves the project as it was**: each failing step of a case is followed by `keep
  vo.packages` and, where directories could have changed, a `vo check main.vo` step.
- **Existing behavior**: every existing case of `tests/cases/packages/`, `tests/package_fetch.py` and
  `tests/packages_check.py` passes unchanged throughout (SC-007).
- **Limits**: 16 MiB per response, 64 MiB inflated, 10,000 files, 60 s per request, 1,000 packages per
  selection; exceeding one is E0706 naming it.
- **Reference budget**: `python3 tests/ref_examples.py` after every reference change; 5 tokens of
  headroom, so word-neutral (research R9).
- **Between edits**: `make quick AREA=packages`; `make test` once per change; never two suites at once.

---

## Phase 1: Setup

- [X] T001 `tools/quick.sh`: the `packages` area's scripts gain `package_git.py` and
  `package_graphs.py`; the path table maps `src/gitfetch.c`, `src/sha1.c`, `src/pkgselect.c`,
  `tests/git_server.py`, `tests/package_git.py`, `tests/package_graphs.py` to `packages`. `Makefile`:
  the three new objects, and the two new scripts in the `test` target's `run_scripts.py` list (both in
  `SLOW` of `tests/run_scripts.py` if they take over 10 s).
- [X] T002 `tests/git_server.py` and the step `server git NAME [OPTION...]` in `tests/steps.py` (beside
  `server https`, same certificate handling, `{port}`): at start, for each `repos/NAME/` of the case
  folder, `git init` a repository in a temporary directory, commit each version folder in version order
  with fixed author, committer and dates, tag it (annotated for even patch numbers, lightweight for odd,
  so both forms are exercised); serve `GET /NAME/info/refs?service=git-upload-pack` and `POST
  /NAME/git-upload-pack` by running `git upload-pack --stateless-rpc` (`--advertise-refs` for the first,
  with the `# service` pkt-line and flush before it), and the same under `/NAME.git/`. Options (research
  R8): `moved=NAME@VERSION` (the tag points at a commit with one changed `.vo` file), `truncate=NAME`
  (the pack cut in half), `badsum=NAME` (the pack's last byte flipped), `huge=NAME` (a 17 MiB ref
  answer), `redirect=NAME` (301 to `/NAME.git/...`), `status=NAME:CODE`. Self-test: `python3
  tests/git_server.py --self-test` clones each fixture with the host's `git` and compares the files.
- [X] T003 [P] `specs/061-package-manager/results.md`: SC-001 to SC-009 (pending), Verification,
  Deviations; baselines: the reference's word count, `vo check` time (best of 5) of
  `examples/notes-api/main.vo` and of a generated 100-package program with local directories (the
  generator kept as `tests/package_graphs.py --hundred DIR`, T016).
- [X] T004 [P] `tests/catalog.py` lists spec 061; `contracts/codes.md` keeps its header and loses its
  rows (each returns with the task that implements the code).

---

## Phase 2: Foundational (blocking)

**Goal**: fetch `SOURCE@VERSION` into a directory, and record a version in the manifest. Checkpoint: a
single versioned package can be added and restored (plan, order of work 1 and 2).

- [X] T005 [P] `tests/package_git.py` (research R8), failing first: writes git packs by hand (Python
  `zlib`, `hashlib`) for generated trees of `.vo` and other files in nested folders and checks, through
  a test-only entry `vo package --unpack-pack PACKFILE COMMIT DIR [FOLDER]` (hidden from usage text),
  that the written files equal the source: at least 200 packs covering plain objects, OFS_DELTA,
  REF_DELTA, delta chains of depth 5, a base after its delta, an annotated tag object, empty blobs,
  stored (level 0) and level 9 streams. Hostile packs each fail with E0706 and write nothing: bad
  magic, version 3, object count too high, a truncated stream, a wrong trailer, a delta with a missing
  base, a delta result size mismatch, a delta cycle, an unsafe name in a tree (`..`, `/`, NUL), a link
  (mode 120000) or a submodule (160000) under the package folder, the inflated total over 64 MiB, a
  missing tree or blob.
- [X] T006 [P] `src/sha1.c`, `src/vo.h`: `sha1(char *p, long n, char *out20)`; checked in
  `tests/package_git.py` through the pack tests (every object is found by its id) and by three known
  vectors in a C self-check run by `vo package --unpack-pack --self-test`.
- [X] T007 `src/inflate.c`: `zlib_unpack(in, n, &out, &out_n, &cap, limit, &used, &why)`: the 2-byte
  zlib header checked (deflate, no dictionary), the existing `inflate`, `used` = input bytes consumed
  including the 4-byte Adler-32 (verified). `gzip_unpack` unchanged.
- [X] T008 `src/gitfetch.c` (new), pack side: `git_unpack(pack, n, commit_id, folder, &why)` returns the
  file list `untar_select` returns (path, bytes): header and trailer check; each object's type and
  size varint, OFS/REF delta headers; inflate with T007; resolve deltas iteratively (no recursion
  deeper than the chain cap of 50); SHA-1 of `"TYPE SIZE\0" + data` into an open-addressing table;
  commit (or tag → `object`) → `tree`; walk to `folder`; collect blobs named `*.vo` and the root
  `vo.packages`; refuse links and submodules there; `path_unsafe` on every name. All buffers mapped
  (`pk_map`), no static buffer over 64 KiB. T005 passes.
- [X] T009 `src/fetch.c`: `fetch_request(url, body, content_type, &why)` (GET when `body` is 0), with
  `fetch_url` as its GET wrapper; redirects apply to GET only (a POST that is redirected fails naming
  the target). `src/gitfetch.c`, network side: `git_versions(source, &why)` (GET `info/refs`; first
  pkt-line `# service=git-upload-pack`, flush, then refs; tags matching `[v]N.N.N` without leading
  zeros, the peeled `^{}` id preferred, `v` preferred over plain; sorted; an answer that names
  `object-format=sha256` is refused) and `git_fetch(source, version, &why)` (the `POST` of research R1,
  the answer's pkt-lines up to `NAK`, then `git_unpack`). The repository URL used for the POST is the
  one the GET ended at after redirects.
- [X] T010 Step case `tests/cases/packages/version_add/` (failing first), then `src/packages.c`:
  `Pkg` gains `version` (three longs, or none) and `indirect`; `pkg_parse_line` reads `URL@N.N.N` and the
  fifth field `indirect` (E0703 with the extended message for a malformed version, leading zeros,
  `indirect` on a version-less entry, a sixth field); `pkg_write` writes them. `cmd_package`: the third
  operand is a repository per research R3; `vo package ENTRY NAME SOURCE@VERSION` fetches with
  `git_fetch`, stages, pins and reports as the archive form does, with the version in the report
  (contracts/package-command.md, Add: the `added` and `updated` lines) and `version`, `source`,
  `direct` in JSON; without `@VERSION` the highest version. The package's root `vo.packages` is written
  beside its `.vo` files and stays outside the hash (research R5). Cases: add at a version; add newest;
  `#FOLDER`; an annotated and a lightweight tag; a plain `1.2.0-plain` tag; `redirect=`; an archive URL
  still taken as an archive; `--dir`.
- [X] T011 E0711 `package_version_unknown` (catalog entry, codes row): an unknown `@VERSION` lists the
  nearest five versions (by distance in the ordered list, lower first); a source without version tags
  says so. Case `tests/cases/packages/version_unknown/`. E0706 cases
  `tests/cases/packages/git_refused/`: `truncate=`, `badsum=`, `huge=`, `status=NAME:404`, a link in the
  package folder, `http://`, credentials in the URL (T0001, as today); after each, `keep vo.packages`
  shows it unchanged.
- [X] T012 `--restore` for versioned entries in `pkg_restore` (`packages.c:975`): a missing directory
  with `URL@VERSION` is fetched with `git_fetch` and accepted only at its pin (E0701), manifest
  untouched. Case `tests/cases/packages/version_restore/`: restore after `rm -r packages`; with
  `moved=money@1.2.0` restore fails with E0701 and writes nothing for it; a tag deleted upstream
  (version folder absent in a second `repos2/` served after `stop-server`) fails with E0711.

**Checkpoint**: `make quick AREA=packages`; quickstart steps 1 (single package) and 3 (restore) work.

---

## Phase 3: User Story 1 - Add a package and get what it needs (P1)

**Goal**: one add brings every required package, all or nothing, with a report per package.

**Independent test**: `report` → `money` → `pads` on the test host; one command, three entries, the
program runs.

- [ ] T013 [US1] Step cases, failing first: `tests/cases/packages/graph/` (repos `report` 2.1.0 requiring
  `money` 1.4.0 requiring `pads` 1.0.2; `money` also at 1.2.0, 1.6.0, 2.0.0; `ledger` 1.0.0 requiring
  `money` 1.2.0; used by later cases through `repos -> ../graph/repos` copies made by a `cp -r` step):
  add `report` gives three blocks and `total gains:`; `vo run main.vo`; `--json` records with
  `required_by`, `direct` and the `package_total` record; `--dry-run` prints `dry run: nothing written`
  and `keep vo.packages` shows no file; a second identical add prints `unchanged` lines and touches
  nothing; a failure in the middle (`truncate=pads`) leaves no manifest and no `packages/` folder.
- [ ] T014 [US1] `src/pkgselect.c` (new), research R4 steps 1 and 2 only (no conflicts yet):
  `Requirement` and `Selection` (data-model.md); `pkg_select(roots, &sel)` visits each `name@version`
  once, taking content from disk when the entry's version and pin match and the directory is intact
  (`pkg_intact`), else `git_fetch` into staging `packages/NAME.new-PID`; reads the staged or present
  `vo.packages` with `pkg_parse_line` into requirements; a higher compatible requirement replaces the
  selected version and discards its staging. Limit: 1,000 packages (E0706).
- [ ] T015 [US1] `src/packages.c`: commit of a selection (research R6): every changed package staged,
  then old directories renamed to `.old-PID`, new ones in, the manifest written last, old ones removed;
  any failure before the manifest write renames everything back and removes the stagings. The named
  package is direct, the others `indirect` (an existing direct entry stays direct). Reports per
  contracts/package-command.md in name order: `added`, `updated OLD -> NEW` with both hashes,
  `unchanged`, `(required by A, B)`, each with effects and gains/losses (`pkg_effects`,
  `pkg_report_fetched`), then `total gains:`/`total loses:` when more than one package changed, and the
  JSON fields. `--dry-run` (new option in `src/main.c`, accepted by add, `--update`, `--repair`,
  `--remove`; T0001 elsewhere) stages, reports and discards. `--dir` applies to the named package only.
  T013 passes.
- [ ] T016 [P] [US1] `tests/package_graphs.py` (research R8), first part: generates 50 dependency graphs
  as `repos/` trees (seeded; 3 to 12 packages, shared dependencies, chains of depth up to 5, several
  versions per package), serves them with `tests/git_server.py`, adds the roots in every order (at most
  24 orders per graph) into fresh projects and requires byte-identical manifests and a passing `vo
  check`; `--hundred DIR` writes the 100-package program of T003.

**Checkpoint**: quickstart step 1.

---

## Phase 4: User Story 2 - One predictable version of each package (P1)

**Goal**: the selection rule's refusals, each naming who asks for what.

**Independent test**: `report` (money 1.4.0) and `ledger` (money 1.2.0) in either order give 1.4.0;
`audit` (money 2.0.0) fails naming both.

- [ ] T017 [US2] Step cases, failing first: `tests/cases/packages/select/`: `ledger` then `report`, and
  the other order in a second project folder, `cmp` of the manifests; money stays 1.4.0 with 1.6.0
  released; the program's own `money@1.6.0` is kept when `report` asks 1.4.0; adding `report` to a
  project with `money@1.2.0` raises it and prints `(required by report)`.
  `tests/cases/packages/conflict/`: `audit` requiring `money` 2.0.0 after `report` → E0707 naming
  `report` 1.4.0 and `audit` 2.0.0 (and the program when its own entry takes part); a 0.x pair
  (`tiny` 0.2.0 and 0.3.0) conflicts on the minor; `--json` carries the requirers in the message only.
  `tests/cases/packages/source_clash/`: `cash` whose manifest names `money` at another source → E0708;
  a package requiring the same source under the name `coins` → E0708 (both names in the message).
  `tests/cases/packages/version_changed/`: `stale` whose manifest pins `money` 1.4.0 with another hash
  → E0710 showing both hashes. `tests/cases/packages/local_wins/`: a program entry `money shared/money
  sha256:…` (no version) and `report` added → `local money (required by report at 1.4.0)`, the entry
  untouched. After every failure `keep vo.packages` and `vo check main.vo`.
- [ ] T018 [US2] `src/pkgselect.c`: research R4 step 3: E0707 (all requirers collected before
  reporting, sorted by name), E0708 (sources compared per data-model.md: one trailing `/` and `.git`
  dropped), E0710 (a requirement's hash against the selected content's, same version only), the `local`
  outcome; `compatible()` with the 0.x rule. Catalog entries and codes rows for E0707, E0708, E0710.
- [ ] T019 [P] [US2] `tests/package_graphs.py`, second part: the generator also produces graphs with a
  conflict, a source clash and a cycle (A 1.0.0 ↔ B 1.0.0); for conflicts the output must name every
  requirer the generator knows; a cycle resolves; SC-002's count (50 graphs, all orders) printed.

**Checkpoint**: quickstart step 2.

---

## Phase 5: User Story 3 - A fresh clone builds the same thing (P1)

**Goal**: the manifest is complete or `vo check` says so, offline; repair fixes it.

**Independent test**: delete a required line by hand: `vo check` gives E0709; `--repair` restores it.

- [ ] T020 [US3] Step cases, failing first: `tests/cases/packages/incomplete/`: after adding `report`,
  the `money` line deleted with `sed` → `vo check main.vo` E0709 at `report`'s line ("does not list
  it", naming `vo package --repair main.vo`), also with `--json`; `money` hand-edited to 1.2.0 (with
  that version's directory and hash) → E0709 "has 1.2.0"; `vo run`, `vo test`, `vo build` refuse the
  same way; `stop-server` before the checks (nothing is fetched). `tests/cases/packages/repair/`: the
  two situations repaired (`added`, `updated` lines); a manifest with an `indirect` entry nothing
  requires → `removed`; two branches' manifests merged by `cat` and deduplicated by hand → repair gives
  the manifest that adding both packages gives (`cmp`); `--repair --dry-run` writes nothing; a complete
  manifest → `unchanged` lines, exit 0.
- [ ] T021 [US3] `src/packages.c` (`pkg_load`, research R5): for each versioned entry with a
  `vo.packages` in its directory, compare its versioned lines with the program's entries; E0709
  `package_requirement` at the requirer's line (catalog entry, codes row); skipped for a built
  executable (`ld_app`) and when the program's entry for the name has no version (`local`). No network.
- [ ] T022 [US3] `--repair ENTRY` in `cmd_package`: selection with the manifest's entries as roots
  (research R4 including step 4, the pruning of unreachable `indirect` entries), commit and reports
  as T015; missing directories are fetched, present ones verified. Usage text and option checks in
  `src/main.c` (`--repair` only with `package`; with `--remove`/`--restore`/`--update`/`--outdated`:
  T0001).
- [ ] T023 [P] [US3] SC-004: `tests/package_fetch.py` gains a check that `vo check`, `run`, `test`,
  `build` and `doc` on the `graph` project open no socket (`strace -f -e trace=socket,connect` when
  `strace` exists, as the script does for spec 041).

**Checkpoint**: quickstart step 3.

---

## Phase 6: User Story 4 - See what is newer, and take it (P2)

- [ ] T024 [US4] Step cases, failing first: `tests/cases/packages/outdated/`: the listing of
  contracts/package-command.md for `money` at 1.2.0 (1.6.0 available; 2.0.0 a new major), a package
  with only a new major, one with nothing newer omitted, `all packages are at their newest version`,
  `--json` (`change":"outdated"`, `compatible`, `newer_major`), exit 0; the manifest untouched.
  `tests/cases/packages/update_versions/` (the existing `update` case is the local-directory one): `--update main.vo money` moves 1.2.0 → 1.6.0 and raises `pads` where 1.6.0 requires it;
  1.6.0 declares `net("rates.example.com")` → `gains:`; `--update main.vo` moves every direct package
  and leaves an `indirect` one where its requirers ask; `--update --dry-run`; `--update main.vo
  unknown` (T0001); `--update` of a version-less entry (T0001 naming the add forms); `money@2.0.0` named
  explicitly is taken.
- [ ] T025 [US4] `src/packages.c`: `--outdated` (`git_versions` once per distinct source; highest
  compatible and highest newer-major per data-model.md) and `--update ENTRY [NAME]` (the roots' versions
  replaced by the highest compatible ones, then selection, commit and reports as T015); options in
  `src/main.c`.

---

## Phase 7: User Story 5 - Write a package that others can depend on (P2)

- [ ] T026 [US5] Step case `tests/cases/packages/author/`, failing first: a package folder with its own
  `vo.packages` (one versioned requirement) and a `test` item: `vo package --restore lib.vo` then `vo
  test lib.vo` inside it; the same content served as `repos/kit/1.0.0` and added by a program brings
  the requirement. `repos/broken/1.0.0` whose manifest has a version-less line → add fails with E0709
  ("`pads` has no source in `broken`'s manifest"), nothing written. Tags `1.2`, `v1.2.0-rc1`,
  `release-3` beside `v1.0.0` (a `tags` file in the repo folder, read by `tests/git_server.py`) are
  ignored by add, `--outdated` and E0711's list. A package in a sub-folder with its manifest there
  (`#kit`) is read from that folder.
- [ ] T027 [US5] `src/pkgselect.c`: the version-less requirement error; confirm the manifest is read
  from the package root under `#FOLDER`. `tests/git_server.py`: the `tags` file.

---

## Phase 8: User Story 6 - Remove a package and what only it needed (P3)

- [ ] T028 [US6] Step case `tests/cases/packages/remove_graph/`, failing first: `--remove main.vo
  report` removes `report`, `money`, `pads` (three `removed` lines, directories gone); `--remove
  main.vo money` while `report` is listed → E0709 ("`money` is required by `report`"), nothing changed;
  with `ledger` also listed, removing `report` keeps `money` and `pads`; `--remove --dry-run`. The
  existing `tests/cases/packages/remove/` (a local directory: the entry goes, the directory stays)
  passes unchanged.
- [ ] T029 [US6] `src/packages.c` (`cmd_package`, remove branch): requirers from the listed packages'
  manifests on disk; pruning of `indirect` entries no remaining entry reaches; directories under
  `packages/` that were fetched (entries with a source) are removed, hand-pinned ones are left.

---

## Phase 9: User Story 7 - Documented (P3)

- [ ] T030 [US7] `docs/reference.md` (research R9): the Packages bullet (`:1171`) rewritten to the same
  word count: the line forms (`… URL@VERSION [indirect]`), add by `SOURCE[@VERSION]` bringing what the
  package requires, the rule (the highest compatible version any manifest asks for), `--update`,
  `--outdated`, `--repair`, `--restore`, `--remove`; `python3 tests/ref_examples.py`; words before and
  after in `results.md`.
- [ ] T031 [P] [US7] `site/docs/tools/package.md` (every form, the output blocks, the limits, what is
  trusted: contracts/package-command.md and manifest.md), `site/docs/guide/modules-and-packages.md`
  (one worked example: add a package with a dependency, update it, restore a clone; how to release a
  package: its `vo.packages` and a tag), `docs/stability.md` (the manifest's new fields; older
  toolchains report them as malformed), `CHANGELOG.md` (0.7.0: the forms, the codes, the archive-ending
  rule of research R3), `docs/roadmap.md` row; `python3 tests/site_examples.py`.

---

## Phase 10: Polish & Cross-Cutting Concerns

- [ ] T032 Version 0.7.0 (`src/build.c`, `src/main.c`, `tests/cases/cli/version*.out`,
  `tests/cases/build/options/expected/out`, `site/docs/tools/version.md`,
  `site/docs/start/install.md`, `docs/stability.md`); `make site-check` (its `--start` step after the
  commit).
- [ ] T033 [P] `tests/fuzz.py`: manifest lines with version and `indirect` noise (no crash, E0703 or
  accepted); `tests/package_git.py --fuzz N` mutates valid packs (no crash, no file outside the target
  directory); 10,000 mutations recorded in `results.md`.
- [ ] T034 SC-008 (research R10): three tasks in bench2 against `tests/git_server.py` (add a package
  with a dependency, update it, restore a clone), run with the isolation of the benchmark memory (no
  tools, empty directory, no user settings); results in `results.md`. Not part of `make test`.
- [ ] T035 Measurements in `results.md`: SC-001 (commands before and after), SC-002 (graphs, orders),
  SC-003 (10-package restore; 100% of moved versions refused), SC-005 (the 100-package check time
  against T003's baseline), SC-006, SC-007 (existing cases unchanged: list from `git diff --stat
  tests/cases/packages` showing only new folders), SC-009.
- [ ] T036 `make test` on x86-64, then `make test ARCH=aarch64` and `ARCH=riscv64` under qemu, one
  after the other; `results.md` complete.

---

## Dependencies & Execution Order

- **Setup (T001–T004)**: first; T002 before every step case; T003, T004 in parallel.
- **Foundational (T005–T012)**: T005, T006 in parallel; T007 → T008 → T009 → T010 → T011, T012.
- **US1 (T013–T016)**: after Phase 2. T013 → T014 → T015; T016 beside T015.
- **US2 (T017–T019)**: after US1 (same file, `src/pkgselect.c`).
- **US3 (T020–T023)**: after US2 (repair uses the full selection). T023 in parallel.
- **US4 (T024–T025)**, **US5 (T026–T027)**, **US6 (T028–T029)**: each after US3, independent of each
  other; all touch `cmd_package`, so one at a time.
- **US7 (T030–T031)**: after US6. **Polish**: T032 → T035 → T036; T033, T034 in parallel with them.

## Parallel Example: Foundational

```text
T005 package_git.py (packs by hand)   |   T006 sha1.c
→ T007 zlib streams → T008 pack reader → T009 requests and ref list → T010 single add
```

## Implementation Strategy

- **MVP**: Phases 1–2: versions without a graph (add `SOURCE@VERSION`, restore). Already better than
  archive URLs: a version is recorded and unknown versions are listed.
- **Then US1–US3** (all P1), in that order: the dependency graph, its refusals, the offline
  completeness check and repair. This is the feature.
- **Then US4–US6**, each a separate increment, then documentation and 0.7.0.
