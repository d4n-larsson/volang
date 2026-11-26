#!/usr/bin/env python3
"""Spec 059 (T021-T026): migrate anonymous functions to the untyped form, and show the cases keep their meaning.

  migrate_059.py apply [PATH...]     apply E0342 (written types) and E0413 (`=> expr`) fixes in place, to every
                                     .vo file under the PATHs (default tests/cases), each checked from its own
                                     folder, up to 4 rounds (E0413 follows once E0342 removed the types)
  migrate_059.py text FILE...         the same by text, where vo's fixes cannot reach: std's doc examples (comment
                                     lines `#     EXPR is VALUE`) and the vo code blocks of Markdown files; the
                                     checks that run them (doc_conformance.py, catalog.py, site_examples.py)
                                     then prove the result
  migrate_059.py compare [--accept] [--quick] [--base REV]
                                     run the golden cases whose source differs from REV (default HEAD) and
                                     compare their output with REV's expected output after normalizing what the
                                     migration may change (positions, source lines, fix text, warning counts);
                                     --accept writes the new expected output of the cases that match; the rest
                                     are listed for review. --quick: the first 60.

tests/cases/lambda_types/ keeps written types on purpose where its cases test E0342.
"""
import json
import os
import re
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CASES = os.path.join(ROOT, "tests", "cases")
VO = os.environ.get("VO_BIN") or os.path.join(ROOT, "build", "vo")
CODES = ("E0342", "E0413")
KEEP = ("written_", "single_return", "lambda_single_return", "lambda_arrow")  # cases that test E0342, E0413 or fmt themselves
SKIP_DIRS = ("fmt", "imports_v03")  # fmt only parses (0.4 input is valid there); imports_v03 keeps the 0.3 form


def offset(text, pos):
    lines = text.split("\n")
    if pos["line"] - 1 >= len(lines):
        return len(text)
    return sum(len(l) + 1 for l in lines[:pos["line"] - 1]) + len(lines[pos["line"] - 1][:pos["col"] - 1])


def fix_once(path):
    """Applies this file's E0342/E0413 fixes (checked from its folder); the number applied."""
    d, name = os.path.split(path)
    try:
        out = subprocess.run([VO, "check", "--json", name], cwd=d, capture_output=True, text=True, timeout=60).stdout
    except subprocess.TimeoutExpired:
        return 0
    fixes = []
    for line in out.splitlines():
        try:
            r = json.loads(line)
        except ValueError:
            continue
        if r.get("kind") == "diagnostic" and r.get("code") in CODES and r["fixes"] and r["file"] == name:
            fixes.append(r["fixes"][0])
    if not fixes:
        return 0
    text = open(path, encoding="utf-8").read()
    last = None  # fixes of one round never overlap (header vs body); apply from the end
    for f in sorted(fixes, key=lambda f: (f["start"]["line"], f["start"]["col"]), reverse=True):
        s, e = offset(text, f["start"]), offset(text, f["end"])
        if last is not None and e > last:
            continue
        text = text[:s] + f["replacement"] + text[e:]
        last = s
    open(path, "w", encoding="utf-8").write(text)
    return len(fixes)


def sources(paths):
    for top in paths:
        if os.path.isfile(top):
            yield top
            continue
        for d, dirs, names in os.walk(top):
            dirs[:] = sorted(x for x in dirs if x != "expected" and x not in SKIP_DIRS)
            for n in sorted(names):
                if n.endswith(".vo") and not n.endswith(".expected.vo") and not n.startswith(KEEP):
                    p = os.path.join(d, n)
                    if re.search(r"\bfn\(", open(p, encoding="utf-8", errors="replace").read()):
                        yield p


def apply(paths):
    applied = 0
    for p in sources(paths or [CASES]):
        for _ in range(4):
            n = fix_once(p)
            if not n:
                break
            applied += n
    print(f"migrate_059 apply: {applied} fixes applied")
    return 0


BEFORE = re.compile(r"(?:[(,=\[{:]|\breturn)\s*$")  # (`:`: a field's value; a type after `:` is refused below)


def close(text, i, open_, shut):
    """Index just after the bracket matching text[i] (open_), skipping strings; -1 if none."""
    depth, n = 0, len(text)
    while i < n:
        c = text[i]
        if c == '"':
            i += 1
            while i < n and text[i] != '"':
                i += 2 if text[i] == "\\" else 1
        elif c == open_:
            depth += 1
        elif c == shut:
            depth -= 1
            if depth == 0:
                return i + 1
        i += 1
    return -1


def text_lambdas(src):
    """Every anonymous function written with types, rewritten as the fixes would: names only, and
    `=> e` for a body that only returns e."""
    out, i = [], 0
    for m in re.finditer(r"\bfn\(", src):
        if m.start() < i or not BEFORE.search(src[max(0, m.start() - 12):m.start()]):
            continue
        pe = close(src, m.end() - 1, "(", ")")
        brace = src.find("{", pe)
        head = src[pe:brace].strip()  # empty, or `-> TYPE`: a function *type* goes on with `,`, `)` or `;`
        if pe < 0 or brace < 0 or head and not (head.startswith("->") and not re.search(r"[,;)=]", strip_brackets(head))):
            continue
        params = [p.split(":")[0].strip() for p in split_top(src[m.end():pe - 1])]
        be = close(src, brace, "{", "}")
        body = src[brace + 1:be - 1]
        one = re.fullmatch(r"\s*return\s+(.*?);\s*", body, re.S)
        header = "fn(" + ", ".join(params) + ")"
        new = header + (" => " + one.group(1) if one and "\n" not in one.group(1) and "#" not in body
                        else " {" + body + "}")
        out.append(src[i:m.start()] + new)
        i = be
    out.append(src[i:])
    return "".join(out)


def strip_brackets(s):
    """s without what its [ ] and ( ) enclose."""
    while True:
        t = re.sub(r"\[[^\[\]]*\]|\([^()]*\)", "", s)
        if t == s:
            return s
        s = t


def split_top(s):
    """s split at commas outside brackets."""
    parts, depth, cur = [], 0, ""
    for c in s:
        depth += c in "([{"
        depth -= c in ")]}"
        if c == "," and depth == 0:
            parts.append(cur)
            cur = ""
        else:
            cur += c
    return [p for p in parts + [cur] if p.strip()]


def text_files(paths):
    for path in paths:
        src = open(path, encoding="utf-8").read()
        if path.endswith(".md"):
            new = re.sub(r"(```(?:vo|vo-test|vo-fmt)\n)(.*?)(```)", lambda m: m.group(1) + text_lambdas(m.group(2)) + m.group(3),
                         src, flags=re.S)
        elif os.path.relpath(path, ROOT).split(os.sep)[0] in ("std", "packages"):  # their code: by fixes
            new = "\n".join(text_lambdas(l) if re.match(r"\s*#", l) else l for l in src.split("\n"))
        else:  # a program the fixes cannot reach (0.3 imports, a fixture): all of it
            new = text_lambdas(src)
        if new != src:
            open(path, "w", encoding="utf-8").write(new)
            print("migrated", os.path.relpath(path, ROOT))
    return 0


NEW_FORM = re.compile(rb"=>|\bfn\([a-z_]\w*(?:, [a-z_]\w*)*\)\s*\{")  # untyped parameters, or `=> e`


def without_positions(res):
    """(stdout, stderr, code) with positions and source excerpts left out of stderr: a program run on an
    engine from before 0.5 and its migrated text on the current one differ in layout only."""
    out, err, code = res
    err = re.sub(rb"(?m)^\s*\d+ \| .*\n|^\s*\| [\s^~]*\n", b"", err)
    err = re.sub(rb"(\.vo):\d+:\d+", rb"\1", err)
    return out, err, code


def normalize(text):
    out = []
    for line in text.splitlines():
        if re.match(r"^\s*\d+ \| ", line) or re.match(r"^\s*\| [\s^~]*$", line):
            continue  # source excerpts and carets
        line = re.sub(r'"(line|col)":\d+', r'"\1":0', line)
        line = re.sub(r"(\.vo):\d+:\d+", r"\1:_", line)
        line = re.sub(r"(\.vo):\d+\b", r"\1:_", line)
        line = re.sub(r'"warnings":\d+', '"warnings":0', line)
        line = re.sub(r"\d+ errors?, \d+ warnings?", lambda m: m.group(0).split(",")[0], line)
        line = re.sub(r'"text":"[^"]*"', '"text":""', line)
        line = re.sub(r'"replacement":"[^"]*"', '"replacement":""', line)
        line = re.sub(r"fix: .*", "fix:", line)
        out.append(line.rstrip())
    return "\n".join(out)


def git_show(rev, path):
    r = subprocess.run(["git", "show", f"{rev}:{os.path.relpath(path, ROOT)}"], cwd=ROOT, capture_output=True)
    return r.stdout.decode("utf-8", "replace") if r.returncode == 0 else None


def changed_cases(base):
    r = subprocess.run(["git", "diff", "--name-only", base, "--", "tests/cases"], cwd=ROOT, capture_output=True, text=True)
    out = set()
    for f in r.stdout.split():
        if not f.endswith(".vo") or f.endswith(".expected.vo"):
            continue
        full = os.path.join(ROOT, f)
        d = os.path.dirname(full)
        step = d
        while step.startswith(CASES) and step != CASES:  # a step case: the folder with `steps`
            if os.path.exists(os.path.join(step, "steps")):
                out.add(("steps", step))
                break
            step = os.path.dirname(step)
        else:
            if os.path.exists(os.path.join(d, "main.vo")) and os.path.basename(full) != "main.vo" and \
                    not any(os.path.exists(full[:-3] + e) for e in (".out", ".err", ".code", ".cmd")):
                out.add(("run", os.path.join(d, "main.vo")))  # a helper of a folder case
            else:
                out.add(("run", full))
    return sorted(out)


def runner(kind, path, accept=False):
    if kind == "steps":
        cmd = [sys.executable, os.path.join(ROOT, "tests", "steps.py")] + (["--accept"] if accept else []) + [path]
    else:
        cmd = [os.path.join(ROOT, "tests", "run.sh")] + (["--accept"] if accept else []) + [path[:-3] + ".vo"]
    return subprocess.run(cmd, cwd=ROOT, capture_output=True, text=True, env=dict(os.environ, VO_JOBS="1")).returncode == 0


def expected_files(kind, path):
    if kind == "steps":
        exp = os.path.join(path, "expected")
        return [os.path.join(d, n) for d, _, ns in os.walk(exp) for n in ns] if os.path.isdir(exp) else []
    base = path[:-3]
    return [base + e for e in (".out", ".err", ".code") if os.path.exists(base + e)]


def restore(kind, path, base, before):
    """Puts back REV's expected files of a case: those it had (`before`), and none that --accept added."""
    for f in set(expected_files(kind, path)) | set(before):
        old = git_show(base, f)
        if old is None:
            os.remove(f) if os.path.exists(f) else None
        else:
            open(f, "w", encoding="utf-8").write(old)


def compare(accept, quick, base):
    cases = changed_cases(base)[:60] if quick else changed_cases(base)
    same, review = [], []
    for kind, path in cases:
        if runner(kind, path):
            same.append(path)
            continue
        before = expected_files(kind, path)
        runner(kind, path, accept=True)  # the new output, then each expected file against REV's
        diffs = []
        for f in set(expected_files(kind, path)) | set(before):
            old = git_show(base, f)
            new = open(f, encoding="utf-8", errors="replace").read() if os.path.exists(f) else None
            if old is None or new is None or normalize(old) != normalize(new):
                diffs.append(os.path.relpath(f, ROOT))
        if diffs:
            review.append((path, diffs))
        else:
            same.append(path)
        if diffs or not accept:  # put REV's expected output back, exactly
            restore(kind, path, base, before)
    for path, diffs in review:
        print("REVIEW", os.path.relpath(path, ROOT), " ".join(diffs))
    print(f"migrate_059: {len(cases)} migrated cases, {len(same)} pass or match, {len(review)} to review")
    return 1 if review else 0


def main():
    args = sys.argv[1:]
    if args[:1] == ["apply"]:
        return apply([os.path.abspath(p) for p in args[1:]])
    if args[:1] == ["text"]:
        return text_files(args[1:])
    if args[:1] == ["compare"]:
        base = args[args.index("--base") + 1] if "--base" in args else "HEAD"
        return compare("--accept" in args, "--quick" in args, base)
    print(__doc__)
    return 2


if __name__ == "__main__":
    sys.exit(main())
