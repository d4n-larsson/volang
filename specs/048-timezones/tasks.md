---

description: "Task list for vo-lang spec 048: Time zones (local time in std.time)"
---

# Tasks: Time zones (local time in std.time)

**Input**: Design documents from `specs/048-timezones/`

**Prerequisites**: plan.md, spec.md, research.md (R1–R9), data-model.md, contracts/timezones.md, quickstart.md

**Tests**: REQUIRED (constitution IV). Each story's golden case and its group in `tests/zones.py`
land first and fail before its code.

**Organization**:
- Setup: the data vendored and embedded, the test script, the area wiring.
- Foundational: `__tz`, the types, the zone reader (`zone_from_bytes`), since every story needs a zone.
- US1 (P1) instant to local time; US2 (P1) local time to instant; US4 (P2) fixed offsets and damaged
  files (before US3: it hardens the reader US3 feeds machine data to); US3 (P2) the machine's zone;
  US5 (P3) `vo version`; US6 (P2) documentation, last because it describes everything.
- Polish: three architectures, `make test`, results.

## Format: `[ID] [P?] [Story] Description`

## Conventions

- **Types, signatures, error messages, builtins, the data file, version output, recording**:
  contracts/timezones.md. Fields and limits: data-model.md. Decisions: research.md R1–R9.
- **Starting code**: the planning prototype (reader, rule line, lookup) is in the session scratchpad
  as `proto/tz.vo`; if it is gone, data-model.md and the contract are enough to write it again.
- **Golden cases**: a case needs a `.code` file to be discovered; run one directory per call of
  `tests/run.sh`; accept with `--accept` and review with `git diff`/`git status tests/cases`.
- **No existing golden case may change**, except the `std.time` listings under `tests/cases/doc/`
  and `tests/cases/cli/version.out`, `version_json.out`.
- **Doc comments**: every exported item has one with examples in the form `#     EXPR is VALUE`
  (results as `EXPR == ok(v) is true`); private functions have none.
- **Existing code in `std/time.vo` is not rewritten**: new code reuses `floor_div`,
  `days_from_civil`, `days_in_month`, `is_leap`, `pad`, `from_ms`, `to_ms`.
- `make quick AREA=zones` between edits; `make test` once, in T022; the gate is not run. Never two
  suites at once.

## Phase 1: Setup

- [X] T001 Create `tools/vendor_tzdata.sh` on the pattern of `tools/vendor_sqlite.sh`:
  `vendor_tzdata.sh VERSION SHA512` downloads
  `https://data.iana.org/time-zones/releases/tzdataVERSION.tar.gz`, checks the SHA-512, runs
  `zic -b fat -d DIR (planned as slim; see research.md R1) africa antarctica asia australasia europe northamerica southamerica etcetera
  backward`, packs `third_party/tzdata/zones.bin` in the format of the contract (header
  `VOTZ1 <release> <count>`, `name offset length` lines sorted by the bytes of the name, then the
  distinct files, identical files stored once), copies `LICENSE` and `version`, and writes
  `VENDOR.md` (source URL, archive SHA-512, the public-domain notice quoted from `LICENSE`, zic
  version and arguments, `- \`FILE\` sha256 HASH` lines for the three kept files, the update
  rule: every toolchain release takes the newest tz release). `--verify` checks the kept files
  against `VENDOR.md`. Run it for 2026f (SHA-512 from <https://www.iana.org/time-zones>, compared
  with the downloaded archive); expect 597 names.
- [X] T002 `tools/embed.py`: one more leading argument, the path of `zones.bin`, written as
  `const unsigned char tz_blob[]` and `const long tz_blob_len`; `Makefile`: pass
  `third_party/tzdata/zones.bin` and make `embedded.c` depend on it; declare both in `src/vo.h`.
  `make` builds; `ls -l build/vo` grows by less than 600 KB (SC-005; note the figure).
- [X] T003 Create `tests/zones.py` on the pattern of `tests/decimals.py` (`--group`, temp directory
  under `build/tmp`), groups `data`, `instants`, `local`, `damaged`, `machine`, `speed` as in
  research.md R7. `data` unpacks `zones.bin` by the contract's format into a directory of TZif files
  (used by the other groups through `zoneinfo.reset_tzpath`), runs `tools/vendor_tzdata.sh --verify`,
  and checks through a vo driver that `zone_names()` equals the index, every name loads and
  `tzdata_version()` is the header's release. Add `zones.py` to the test list in `Makefile` and area
  `zones` to `tools/quick.sh` (golden areas `time run check effects replay cli doc`; scripts
  `ref_examples.py doc_conformance.py zones.py`; file mapping for `std/time.vo`, `src/tz.c`,
  `tests/zones.py`, `third_party/tzdata/`, `tools/vendor_tzdata.sh`). All groups fail now.

## Phase 2: Foundational

- [X] T004 `__tz(op: int, name: str) -> result[bytes]` (contract, research.md R3): `B_TZ` in
  `src/vo.h`, its name in the table of `src/check_expr.c` (usable from `std/time.vo` only, as
  `__sleep`), no effect, dispatch in `src/rt_call.c`, body in a new `src/tz.c`: `op` 1 scans the
  index lines of `tz_blob` for `name` followed by a space and returns that slice of the data, or
  `err("unknown")`; `op` 2 returns the header and index text; any other `op` is `err`. A name that
  is empty or holds a space, newline or NUL finds nothing. Mark the scan
  `ponytail: linear over the index, binary search if the index grows past a few thousand names`.
  Builds with zero warnings.
- [X] T005 In `std/time.vo`: the exported records and variant of the contract (`Zone`, `ZoneRule`,
  `RuleDay`, `Repeated`, `LocalError`), each with a doc comment; `Zone`'s says that only `name` is
  for programs and that zones are made by `load_zone`, `fixed_zone`, `zone_from_bytes` and
  `local_zone`. Update the file's header comment (UTC and zones).
- [X] T006 Golden case `tests/cases/time/zone_bytes` (through `load_zone`, so it fails until T007 and
  T008): a zone with no changes (`UTC`, `Etc/GMT+5`), one with a rule, one alias; printed `name`,
  number of `starts`, first and last `offsets`.
- [X] T007 `zone_from_bytes(name, data)` in `std/time.vo` with every limit of data-model.md, quoted:
  version byte `2` or above; leap-second records 0; time types 1 to 256, offset within ±93,600 s,
  abbreviation inside the table, ended by a zero byte, UTF-8; listed changes at most 100,000,
  strictly ascending, within ±2^62 s, type index below the number of types; every table inside the
  bytes; rule line between two newlines at the end, empty or the grammar of ZoneRule (`<...>` or
  three or more letters for names; offsets `[+-]h[h][:mm[:ss]]` up to 24 h, west-positive in the
  text; `Mm.w.d` with month 1 to 12, week 1 to 5, weekday 0 to 6; `Jn` 1 to 365; `n` 0 to 365;
  `/time` within ±167 h, 7200 s when absent; a summer name with no dates is an error). Errors are
  `invalid zone data: REASON`. No index is read before it is checked against `len(data)`.
- [X] T008 `load_zone(name)` (`__tz(1, name)` then `zone_from_bytes`; error
  `unknown time zone: NAME`), `zone_names()` and `tzdata_version()` (from `__tz(2, "")`) in
  `std/time.vo`. T006 and group `data` of `tests/zones.py` pass.

## Phase 3: User Story 1 - Show an instant as local time in a named zone (P1)

**Goal**: `offset_at`, `abbreviation_at`, `in_zone`, `format_in`.

**Independent test**: `python3 tests/zones.py --group instants` agrees with Python's `zoneinfo` for
every zone (SC-001).

- [X] T009 [US1] Golden case `tests/cases/time/zone_show`: spec US1 scenarios 2 to 6 and the edge
  cases that concern showing (an instant exactly at a change, India and Nepal, year 1 and year 9999,
  1700 with `+00:53:28`, `+00:00` for a zero offset, `parse_time(format_in(z, ms)) == ok(ms)`); and
  US1 scenario 3 (unknown names, including `""`, `"../etc/passwd"`, `"europe/stockholm"`). Group
  `instants` in `tests/zones.py`: per zone 200 random instants between 1900 and 2100 plus one second
  before, at and after each listed change in that span; a vo driver prints offset, abbreviation and
  the fields of `in_zone`; compare with `zoneinfo`. Both fail now.
- [X] T010 [US1] In `std/time.vo`: private `segment` (binary search), `rule_instant`, `rule_is_dst`
  (data-model.md: both days in the UTC year of the instant; `start <= t < end` if `start < end`,
  otherwise not `end <= t < start`), then `offset_at` (milliseconds), `abbreviation_at`, `in_zone`.
  Instants are floored to seconds for the lookup.
- [X] T011 [US1] `format_in(z, ms)` and the offset text shared with `fixed_zone` (`+HH:MM`,
  `+HH:MM:SS` only when seconds are not 0); `parse_time` accepts an offset with `:SS` (00 to 59) and
  nothing else new; `tests/cases/time/` existing cases unchanged. T009 passes.

## Phase 4: User Story 2 - Turn a local calendar time into an instant (P1)

**Goal**: `from_local`.

**Independent test**: `python3 tests/zones.py --group local` agrees with Python for every zone
(SC-002).

- [X] T012 [US2] Golden case `tests/cases/time/zone_local`: spec US2 scenarios 1 to 4 with
  Stockholm 2026-03-29 02:30 (skipped, with the change instant) and 2026-10-25 02:30 (repeated,
  earlier first), invalid fields with the messages of `to_ms`, a southern zone, Apia on 30 December
  2011, a repeated time at a change of standard offset, and the law
  `from_local(z, in_zone(z, ms))` of the contract. Group `local` in `tests/zones.py`: per zone 200
  random local times plus times around each listed change; expected from `zoneinfo` with `fold` 0
  and 1 (instants that show the time again: one is once, two are repeated, none is skipped). Both
  fail now.
- [X] T013 [US2] `from_local(z, d)` in `std/time.vo` exactly as research.md R6 (window 100,000 s;
  listed changes in the window plus the rule's changes for the three years; the two earliest
  instants when repeated; the jumping change when skipped; `d.ms` added; `invalid` from `to_ms`).
  Mark the window `ponytail: fixed window above the largest offset; exact for every change in the
  data`. T012 passes.

## Phase 5: User Story 4 - Fixed offsets and a program's own zone data (P2)

**Goal**: `fixed_zone`; `zone_from_bytes` proven against any bytes.

**Independent test**: `python3 tests/zones.py --group damaged` ends with status 0 for 10,000 files
(SC-003).

- [X] T014 [US4] Golden cases `tests/cases/time/zone_fixed` (US4 scenario 1; `from_local` on a fixed
  zone is always once), `tests/cases/time/zone_fixed_bad` (R0012 for 1440), and
  `tests/cases/check/zone_pure` (a zone function in a `requires` clause and in a `prop`). Group
  `damaged` in `tests/zones.py`: 10,000 files (random bytes; real files cut at every kind of
  boundary; real files with one byte changed; a version 1 file; a file from `right/` made with
  `zic -L` if `zic` is present, else a hand-built leap count), read by a driver with a relative
  `file.read` scope, each through `zone_from_bytes` and, on `ok`, `offset_at`, `in_zone`,
  `format_in` and `from_local` at ten fixed instants; the run ends with status 0 and prints the
  counts of `ok` and `err`.
- [X] T015 [US4] `fixed_zone(minutes)` in `std/time.vo` with its `requires` clause (private predicate
  named for the limit, as `places_0_to_18` in `std/decimal.vo`). Fix whatever group `damaged` finds
  in T007's checks. T014 passes.

## Phase 6: User Story 3 - The zone of this machine (P2)

**Goal**: `local_zone` under `clock`, recorded and replayed.

**Independent test**: `python3 tests/zones.py --group machine`.

- [X] T016 [US3] Golden cases `tests/cases/effects/local_zone` (E0501 with its fix when `clock` is
  not declared) and `tests/cases/time/zone_local_fixed` (with `--clock-ms 0`: name `UTC`, offset 0).
  Group `machine` in `tests/zones.py`: `TZ=America/New_York`, `TZ=:Asia/Kolkata`, `TZ=nonsense`
  (falls to `/etc/localtime`: compare with `zoneinfo` on that file when it exists, else UTC),
  `--clock-ms`, and a recording made under `TZ=Asia/Tokyo` replayed under `TZ=UTC` printing Tokyo
  both times. All fail now.
- [X] T017 [US3] `__local_tz() -> bytes` (research.md R3, R4): `B_LOCAL_TZ` in `src/vo.h`, name in
  `src/check_expr.c`, `[B_LOCAL_TZ] = EF_CLOCK` in `src/check_effects.c`, dispatch in
  `src/rt_call.c`; `rt_local_tz` in `src/tz.c` in the order: fixed clock gives no bytes; `TZ` (after
  an optional `:`) naming an index entry gives that name and the embedded bytes; `/etc/localtime`
  read directly, at most 64 KiB, named by its link target after `zoneinfo/` when that is valid UTF-8
  without a newline, otherwise `local`; else no bytes. `io_local_tz` in `src/replay.c` on the
  pattern of `io_entropy` with a new `EVK_LOCAL_TZ` (`"local_tz"` in the kind names, bytes written
  and read back like `entropy`). `local_zone()` in `std/time.vo`: split at the first newline,
  `zone_from_bytes`; no bytes or an error gives the zero fixed zone named `UTC`. Zero warnings;
  T016 passes; `tests/run.sh tests/cases/replay` unchanged.

## Phase 7: User Story 5 - Know how old the data is (P3)

- [X] T018 [US5] `src/main.c`: `vo version` prints `tzdata <release>` as a second line and adds
  `"tzdata":"<release>"` to the JSON record, read from the header of `tz_blob`; `vo_version_text`
  in `src/build.c` is not changed. Accept `tests/cases/cli/version.out` and `version_json.out`;
  check that `vo build` still accepts its own base (`tests/run.sh tests/cases/build`). Update the
  `version` record in the schema section of `docs/reference.md` only if it lists the fields (count
  the words under T019).

## Phase 8: User Story 6 - Documented, within budget (P2)

- [X] T019 [US6] `docs/reference.md` (research.md R8): the `std.time` row becomes
  ``calendar times in UTC and zones (`load_zone`, `in_zone`), ISO 8601 text, durations, `now`
  (clock)``; the `clock` row gains `, std.time.local_zone`; remove at least as many words elsewhere
  by tightening wording without removing a fact, and list the changed sentences and the word count
  before and after in results.md. `python3 tests/ref_examples.py` passes (at most 15,000).
- [X] T020 [P] [US6] `site/docs/guide/standard-library.md`: a section on zones under `std.time`
  showing the five things of spec US6 scenario 3 (the quickstart example as the base);
  `python3 tests/site_examples.py` and `make site-check` pass. `docs/security.md`: `clock` also
  reads the machine's time zone setting (`TZ`, `/etc/localtime`) and nothing else of either; a
  recording holds it. The replay contract document that lists event kinds gains `local_tz`.
- [X] T021 [P] [US6] `CHANGELOG.md`; `CONTRIBUTING.md` (a toolchain release takes the newest tz
  release: `tools/vendor_tzdata.sh`); `docs/roadmap.md` (row and section "048: time zones");
  `docs/review-2026-11/README.md` (row "Time zones (ecosystem, standard library)"). Accept the
  `std.time` listings under `tests/cases/doc/`; `python3 tests/doc_conformance.py` passes.

## Phase 9: Polish

- [X] T022 Group `speed` of `tests/zones.py` (SC-004: all zones loaded under 1 s, 100,000
  `in_zone` under 1 s; skipped when `CI` is set or under emulation). Three architectures:
  `make ARCH=aarch64` and `make ARCH=riscv64`; with `VO_BIN` set, run `tests/run.sh tests/cases/time`
  and `python3 tests/zones.py` on each (SC-006: same output).
- [X] T023 `python3 tools/progress.py run test` once; read `build/progress/test.log`; fix and rerun
  only on failure. The instrumented build of `src/tz.c` only if the gate is asked for: say so in
  results.md if it was not run.
- [X] T024 Write `specs/048-timezones/results.md` (SC-001 to SC-007 with figures, the binary's
  growth, the reference's words before and after, changed cases, deviations, what was not verified,
  such as a `FROM scratch` container if none was run); set the spec's status to Implemented.

## Dependencies

- T001 → T002 → T003, T004 → T005 → T006 → T007 → T008 → every story.
- US1 (T009–T011) → US2 (T012, T013): `from_local` uses `offset_at`.
- US4 (T014, T015) needs T011 for the offset text and T013 for the driver's `from_local` calls.
- US3 (T016, T017) needs T015 (`fixed_zone` for UTC) and is otherwise independent of US1 and US2.
- US5 (T018) needs only T002.
- US6 (T019–T021) needs every function. T020 and T021 touch different files and can run in parallel
  after T019.

## Parallel example

After T019: T020 (`site/`, `docs/security.md`) and T021 (`CHANGELOG.md`, `CONTRIBUTING.md`,
`docs/roadmap.md`, `docs/review-2026-11/README.md`, `tests/cases/doc/`) together. Nothing else is
parallel: almost every task edits `std/time.vo`, and suites are never run at the same time.

## Implementation strategy

MVP is Phase 1, Phase 2 and US1: a program can print an instant for a person in a named zone. US2
makes input possible. Stop after any phase with `make quick AREA=zones` green.
