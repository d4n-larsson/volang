# Implementation Plan: Time zones (local time in std.time)

**Branch**: `048-timezones` (work on `main`, as for specs 001–047) | **Date**: 2026-11-23 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/048-timezones/spec.md`

## Summary

Zones are added to `std/time.vo`, written in vo: reading a compiled zone (TZif), its rule line, the
lookups and the conversions. C gains only what vo cannot do: handing out the embedded data, and
reading the machine's setting.

- **The data** is tzdata 2026f compiled once, at vendoring time, with `zic -b fat` (slim was planned
  and found wrong for one zone: R1), and packed into one file, `third_party/tzdata/zones.bin`: 597
  names over 344 distinct zones, 424 KB (R1, R2).
  `tools/embed.py` adds it to `embedded.c` as it does the standard library.
- **Two builtins**, for `std.time` only: `__tz(op, name)`, pure, returns a zone's bytes, or the index;
  `__local_tz()`, under `clock`, returns the machine's zone as a name and bytes, recorded and replayed
  like `__entropy` (R3, R4).
- **No primitive for speed**: a prototype of the reader and the lookup in vo loads all 597 zones in
  12 ms and converts 100,000 instants in 84 ms; the bounds are 1 s each (R5).
- **`from_local`** tries every offset in force within 100,000 seconds of the local time and keeps the
  instants that show it; none is `skipped`, two is `repeated` (R6).
- **The reference has no room**: it measures 15,000 of 15,000 tokens. The seven words this feature
  adds are paid for by wording removed in the same change (R8).

| Part | File | What |
|---|---|---|
| Data | `third_party/tzdata/` (`zones.bin`, `LICENSE`, `version`, `VENDOR.md`), `tools/vendor_tzdata.sh` (new) | fetch, check, compile, pack; `--verify` |
| Embedding | `tools/embed.py`, `Makefile` | `tz_blob`, `tz_blob_len` in `embedded.c` |
| Builtins | `src/tz.c` (new, about 120 lines), `src/vo.h`, `src/check_expr.c`, `src/check_effects.c`, `src/rt_call.c`, `src/replay.c` | `__tz`, `__local_tz`; event kind `local_tz` |
| Version | `src/main.c` | `tzdata 2026f` line; `"tzdata"` field |
| Library | `std/time.vo` | `Zone`, `ZoneRule`, `RuleDay`, `LocalError`, `Repeated`, twelve functions; `parse_time` reads `+HH:MM:SS` |
| Docs | `docs/reference.md` (two rows, net zero), `docs/security.md` (what `clock` now reads), `site/docs/guide/standard-library.md` (section), `CHANGELOG.md`, `CONTRIBUTING.md`, `docs/roadmap.md`, `docs/review-2026-11/README.md`, the replay contract | |
| Tests | `tests/zones.py` (new, in `make test`), `tests/cases/run/zone*`, `tests/cases/effects/local_zone*`, `tests/cases/doc/` listings, `tests/cases/cli/version*`, `tools/quick.sh` (area `zones`) | R7 |

## Technical Context

**Language/Version**: vo for the library (about 450 lines); C11 for the two builtins; Python and sh
for the vendoring script and the tests.

**Primary Dependencies**: IANA tzdata 2026f (public domain), vendored as compiled data. `zic` is
needed only by whoever runs `tools/vendor_tzdata.sh`, never by `make`.

**Storage**: none.

**Testing**: `tests/zones.py` unpacks `zones.bin` into a directory, points Python's `zoneinfo` at it
and compares every zone (SC-001, SC-002); damages files (SC-003); times loading and converting
(SC-004); runs `local_zone` under `TZ`, a fixed clock and record/replay. Golden cases for the
acceptance scenarios, the edge cases, E0501 on `local_zone` and R0012 on `fixed_zone`. Doc examples
under `tests/doc_conformance.py`. `make quick AREA=zones` between edits, `make test` once; the gate
only when asked.

**Target Platform**: Linux x86-64, ARM64, RISC-V 64.

**Project Type**: compiler toolchain with its standard library.

**Performance Goals**: all zones loaded in under 1 s, 100,000 conversions in under 1 s (SC-004);
prototype: 12 ms and 84 ms.

**Constraints**: binary growth under 600 KB (measured: 460 KB); reference at most 15,000 tokens
(15,000 now: net zero words); `DateTime` and existing functions unchanged; no input stops
`zone_from_bytes`.

**Scale/Scope**: about 450 lines of vo, 120 of C, 100 of sh and Python for vendoring, 400 of tests,
80 of guide.

## Constitution Check

| Principle | Check | Status |
|---|---|---|
| I. One form per construct | No syntax. One function per question; `from_local` has no "pick for me" variant. `format_time` (UTC, `Z`) and `format_in` (a zone, numeric offset) answer different questions. | Pass |
| II. Machine-readable feedback | No new code. `vo version --json` gains the field `tzdata`; recordings gain the event kind `local_tz` (additions within schema 1). | Pass |
| III. Determinism and defined behaviour | Everything pure except `local_zone`, which is under `clock`, UTC with a fixed clock, and recorded. `zone_from_bytes` checks every count and index before use: no input reaches R0003. Integer arithmetic only. | Pass |
| IV. Test-first | Golden cases and `tests/zones.py` before the functions they cover. | Pass |
| V. Minimal surface, reference budget | Two internal builtins; the reference changes by zero words net. `ponytail:` notes: linear scan of the name index (597 names), `from_local`'s fixed window. | Pass |
| Vendored material | Pinned release, archive SHA-512, public-domain notice and kept files in `VENDOR.md`. The kept data file is compiled from the release, not a file of it: see Complexity Tracking. | Pass, with one entry |
| C rules | `src/tz.c` under the strict warning set and the instrumented build; no assembly. `/etc/localtime` is read with a size limit of 64 KiB. | Pass |

Re-checked after Phase 1: unchanged.

## Project Structure

### Documentation (this feature)

```text
specs/048-timezones/
├── plan.md
├── research.md          # R1–R9
├── data-model.md
├── quickstart.md
├── contracts/
│   └── timezones.md     # the types, the functions, the builtins, the data file, version, recording
└── tasks.md             # /speckit-tasks
```

### Source Code (repository root)

```text
third_party/tzdata/ (new), tools/vendor_tzdata.sh (new), tools/embed.py, Makefile, tools/quick.sh
src/tz.c (new), src/vo.h, src/check_expr.c, src/check_effects.c, src/rt_call.c, src/replay.c, src/main.c
std/time.vo
docs/reference.md, docs/security.md, site/docs/guide/standard-library.md, CHANGELOG.md, CONTRIBUTING.md
tests/zones.py (new), tests/cases/run/, tests/cases/effects/, tests/cases/doc/, tests/cases/cli/
```

**Structure Decision**: the zone code stays in `std/time.vo` (no new module: no reference row); one
new C file for the two builtins.

## Order of work

1. Vendor: `tools/vendor_tzdata.sh 2026f SHA512` writes `third_party/tzdata/`; `embed.py` and the
   Makefile carry `zones.bin` into the binary.
2. `tests/zones.py` and the golden cases, written and failing.
3. `__tz` (C), then `zone_from_bytes`, `load_zone`, `zone_names`, `tzdata_version`, `fixed_zone`.
4. `offset_at`, `abbreviation_at`, `in_zone`, `format_in`; `parse_time` with seconds in offsets.
5. `from_local`.
6. `__local_tz` (C, recording, replay), `local_zone`.
7. `vo version`; docs with the reference kept at budget; three architectures; `make test`; results.

## Complexity Tracking

| Violation | Why Needed | Simpler Alternative Rejected Because |
|---|---|---|
| The vendored file `zones.bin` is derived from the release (compiled by `zic`, packed by the script), where the constitution says vendored files are kept unmodified | vo reads compiled zones; the release holds rule sources. `VENDOR.md` records the archive checksum, the `zic` version and flags, and the SHA-256 of `zones.bin`; the script reproduces it | Keeping the sources and running `zic` in `make`: every build machine and CI image needs `zic`, and the binary's bytes would depend on its version. Reading the rule sources in vo or C: a second `zic` to write and keep right |
| Two builtins, where spec 044 needed none | embedded data and `/etc/localtime` cannot be reached from vo: effect scopes cannot be absolute (E0506) | `std.fs.read_bytes` on `/usr/share/zoneinfo`: needs an unscoped `file.read` grant in every program and a directory that scratch containers do not have (the spec's Background) |
