#!/usr/bin/env python3
"""Spec 051 SC-005: rendering a page of about 1 MiB from 10,000 values takes under 50 ms (median of 3).

The program renders the page 11 times, timing the last 10 with the clock, so start-up is not counted.
"""
import os
import statistics
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")

SRC = """import std.html.Html;
import std.html.html;
import std.html.html_text;
import std.time.now;

fn page(texts: list[str]) -> Html {
    let items = new list[Html];
    for i, t in texts {
        push(items, html(`<li id="n{}"><a href="/notes/{}">{}</a></li>`, i, i, t));
    }
    return html("<!DOCTYPE html><html><body><ul>{}</ul></body></html>", items);
}

fn main() uses console, clock {
    let texts = new list[str];
    for i in 0..10000 {
        push(texts, "note <" + str(i) + "> & some text that makes the page about one mebibyte long, " + str(i * 7));
    }
    var size = len(html_text(page(texts)));
    let t0 = now();
    for _ in 0..10 {
        set size = len(html_text(page(texts)));
    }
    print(str(size) + " " + str((now() - t0) / 10));
}
"""


def main():
    with tempfile.TemporaryDirectory() as d:
        with open(os.path.join(d, "speed.vo"), "w") as f:
            f.write(SRC)
        times = []
        size = 0
        for _ in range(3):
            p = subprocess.run([VO, "run", "--timeout", "120", "speed.vo"], cwd=d, capture_output=True, text=True)
            if p.returncode != 0:
                print("FAIL html_speed:", p.stderr[:400])
                return 1
            size, ms = (int(x) for x in p.stdout.split())
            times.append(ms)
    ms = statistics.median(times)
    print(f"html_speed: a page of {size} bytes from 10,000 values in {ms} ms (median of 3; SC-005: under 50)")
    return 0 if ms < 50 else 1


if __name__ == "__main__":
    sys.exit(main())
