#!/usr/bin/env python3
"""Python transcriptions of the SC-001 workloads (spec 003 T007): independent expected outputs.
Usage: transcribe.py [--write]   (prints each workload's output; --write stores <name>.out)"""
import os
import sys


def string_build():
    s = "".join("piece" + str(10000 + i % 90000) for i in range(20000))
    parts = s.split("piece")
    return f"{len(s)} {len(parts)} {parts[1]} {parts[20000]}"


def list_churn():
    base = [k * 3 for k in range(1000)]
    total = 0
    for i in range(140000):
        xs = [k + i % 7 for k in range(10)]
        window = base[i % 10:990 + i % 10]
        total += sum(xs) + window[0] + len(window)
    return str(total)


def map_churn():
    base = list(range(500))
    m = {}
    for i in range(250000):
        m["k" + str(i)] = base[i % 3:497 + i % 3]
        if i >= 1000:
            del m["k" + str(i - 1000)]
    return f"{len(m)} {sum(v[0] + len(v) for v in m.values())}"


def closures():
    base = list(range(500))
    total = 0
    for i in range(260000):
        items = base[i % 5:495 + i % 5]
        total += i % 3 + i % 100 + len(items) + items[1]
    return str(total)


def generic_container():
    base = list(range(500))
    lists, words, total = [], [], 0
    for i in range(260000):
        lists.append(base[i % 4:496 + i % 4])
        words.append("w" + str(i))
        if i % 2 == 1:
            total += lists.pop()[0] + len(words.pop())
            total += lists.pop()[1] + len(words.pop())
    return f"{len(lists)} {len(words)} {total}"


WORKLOADS = [string_build, list_churn, map_churn, closures, generic_container]

if __name__ == "__main__":
    here = os.path.dirname(os.path.abspath(__file__))
    for f in WORKLOADS:
        out = f() + "\n"
        print(f.__name__, out, end="")
        if "--write" in sys.argv:
            open(os.path.join(here, f.__name__ + ".out"), "w").write(out)
