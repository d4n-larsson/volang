# Results: Package Manager

## Setup

- Baseline (commit `8f82807`, x86-64): the reference is 11,535 words (~14,995 tokens of 15,000).

## SC-001 to SC-009

Pending (measured in T035).

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
