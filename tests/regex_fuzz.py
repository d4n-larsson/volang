#!/usr/bin/env python3
"""Spec 025: patterns are hostile input. Feeds random strings (mostly metacharacters, digits, braces
and escapes, some non-ASCII) to `parse_regex`, and every pattern that compiles to `is_match` and
`all_matches` on a random text. The program must run to its end (exit 0) whatever the patterns: every
result is `ok` or `err`, never a crash.

  regex_fuzz.py [--count N] [--seed S]      default 100,000 patterns, seed 1; VO_BIN selects the build
"""
import os
import random
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")
ALPHABET = list("()[]{}|*+?.^$\\-,:<>=!#PpkdDwWsSbBnt0123456789aAzZ_ é") + ["(?", "(?<", "(?P<", "{1,", "\\\\", "[^", "日"]
CHUNK = 5000

PRELUDE = """import std.regex.parse_regex;
import std.regex.is_match;
import std.regex.all_matches;

fn try_one(p: str, s: str) -> int {
    match parse_regex(p) {
        case ok(re) {
            if is_match(re, s) {
                return 1 + len(all_matches(re, s));
            }
            return 1;
        }
        case err(_) {
            return 0;
        }
    }
}

fn main() uses console {
    var total = 0;
"""


def vo_str(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n") + '"'


def main():
    args = sys.argv[1:]
    count = int(args[args.index("--count") + 1]) if "--count" in args else 100000
    seed = int(args[args.index("--seed") + 1]) if "--seed" in args else 1
    rng = random.Random(seed)
    tmp = tempfile.mkdtemp(prefix="voregexfuzz-")
    bad = 0
    score = 0
    try:
        for c0 in range(0, count, CHUNK):
            n = min(CHUNK, count - c0)
            path = os.path.join(tmp, "f.vo")
            with open(path, "w") as f:
                f.write(PRELUDE)
                for _ in range(n):
                    p = "".join(rng.choice(ALPHABET) for _ in range(rng.randint(0, 24)))
                    s = "".join(rng.choice("ab1 -\né") for _ in range(rng.randint(0, 30)))
                    f.write(f"    set total = total + try_one({vo_str(p)}, {vo_str(s)});\n")
                f.write('    print(str(total));\n}\n')
            r = subprocess.run([VO, "run", "--timeout", "120", path], capture_output=True)
            if r.returncode != 0:
                bad += 1
                print(f"FAIL chunk {c0 // CHUNK}: exit {r.returncode}: {r.stderr.decode(errors='replace')[:1500]}")
                break
            score += int(r.stdout.decode().strip())
    finally:
        for name in os.listdir(tmp):
            os.remove(os.path.join(tmp, name))
        os.rmdir(tmp)
    print(f"regex_fuzz: {count} patterns, {bad} failures (compiled, matched and listed: {score})")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
