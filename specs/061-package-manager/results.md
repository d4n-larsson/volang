# Results: Package Manager

## Setup

- Baseline (commit `8f82807`, x86-64): the reference is 11,535 words (~14,995 tokens of 15,000).

## SC-001 to SC-009

- **SC-001**: `report` (requiring `money`, which requires `pads`) is added with 1 command and the
  program importing it runs (`tests/cases/packages/graph`). Before: 3 commands, one per package, each
  with an archive URL found by hand, the second and third only after an unresolved import said so.
- **SC-002**: `tests/package_graphs.py`: 50 generated graphs, their roots added in every order (274
  orders): every manifest equals the independent computation of the rule; the 25 graphs whose versions
  only add requirements give byte-identical manifests in every order. 12 further graphs: 4 conflicts
  (every requirer and version in the message), 4 source clashes, 4 cycles (resolve). As amended: order
  independence is claimed only for the graphs of the second kind (Deviations).
- **SC-003**: the same script: a 10-package program with `packages/` deleted is restored by one command
  (10 `restored` lines, the manifest byte-identical, `vo check` passes); with each package's pin
  changed in turn, restore refuses 10 of 10 (E0701) and creates no directory. A changed version is
  simulated by a changed pin: the comparison is the same.
- **SC-004**: `tests/package_fetch.py`, quiet group: `check`, `run`, `test`, `build`, `doc` open no
  socket (`strace`) on a program with versioned packages, complete and with a requirement missing (10
  cases); `tests/cases/packages/incomplete` and `remove_graph` run with the server stopped.
- **SC-005**: `vo check` of a program importing 100 packages on disk (`tests/package_graphs.py
  --hundred`): 10 to 20 ms over 5 runs. These are 100 directory packages; the per-package cost a
  versioned entry adds is reading one small `vo.packages`.
- **SC-006**: the two updates that gain an effect (`graph`: `money` 1.4.0 → 1.6.0; `update_versions`:
  1.2.0 → 1.6.0) show `gains: clock` in text, JSON and with `--dry-run` (5 of 5 reports). After each
  `--dry-run` the cases' next step shows the unchanged state (`vo check` still failing, the manifest
  as before).
- **SC-007**: no existing manifest changed meaning. Of the cases under `tests/cases/packages` that
  existed before, 3 have a changed expected output, all message texts: `bad_name` and `url_field` (the
  usage lines now list the new forms), `fetch` (`vo explain E0706` mentions repositories). Every other
  change under that folder is a new case.
- **SC-008**: `bench2/package_tasks.py`: an isolated agent (no tools, empty directory, no user
  settings; the CLI's default model) given `vo reference`, the task and each command's output: add a
  package with a dependency, update a package and name the effect gained, restore a clone. 3 runs of
  each: 9 of 9 solved, 2 or 3 commands each, 0 usage errors.
- **SC-009**: the reference is 11,538 words (~14,999 tokens of 15,000). The three architectures: see
  Verification, last entry.

## Verification

- Phase 2 (T005–T012): a versioned package is added, updated and restored from the test git host
  (`tests/cases/packages/version_add`, `version_restore`): annotated, lightweight and plain (`1.3.0`)
  tags, a `#FOLDER`, a redirect to `NAME.git`, `--dir`; tags that are not versions are ignored.
  `tests/package_git.py`: 200 generated packs (plain, OFS_DELTA and REF_DELTA objects, a base after its
  delta, annotated tags, every compression level) unpack to their source, and 17 hostile packs are
  refused with E0706 and write nothing; 400 mutated packs (`--fuzz 400`) cause no crash and no file
  outside the target. `tests/cases/packages/git_refused`: a truncated pack, a bad trailer, a 17 MiB ref
  list, a 404, a link in the package. E0711 lists the nearest versions (`version_unknown`).
  The existing package cases pass unchanged except `fetch`, whose `vo explain E0706` step prints the
  catalog text, which now also names repositories.

- US1 (T013–T016): `tests/cases/packages/graph`: one add of `report` brings `money` and `pads` (three
  blocks and a total), `--dry-run` and `--json --dry-run` write nothing, a failing dependency
  (`truncate=trunc`) leaves no manifest and no `packages/` folder, an unchanged add says so, a later
  explicit `money@1.6.0` reports the gained effect. `tests/package_graphs.py`: 50 generated graphs, 274
  orders of adds, each matching an independent computation of the rule; the 25 graphs whose versions
  only add requirements give byte-identical manifests in every order.
- US2 (T017–T019): `tests/cases/packages/select` (four sibling projects: `ledger` then `report` and the
  other order give the same manifest, checked with `cmp` when the output was accepted; `money` stays
  1.4.0 with 1.6.0 released; the program's own 1.6.0 is kept; its own 1.2.0 is raised and reported with
  `required_by`), `conflict` (E0707 with every requirer, `this command`, `this program`, the 0.x rule),
  `source_clash` (E0708, both forms; a `.git/` ending is the same source), `version_changed` (E0710,
  with the package fetched and with it on disk), `local_wins`. After each refusal the manifest is as it
  was. `tests/package_graphs.py` adds 12 graphs: conflicts with 1 to 4 requirers at 1.x and one at
  2.0.0 (every requirer and version must be in the message), source clashes, cycles of 2 to 5 packages
  (resolve). The generated graphs found one bug in this phase's own check: a package the command asks
  lower than a requirement allows kept the hash of the version asked, and E0710 fired; fixed.
- US3 (T020–T023): `tests/cases/packages/incomplete` (server stopped: E0709 "does not list it" and "has
  1.2.0" at `report`'s line from `check`, `run`, `test`, `build`, also as JSON; a version-less `money`
  entry is not checked) and `repair` (a complete manifest: `unchanged` lines; a merged manifest: the
  result equals the manifest of adding both packages, compared with `cmp` at acceptance; `--dry-run`
  writes nothing; a deleted line added back; an `indirect` entry nothing requires removed; a pin that
  is not the version's content: E0701; option checks). `tests/package_fetch.py` quiet group: `check`,
  `run`, `test`, `build`, `doc` on a program with versioned packages open no socket, with the manifest
  complete and with a requirement missing (10 cases, SC-004).
- US4 (T024–T025): `tests/cases/packages/outdated` (a compatible update and a new major, only a new
  major, nothing newer omitted, `all packages are at their newest version`, JSON, option checks, E0706
  with the server stopped, manifest untouched) and `update_versions` (`--update ENTRY money` 1.2.0 →
  1.6.0 with `pads` raised and `gains: clock`; `--dry-run`; JSON; `--update ENTRY` moves the direct
  packages and leaves the indirect `pads` at 1.0.2 with 2.1.0 released; unknown and version-less names
  are T0001; `@2.0.0` named explicitly is taken).
- US5 (T026–T027): `tests/cases/packages/author`: a package folder with its own manifest and a test:
  `vo test` fails with E0702 until `--restore`, then passes; the same content tagged as `kit` 1.0.0 and
  added by a program brings `pads`; the tags `1.2`, `v1.2.0-rc1`, `release-3` are ignored by add,
  `--outdated` and E0711's list; a manifest with a version-less line is E0709 from add, nothing
  written; a package in a sub-folder (`mono#kit`) has its requirements read from that folder's manifest,
  not the repository root's (which names a package that does not exist).
- US6 (T028–T029): `tests/cases/packages/remove_graph` (server stopped: removing `money` or `pads`
  while `report` requires them is E0709 naming every requirer; `--remove --dry-run`; removing `report`
  removes `money` and `pads` with their directories; with `ledger` listed, removing `report` keeps
  them; a local directory's entry goes and its directory stays). The existing `remove` case passes
  unchanged.
- US7 (T030–T031): the reference is 11,538 words (~14,999 tokens of 15,000), 3 more than the baseline's
  11,535: the Packages bullet names the line form with `[SOURCE@VERSION [indirect]]`, the versioned
  add, the selection rule, `--update`/`--outdated` and `--repair` (E0709); the command list has every
  form. `--restore` and `--remove` are in the command list only. `site/docs/tools/package.md` (Versioned
  packages, The manifest, options, output, statuses), `site/docs/guide/modules-and-packages.md`
  (Versions and dependencies, Releasing a package), `docs/stability.md`, `CHANGELOG.md` 0.7.0,
  `docs/roadmap.md`.
- Polish (T032–T035): version 0.7.0. Fuzzing: `tests/fuzz.py --manifests 5000 --seed 3` (manifest
  lines from valid and broken names, hashes, URLs, versions and `indirect` noise: no crash, only E0703,
  E0704 or E0709 on the manifest) and `tests/package_git.py --fuzz 5000` (mutated packs: no crash,
  nothing written outside the target): 10,000 mutations, 0 problems. `make test` runs 200 manifests.

## Deviations

- **FR-008, SC-002 and US2 scenario 4 amended** (research R4, "Found during implementation"): the
  result of a command is a function of the manifest and the command; it is independent of the order of
  earlier commands only where higher versions require at least what lower ones do.
- Selection lives in `src/packages.c` (the plan named a new `src/pkgselect.c`): it uses the staging,
  hashing, effects and report helpers there.
- Dropping indirect packages that nothing requires any more (research R4 step 4, planned for
  `--repair`) is already done by add: without it the manifest depended on the order of adds.
- Effects of a staged package are read before anything is swapped in (the manifest entries point at the
  staged directories in memory), so `--dry-run` reports them without writing to the project.

- Research R1 said the commit behind an annotated tag is asked for; the tag's own id is asked for
  instead (hosts only promise to serve advertised ids), and the reader follows the tag object.
- T002's `moved=NAME@VERSION` server option was not needed: a changed version is simulated by a manifest
  whose pin differs from the version's content (`version_restore`), the same comparison.
- T006's separate SHA-1 vector check was dropped: every object of the 200 generated packs is looked up
  by the SHA-1 that Python's `hashlib` computed, which fails on any wrong digest.
- `tests/steps.py` has no `sed`, `cmp` or `cat`; cases write manifests with `write` (which now
  substitutes `{port}`) and manifest comparisons are done by `tests/package_graphs.py`.
- The manifest's header comment is unchanged (contracts/manifest.md showed a shorter one): changing it
  would rewrite every existing manifest on its next update.
- A malformed version field or a misplaced `indirect` has its own E0703 message; the message for
  the other malformed lines is as before, so existing outputs do not change.
- E0707 is reported when the round that reads the conflicting requirement ends, with the requirers read
  so far (T018 said "all requirers collected"): a requirer that only a later round would read is not
  listed. When no listed requirer asks the selected version, who set it is appended.
- T017's `cmp` of two manifests is two kept files in one case (`a/vo.packages`, `b/vo.packages`),
  compared once by hand; the generated graphs do the comparison on every run.
- A program's own package that a requirement raises is reported with `(required by ...)`, as T017 asks;
  the command's contract listed that only for indirect packages.
- E0709 names the entry file (`vo package --repair main.vo`), not the word ENTRY: it is the file whose
  import loaded the manifest.
- Repair (and add) replace a directory that no manifest entry accounts for when its content is exactly
  what was fetched for it (a line lost in a merge leaves such a directory); any other content is still
  E0706.
- Repair reports a package fetched again at its pin as `restored`; a directory that is present with
  other content than its pin is E0701 and is not replaced (remove it first).
- T020's `sed` and `cat` are `write` steps with the manifest text.
- `--outdated` asks each package's repository once (T025 said once per distinct source); packages of one
  repository are asked again (`ponytail:` comment in `pkg_outdated`). Below 1.0.0 a higher minor is
  listed as "a new major". With `--json` and nothing newer it prints nothing.
- `--update ENTRY NAME` of an indirect package raises it and leaves it indirect.
- The update case's new version gains `clock`, not `net("rates.example.com")`: it reuses the `graph`
  repositories.
- `--remove --dry-run` of a version-less entry is T0001 (one line changes; nothing to preview).
- T034's tasks are not bench2 tasks of the usual shape (those are answered with code): they have their
  own small harness, `bench2/package_tasks.py`, where the agent answers with one `vo` command per turn.
- T003's baseline time for the 100-package check was not recorded; SC-005 is reported as an absolute
  time.
