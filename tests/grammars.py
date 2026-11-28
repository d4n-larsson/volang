#!/usr/bin/env python3
"""Spec 037: the editor grammars under editors/ agree with the language
(specs/037-editor-grammars/contracts/grammars.md).

Checks (the first two and the Vim one need no grammar tools):
  word lists          every grammar's keywords, types and effects equal the reference's and the parser's
  grammar hash        editors/tree-sitter-vo/grammar.js is the one the committed parser was made from
  generated parser    regenerating gives the committed src/ byte for byte
  highlight tests     tree-sitter's highlight assertions; the Zed and editor queries compile
  tree-sitter corpus  every .vo file `vo fmt --check` accepts parses with no ERROR or MISSING node
  TextMate agreement  the TextMate grammar gives every token the class the tree-sitter grammar gives it
  Vim                 the syntax files load without a message

The tools are editors/node_modules (`npm ci` in editors/). Without them the checks that need them
print `skip:` and the script passes; VO_GRAMMARS=require (CI) makes a skip a failure.

  grammars.py                  all checks (part of `make test`)
  grammars.py --lists          print the language's word lists
  grammars.py --reference F    read the reference from F (the drift experiment)
  grammars.py --hash           write the grammar hash (used by `make grammars`)
  grammars.py --time FILE      seconds each grammar takes on FILE
"""
import glob
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
from concurrent.futures import ThreadPoolExecutor

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ED = os.path.join(ROOT, "editors")
TSDIR = os.path.join(ED, "tree-sitter-vo")
TS = os.path.join(ED, "node_modules", ".bin", "tree-sitter")
VO = os.path.join(ROOT, "bin", "vo")
HAVE_TOOLS = os.path.exists(TS) and shutil.which("node") is not None
REQUIRE = os.environ.get("VO_GRAMMARS") == "require"
NEED = "run npm ci in editors/"
# tree-sitter compiles the parser once; keep that inside the build directory, not the home directory
ENV = dict(os.environ, TREE_SITTER_LIBDIR=os.path.join(ROOT, "build", "grammars"))
GENERATED = ["parser.c", "grammar.json", "node-types.json", "tree_sitter/alloc.h", "tree_sitter/array.h",
             "tree_sitter/parser.h"]


def read(*parts):
    return open(os.path.join(ROOT, *parts), encoding="utf-8").read()


# ---- the language's lists (research R1) ----

def language_lists(reference):
    """keywords, contextual words, collection types, constraints, effects; and problems found on the way."""
    problems = []
    ref = open(reference, encoding="utf-8").read()
    m = re.search(r"\*\*Keywords\*\* \(cannot be used as names\): `([a-z_ ]+)`", ref)
    keywords = set(m.group(1).split()) if m else set()
    parse_c = read("src", "parse.c")
    spell = re.search(r"tok_spell\[\] = \{(.*?)\};", parse_c, re.S).group(1)
    in_parser = {w for w in re.findall(r'"([^"]*)"', spell) if re.fullmatch(r"[a-z]+", w) and w not in ("name", "number", "string", "comment")}
    for w in sorted(keywords ^ in_parser):
        problems.append(f"keywords: `{w}` is in {'the reference' if w in keywords else 'src/parse.c'} only")
    contextual = set(re.findall(r'str_eq_bytes\(str_from_cstr\("([a-z_]+)"\)', parse_c))            # const, effects
    constraints = set(re.findall(r'str_eq_cstr\(p_tok_str\(p_tok\(\)\), "([a-z_]+)"\)', parse_c))   # ordered, ...
    collections = set(re.findall(r'"([a-z_]+)"', re.search(r"p_coll_kind.*?names\[\] = \{(.*?)\}", parse_c, re.S).group(1)))
    table = re.search(r"\| Effect \| Covers \|\n\|[-|]+\|\n((?:\|.*\n)+)", ref)
    effects = set()
    for row in (table.group(1).splitlines() if table else []):
        effects |= {e.strip() for e in row.split("|")[1].split(",")}
    for name, got in (("contextual words", contextual), ("constraints", constraints), ("collection types", collections),
                      ("effects", effects)):
        if not got:
            problems.append(f"{name}: none found (the extraction in tests/grammars.py no longer matches its source)")
        for w in sorted(got):
            if not re.search(r"(?<![A-Za-z0-9_])" + re.escape(w) + r"(?![A-Za-z0-9_])", ref):
                problems.append(f"{name}: `{w}` is in the toolchain but not in the reference")
    return {"keywords": keywords, "contextual": contextual, "constraints": constraints, "collections": collections,
            "effects": effects}, problems


def diff(label, path, got, want):
    return [f"{label}: `{w}` missing in {path}" for w in sorted(want - got)] + \
           [f"{label}: `{w}` extra in {path}" for w in sorted(got - want)]


def check_word_lists(reference):
    lists, problems = language_lists(reference)
    words = lists["keywords"] | lists["contextual"] | lists["constraints"] | lists["collections"]
    # the site's lexer: three tuples
    path = "site/pygments_vo/pygments_vo.py"
    src = read(path)
    tup = lambda name: set(re.findall(r'"([a-z_.]+)"', re.search(name + r" = \((.*?)\)", src, re.S).group(1)))
    problems += diff("words", path, tup("KEYWORDS") | tup("TYPES") | {"true", "false"}, words)
    problems += diff("effects", path, tup("EFFECTS"), {e.split(".")[0] for e in lists["effects"]})
    # the tree-sitter grammar: every word-like token of the generated grammar
    path = "editors/tree-sitter-vo/src/grammar.json"
    tokens = set()

    def walk(node):
        if isinstance(node, dict):
            if node.get("type") == "STRING" and re.fullmatch(r"[a-z][a-z_]*", node.get("value", "")):
                tokens.add(node["value"])
            for v in node.values():
                walk(v)
        elif isinstance(node, list):
            for v in node:
                walk(v)
    walk(json.loads(read(path)))
    problems += diff("words", path, tokens, words)
    # the TextMate grammar: every alternation of words
    path = "editors/vscode/syntaxes/vo.tmLanguage.json"
    tm = set()
    for group in re.findall(r"\((?:\?:)?([a-z_]+(?:\|[a-z_]+)*)\)", json.dumps(json.loads(read(path)))):
        tm |= set(group.split("|"))
    problems += diff("words", path, tm, words | {"uses"})
    # Vim: `syn keyword` lines and \(a\|b\) alternations; effects from the voEffect line
    path = "editors/vim/syntax/vo.vim"
    vim, vim_effects = set(), set()
    for line in read(path).splitlines():
        if line.startswith("syn keyword"):
            vim |= set(line.split()[3:])
        for group in re.findall(r"\\\(([a-z_.\\|]+)\\\)", line):
            (vim_effects if "voEffect" in line else vim).update(group.replace("\\.", ".").split("\\|"))
    vim.add("uses") if re.search(r"start=/\\<uses\\>/", read(path)) else None
    problems += diff("words", path, vim, words | {"uses"})
    problems += diff("effects", path, vim_effects, lists["effects"])
    return problems


# ---- the checks ----

def sha(path):
    return hashlib.sha256(open(path, "rb").read()).hexdigest()


def check_hash():
    want = sha(os.path.join(TSDIR, "grammar.js"))
    got = read("editors", "tree-sitter-vo", "src", "grammar.sha256").strip()
    return [] if got == want else ["editors/tree-sitter-vo/grammar.js changed since the parser was generated: run `make grammars`"]


def run(args, cwd=TSDIR, **kw):
    return subprocess.run(args, cwd=cwd, capture_output=True, text=True, env=ENV, **kw)


def check_generated():
    tmp = tempfile.mkdtemp(prefix="vo-grammar-")
    try:
        for f in ("grammar.js", "tree-sitter.json"):
            shutil.copy(os.path.join(TSDIR, f), tmp)
        r = run([TS, "generate", "--abi", "14"], cwd=tmp)
        if r.returncode:
            return [f"tree-sitter generate fails: {r.stderr.strip()[-400:]}"]
        return [f"editors/tree-sitter-vo/src/{f} is not what grammar.js generates: run `make grammars`"
                for f in GENERATED
                if not os.path.exists(os.path.join(TSDIR, "src", f)) or
                open(os.path.join(tmp, "src", f), "rb").read() != open(os.path.join(TSDIR, "src", f), "rb").read()]
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def check_highlights():
    problems = []
    r = run([TS, "test"])
    if r.returncode:
        problems.append("tree-sitter test fails:\n" + re.sub(r"\x1b\[[0-9;]*m", "", r.stdout + r.stderr).strip()[-1200:])
    sample = os.path.join(ROOT, "examples", "expenses", "src", "main.vo")
    queries = sorted(glob.glob(os.path.join(TSDIR, "queries", "*.scm")) + glob.glob(os.path.join(ED, "zed", "languages", "vo", "*.scm")))
    for q in queries:
        r = run([TS, "query", q, sample])
        if r.returncode:
            problems.append(f"{os.path.relpath(q, ROOT)} does not compile against the grammar: {r.stderr.strip()[-300:]}")
    if read("editors", "zed", "languages", "vo", "highlights.scm") != read("editors", "tree-sitter-vo", "queries", "highlights.scm"):
        problems.append("editors/zed/languages/vo/highlights.scm is not a copy of editors/tree-sitter-vo/queries/highlights.scm")
    return problems


_corpus = None


def corpus():
    """(parses, rejected): the repository's .vo files, split by whether the toolchain parses them."""
    global _corpus
    if _corpus is None:
        files = sorted(f for d in ("tests/cases", "examples", "std")
                       for f in glob.glob(os.path.join(ROOT, d, "**", "*.vo"), recursive=True))
        with ThreadPoolExecutor(os.cpu_count() or 4) as pool:
            codes = list(pool.map(lambda f: subprocess.run([VO, "fmt", "--check", f], capture_output=True).returncode, files))
        _corpus = ([f for f, c in zip(files, codes) if c in (0, 1)], [f for f, c in zip(files, codes) if c not in (0, 1)])
    return _corpus


def paths_file(tmp, name, files):
    path = os.path.join(tmp, name)
    open(path, "w").write("".join(f + "\n" for f in files))
    return path


def check_corpus():
    parses, rejected = corpus()
    if len(parses) < 1000:
        return [f"only {len(parses)} of {len(parses) + len(rejected)} files parse with the toolchain: is build/vo built?"]
    tmp = tempfile.mkdtemp(prefix="vo-grammar-")
    try:
        r = run([TS, "parse", "--quiet", "--paths", paths_file(tmp, "parses", parses)])
        bad = [line for line in r.stdout.splitlines() if "ERROR" in line or "MISSING" in line]
        problems = [re.sub(r"\s+", " ", line.replace(ROOT + "/", "")) for line in bad[:20]]
        if len(bad) > 20:
            problems.append(f"... and {len(bad) - 20} more")
        if r.returncode and not bad:
            problems.append(f"tree-sitter parse fails: {r.stderr.strip()[-300:]}")
        try:  # what the toolchain rejects only has to end
            run([TS, "parse", "--quiet", "--paths", paths_file(tmp, "rejected", rejected)], timeout=60)
        except subprocess.TimeoutExpired:
            problems.append("the parser does not end on the files the toolchain rejects")
        check_corpus.note = f"{len(parses)} files parse, {len(rejected)} rejected by the toolchain"
        return problems
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def check_textmate():
    problems = []
    for f in ("vscode/package.json", "vscode/language-configuration.json", "vscode/syntaxes/vo.tmLanguage.json"):
        try:
            json.loads(read("editors", f))
        except ValueError as e:
            problems.append(f"editors/{f} is not JSON: {e}")
    pkg = json.loads(read("editors", "vscode", "package.json"))["contributes"]
    for rel in [pkg["languages"][0]["configuration"], pkg["grammars"][0]["path"]]:
        if not os.path.exists(os.path.join(ED, "vscode", rel)):
            problems.append(f"editors/vscode/package.json names {rel}, which does not exist")
    if problems:
        return problems
    tmp = tempfile.mkdtemp(prefix="vo-grammar-")
    try:
        r = run(["node", "check.js", "agree", paths_file(tmp, "parses", corpus()[0])], cwd=ED)
        if r.returncode:
            problems += (r.stdout + r.stderr).strip().splitlines()[-23:]
        else:
            check_textmate.note = r.stdout.strip().splitlines()[-1]
        r = run(["node", "check.js", "recover"], cwd=ED)
        if r.returncode:
            problems += (r.stdout + r.stderr).strip().splitlines()
        return problems
    finally:
        shutil.rmtree(tmp, ignore_errors=True)


def check_vim():
    out = tempfile.mktemp(prefix="vo-vim-")
    sample = os.path.join(ROOT, "examples", "expenses", "src", "main.vo")
    # the group under the `uses` of line 1's function, and every message Vim printed
    subprocess.run(["vim", "-es", "-u", "NONE", "-c", f"set rtp+={ED}/vim", "-c", "syntax on", "-c", "filetype plugin on",
                    "-c", "runtime ftdetect/vo.vim", "-c", f"edit {sample}", "-c", f"redir! > {out}",
                    "-c", 'silent echo "ft=" . &filetype . " syn=" . get(b:, "current_syntax", "") . " group=" . '
                          'synIDattr(synID(search("^fn"), 1, 1), "name")',
                    "-c", "silent messages", "-c", "redir END", "-c", "qa!"], capture_output=True, timeout=30)
    text = open(out).read() if os.path.exists(out) else ""
    if os.path.exists(out):
        os.remove(out)
    problems = [] if "ft=vo syn=vo group=voKeyword" in text else [f"the Vim files do not load or colour: {text.strip()[:300]!r}"]
    problems += [f"Vim message: {line}" for line in text.splitlines() if re.match(r"E\d+:", line.strip())]
    return problems


def timing(path):
    r = run([TS, "parse", "--quiet", "--time", path])
    t = time.perf_counter()
    run([TS, "parse", "--quiet", path])
    print(f"tree-sitter: {time.perf_counter() - t:.3f} s for the whole command ({r.stdout.strip()[-60:]})")
    print(f"TextMate: {run(['node', 'check.js', 'time', path], cwd=ED).stdout.strip()} s")


def main():
    reference = os.path.join(ROOT, "docs", "reference.md")
    if "--reference" in sys.argv:
        reference = sys.argv[sys.argv.index("--reference") + 1]
    if "--lists" in sys.argv:
        lists, problems = language_lists(reference)
        for name, words in lists.items():
            print(f"{name}: {' '.join(sorted(words))}")
        print("\n".join(problems))
        return 0
    if "--hash" in sys.argv:
        open(os.path.join(TSDIR, "src", "grammar.sha256"), "w").write(sha(os.path.join(TSDIR, "grammar.js")) + "\n")
        return 0
    if "--time" in sys.argv:
        timing(os.path.abspath(sys.argv[sys.argv.index("--time") + 1]))
        return 0
    checks = [("word lists", lambda: check_word_lists(reference), None),
              ("grammar hash", check_hash, None),
              ("generated parser", check_generated, NEED),
              ("highlight tests", check_highlights, NEED),
              ("tree-sitter corpus", check_corpus, NEED),
              ("TextMate agreement", check_textmate, NEED),
              ("Vim", check_vim, None if shutil.which("vim") else "install vim")]
    skipped = failed = 0
    for name, check, need in checks:
        if need and (need != NEED or not HAVE_TOOLS):
            skipped += 1
            failed += REQUIRE
            print(f"{'FAIL (required)' if REQUIRE else 'skip'}: {name} ({need})")
            continue
        problems = check()
        note = getattr(check, "note", "")
        print(f"{'FAIL' if problems else 'ok'}: {name}{' (' + note + ')' if note and not problems else ''}")
        for p in problems:
            print(f"  {p}")
        failed += bool(problems)
    print(f"grammars: {len(checks)} checks, {skipped} skipped, {failed} failures")
    return 1 if failed else 0


sys.exit(main())
