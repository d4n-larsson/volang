# Feature Specification: Time zones (local time in std.time)

**Feature Branch**: `048-timezones`

**Created**: 2026-11-23

**Status**: Implemented ([results](results.md))

**Input**: User description: "Time zones (review docs/review-2026-11/ecosystem.md, standard library row
'Time zones': '`std.time` is UTC only; embed the tz database or read the system's under `file.read`;
local time is needed by anything that prints a date for a person'). Decided before specifying: the tz
database is embedded in the binary, and the machine's own zone is available under the `clock` effect.
Must keep: no new syntax, no new effect, determinism, three architectures, the reference under 15,000
tokens, `DateTime` and every existing `std.time` function unchanged for existing programs."

Background: `std.time` turns an instant (milliseconds since 1970 UTC) into UTC calendar fields and ISO
8601 text ending in `Z`. A program that prints "your booking is at 14:00", groups sales by local day, or
runs a job "at 02:30 Stockholm time" has nothing: it cannot know that Stockholm is one hour ahead in
November and two in July. Hand-written offsets are wrong twice a year.

Reading the system's database was the other option. It was declined because an effect scope cannot be
an absolute directory (E0506), so a program would have to be granted every file to read
`/usr/share/zoneinfo`; because a `FROM scratch` container and a `vo build` executable often have no
such directory; and because the same program would answer differently on two machines.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Show an instant as local time in a named zone (Priority: P1)

A developer has an instant and a zone name from configuration (`"Europe/Stockholm"`), and prints the
date and time a person there would read on the wall, with the offset or the abbreviation.

**Why this priority**: it is the gap the review names; everything else builds on it.

**Independent Test**: for every zone name and thousands of random instants between 1900 and 2100, the
local fields, offset and abbreviation are what Python's `zoneinfo` gives from the same data.

**Acceptance Scenarios**:

1. **Given** the name `"Europe/Stockholm"`, **When** the zone is loaded, **Then** the result is a zone
   with that name, with no effect declared: the same on every machine.
2. **Given** that zone and the instant 2026-07-01T12:00:00Z, **When** shown in the zone, **Then** the
   calendar time is 14:00 on 1 July, the offset is two hours, the abbreviation is `"CEST"`, and the ISO
   text is `"2026-07-01T14:00:00+02:00"`; for 2026-12-01T12:00:00Z it is 13:00, one hour, `"CET"`.
3. **Given** a name that is not in the database (`"Europe/Atlantis"`, `""`, `"../etc/passwd"`,
   `"europe/stockholm"`), **When** loaded, **Then** the result is an error that quotes the name; the
   program does not stop and nothing is read from the machine.
4. **Given** an instant after the last change the database lists (the year 2090), **When** shown,
   **Then** the zone's current rule is applied (Stockholm: summer time from the last Sunday of March
   01:00 UTC to the last Sunday of October 01:00 UTC).
5. **Given** an instant before the zone's first recorded change (the year 1700), **When** shown,
   **Then** the zone's earliest offset is used (local mean time), and the ISO text shows an offset with
   seconds (`+00:53:28`: in the current database Stockholm shares Berlin's history) that `parse_time`
   reads back to the same instant.
6. **Given** the list of zone names, **When** asked for, **Then** it is sorted, holds every name of the
   embedded database (zones and their old aliases such as `"US/Eastern"`), and every name in it loads.

---

### User Story 2 - Turn a local calendar time into an instant (Priority: P1)

A developer has "2026-03-29 09:00 in Stockholm" from a form and needs the instant, to store or to
compare with now.

**Why this priority**: input from people is in local time; without this direction the first story only
prints.

**Independent Test**: for every zone and thousands of random local calendar times, the answer agrees
with Python's `zoneinfo` (its `fold` 0 and 1 answers, and a gap detected by the round trip).

**Acceptance Scenarios**:

1. **Given** a zone and a calendar time that happens once there, **When** converted, **Then** the
   result is that instant, and showing the instant in the zone gives the calendar time back.
2. **Given** a calendar time that the zone skips (Stockholm, 2026-03-29 02:30: clocks go from 02:00 to
   03:00), **When** converted, **Then** the result is an error value that says it was skipped and
   carries the instant of the change; no instant is guessed.
3. **Given** a calendar time that happens twice (Stockholm, 2026-10-25 02:30: clocks go back from 03:00
   to 02:00), **When** converted, **Then** the result is an error value that says it is repeated and
   carries both instants, the earlier first; the caller chooses.
4. **Given** calendar fields that are not a time (month 13, 30 February, hour 24), **When** converted,
   **Then** the result is an error value with the same message `to_ms` gives.

---

### User Story 3 - The zone of this machine (Priority: P2)

A developer writes a command-line tool that prints times for whoever runs it, without asking for a
zone name.

**Why this priority**: common for tools, but a server or a report names its zone.

**Independent Test**: run with `TZ=America/New_York`, the program prints New York time; run with a
fixed clock or under `vo test`, it prints UTC; a recording made in one zone replays the same on a
machine in another.

**Acceptance Scenarios**:

1. **Given** a machine whose time settings name a zone, **When** the machine's zone is asked for,
   **Then** the function needs the `clock` effect and returns that zone: the one named by the `TZ`
   variable when that is a name in the embedded database (a leading `:` is allowed), otherwise the
   machine's `/etc/localtime`.
2. **Given** no usable setting (no `TZ`, no `/etc/localtime`, or a file that is not zone data),
   **When** asked for, **Then** the result is UTC: the function does not fail.
3. **Given** a fixed clock (`--clock-ms`, every `vo test`), **When** asked for, **Then** the result is
   UTC whatever the machine says.
4. **Given** a recording of a run, **When** replayed on a machine with other time settings, **Then**
   the output is that of the recorded run.
5. **Given** a function that asks for the machine's zone without declaring `clock`, **When** checked,
   **Then** it is E0501 with the usual fix.

---

### User Story 4 - Fixed offsets and a program's own zone data (Priority: P2)

A developer handles a time "at +05:30" with no zone name, or needs zone rules newer than the toolchain
(a government changed its summer time last month) and ships a zone file with the program.

**Why this priority**: the embedded data is as old as the toolchain; this is the way out that needs no
new release.

**Independent Test**: every file of a system's zone directory, given as bytes, behaves exactly as
Python's `zoneinfo` reading the same file; 10,000 damaged files give an error and never stop the
program.

**Acceptance Scenarios**:

1. **Given** a number of minutes between -1439 and 1439, **When** a fixed zone is made, **Then** its
   offset is always that, its name and abbreviation are the offset as text (`"+05:30"`), and it has no
   skipped or repeated times. A number outside the range is a contract failure.
2. **Given** the bytes of a compiled zone file (TZif version 2 or later) and a name, **When** made into
   a zone, **Then** the zone behaves as the file says, with no effect: reading the file is the
   program's business, under its own `file.read` scope.
3. **Given** bytes that are not such a file (empty, cut short, a version 1 file, a file with leap
   seconds, counts that point outside the data, a rule line that does not parse), **When** made into a
   zone, **Then** the result is an error that says what is wrong; the program never stops with a
   runtime error, whatever the bytes.

---

### User Story 5 - Know how old the data is (Priority: P3)

A developer or an operator asks which release of the tz database a toolchain or a built program holds.

**Acceptance Scenarios**:

1. **Given** the toolchain, **When** `vo version` is run, **Then** the release of the embedded
   database (such as `2026b`) is shown, and is a field of its JSON output.
2. **Given** a program, **When** it asks the module, **Then** it gets the same text.

---

### User Story 6 - Documented, within budget (Priority: P2)

A developer finds zones in the standard-library guide with a worked example; `vo doc std.time` lists
the new functions with examples that are run; the reference changes by one library row and one effect
row.

**Acceptance Scenarios**:

1. **Given** the reference, **When** measured, **Then** it is under 15,000 tokens.
2. **Given** the module's doc comments, **When** the documentation checks run, **Then** every example
   is run and passes.
3. **Given** the guide, **When** read, **Then** it shows loading a zone, printing an instant for a
   person, converting form input with the skipped and repeated cases handled, the machine's zone, and
   what to do when the data is too old.

### Edge Cases

- A zone with no changes at all (`"UTC"`, `"Etc/GMT+5"`) has one offset for every instant.
- A zone whose rule keeps summer time all year, or whose changes cross midnight or the year's end,
  follows the rule as Python does.
- A zone that skipped a whole day (Pacific/Apia, 30 December 2011): every time of that day is skipped.
- An instant exactly at a change belongs to the new offset.
- Offsets that are not whole hours (India +05:30, Nepal +05:45) and, before standard time, not whole
  minutes.
- Negative instants (before 1970) and instants far outside the database (year 1, year 9999) convert
  without overflow.
- The ISO text of an instant whose offset is zero ends in `+00:00`, not `Z`: `format_time` is the UTC
  form.
- A repeated local time across a change of standard offset (not summer time) is still reported as
  repeated.
- `TZ` set to something that is not a name in the database (a POSIX rule string, a path) is ignored.
- A zone value is ordinary data: it can be kept in a record, passed to a worker and compared.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The binary MUST embed a released IANA tz database: every zone and every alias of that
  release, with its release name. Built programs (`vo build`) MUST carry it as the toolchain does.
- **FR-002**: `std.time` MUST export a record `Zone` whose `name` field is the zone's name. Its other
  fields are the zone's data and are not for programs to build by hand.
- **FR-003**: `load_zone(name)` MUST return a result: the zone of that exact name from the embedded
  database, or an error quoting the name. It MUST be pure and MUST NOT read the machine.
- **FR-004**: `zone_names()` MUST return the sorted list of every name `load_zone` accepts;
  `tzdata_version()` MUST return the release name. Both pure.
- **FR-005**: `fixed_zone(minutes)` MUST return the zone that is always `minutes` east of UTC, named by
  its offset text; a value outside -1439 to 1439 is a contract failure.
- **FR-006**: `zone_from_bytes(name, data)` MUST return a result: the zone a TZif file of version 2 or
  later describes, including its rule for instants after its last listed change; an error for anything
  else, including version 1 files and files with leap seconds. It MUST be pure and MUST NOT stop the
  program for any input.
- **FR-007**: `offset_at(zone, ms)` MUST return the zone's offset from UTC at that instant in
  milliseconds (east positive); `abbreviation_at(zone, ms)` its abbreviation there (`"CEST"`).
- **FR-008**: `in_zone(zone, ms)` MUST return the `DateTime` a clock in the zone shows at that instant,
  weekday included.
- **FR-009**: `from_local(zone, d)` MUST return a result with a typed error: the instant when the
  calendar time `d` happens exactly once in the zone; otherwise an error that is one of `invalid`
  (with the message of `to_ms`), `skipped` (with the instant of the change that skips it) or `repeated`
  (with both instants, earlier first). It MUST NOT choose for the caller.
- **FR-010**: `format_in(zone, ms)` MUST return ISO 8601 text of the local time followed by the offset
  as `+HH:MM` or `-HH:MM` (`+HH:MM:SS` when the offset has seconds), with milliseconds only when they
  are not 0. `parse_time` MUST accept an offset with seconds, so that
  `parse_time(format_in(z, ms)) == ok(ms)` for every zone and instant in years 1 to 9999.
- **FR-011**: `local_zone()` MUST need the `clock` effect and return the machine's zone by the order of
  User Story 3; UTC when nothing usable is found or when the clock is fixed. What it returned MUST be
  part of a recording and MUST be given again on replay.
- **FR-012**: Instants before a zone's first change MUST use its earliest offset; instants after its
  last listed change MUST use its rule; a zone with no rule keeps its last offset.
- **FR-013**: All functions except `local_zone` MUST be pure: usable in contracts, properties and
  tests, with the same results on x86-64, ARM64 and RISC-V.
- **FR-014**: `vo version` MUST show the release of the embedded database, as a field added to its
  JSON output.
- **FR-015**: The new functions MUST have doc comments with run examples; the standard-library guide
  MUST gain a section on zones; the reference MUST mention zones in the `std.time` row and
  `local_zone` in the `clock` row, and stay under 15,000 tokens.
- **FR-016**: The embedded database MUST follow the constitution's rules for vendored material:
  pinned release, checksum, public-domain dedication recorded, files unmodified. The contribution
  guide MUST say that a toolchain release takes the newest tz release.
- **FR-017**: No syntax, effect, diagnostic code or runtime code is added. `DateTime`, `from_ms`,
  `to_ms`, `format_time` and every other existing export MUST behave as before; existing golden cases
  MUST be unchanged, except listings of the module and of `vo version`.

### Key Entities

- **Zone**: a name and the rules that give, for every instant, an offset from UTC and an abbreviation.
  A value like any other; made from the embedded database, from bytes, from a fixed offset, or from the
  machine's settings.
- **Instant**: milliseconds since 1970-01-01T00:00:00Z, as everywhere in `std.time`. Never local.
- **Local calendar time**: a `DateTime` read in a zone. In a given zone it names one instant, none
  (skipped) or two (repeated).
- **Embedded database**: one release of the IANA tz database, identified by its release name.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: For every name of the embedded database and 200 random instants each between 1900 and
  2100 (plus the instants one second before, at and after each of the zone's changes), offset,
  abbreviation and local fields equal those of Python's `zoneinfo` on the same data.
- **SC-002**: For every name and 200 random local calendar times each, the answer (once, skipped,
  repeated, and the instants) agrees with Python's.
- **SC-003**: 10,000 damaged zone files (random bytes, truncations and single-byte changes of real
  files) each give either an error or a zone that answers; none stops the program.
- **SC-004**: Loading every zone of the database takes under 1 second in total, and 100,000
  conversions of an instant to local time in one zone take under 1 second, on x86-64.
- **SC-005**: The binary grows by less than 600 KB.
- **SC-006**: A program that prints a local time runs unchanged, with the same output, on the three
  architectures and in a container with no files but the executable.
- **SC-007**: The reference is under 15,000 tokens; every doc example passes; the full test suite
  passes on x86-64, and the module's cases pass on ARM64 and RISC-V.

## Assumptions

- **The data is as old as the toolchain.** Zone rules change a few times a year, nearly always for a
  handful of zones. `tzdata_version` says what is inside; `zone_from_bytes` with a file the program
  ships is the way to be newer without a new toolchain. The toolchain does not read
  `/usr/share/zoneinfo`.
- **`local_zone` is under `clock`**: the machine's time settings are part of its clock, and a program
  that already says `uses clock` to print "now" should not need a second effect to print it for the
  person at the keyboard. It reads the `TZ` variable without the `env` effect for the same reason.
  `/etc/localtime` is used with the machine's own data, since that is what the machine says.
- **A skipped or repeated local time is an error value, not a third kind of success.** The usual call
  site then handles one instant, and the two rare cases cannot be forgotten: the error type has no
  text form, so passing it up means deciding.
- **`DateTime` gains no field.** Adding an offset to the record would break every program that builds
  one. The offset and abbreviation are asked for separately.
- **Offsets are milliseconds**, like every duration in the module.
- **Names are exact**: case matters and nothing is guessed (`"europe/stockholm"` is an error).
- **Version 1 zone files and leap-second (`right/`) files are refused**: nothing has written the first
  since 2005, and the second would silently shift every time by half a minute.
- **Python's `zoneinfo` is the reference** for behaviour, reading the same data.
- Out of scope: month and weekday names, locale formats, `strftime`-style patterns, calendar
  arithmetic ("one month later"), recurring-event rules, `DateTime` with a zone inside, windows zone
  names, reading the system database, a `TZ` POSIX rule string, updating the data without a toolchain
  release other than through `zone_from_bytes`.
