# Research: Time zones

## R1: which data, in which form

**Decision**: tzdata 2026f (the newest release on 2026-11-23), the files `africa`, `antarctica`,
`asia`, `australasia`, `europe`, `northamerica`, `southamerica`, `etcetera` and `backward`, compiled
with `zic -b fat`. Not `backzone`, `factory`, `posix/`, `right/` or `posixrules`.

**Rationale**: measured with zic 2.39:

| Form | Names | Distinct zones | Bytes of distinct zones |
|---|---|---|---|
| slim | 597 | 344 | 205,999 |
| fat | 597 | 344 | 407,873 |

A slim file lists the changes up to the point where the zone's rule line takes over; a fat file
repeats the rule's changes to 2037. The plan chose slim for its size. **Changed during
implementation**: the slim output of this `zic` is wrong. For `America/Ojinaga` its last listed
change (30 October 2022, to CST) is contradicted by its rule line, which puts the following week in
summer time; `zdump` on the slim file shows CDT for that week and on the fat file CST, which is what
the source says. The first run of `tests/zones.py` found it as one instant where Python and vo read
the same file differently. Fat is the form this `zic` has always written and the form Linux
distributions install; it costs 200 KB more and stays under SC-005. The rule line is still read:
it holds after 2037.

`backward` gives the old names people still have in configuration (`US/Eastern`, `Europe/Kiev`).

**Alternatives considered**: `tzdata.zi` (114 KB of rule source): smaller, but reading it is writing
`zic`. Building a newer `zic` from tzcode, whose slim output is right: one more thing to vendor and
build for a size nobody has asked for. Leaving out `backward`: 250 names fewer, 10 KB saved, and
configuration files that worked everywhere else fail.

## R2: how the data gets into the binary

**Decision**: `tools/vendor_tzdata.sh VERSION SHA512` downloads
`https://data.iana.org/time-zones/releases/tzdataVERSION.tar.gz`, checks the SHA-512, runs `zic`,
and packs the result into `third_party/tzdata/zones.bin` (format in the contract: a text index of
`name offset length` lines sorted by name, then the distinct files end to end). It keeps `LICENSE`
and `version` from the archive and writes `VENDOR.md` (archive checksum, zic version and arguments,
SHA-256 of each kept file). `--verify` checks the kept files against `VENDOR.md`, like
`vendor_sqlite.sh`. `tools/embed.py` takes `zones.bin` as one more input and writes `tz_blob` and
`tz_blob_len`.

**Rationale**: `make` needs nothing new and gives the same binary everywhere. Updating is one command
and a commit of one file of about 230 KB, a few times a year.

**Alternatives considered**: `zic` in `make` (see the plan's Complexity Tracking). 597 files in the
repository: a tree of small binaries, and `embed.py` would have to find the duplicates itself.

## R3: what C does

**Decision**: one new file, `src/tz.c`, and two builtins usable only from `std/time.vo`.

- `__tz(op: int, name: str) -> result[bytes]`, pure. `op` 1: the TZif bytes of `name`
  (`err("unknown")` when absent); `op` 2: the index text (first line `VOTZ1 VERSION COUNT`, then the
  names). The lookup is a scan of the index lines for `name` followed by a space: a name with a space,
  a newline or a NUL finds nothing, and no name reaches a path.
- `__local_tz() -> bytes`, `clock` (`[B_LOCAL_TZ] = EF_CLOCK`). Returns `NAME\nTZIF` or nothing:
  1. with a fixed clock (`opt_clock_set`): nothing;
  2. `TZ` set, after an optional `:`, to a name in the index: that name and the embedded bytes;
  3. `/etc/localtime` readable, at most 64 KiB: its bytes, named by the part of its link target after
     `zoneinfo/` when it is a link with such a part and that part is valid UTF-8 without a newline,
     otherwise `local`;
  4. otherwise nothing.
  `local_zone` in vo splits at the first newline and calls `zone_from_bytes`; nothing, or an error
  from it, gives `fixed_zone(0)` named `UTC`.

**Rationale**: the smallest C that vo cannot replace. All judgement about the bytes stays in one
place, `zone_from_bytes`, which the fuzz test covers.

**Alternatives considered**: looking up and parsing in C: a second parser, and `zone_from_bytes`
would still be needed. One builtin with the effect decided by `op` (as `__fsb`): saves an id, costs
a special case in the effect checker.

## R4: recording and replay

**Decision**: `io_local_tz()` in `src/replay.c`, shaped like `io_entropy`: when recording, the bytes
are written as an event of a new kind `local_tz` (`EVK_LOCAL_TZ`); on replay the event's bytes are
returned and the machine is not read. `/etc/localtime` and `TZ` are read by the builtin directly, not
through the `file.read` or `env` checks: the grant is `clock`.

**Rationale**: FR-011, and the existing rule that a recording holds everything a program received.
`docs/security.md` gains a line: `clock` also reads the machine's time zone setting (`TZ`,
`/etc/localtime`), and nothing else of either.

## R5: is a primitive needed for speed?

**Decision**: no.

**Rationale**: a prototype in vo (the TZif reader with every check, the rule-line reader, binary
search over the changes, the rule evaluation) on the slim files of 2026f, x86-64:

| Measure | Result | Bound (SC-004) |
|---|---|---|
| read and build all 597 zones | 12 ms | 1 s |
| 100,000 × instant → local `DateTime` in Europe/Stockholm (all through the rule) | 84 ms | 1 s |

With the fat data and the final code: 12 ms and 26 ms (the instants fall among listed changes).

The rule line is read once, when the zone is made, into a `ZoneRule` record; evaluating it per
instant is two day computations.

## R6: `from_local`

**Decision**: with `L` the calendar time as seconds read as if UTC, and W = 100,000 s (more than the
largest offset, 26 h):

1. `changes`: the zone's changes in `(L - W, L + W)`: listed ones by binary search; when that span
   reaches past the last listed change and the zone has a rule with summer time, the rule's two
   changes and the first second of each year from the year of `L - W` to that of `L + W` (the rule is
   evaluated per UTC year, so a year's start is where an odd rule could change).
2. `offsets`: the offset at `L - W` and at each of `changes`.
3. `instants`: sorted, distinct `L - o` for `o` in `offsets` with `offset_at(L - o) == o`.
4. One: `ok`. Two or more: `repeated(first, second)` (the two earliest; three would need two
   fall-backs within 28 hours, which no zone has had). None: `skipped(c)`, with `c` the change in
   `changes` whose local time jumps over `L` (`L - offset before c >= c` and `L - offset at c < c`).
5. The answer is in milliseconds, plus `d.ms`. Invalid fields: `invalid(message)` from `to_ms`.

**Rationale**: exact for every change in the data, including a skipped day (Apia 2011, 24 h) and
changes of standard offset; no case analysis by kind of change.

**Alternatives considered**: the two-probe trick (offsets at `L ± 1 day`): misses a zone with two
changes inside the window. Python's `fold` model: it picks for the caller, which the spec rules out.

## R7: tests

**Decision**:
- `tests/zones.py`, groups:
  - `data`: unpacks `zones.bin` by its documented format into a temporary directory;
    `tools/vendor_tzdata.sh --verify`; `zone_names()` equals the index; every name loads.
  - `instants` (SC-001) and `local` (SC-002): Python writes query lines per zone, one vo driver
    answers, Python compares with `zoneinfo` after `reset_tzpath` to the unpacked directory. For
    `local`: Python's `fold=0` and `fold=1` instants, classed by whether they show the time again.
  - `damaged` (SC-003): 10,000 files (random bytes; real files cut, and with one byte changed); a
    driver calls `zone_from_bytes` and, on `ok`, `offset_at`, `in_zone`, `format_in` and `from_local`
    at fixed instants; the run must end with status 0.
  - `machine`: `TZ=America/New_York`, `TZ=:Asia/Kolkata`, `TZ=nonsense`, `--clock-ms`, and a
    recording made under one `TZ` replayed under another.
  - `speed` (SC-004), skipped when `CI` is set or under emulation, as in `tests/bits.py`.
- Golden cases: `run/zone` (the acceptance scenarios, printed), `run/zone_local_errors` (skipped,
  repeated, invalid), `run/zone_fixed_bad` (R0012), `run/time` unchanged, `effects/local_zone`
  (E0501 and its fix), `check/zone_pure` (in a contract and a property), `cli/version*`, and the
  module listing in `doc/`.
- `tests/zones.py` uses Python's `zoneinfo` (3.9 or later; 3.12 here).

## R8: the reference budget

**Decision**: the reference is at 11,539 words, which the check counts as 15,000 tokens of 15,000:
one more word fails it. The change is

- `std.time` row: `UTC calendar times, ...` becomes `calendar times in UTC and zones (`load_zone`,
  `in_zone`), ...` (6 words more);
- `clock` row: `, std.time.local_zone` (1 word more);
- 7 words or more removed elsewhere by tightening wording, with no fact removed; the task names the
  sentences when it is done, and the change states the count (constitution, quality gates).

**Rationale**: the functions themselves are in `vo doc std.time`, as for every module.

## R9: `vo version` and built programs

**Decision**: the text form prints a second line, `tzdata 2026f`; the JSON record gains
`"tzdata":"2026f"`. The constant `vo_version_text` is not changed: `src/build.c` compares it to
recognise a base of the same version. A built program is a copy of the toolchain binary, so it holds
the data with no further work; a `--base` of another architecture holds its own copy.

## Findings while planning

- In tzdata 2026f `Europe/Stockholm` is an alias of `Europe/Berlin`, so its time before 1893 is
  Berlin's local mean time (+00:53:28), not Stockholm's (+01:12:12). The spec's example was corrected.
- This machine's `/usr/share/zoneinfo` is older; the tests do not use it.
- A changed byte in a zone file could put a newline in an abbreviation, which a program then prints.
  Abbreviations must be printable ASCII without spaces (data-model.md).
- `minutes` and `seconds` are functions of `std.time` and vo has no shadowing: `fixed_zone`'s
  parameter is `east`.
- `vo version` gained a line, so `tools/install.sh` prints only the first.
