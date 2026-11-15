#!/usr/bin/env python3
"""Spec 007 SC-005: std.time against Python's datetime (proleptic Gregorian, UTC).

10,000 seeded times: uniform over years 1-9999, within a century of 1970, around leap days, and at
day boundaries (midnight and one millisecond either side). tests/time/driver.vo prints, for each,
the fields of from_ms, format_time, parse_time of that text and to_ms of the fields; every one must
equal what datetime computes, and both round trips must give the time back.
"""
import datetime
import os
import random
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")
EPOCH = datetime.datetime(1970, 1, 1)
LO = -62135596800000            # 0001-01-01T00:00:00Z
HI = 253402300799999            # 9999-12-31T23:59:59.999Z
DAY = 86400000


def times(n=10000, seed=7):
    r = random.Random(seed)
    out = [0, -1, 1, LO, HI, LO + 1, HI - 1]
    while len(out) < n:
        k = len(out) % 4
        if k == 0:
            out.append(r.randint(LO, HI))
        elif k == 1:
            out.append(r.randint(-100 * 365 * DAY, 100 * 365 * DAY))
        elif k == 2:  # around Feb 28 - Mar 1 of a random year
            y = r.randint(1, 9999)
            base = int((datetime.datetime(y, 2, 28) - EPOCH).total_seconds()) * 1000
            out.append(min(HI, base + r.randint(0, 3 * DAY - 1)))
        else:  # a day boundary, or a millisecond before or after it
            y, m = r.randint(1, 9999), r.randint(1, 12)
            base = int((datetime.datetime(y, m, 1) - EPOCH).total_seconds()) * 1000
            out.append(min(HI, max(LO, base + r.choice([-1, 0, 1]))))
    return out


def expect(ms):
    t = EPOCH + datetime.timedelta(milliseconds=ms)
    frac = ms % 1000
    text = t.strftime("%Y-%m-%dT%H:%M:%S").rjust(19, "0") + (f".{frac:03d}" if frac else "") + "Z"
    if t.year < 1000:  # strftime drops leading zeros of small years on some platforms
        text = f"{t.year:04d}" + text[text.index("-"):]
    return (f"{t.year} {t.month} {t.day} {t.hour} {t.minute} {t.second} {frac} {t.isoweekday()} "
            f"{text} {ms} {ms}")


def main():
    ts = times()
    r = subprocess.run([VO, "run", os.path.join(ROOT, "tests", "time", "driver.vo")],
                       input="\n".join(map(str, ts)).encode(), capture_output=True, timeout=600)
    got = r.stdout.decode().splitlines()
    if r.returncode != 0 or len(got) != len(ts):
        print(f"time_check: the driver failed (exit {r.returncode}, {len(got)} of {len(ts)} lines)")
        print(r.stderr.decode()[-2000:])
        return 1
    bad = 0
    for ms, line in zip(ts, got):
        want = expect(ms)
        if line != want:
            bad += 1
            if bad <= 10:
                print(f"FAIL {ms}:\n  vo     {line}\n  python {want}")
    print(f"time_check: {len(ts)} times, {bad} differences from datetime (years 1-9999, around 1970, "
          "leap days, day boundaries)")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
