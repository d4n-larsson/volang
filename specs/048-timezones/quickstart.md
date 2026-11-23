# Quickstart: checking time zones

```sh
tools/vendor_tzdata.sh --verify   # third_party/tzdata matches VENDOR.md
make
make quick AREA=zones             # golden cases, doc examples, tests/zones.py
python3 tests/zones.py            # groups data, instants, local, damaged, machine, speed
python3 tests/ref_examples.py     # the reference: at most 15,000 tokens
bin/vo version                    # second line: tzdata 2026f
make test                         # once, at the end
```

By hand:

```text
import std.time.DateTime;
import std.time.date;
import std.time.format_in;
import std.time.from_local;
import std.time.hours;
import std.time.in_zone;
import std.time.load_zone;

fn main() -> result[void] uses console {
    let z = try load_zone("Europe/Stockholm");
    let july = try date(2026, 7, 1);
    print(format_in(z, july + hours(12)));
    let march = try date(2026, 3, 29);
    print(str(in_zone(z, march + hours(1)).hour));
    let half_past_two = new DateTime { year: 2026, month: 3, day: 29, hour: 2, minute: 30, second: 0, ms: 0, weekday: 0 };
    match from_local(z, half_past_two) {
        case ok(ms) {
            print(format_in(z, ms));
        }
        case err(_) {
            print("not one instant");
        }
    }
    return ok();
}
```

Expected: `2026-07-01T14:00:00+02:00`, then `3` (01:00 UTC on 29 March is 03:00 in Stockholm: the
clocks have just gone forward), then `not one instant` (02:30 that night is skipped).

The machine's zone:

```sh
TZ=America/New_York bin/vo run local.vo     # local_zone().name is America/New_York
bin/vo run --clock-ms 0 local.vo            # UTC
TZ=Asia/Tokyo bin/vo run --record r.json local.vo && TZ=UTC bin/vo replay r.json   # Tokyo both times
```

A container with nothing but the program: `vo build main.vo -o app`, then a `FROM scratch` image with
`app`; `load_zone` works with no `/usr/share/zoneinfo`.

Other architectures: `make ARCH=aarch64`, then
`VO_BIN=$PWD/build-aarch64/vo tests/run.sh tests/cases/run` and `VO_BIN=... python3 tests/zones.py`;
the same for `riscv64`.

Updating the data: `tools/vendor_tzdata.sh 2027a SHA512` (the checksum from
<https://www.iana.org/time-zones>), `make test`, commit `third_party/tzdata/`.
