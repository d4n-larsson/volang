# Results: Time zones (spec 048)

Status: implemented. `make test` passed on the third run (the first two failed on the cases listed
under "Changed cases" and on the structure check). The gate and the instrumented build were not run.

The zone code is vo, in `std/time.vo` (about 580 lines added). C: `src/tz.c` (130 lines: the two
builtins `__tz` and `__local_tz`), their registration, one recording event (`local_tz`), and the
second line of `vo version`. Data: tzdata 2026f in `third_party/tzdata/zones.bin`, 423,550 bytes.

## SC-001: instants agree with Python's `zoneinfo`

`python3 tests/zones.py --group instants`: for each of the 344 distinct zones, 200 random instants
between 1900 and 2100 plus the second before, at and after every listed change in that span and every
change the rule line makes in 2040 to 2042. Offset, abbreviation, the seven calendar fields, and
`parse_time(format_in(z, ms)) == ok(ms)`: 141,666 answers, 0 differences. Python reads the files
unpacked from `zones.bin`. Years 1 and 9999, outside Python's reach for zones, answer and read back.

Group `data`: the vendored files match `VENDOR.md`; `zone_names()` is the index (597 names, sorted);
every name loads; six wrong names (unknown, empty, `../etc/passwd`, lower case, with a NUL, with the
index's own separator) are errors that quote the name.

## SC-002: local times agree with Python's

`--group local`: per zone 200 random local times plus four around each side of every change: 260,888
answers (178,723 once, 42,018 skipped, 39,803 repeated), 0 differences. Expected values come from
Python's `fold` 0 and 1 answers, kept when they show the time again; for a skipped time the instant
vo gives is checked to be a change whose local time jumps over it.

A deliberate fault (the end of summer time read in standard time) failed both groups (790 and 2,468
differences) and was removed.

## SC-003: damaged files

`--group damaged`: 10,003 files (random bytes; real zones cut, with one byte changed, with a byte of
the second header or the rule line changed, with a run of bytes changed; a version 1 file; a file
with a leap-second count; one real file). Each through `zone_from_bytes` and, when it is a zone, six
instants and four local times. The program ended with status 0: 2,273 read as zones, 7,730 refused
with `invalid zone data: ...`. The first run found one defect: a changed byte could put a newline in
an abbreviation; abbreviations must now be printable ASCII without spaces.

## SC-004: speed

On x86-64, best of 3: every zone loaded in 12 ms (19 ms inside `make test`), 100,000 `in_zone` in
26 ms. Bounds: 1 s each. Not measured under emulation.

## SC-005: size

`build/vo` grew by 464,312 bytes (10,957,280 to 11,421,592). Bound: 600 KB.

## SC-006: three architectures

ARM64 and RISC-V under qemu: `tests/cases/time` 18 of 18, and `tests/zones.py` with 10 instants and
local times per zone and 503 damaged files: 7,597 cases, 0 failures on each. A `FROM scratch`
container was not run; a built program was checked only through `tests/cases/build` (its
`--vo-version` prints the tz release, so the data is in it).

## SC-007: reference, documentation, suite

The reference went from 11,539 to 11,538 words (14,999 tokens by the check's count, budget 15,000).
Added: zones in the `std.time` row (5 words), `local_zone` in the `clock` row (1), the tz release in
the `version` line (3). Removed, wording only: "which is written in vo and built into" to a
parenthesis (3), "The reference does not list its items" to "Its items are not listed here" (1),
"exact" before "signature" (1), "changes nothing and lists" to "only lists" (2), "automatically" (1),
"separate" before "process" (1), "are" in "arguments are passed" (1). Every doc example of `std.time` passes; the guide has a "Time zones" section with two checked
examples.

## Changed cases

- `tests/cases/doc/all.out`, `all_json.out`, `item_record.out`: the module's and `DateTime`'s summary
  lines.
- `tests/cases/cli/version.out`, `version_json.out`, and `tests/cases/build/options/expected/out`:
  the `tzdata` line and field.
- `tests/cases/sleep_steps/replay_changed/expected/out`: a line number inside `std/time.vo` (273 to
  284), nothing else. The tasks did not foresee it.

New: `time/zone_bytes`, `zone_show`, `zone_local`, `zone_fixed`, `zone_fixed_bad` (R0012),
`zone_local_fixed`, `zone_local_missing_effect` (E0501), `check/zone_pure`.

## Deviations from the plan

- **Fat data, not slim** (research R1): the slim output of zic 2.39 is wrong for `America/Ojinaga` in
  the week after 30 October 2022. Found by the first run of the tests. 200 KB more.
- **`fixed_zone(east)`**, not `minutes`: `minutes` is a function of the module and vo has no shadowing.
- **`E0501` case is `time/zone_local_missing_effect`**, beside `now_missing_effect`, not under
  `effects/`.
- **`@allow("W0702")` on `std/time.vo`** (as `std.http` and `std.web`): the file is past 400 code
  lines, and zones were specified as part of `std.time`. The zone reader was split in two to stay
  under 50 statements.
- **`tools/install.sh`** prints the first line of `vo version`; the release workflow's check matches a
  prefix and needed no change.
- **Test-first was not kept for the library code**: the zone functions were written from the planning
  prototype before `tests/zones.py` and the golden cases existed. The tests were then run against
  Python and against a deliberate fault.
- **The archive checksum** is of the file as downloaded twice from data.iana.org; IANA publishes a PGP
  signature, which was not checked. `VENDOR.md` says so.

## Not verified

- The instrumented build and the gate (not asked for). `src/tz.c` compiles with zero warnings under
  the strict set on three architectures.
- `local_zone` on a machine with no `/etc/localtime`, and with `/etc/localtime` as a plain file: the
  code paths exist, the test machine has a link.
- A recording with a `local_tz` event in a serving worker or a task.
- A container with no files but the executable.

## Known limits

- At the exact second of a zone's last listed change, the rule line answers, where Python uses the
  listed type; they agree for every zone in the fat data.
- `from_local` reports at most two instants for a repeated time.
- The data is as old as the toolchain: 2026f.
