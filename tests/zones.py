#!/usr/bin/env python3
"""Spec 048: time zones in std.time against Python's zoneinfo, on the same data.

  zones.py                 every group (part of `make test`)
  zones.py --group NAME    one group: data, instants, local, damaged, machine, speed

data      third_party/tzdata matches VENDOR.md; zone_names() is the index; every name loads
instants  per zone: offset, abbreviation and local fields at random instants and around every change (SC-001)
local     per zone: from_local at random local times and around every change (SC-002)
damaged   10,000 damaged zone files: an error or a zone that answers, never a stopped program (SC-003)
machine   local_zone under TZ, a fixed clock, and record then replay under another TZ
speed     all zones loaded, and 100,000 conversions, each under 1 s on x86-64 (SC-004)

Python reads the zones unpacked from third_party/tzdata/zones.bin (the format is in
specs/048-timezones/contracts/timezones.md), so both sides have the same release.
"""
import os
import random
import struct
import subprocess
import sys
import tempfile
import zoneinfo
from datetime import datetime, timedelta, timezone

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")
BLOB = os.path.join(ROOT, "third_party", "tzdata", "zones.bin")
SLOW = 20 if any(a in os.environ.get("VO_BIN", "") for a in ("aarch64", "riscv64")) else 1
PER_ZONE = 200 if SLOW == 1 else 10
EPOCH = datetime(1970, 1, 1)
UTC = timezone.utc
Y1900 = int((datetime(1900, 1, 1) - EPOCH).total_seconds())
Y2100 = int((datetime(2100, 1, 1) - EPOCH).total_seconds())

failures = []
cases = 0

# One line per query; `Z name` and `B path` choose the zone for the lines after them.
DRIVER = """import std.fs;
import std.time;

fn local_answer(z: time.Zone, d: time.DateTime) -> str {
    match time.from_local(z, d) {
        case ok(ms) {
            return "once " + str(ms);
        }
        case err(e) {
            match e {
                case invalid(m) {
                    return "invalid " + m;
                }
                case skipped(c) {
                    return "skipped " + str(c);
                }
                case repeated(r) {
                    return "repeated " + str(r.first) + " " + str(r.second);
                }
            }
        }
    }
}

fn instant_answer(z: time.Zone, ms: int) -> str {
    let d = time.in_zone(z, ms);
    var back = "bad";
    if time.parse_time(time.format_in(z, ms)) == ok(ms) {
        set back = "back";
    }
    return format("{} {} {} {} {} {} {} {} {} {}", time.offset_at(z, ms), time.abbreviation_at(z, ms), d.year, d.month, d.day, d.hour, d.minute, d.second, d.weekday, back);
}

fn main() -> result[void] uses console, file.read {
    var z = time.offset_zone(0);
    for line in split(try read_file("cases.txt"), "\\n") {
        let p = split(line, " ");
        if p[0] == "Z" {
            match time.load_zone(slice(line, 2, len(line))) {
                case ok(v) {
                    set z = v;
                    print("Z ok " + v.name);
                }
                case err(e) {
                    print("Z " + e);
                }
            }
        } else if p[0] == "B" {
            match time.zone_from_bytes(p[1], try fs.read_bytes(p[1])) {
                case ok(v) {
                    set z = v;
                    print("B ok");
                }
                case err(e) {
                    set z = time.offset_zone(0);
                    print("B " + e);
                }
            }
        } else if p[0] == "I" {
            print(instant_answer(z, try parse_int(p[1])));
        } else if p[0] == "L" {
            print(local_answer(z, new time.DateTime { year: try parse_int(p[1]), month: try parse_int(p[2]), day: try parse_int(p[3]), hour: try parse_int(p[4]), minute: try parse_int(p[5]), second: try parse_int(p[6]), ms: try parse_int(p[7]), weekday: 0 }));
        } else if p[0] == "N" {
            print(join(time.zone_names(), " "));
            print(time.tzdata_version());
        }
    }
    return ok();
}
"""

LOCAL = """import std.time;

fn main() uses console, clock {
    let z = time.local_zone();
    print(z.name + " " + time.format_in(z, 1782907200000));
}
"""

SPEED = """import std.time;

fn main() -> result[void] uses console, clock {
    let t0 = now_ms();
    var z = time.offset_zone(0);
    for name in time.zone_names() {
        set z = try time.load_zone(name);
    }
    let t1 = now_ms();
    set z = try time.load_zone("Europe/Stockholm");
    var sum = 0;
    var i = 0;
    while i < 100000 {
        set sum = sum + time.in_zone(z, 1700000000000 + i * 3600000).hour;
        set i = i + 1;
    }
    print(format("{} {} {}", t1 - t0, now_ms() - t1, sum));
    return ok();
}
"""


def case(name, ok, detail=""):
    global cases
    cases += 1
    if not ok:
        failures.append(name)
        print(f"FAIL {name}: {detail}")


def unpack(d):
    """zones.bin -> d/NAME files; the names in index order and the release."""
    blob = open(BLOB, "rb").read()
    head, rest = blob.split(b"\n", 1)
    magic, release, count = head.split(b" ")
    assert magic == b"VOTZ1"
    lines = rest.split(b"\n", int(count))
    data = lines[-1]
    names = []
    for line in lines[:-1]:
        name, off, size = line.split(b" ")
        path = os.path.join(d, name.decode())
        os.makedirs(os.path.dirname(path), exist_ok=True)
        with open(path, "wb") as f:
            f.write(data[int(off):int(off) + int(size)])
        names.append(name.decode())
    return names, release.decode()


def zones_dir(d):
    z = os.path.join(d, "zones")
    names, release = unpack(z)
    zoneinfo.reset_tzpath(to=[z])
    zoneinfo.ZoneInfo.clear_cache()
    return z, names, release


def distinct(z, names):
    """One name per distinct file."""
    seen, out = set(), []
    for n in names:
        body = open(os.path.join(z, n), "rb").read()
        if body not in seen:
            seen.add(body)
            out.append(n)
    return out


def listed_changes(path):
    """The change instants a TZif file lists (its 64-bit block)."""
    b = open(path, "rb").read()
    c = struct.unpack(">6l", b[20:44])
    h = 44 + c[3] * 5 + c[4] * 6 + c[5] + c[2] * 8 + c[1] + c[0]
    times = struct.unpack(">6l", b[h + 20:h + 44])[3]
    return list(struct.unpack(f">{times}q", b[h + 44:h + 44 + times * 8]))


def offset(zi, t):
    return int((EPOCH + timedelta(seconds=t)).replace(tzinfo=UTC).astimezone(zi).utcoffset().total_seconds())


def rule_changes(zi, lo, hi):
    """The instants in lo..hi at which Python's offset or abbreviation changes, found by bisection."""
    def key(t):
        dt = (EPOCH + timedelta(seconds=t)).replace(tzinfo=UTC).astimezone(zi)
        return dt.utcoffset(), dt.tzname()
    out = []
    t = lo
    while t < hi:
        if key(t) != key(t + 86400 * 5):
            a, b = t, t + 86400 * 5
            while b - a > 1:
                m = (a + b) // 2
                if key(m) == key(a):
                    a = m
                else:
                    b = m
            out.append(b)
            t = b
        else:
            t += 86400 * 5
    return out


def changes_of(z, name, zi):
    ch = [t for t in listed_changes(os.path.join(z, name)) if Y1900 < t < Y2100]
    last = max(ch) if ch else Y1900
    y = int((datetime(2040, 1, 1) - EPOCH).total_seconds())  # past the listed changes: the rule line
    return ch + [t for t in rule_changes(zi, max(y, last + 1), y + 86400 * 365 * 3)]


def run_driver(d, lines, timeout=600):
    with open(os.path.join(d, "driver.vo"), "w") as f:
        f.write(DRIVER)
    with open(os.path.join(d, "cases.txt"), "w") as f:
        f.write("\n".join(lines))
    r = subprocess.run([VO, "run", "--timeout", str(300 * SLOW), "driver.vo"], cwd=d, capture_output=True, text=True,
                       timeout=timeout * SLOW)
    return r.returncode, r.stdout.splitlines(), r.stderr


def compare(name, d, queries, expected):
    code, out, err = run_driver(d, queries)
    case(f"{name}: the driver ran", code == 0 and len(out) == len(expected), f"status {code}, {len(out)} of {len(expected)} lines: {err[:300]}")
    bad = [(q, e, o) for q, e, o in zip(queries, expected, out) if e != o]
    zone = ""
    shown = 0
    for q, e, o in zip(queries, expected, out):
        if q.startswith("Z "):
            zone = q
        elif e != o and shown < 8:
            shown += 1
            print(f"  {zone} | {q}: expected `{e}`, got `{o}`")
    global cases
    cases += len(expected)
    case(f"{name}: {len(expected)} answers", not bad, f"{len(bad)} differ")


def group_data(d):
    r = subprocess.run([os.path.join(ROOT, "tools", "vendor_tzdata.sh"), "--verify"], capture_output=True, text=True)
    case("data: the vendored files match VENDOR.md", r.returncode == 0, r.stderr)
    z, names, release = zones_dir(d)
    case("data: the index is sorted and has no duplicates", names == sorted(set(names), key=lambda n: n.encode()))
    queries = ["N"] + [f"Z {n}" for n in names] + ["Z Europe/Atlantis", "Z ", "Z ../etc/passwd", "Z europe/stockholm", "Z UTC\x00", "Z Africa/Abidjan 0"]
    code, out, err = run_driver(d, queries)
    case("data: the driver ran", code == 0 and len(out) >= 2, err[:300])
    if code != 0 or len(out) < 2:
        return
    case("data: zone_names() is the index", out[0].split(" ") == names, f"{len(out[0].split(' '))} names")
    case("data: tzdata_version() is the release", out[1] == release == open(os.path.join(ROOT, "third_party", "tzdata", "version")).read().strip(), out[1])
    loads = out[2:2 + len(names)]
    case("data: every name loads", loads == [f"Z ok {n}" for n in names], str([x for x in loads if not x.startswith("Z ok")][:5]))
    wrong = out[2 + len(names):]
    case("data: unknown names are errors that quote the name",
         wrong[:4] == ["Z unknown time zone: Europe/Atlantis", "Z unknown time zone: ", "Z unknown time zone: ../etc/passwd", "Z unknown time zone: europe/stockholm"]
         and all(w.startswith("Z unknown time zone: ") for w in wrong) and len(wrong) == 6, str(wrong))


def group_instants(d):
    z, names, _ = zones_dir(d)
    rnd = random.Random(48)
    queries, expected = [], []
    for name in distinct(z, names):
        zi = zoneinfo.ZoneInfo(name)
        queries.append(f"Z {name}")
        expected.append(f"Z ok {name}")
        ts = [rnd.randrange(Y1900, Y2100) for _ in range(PER_ZONE)]
        if SLOW == 1:
            for c in changes_of(z, name, zi):
                ts += [c - 1, c, c + 1]
        for t in ts:
            ms = t * 1000 + rnd.choice((0, 0, 999, 500))
            dt = (EPOCH + timedelta(milliseconds=ms)).replace(tzinfo=UTC).astimezone(zi)
            queries.append(f"I {ms}")
            expected.append(f"{int(dt.utcoffset().total_seconds()) * 1000} {dt.tzname()} {dt.year} {dt.month} {dt.day} {dt.hour} {dt.minute} {dt.second} {dt.isoweekday()} back")
    compare("instants", d, queries, expected)
    # Outside Python's reach: years 1 and 9999 must answer, and format_in must read back.
    far = ["Z Europe/Stockholm", "I -62135510400000", "I 253402214400000", "Z Pacific/Apia", "I -62135510400000", "I 253402214400000", "Z Asia/Kolkata", "I 0"]
    code, out, err = run_driver(d, far)
    case("instants: years 1 and 9999 answer and read back", code == 0 and len(out) == 8 and all(o.endswith(" back") for o in out if not o.startswith("Z")), f"{out} {err[:200]}")
    case("instants: India is +05:30", len(out) == 8 and out[7].startswith("19800000 IST 1970 1 1 5 30 0 4"), str(out[-1:]))


def local_expected(zi, naive):
    """What from_local must say for a naive local time, from Python's fold 0 and fold 1 answers."""
    local = int((naive - EPOCH).total_seconds())
    found = set()
    for fold in (0, 1):
        off = int(naive.replace(tzinfo=zi, fold=fold).utcoffset().total_seconds())
        u = local - off
        if (EPOCH + timedelta(seconds=u)).replace(tzinfo=UTC).astimezone(zi).replace(tzinfo=None) == naive:
            found.add(u)
    return local, sorted(found)


def group_local(d):
    z, names, _ = zones_dir(d)
    rnd = random.Random(4800)
    queries, zones, locals_ = [], [], []
    for name in distinct(z, names):
        zi = zoneinfo.ZoneInfo(name)
        queries.append(f"Z {name}")
        zones.append(zi)
        locals_.append(None)
        ls = [rnd.randrange(Y1900 + 200000, Y2100 - 200000) for _ in range(PER_ZONE)]
        if SLOW == 1:
            for c in changes_of(z, name, zi):
                for off in {offset(zi, c - 1), offset(zi, c)}:
                    ls += [c + off - 1, c + off, c + off + 1, c + off + rnd.randrange(-7200, 7200)]
        for l in ls:
            naive = EPOCH + timedelta(seconds=l)
            queries.append(f"L {naive.year} {naive.month} {naive.day} {naive.hour} {naive.minute} {naive.second} 250")
            zones.append(zi)
            locals_.append(naive)
    code, out, err = run_driver(d, queries)
    case("local: the driver ran", code == 0 and len(out) == len(queries), f"status {code}, {len(out)} of {len(queries)} lines: {err[:300]}")
    bad = 0
    counts = {"once": 0, "skipped": 0, "repeated": 0}
    zone = ""
    for q, zi, naive, o in zip(queries, zones, locals_, out):
        if naive is None:
            zone = q
            continue
        local, found = local_expected(zi, naive)
        got = o.split(" ")
        if len(found) == 1:
            good = o == f"once {found[0] * 1000 + 250}"
        elif len(found) == 2:
            good = o == f"repeated {found[0] * 1000 + 250} {found[1] * 1000 + 250}"
        else:  # skipped: the instant given is a change whose local time jumps over this one
            good = got[0] == "skipped" and len(got) == 2 and int(got[1]) % 1000 == 0
            if good:
                c = int(got[1]) // 1000
                good = local - offset(zi, c - 1) >= c > local - offset(zi, c)
        if got[0] in counts:
            counts[got[0]] += 1
        if not good:
            bad += 1
            if bad <= 8:
                print(f"  {zone} | {q}: Python finds {found}, got `{o}`")
    global cases
    cases += len(queries)
    case(f"local: {len(queries)} answers ({counts})", bad == 0, f"{bad} differ")
    case("local: every kind of answer was seen", SLOW != 1 or all(counts.values()), str(counts))


def group_damaged(d):
    z, names, _ = zones_dir(d)
    rnd = random.Random(480)
    real = [open(os.path.join(z, n), "rb").read() for n in distinct(z, names)]
    files = []
    n = 10000 if SLOW == 1 else 500
    files += [bytes(rnd.randrange(256) for _ in range(rnd.randrange(0, 300))) for _ in range(n // 10)]
    files += [b"TZif2" + bytes(rnd.randrange(256) for _ in range(rnd.randrange(0, 400))) for _ in range(n // 10)]
    while len(files) < n:
        b = bytearray(rnd.choice(real))
        how = rnd.randrange(4)
        if how == 0:  # cut
            b = b[:rnd.randrange(len(b))]
        elif how == 1:  # one byte changed
            b[rnd.randrange(len(b))] = rnd.randrange(256)
        elif how == 2:  # one byte of the second header or the rule line changed
            i = rnd.choice((len(b) - 1 - rnd.randrange(min(40, len(b))), b.find(b"TZif", 4) + 20 + rnd.randrange(24)))
            b[i] = rnd.randrange(256)
        else:  # a run of bytes changed
            i = rnd.randrange(len(b))
            for j in range(i, min(len(b), i + rnd.randrange(1, 12))):
                b[j] = rnd.randrange(256)
        files.append(bytes(b))
    v1 = bytearray(real[0])
    v1[4] = 0
    leap = bytearray(real[0])
    at = leap.find(b"TZif", 4)
    leap[at + 28:at + 32] = struct.pack(">l", 1)
    files += [bytes(v1), bytes(leap), real[0]]
    os.makedirs(os.path.join(d, "f"))
    queries = []
    probes = ["I 0", "I -5000000000000", "I 1782907200123", "I 4102444800000", "I 253402300799999", "I -62135596800000",
              "L 2026 3 29 2 30 0 0", "L 2026 10 25 2 30 0 0", "L 1970 1 1 0 0 0 0", "L 2090 6 1 12 0 0 0"]
    for i, body in enumerate(files):
        with open(os.path.join(d, "f", str(i)), "wb") as f:
            f.write(body)
        queries.append(f"B f/{i}")
        queries += probes
    code, out, err = run_driver(d, queries)
    case(f"damaged: {len(files)} files, the program did not stop", code == 0 and len(out) == len(queries), f"status {code}, {len(out)} of {len(queries)} lines: {err[-400:]}")
    heads = [o for o in out if o.startswith("B ")]
    oks = sum(1 for h in heads if h == "B ok")
    print(f"zones: damaged files: {oks} read as zones, {len(heads) - oks} refused")
    case("damaged: errors say `invalid zone data`", all(h == "B ok" or h.startswith("B invalid zone data: ") for h in heads))
    case("damaged: both kinds were seen", 0 < oks < len(heads))
    if len(heads) == len(files):
        case("damaged: a version 1 file is refused", "version 1" in heads[-3], heads[-3])
        case("damaged: a file with leap seconds is refused", "leap" in heads[-2], heads[-2])
        case("damaged: the real file is read", heads[-1] == "B ok", heads[-1])


def run_local(d, env, *flags):
    e = {k: v for k, v in os.environ.items() if k != "TZ"}
    e.update(env)
    r = subprocess.run([VO, "run", *flags, "local.vo"], cwd=d, capture_output=True, text=True, env=e, timeout=60 * SLOW)
    return r.stdout.strip() if r.returncode == 0 else f"status {r.returncode}: {r.stderr[:200]}"


def group_machine(d):
    with open(os.path.join(d, "local.vo"), "w") as f:
        f.write(LOCAL)
    case("machine: TZ names the zone", run_local(d, {"TZ": "America/New_York"}) == "America/New_York 2026-07-01T08:00:00-04:00", run_local(d, {"TZ": "America/New_York"}))
    case("machine: TZ with a colon", run_local(d, {"TZ": ":Asia/Kolkata"}) == "Asia/Kolkata 2026-07-01T17:30:00+05:30", run_local(d, {"TZ": ":Asia/Kolkata"}))
    case("machine: a fixed clock is UTC", run_local(d, {"TZ": "Asia/Tokyo"}, "--clock-ms", "0") == "UTC 2026-07-01T12:00:00+00:00", run_local(d, {"TZ": "Asia/Tokyo"}, "--clock-ms", "0"))
    # Without a usable TZ: /etc/localtime when it is a zone file, else UTC.
    want = "UTC 2026-07-01T12:00:00+00:00"
    if os.path.exists("/etc/localtime"):
        body = open("/etc/localtime", "rb").read()
        link = os.path.realpath("/etc/localtime")
        name = link.split("zoneinfo/")[-1] if os.path.islink("/etc/localtime") and "zoneinfo/" in link else "local"
        if body[:4] == b"TZif" and body[4:5] >= b"2":
            os.makedirs(os.path.join(d, "etc"))
            with open(os.path.join(d, "etc", "localtime"), "wb") as f:
                f.write(body)
            zoneinfo.reset_tzpath(to=[os.path.join(d, "etc")])
            zoneinfo.ZoneInfo.clear_cache()
            dt = datetime(2026, 7, 1, 12, tzinfo=UTC).astimezone(zoneinfo.ZoneInfo("localtime"))
            off = dt.strftime("%z")
            want = f"{name} {dt.strftime('%Y-%m-%dT%H:%M:%S')}{off[:3]}:{off[3:5]}"
    if os.path.islink("/etc/localtime") and "zoneinfo/" not in os.path.realpath("/etc/localtime") and "zoneinfo/" in os.readlink("/etc/localtime"):
        want = None  # a link through another link: the name is the first link's, not checked here
    for tz in ({}, {"TZ": "nonsense"}, {"TZ": "CET-1CEST"}, {"TZ": "/etc/passwd"}):
        got = run_local(d, tz)
        case(f"machine: {tz or 'no TZ'} falls to /etc/localtime or UTC", want is None or got == want, f"got `{got}`, expected `{want}`")
    e = {k: v for k, v in os.environ.items() if k != "TZ"}
    r = subprocess.run([VO, "run", "--record", "r.json", "local.vo"], cwd=d, capture_output=True, text=True, env={**e, "TZ": "Asia/Tokyo"}, timeout=60 * SLOW)
    p = subprocess.run([VO, "replay", "r.json"], cwd=d, capture_output=True, text=True, env={**e, "TZ": "UTC"}, timeout=60 * SLOW)
    case("machine: a recording replays its zone under another TZ",
         r.returncode == 0 and p.returncode == 0 and r.stdout == p.stdout == "Asia/Tokyo 2026-07-01T21:00:00+09:00\n", f"{r.stdout!r} {p.stdout!r} {p.stderr[:200]}")
    case("machine: the recording has a local_tz event", '"kind":"local_tz"' in open(os.path.join(d, "r.json")).read())


def group_speed(d):
    if SLOW != 1 or os.environ.get("CI"):
        print("zones: speed: skipped (CI or emulation)")
        return
    with open(os.path.join(d, "speed.vo"), "w") as f:
        f.write(SPEED)
    best = None
    for _ in range(3):
        r = subprocess.run([VO, "run", "speed.vo"], cwd=d, capture_output=True, text=True, timeout=120)
        if r.returncode != 0:
            case("speed: the program ran", False, r.stderr[:300])
            return
        a, b, _ = (int(x) for x in r.stdout.split())
        best = (a, b) if best is None else (min(best[0], a), min(best[1], b))
    print(f"zones: every zone loaded in {best[0]} ms, 100,000 conversions in {best[1]} ms (bounds 1000 on x86-64)")
    case("speed: every zone loads in under 1 s", best[0] < 1000, str(best[0]))
    case("speed: 100,000 conversions in under 1 s", best[1] < 1000, str(best[1]))


GROUPS = {"data": group_data, "instants": group_instants, "local": group_local, "damaged": group_damaged,
          "machine": group_machine, "speed": group_speed}


def main():
    args = sys.argv[1:]
    names = [args[args.index("--group") + 1]] if "--group" in args else list(GROUPS)
    tmp = os.path.join(ROOT, "build", "tmp")
    os.makedirs(tmp, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=tmp) as base:
        for n in names:
            sub = os.path.join(base, n)
            os.makedirs(sub)
            GROUPS[n](sub)
    print(f"zones: {len(names)} groups, {cases} cases, {len(failures)} failures")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
