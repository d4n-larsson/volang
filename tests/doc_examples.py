#!/usr/bin/env python3
"""The example checker shared by tests/ref_examples.py (docs/reference.md) and tests/site_examples.py (the
documentation site, spec 015). Conventions (specs/015-docs-site/contracts/examples.md):

- every ```vo example runs (followed by ```output), fails with a code (followed by ```error E####),
  or checks clean; consecutive blocks starting with `# file: name.vo` form one multi-file program
- a ```stdin block between a ```vo example and its ```output is the program's standard input
- ```vo-test blocks run with `vo test` against the following ```output (human format, ms normalized)
- ```vo-fmt blocks are formatted and compared with the following ```vo block
- a ```console block after a ```vo example runs each `$ vo ...` line in the example's directory
  and compares its stdout with the lines below it
Commands that are not implemented yet (exit 125) are reported as pending, not as failures.
"""
import os
import re
import subprocess
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")

FENCE = re.compile(r"^```([\w-]*)(?: (\S+))?\n(.*?)^```\n", re.M | re.S)


def vo(args, cwd, stdin=None):
    return subprocess.run([VO] + args, capture_output=True, cwd=cwd, timeout=60,
                          input=stdin if stdin is not None else b"")


def norm(text):
    return re.sub(r"\b\d+ms\)", "0ms)", text)


def blocks_of(text):
    return [(m.start(), m.group(1), m.group(2), m.group(3)) for m in FENCE.finditer(text)]


def check_text(text, page=""):
    """(examples, problems, pending) for every example in a Markdown text; page prefixes locations."""
    problems, pending = [], []
    blocks = blocks_of(text)

    def where(pos):
        return f"{page + ':' if page else 'line '}{text.count(chr(10), 0, pos) + 1}"

    i = 0
    examples = 0
    while i < len(blocks):
        pos, lang, arg, body = blocks[i]
        if lang not in ("vo", "vo-test", "vo-fmt"):
            i += 1
            continue
        examples += 1
        group = [(pos, body)]
        if lang == "vo" and body.startswith("# file: "):
            while i + 1 < len(blocks) and blocks[i + 1][1] == "vo" and blocks[i + 1][3].startswith("# file: "):
                i += 1
                group.append((blocks[i][0], blocks[i][3]))
        nxt = blocks[i + 1] if i + 1 < len(blocks) else None
        stdin = None
        if lang == "vo" and nxt and nxt[1] == "stdin":  # spec 015: input for the run
            stdin = nxt[3].encode()
            i += 1
            nxt = blocks[i + 1] if i + 1 < len(blocks) else None
        with tempfile.TemporaryDirectory() as d:
            paths = []
            for k, (_, b) in enumerate(group):
                m = re.match(r"# file: (\S+)\n", b)
                name = m.group(1) if m else "main.vo"
                p = os.path.join(d, name)
                os.makedirs(os.path.dirname(p), exist_ok=True)
                open(p, "w", encoding="utf-8").write(b)
                paths.append((p, "fn main" in b))
            entry = next((p for p, has in paths if has), paths[0][0])
            label = where(pos)
            if lang == "vo-test":
                r = vo(["test", os.path.relpath(entry, d)], d)
                if r.returncode == 125:
                    pending.append(f"{label}: vo-test (vo test not implemented yet)")
                elif not nxt or nxt[1] != "output":
                    problems.append(f"{label}: vo-test block needs a following ```output block")
                elif norm(r.stdout.decode()) != nxt[3]:
                    problems.append(f"{label}: vo test output differs:\n{r.stdout.decode()}")
                if nxt and nxt[1] == "output":
                    i += 1
            elif lang == "vo-fmt":
                r = vo(["fmt", entry], d)
                if r.returncode == 125:
                    pending.append(f"{label}: vo-fmt (vo fmt not implemented yet)")
                elif not nxt or nxt[1] != "vo":
                    problems.append(f"{label}: vo-fmt block needs a following ```vo block")
                elif open(entry, encoding="utf-8").read() != nxt[3]:
                    problems.append(f"{label}: formatted text differs from the following block")
                if nxt and nxt[1] == "vo":
                    i += 1
            elif nxt and nxt[1] == "console":
                cmds = re.split(r"^\$ vo ", nxt[3], flags=re.M)[1:]
                for c in cmds:
                    first, _, expected = c.partition("\n")
                    r = vo(first.split(), d)
                    if norm(r.stdout.decode()) != norm(expected):
                        problems.append(f"{label}: `vo {first}` output differs.\n  expected: {expected!r}\n  got: {r.stdout.decode()!r}")
                i += 1
            elif nxt and nxt[1] == "output":
                r = vo(["run", entry], d, stdin)
                if r.stdout.decode() != nxt[3]:
                    problems.append(f"{label}: output differs.\n  expected: {nxt[3]!r}\n  got: {r.stdout.decode()!r}\n  stderr: {r.stderr.decode()[:300]}")
                i += 1
            elif nxt and nxt[1] == "error":
                code = nxt[2] or ""
                r = vo(["check", "--json", entry], d)
                codes = re.findall(r'"code":"(\w+)"', r.stdout.decode())
                if code not in codes:
                    r2 = vo(["run", "--json", entry], d)
                    codes += re.findall(r'"code":"(\w+)"', r2.stderr.decode())
                if code not in codes:
                    problems.append(f"{label}: expected {code}, got {codes}")
                i += 1
            else:
                r = vo(["check", "--json", entry], d)
                diags = re.findall(r'"code":"(\w+)"', r.stdout.decode())
                if r.returncode != 0 or diags:
                    problems.append(f"{label}: example does not check clean: {diags}")
        i += 1
    return examples, problems, pending
