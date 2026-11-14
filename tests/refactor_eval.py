#!/usr/bin/env python3
"""Spec 006 SC-001/SC-002 stand-in: do checked edits and queries get the refactor tasks'
mechanical steps right where text replacement and text search do not?

For each tests/refactor_eval/rNN-*/ (the bench2 refactor task of the same name):
  edits.txt  one edit per line, each applied alone to the task's starter program:
             `rename TARGET NEW_NAME` or `add-field RECORD FIELD: TYPE = DEFAULT`
  refs.txt   blocks of `refs TARGET` / `callers NAME`, then the expected `FILE:LINE:COL ROLE`
             lines, then a blank line; written by hand from the starter (never with vo query)

Per edit: `vo edit` must exit 0, `vo check` must be clean, and the program must print exactly
what the unedited starter prints on every visible and hidden input. The same rename done by
whole-word text replacement over the program's files is checked and run too; the script counts
how often that breaks the program (does not check, or prints something else).
Per query: the answer must equal the expected lines; whole-word text-search hits of the same
name that are not expected are counted.
Exit 1 if any vo edit or query is wrong, or there are fewer than 30 edits or 30 names.

  refactor_eval.py [TASK...]
"""
import concurrent.futures as cf
import os
import re
import shutil
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")
EVAL = os.path.join(ROOT, "tests", "refactor_eval")
TASKS = os.path.join(ROOT, "bench2", "tasks")


def vo(args, cwd, stdin=b""):
    return subprocess.run([VO] + args, cwd=cwd, input=stdin, capture_output=True, timeout=300)


def copy_starter(task):
    d = tempfile.mkdtemp()
    shutil.copytree(os.path.join(TASKS, task, "starter", "vo"), d, dirs_exist_ok=True)
    return d


def inputs(task):
    out = []
    for kind in ("visible", "hidden"):
        cdir = os.path.join(TASKS, task, kind)
        out += sorted(os.path.join(cdir, n) for n in os.listdir(cdir) if n.endswith(".in"))
    return out


def outputs(d, ins):
    return [vo(["run", "--timeout", "30", "main.vo"], d, open(i, "rb").read()).stdout for i in ins]


def program_files(d):
    return sorted(n for n in os.listdir(d) if n.endswith(".vo"))


def read_lines(path):
    if not os.path.exists(path):
        return []
    return [l.rstrip("\n") for l in open(path, encoding="utf-8") if l.strip() and not l.startswith("#")]


def text_rename(d, target, new):
    """Whole-word replacement of the target's name over every program file (a naive refactor)."""
    if re.match(r".+:\d+:\d+$", target):
        f, line, col = target.rsplit(":", 2)
        text = open(os.path.join(d, f), encoding="utf-8").read().split("\n")[int(line) - 1]
        old = re.match(r"\w+", text[int(col) - 1:]).group(0)
    else:
        old = target.split(".")[-1]
    for f in program_files(d):
        p = os.path.join(d, f)
        s = open(p, encoding="utf-8").read()
        open(p, "w", encoding="utf-8").write(re.sub(r"\b%s\b" % re.escape(old), new, s))
    return old


def judge_edit(task, line, base_out, ins):
    words = line.split()
    kind = words[0]
    d = copy_starter(task)
    try:
        r = vo(["edit", kind, "main.vo"] + words[1:], d)
        problem = None
        if r.returncode != 0:
            problem = "vo edit exited %d: %s" % (r.returncode, (r.stdout + r.stderr).decode()[-300:])
        else:
            c = vo(["check", "main.vo"], d)
            if c.returncode != 0:
                problem = "vo check fails after the edit"
            elif outputs(d, ins) != base_out:
                problem = "behaviour changed"
    finally:
        shutil.rmtree(d)
    naive = None
    if kind == "rename":
        d = copy_starter(task)
        try:
            text_rename(d, words[1], words[2])
            if vo(["check", "main.vo"], d).returncode != 0:
                naive = "does not check"
            elif outputs(d, ins) != base_out:
                naive = "behaviour changed"
            else:
                naive = "ok"
        finally:
            shutil.rmtree(d)
    return line, problem, naive


def parse_refs(path):
    blocks, cur = [], None
    for raw in open(path, encoding="utf-8").read().split("\n"):
        line = raw.strip()
        if line.startswith("#"):
            continue
        if not line:
            cur = None
            continue
        if cur is None:
            q, target = line.split(None, 1)
            cur = (q, target, set())
            blocks.append(cur)
        else:
            cur[2].add(line)
    return blocks


def judge_query(task, q, target, expected):
    d = copy_starter(task)
    try:
        r = vo(["query", q, "main.vo", target], d)
        got = set()
        for l in r.stdout.decode().split("\n"):
            m = re.match(r"(\S+:\d+:\d+) (\w+)", l)
            if m:
                got.add(m.group(1) + " " + m.group(2))
        name = target.split(".")[-1] if not re.match(r".+:\d+:\d+$", target) else None
        if name is None:
            f, line, col = target.rsplit(":", 2)
            text = open(os.path.join(d, f), encoding="utf-8").read().split("\n")[int(line) - 1]
            name = re.match(r"\w+", text[int(col) - 1:]).group(0)
        hits = set()
        for f in program_files(d):
            for i, l in enumerate(open(os.path.join(d, f), encoding="utf-8").read().split("\n"), 1):
                for m in re.finditer(r"\b%s\b" % re.escape(name), l):
                    hits.add("%s:%d:%d" % (f, i, len(l[:m.start()]) + 1))
        exp_pos = {e.split()[0] for e in expected}
        wrong_hits = len(hits - exp_pos)
    finally:
        shutil.rmtree(d)
    return q, target, got == expected, sorted(expected - got), sorted(got - expected), wrong_hits


def main():
    only = sys.argv[1:]
    rows, problems = [], []
    n_edits = n_names = 0
    for task in sorted(os.listdir(EVAL)):
        if only and task not in only:
            continue
        tdir = os.path.join(EVAL, task)
        edits = read_lines(os.path.join(tdir, "edits.txt"))
        ins = inputs(task)
        d = copy_starter(task)
        try:
            base_out = outputs(d, ins)
        finally:
            shutil.rmtree(d)
        with cf.ThreadPoolExecutor(os.cpu_count() or 4) as ex:
            results = list(ex.map(lambda e: judge_edit(task, e, base_out, ins), edits))
            blocks = parse_refs(os.path.join(tdir, "refs.txt")) if os.path.exists(os.path.join(tdir, "refs.txt")) else []
            qres = list(ex.map(lambda b: judge_query(task, *b), blocks))
        ok_edits = sum(1 for _, p, _ in results if p is None)
        renames = [r for r in results if r[2] is not None]
        naive_broken = sum(1 for _, _, n in renames if n != "ok")
        ok_queries = sum(1 for r in qres if r[2])
        wrong_hits = sum(r[5] for r in qres)
        for line, p, _ in results:
            if p:
                problems.append("%s: `%s`: %s" % (task, line, p))
        for q, t, ok, missing, extra, _ in qres:
            if not ok:
                problems.append("%s: query %s %s: missing %s, extra %s" % (task, q, t, missing, extra))
        rows.append((task, len(edits), ok_edits, len(renames), naive_broken, len(blocks), ok_queries, wrong_hits))
        n_edits += len(edits)
        n_names += len(blocks)
    print("%-28s %5s %7s %7s %8s %7s %7s %9s" % ("task", "edits", "vo ok", "renames", "text bad",
                                                    "names", "query ok", "grep extra"))
    tot = [0] * 7
    for row in rows:
        print("%-28s %5d %7d %7d %8d %7d %7d %9d" % row)
        tot = [a + b for a, b in zip(tot, row[1:])]
    print("%-28s %5d %7d %7d %8d %7d %7d %9d" % tuple(["total"] + tot))
    if not only and (n_edits < 30 or n_names < 30):
        problems.append("need at least 30 edits and 30 names (have %d and %d)" % (n_edits, n_names))
    for p in problems:
        print("FAIL", p)
    return 1 if problems else 0


if __name__ == "__main__":
    sys.exit(main())
