#!/usr/bin/env python3
"""Spec 058 (T028): the golden cases in the module form keep their meaning.

  migrate_058.py apply [PATH...]     migrate every .vo file under tests/cases (or PATHs) in place with vo's own
                                     E0215 fix, one file at a time, up to 3 rounds (an import of a std item
                                     renamed in 0.2 keeps the 0.3 form: those cases test E0214 and E0215)
  migrate_058.py text FILE...         the same by text, for programs the resolver cannot reach (a package
                                     without its manifest, a program inside a test script); see text_migrate
  migrate_058.py compare [--accept] [--quick] [--base REV]
                                     run the cases whose source differs from REV (default HEAD) and compare
                                     their output with REV's expected output after normalizing what the
                                     migration may change: positions, source lines, module names before an
                                     item (`json.` in `json.parse_json`), the 0.3 import lines and the
                                     warning count; --accept writes the new expected output of the cases
                                     that match (tests/run.sh --accept, steps.py --accept); the rest are
                                     listed for review. --quick: the first 60.

tests/cases/imports_v03/ is never migrated: it keeps the 0.3 form on purpose.
"""
import json
import os
import re
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CASES = os.path.join(ROOT, "tests", "cases")
VO = os.environ.get("VO_BIN") or os.path.join(ROOT, "build", "vo")


def offset(text, pos):
    lines = text.split("\n")
    if pos["line"] - 1 >= len(lines):
        return len(text)
    return sum(len(l) + 1 for l in lines[:pos["line"] - 1]) + len(lines[pos["line"] - 1][:pos["col"] - 1])


def fix_once(path):
    """Applies the E0215 fix of the file at path (checked from its own directory); 1 when it changed."""
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
        if r.get("kind") == "diagnostic" and r.get("code") == "E0215" and r["fixes"] and r["file"] == name:
            fixes.append(r["fixes"][0])
    if not fixes:
        return 0
    text = open(path, encoding="utf-8").read()
    for f in sorted(fixes, key=lambda f: (f["start"]["line"], f["start"]["col"]), reverse=True):
        s, e = offset(text, f["start"]), offset(text, f["end"])
        text = text[:s] + f["replacement"] + text[e:]
    open(path, "w", encoding="utf-8").write(text)
    return 1


def sources(paths):
    for top in paths:
        if os.path.isfile(top):
            yield top
            continue
        for d, dirs, names in os.walk(top):
            dirs[:] = [x for x in dirs if x != "expected"]
            if "imports_v03" in d.split(os.sep):
                continue
            for n in sorted(names):
                if n.endswith(".vo") and not n.endswith(".expected.vo"):
                    p = os.path.join(d, n)
                    if re.search(r"^import ", open(p, encoding="utf-8", errors="replace").read(), re.M):
                        yield p


def apply(paths):
    changed = 0
    for p in sources(paths or [CASES]):
        for _ in range(3):
            if not fix_once(p):
                break
            changed += 1
    print(f"migrate_058 apply: {changed} fixes applied")
    return 0


OLD_IMPORT = re.compile(r"^(?P<ind>[ \t]*)import (?P<mod>(?:[a-z_]\w*\.)*[a-z_]\w*)\.(?P<item>[A-Za-z_]\w*);[ \t]*$", re.M)


def is_old(mod, item):
    """The 0.3 form, as far as the text tells: an item of std.M, a capitalized item, or a path of 3+ parts."""
    parts = mod.split(".")
    if parts[0] == "std":
        return len(parts) >= 2
    return item[0].isupper() or len(parts) >= 2


def text_migrate(src, keep_old=(), old_file=None):
    """The 0.3 imports of a vo text in the module form, by text (for programs the resolver cannot reach:
    inside test scripts, or using a package that needs its manifest): each `import a.b.item;` becomes
    `import a.b;`, and each use of `item` outside strings, comments, field names (`item:`) and member
    accesses (`.item`) becomes `b.item`. Items named in keep_old stay as they are. Returns the new text and
    the module names it introduced."""
    mods, items = [], {}
    def head(m):
        old = is_old(m.group("mod"), m.group("item"))
        if not old and old_file:  # `import report.render;`: the 0.3 form when report.vo declares render
            old = old_file(m.group("mod"), m.group("item"))
        if not old or m.group("item") in keep_old:
            return m.group(0)
        mod = m.group("mod")
        items[m.group("item")] = mod.rsplit(".", 1)[-1]
        if mod in mods:
            return "\0drop\0"
        mods.append(mod)
        return f"{m.group('ind')}import {mod};"
    out = OLD_IMPORT.sub(head, src)
    out = re.sub(r"\0drop\0\n?", "", out)
    if not items:
        return src, []
    # strings, comments and `uses` clauses (effects such as serve(...) are not items) stay as written
    parts = re.split(r'("(?:[^"\\\n]|\\.)*"|`[^`]*`|#[^\n]*|\buses [^{;=]*)', out)
    for i in range(0, len(parts), 2):  # code between strings and comments
        parts[i] = re.sub(r"(?<![\w.])([A-Za-z_]\w*)\b(?!\s*:(?!:))",
                          lambda m: f"{items[m.group(1)]}.{m.group(1)}" if m.group(1) in items else m.group(1),
                          parts[i])
    out = "".join(parts)
    return out, sorted(set(items.values()))


_STD_ITEMS = {}


def std_items(module):
    """The exported item names of std.MODULE (from `vo doc --json`), cached."""
    if module not in _STD_ITEMS:
        r = subprocess.run([VO, "doc", "--json", "std." + module], capture_output=True, text=True)
        names = set()
        for line in r.stdout.splitlines():
            try:
                rec = json.loads(line)
            except ValueError:
                continue
            if rec.get("kind") == "doc" and rec.get("name"):
                names.add(rec["name"])
        _STD_ITEMS[module] = names
    return _STD_ITEMS[module]


def module_form(src):
    """A test program assembled from fragments that write std items bare, in the module form: each bare
    use of an item of a std module the program imports (`import std.json;`) becomes `json.NAME`, outside
    strings, comments, `uses` clauses, field names and member accesses, and not for names the program
    declares itself (functions, records, variants, constants, locals, parameters)."""
    items = {}
    for m in re.findall(r"^import std\.(\w+);", src, re.M):
        for n in std_items(m):
            items.setdefault(n, m)
    own = set(re.findall(r"\b(?:fn|record|variant|const|effects|let|var|for)\s+([A-Za-z_]\w*)", src))
    own |= set(re.findall(r"[(,]\s*([a-z_]\w*)\s*:", src))  # parameters
    for n in own:
        items.pop(n, None)
    if not items:
        return src
    parts = re.split(r'("(?:[^"\\\n]|\\.)*"|`[^`]*`|#[^\n]*|\buses [^{;=]*|^import [^\n]*)', src, flags=re.M)
    for i in range(0, len(parts), 2):
        parts[i] = re.sub(r"(?<![\w.])([A-Za-z_]\w*)\b(?!\s*:(?!:))",
                          lambda m: f"{items[m.group(1)]}.{m.group(1)}" if m.group(1) in items else m.group(1),
                          parts[i])
    return "".join(parts)


TRIPLE = re.compile(r'(?P<pre>(?<!\w)[rRfFbB]{0,2})(?P<q>"""|\'\'\')(?P<body>.*?)(?P=q)', re.S)
PLACEHOLDER = re.compile(r"(?<!\{)\{(?!\{)[^{}\n]*\}(?!\})")


def py_migrate(src):
    """The vo programs in a Python file's triple-quoted strings, by text_migrate; in an f-string the `{expr}`
    placeholders are left alone (vo's own braces are doubled there)."""
    def one(m):
        body, pre = m.group("body"), m.group("pre").lower()
        if not OLD_IMPORT.search(body):
            return m.group(0)
        if "f" in pre:
            keep = {}
            def hide(p):
                k = f"\x01{len(keep)}\x01"
                keep[k] = p.group(0)
                return k
            masked = PLACEHOLDER.sub(hide, body)
            new, _ = text_migrate(masked)
            for k, v in keep.items():
                new = new.replace(k, v)
        else:
            new, _ = text_migrate(body)
        return m.group("pre") + m.group("q") + new + m.group("q")
    return TRIPLE.sub(one, src)


def text_files(paths):
    n = 0
    for p in paths:
        src = open(p, encoding="utf-8").read()
        d = os.path.dirname(os.path.abspath(p))
        def old_file(mod, item):
            base = os.path.join(d, *mod.split("."))
            return os.path.exists(base + ".vo") and not os.path.exists(os.path.join(base, item + ".vo"))
        new, mods = text_migrate(src, old_file=old_file)
        if new != src:
            open(p, "w", encoding="utf-8").write(new)
            n += 1
            print(f"text: {os.path.relpath(p, ROOT)}: modules {', '.join(mods)}")
    return n


QUAL = re.compile(r"\b[a-z_][a-z0-9_]*\.(?=[A-Za-z_])")


def normalize(text):
    out = []
    for line in text.splitlines():
        if re.match(r"^\s*\d+ \| ", line) or re.match(r"^\s*\| [\s^~]*$", line):
            continue  # source excerpts and carets
        if re.match(r"^\s*import [\w.]+;", line) or '"code":"W0203"' in line or "warning[W0203]" in line:
            continue
        line = re.sub(r'"(line|col)":\d+', r'"\1":0', line)
        line = re.sub(r"(\.vo):\d+:\d+", r"\1:_", line)
        line = re.sub(r"(\.vo):\d+\b", r"\1:_", line)
        line = re.sub(r'"warnings":\d+', '"warnings":0', line)
        line = re.sub(r"\d+ errors?, \d+ warnings?", lambda m: m.group(0).split(",")[0], line)
        line = re.sub(r'"text":"import [^"]*"', '"text":"import"', line)
        line = re.sub(r'"replacement":"[^"]*"', '"replacement":""', line)
        line = re.sub(r"fix: .*", "fix:", line)
        line = QUAL.sub("", line)
        out.append(line.rstrip())
    return "\n".join(out)


def git_show(rev, path):
    r = subprocess.run(["git", "show", f"{rev}:{os.path.relpath(path, ROOT)}"], cwd=ROOT, capture_output=True)
    return r.stdout.decode("utf-8", "replace") if r.returncode == 0 else None


def changed_cases(base):
    r = subprocess.run(["git", "diff", "--name-only", base, "--", "tests/cases"], cwd=ROOT, capture_output=True, text=True)
    out = set()
    for f in r.stdout.split():
        if not f.endswith(".vo") or "/imports_v03/" in f:
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
                    not any(os.path.exists(full[:-3] + e) for e in (".out", ".err", ".code")):
                out.add(("run", os.path.join(d, "main.vo")))  # a helper of a folder case
            else:
                out.add(("run", full))
    return sorted(out)


def run_case(kind, path):
    """(actual outputs {name: text}) for a case, by running the runner on a copy of its output files."""
    if kind == "steps":
        r = subprocess.run([sys.executable, os.path.join(ROOT, "tests", "steps.py"), path], cwd=ROOT, capture_output=True, text=True)
    else:
        r = subprocess.run([os.path.join(ROOT, "tests", "run.sh"), path], cwd=ROOT, capture_output=True, text=True)
    return r.returncode == 0, r.stdout


def expected_files(kind, path):
    if kind == "steps":
        exp = os.path.join(path, "expected")
        return [os.path.join(d, n) for d, _, ns in os.walk(exp) for n in ns] if os.path.isdir(exp) else []
    base = path[:-3]
    return [base + e for e in (".out", ".err", ".code") if os.path.exists(base + e)]


def compare(accept, quick, base):
    cases = changed_cases(base)
    if quick:
        cases = cases[:60]
    same, review = [], []
    for kind, path in cases:
        ok, _ = run_case(kind, path)
        if ok:
            same.append(path)
            continue
        # the new output, then compare each expected file with REV's
        if kind == "steps":
            subprocess.run([sys.executable, os.path.join(ROOT, "tests", "steps.py"), "--accept", path], cwd=ROOT, capture_output=True)
        else:
            subprocess.run([os.path.join(ROOT, "tests", "run.sh"), "--accept", path], cwd=ROOT, capture_output=True)
        diffs = []
        for f in expected_files(kind, path):
            old = git_show(base, f)
            new = open(f, encoding="utf-8", errors="replace").read()
            if old is None or normalize(old) != normalize(new):
                diffs.append(os.path.relpath(f, ROOT))
        if diffs:
            review.append((path, diffs))
            if not accept:  # put REV's expected output back
                for f in expected_files(kind, path):
                    old = git_show(base, f)
                    if old is not None:
                        open(f, "w", encoding="utf-8").write(old)
        elif not accept:
            for f in expected_files(kind, path):
                old = git_show(base, f)
                if old is not None:
                    open(f, "w", encoding="utf-8").write(old)
        else:
            same.append(path)
    for path, diffs in review:
        print("REVIEW", os.path.relpath(path, ROOT), " ".join(diffs))
    print(f"migrate_058: {len(cases)} migrated cases, {len(same)} pass or match, {len(review)} to review")
    return 1 if review else 0


def main():
    args = sys.argv[1:]
    if not args:
        print(__doc__)
        return 64
    if args[0] == "apply":
        return apply(args[1:])
    if args[0] == "text":
        print(f"migrate_058 text: {text_files(args[1:])} files changed")
        return 0
    if args[0] == "py":
        for p in args[1:]:
            src = open(p, encoding="utf-8").read()
            new = py_migrate(src)
            if new != src:
                open(p, "w", encoding="utf-8").write(new)
            left = len(re.findall(r"import (?:std|postgres)\.[a-z_]+\.[A-Za-z_{]", new))
            print(f"py: {os.path.relpath(p, ROOT)}: {'changed' if new != src else 'unchanged'}, {left} 0.3 imports left")
        return 0
    if args[0] == "compare":
        base = "HEAD"
        if "--base" in args:
            base = args[args.index("--base") + 1]
        return compare("--accept" in args, "--quick" in args, base)
    if args[0] == "--quick":  # tools/quick.sh: compare without writing
        return compare(False, True, "HEAD")
    print(__doc__)
    return 64


if __name__ == "__main__":
    sys.exit(main())
