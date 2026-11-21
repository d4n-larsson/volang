#!/usr/bin/env python3
"""Assembles the documentation site (spec 015) from the repository; run by `make site` with the site's
virtual environment (it needs PyYAML, a dependency of MkDocs).

  tools/site.py            build/site-src: the hand-written pages (site/docs) plus the pages generated from
                           the toolchain's sources (reference, error catalog, standard library, examples,
                           security, roadmap, specs), and build/mkdocs.gen.yml with the navigation and version
  tools/site.py --agents   after `mkdocs build`: a Markdown copy of every page, llms.txt and llms-full.txt in
                           build/site (specs/015-docs-site/contracts/agents.md)

Generated pages are never committed and never edited by hand (FR-010); their content is checked against the
toolchain by tests/ref_examples.py, tests/catalog.py and tests/doc_conformance.py.
"""
import json
import os
import re
import shutil
import subprocess
import sys

import yaml

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")
SRC = os.path.join(ROOT, "build", "site-src")
OUT = os.path.join(ROOT, "build", "site")
GEN = os.path.join(ROOT, "build", "mkdocs.gen.yml")
REPO = "https://github.com/d4n-larsson/volang"
SITE = "https://d4n-larsson.github.io/volang/"

START = ["install", "first-program", "tests-and-effects"]
GUIDE = ["basics", "types", "records-and-variants", "functions", "generics", "results-and-errors", "lists-and-maps",
         "collections", "effects", "tests", "properties", "contracts", "holes", "modules-and-packages",
         "standard-library", "files-and-processes", "concurrency", "apis", "determinism", "structure"]
TOOLS = ["check", "run", "test", "build", "fmt", "effects", "explain", "doc", "reference", "query", "impact", "edit",
         "replay", "package", "mcp", "editors", "version", "json-output", "exit-statuses"]

# repository file -> generated page (site-src path), for rewriting links between sources
PAGES = {"docs/reference.md": "reference/index.md", "docs/errors.md": "errors/index.md",
         "docs/security.md": "security/index.md", "docs/security-review-1.md": "security/review-1.md",
         "docs/roadmap.md": "roadmap.md", "examples/README.md": "examples/index.md"}


def run(args, **kw):
    return subprocess.run(args, capture_output=True, text=True, check=True, cwd=ROOT, **kw).stdout


def slug(title):
    return re.sub(r"[^a-z0-9]+", "-", title.lower()).strip("-")


def write(path, text, source=None):
    """A generated page; source (a repository path) becomes its edit link."""
    full = os.path.join(SRC, path)
    os.makedirs(os.path.dirname(full), exist_ok=True)
    head = f"---\nsource: {source}\n---\n\n" if source else ""
    open(full, "w", encoding="utf-8").write(head + text)


def outside_code(text, fn):
    """Applies fn to the parts of a Markdown text outside code blocks and inline code."""
    parts = re.split(r"(^```.*?^```\n)", text, flags=re.M | re.S)
    out = []
    for k, part in enumerate(parts):
        if k % 2:
            out.append(part)
            continue
        spans = re.split(r"(`[^`\n]*`)", part)
        out.append("".join(s if j % 2 else fn(s) for j, s in enumerate(spans)))
    return "".join(out)


def rewrite_links(text, source, page):
    """Relative links of a repository file (source) as seen from a site page: to its page when the target
    is on the site, else to the repository."""
    def one(m):
        label, target = m.group(1), m.group(2)
        if re.match(r"[a-z]+:|#", target):
            return m.group(0)
        path, _, anchor = target.partition("#")
        repo_path = os.path.normpath(os.path.join(os.path.dirname(source), path)).replace(os.sep, "/")
        if repo_path in PAGES:
            rel = os.path.relpath(PAGES[repo_path], os.path.dirname(page) or ".").replace(os.sep, "/")
            return f"[{label}]({rel}{'#' + anchor if anchor else ''})"
        if re.match(r"examples/[\w-]+$", repo_path):
            rel = os.path.relpath(repo_path + ".md", os.path.dirname(page) or ".").replace(os.sep, "/")
            return f"[{label}]({rel})"
        kind = "tree" if path.endswith("/") or os.path.isdir(os.path.join(ROOT, repo_path)) else "blob"
        return f"[{label}]({REPO}/{kind}/main/{repo_path}{'#' + anchor if anchor else ''})"
    return outside_code(text, lambda s: re.sub(r"\[([^\]]*)\]\(([^)\s]+)\)", one, s))


def demote(text):
    """### -> ##, #### -> ### outside code: a section becomes a page whose title is the only # heading."""
    return outside_code(text, lambda s: re.sub(r"^#(#+ )", r"\1", s, flags=re.M))


def first_sentence(text):
    for para in re.split(r"\n\s*\n", text):
        para = para.strip()
        if para and not para.startswith(("#", "```", "|", "-", ">", "<!--")):
            para = re.sub(r"\s+", " ", para)
            m = re.match(r"(.+?[.!?])(\s|$)", para)
            return m.group(1) if m else para
    m = re.search(r"^- (.+?[.!?])(\s|$)", text, re.M)  # a page that opens with a list: its first item
    return m.group(1) if m else ""


# ---- generated pages ----

def reference():
    text = open(os.path.join(ROOT, "docs", "reference.md"), encoding="utf-8").read()
    pieces = re.split(r"^## (.+)$", text, flags=re.M)
    preamble, sections = pieces[0], list(zip(pieces[1::2], pieces[2::2]))
    nav = [{"Overview": "reference/index.md"}]
    lines = [f"- [{t}]({slug(t)}.md)" for t, _ in sections]
    intro = re.sub(r"^# .*\n", "", preamble).strip()
    write("reference/index.md", "# Language reference\n\nThe complete reference of the vo language, as `vo reference` "
          "prints it and as agents read it (under 15,000 tokens), one page per section.\n\n" + intro + "\n\n## Sections\n\n"
          + "\n".join(lines) + "\n", "docs/reference.md")
    for title, body in sections:
        page = f"reference/{slug(title)}.md"
        write(page, f"# {title}\n" + rewrite_links(demote(body), "docs/reference.md", page), "docs/reference.md")
        nav.append({title: page})
    return nav


def errors():
    text = open(os.path.join(ROOT, "docs", "errors.md"), encoding="utf-8").read()
    entries = re.findall(r"^## ([EWRT]\d{4}) (\w+)\n(.*?)(?=^## [EWRT]\d{4} |\Z)", text, re.M | re.S)
    known = set()
    for code, name, body in entries:
        out = subprocess.run([VO, "explain", code], capture_output=True, text=True)
        if out.returncode == 0:
            known.add(code)
        page = f"errors/{code}.md"
        write(page, f"# {code} {name}\n" + rewrite_links(demote(body), "docs/errors.md", page), "docs/errors.md")
    if known != {c for c, _, _ in entries}:
        sys.exit(f"site: the catalog and `vo explain` disagree on {sorted(known ^ {c for c, _, _ in entries})}")
    rows = "\n".join(f"| [{c}]({c}.md) | `{n}` | {first_sentence(b)} |" for c, n, b in entries)
    groups = ("E01 syntax, E02 names and imports, E03 types, E04 control flow, E05 effects, E06 holes, properties "
              "and contracts, E07 packages, W warnings, R runtime errors, T toolchain errors")
    write("errors/index.md", "# Error catalog\n\nEvery code the toolchain reports, with its meaning, an example and "
          "its fix; `vo explain CODE` prints the same entry. Codes are grouped by prefix: " + groups + ".\n\n"
          "| Code | Name | Meaning |\n|---|---|---|\n" + rows + "\n", "docs/errors.md")
    return [{"Error catalog": "errors/index.md"}]


def stdlib():
    recs = [json.loads(l) for l in run([VO, "doc", "--json"]).splitlines() if l.strip()]
    modules = [r for r in recs if r.get("kind") == "doc" and r.get("item") == "module"]
    nav = [{"Overview": "std/index.md"}]
    rows = []
    for m in modules:
        name = m["name"]
        short = name.split(".")[-1]
        items = [json.loads(l) for l in run([VO, "doc", "--json", name]).splitlines() if l.strip()]
        items = [r for r in items if r.get("kind") == "doc" and r.get("item") != "module"]
        parts = [f"# {name}\n\n{m.get('description', '')}\n\nImport items one by one: `import {name}.NAME;`. "
                 f"`vo doc {name}.NAME` prints the same entry.\n"]
        for r in items:
            parts.append(f"\n## {r['name']}\n\n```text\n{r['signature']}\n```\n\n{r.get('description', '')}\n")
            if r.get("example"):
                parts.append(f"\nExample (`EXPR is VALUE`):\n\n```text\n{r['example']}\n```\n")
        write(f"std/{short}.md", "".join(parts), f"std/{short}.vo")
        nav.append({name: f"std/{short}.md"})
        rows.append(f"| [{name}]({short}.md) | {m.get('description', '')} |")
    write("std/index.md", "# Standard library\n\nThe modules built into `vo`, written in vo; `vo doc` prints the "
          "same documentation. Builtins (`len`, `push`, `sort`, ...) are listed in the reference's "
          "[Builtins](../reference/builtins.md) section.\n\n| Module | Contents |\n|---|---|\n" + "\n".join(rows) + "\n")
    return nav


def examples():
    nav = [{"Overview": "examples/index.md"}]
    readme = open(os.path.join(ROOT, "examples", "README.md"), encoding="utf-8").read()
    write("examples/index.md", rewrite_links(readme, "examples/README.md", "examples/index.md"), "examples/README.md")
    for name in sorted(os.listdir(os.path.join(ROOT, "examples"))):
        d = os.path.join(ROOT, "examples", name)
        if not os.path.isdir(d):
            continue
        page = f"examples/{name}.md"
        text = rewrite_links(open(os.path.join(d, "README.md"), encoding="utf-8").read(), f"examples/{name}/README.md", page)
        src = [f for f in sorted(os.listdir(d)) if f.endswith(".vo")]
        text += "\n## Source\n\nThe program as it is in the repository ([examples/" + name + "](" + REPO + "/tree/main/examples/" \
            + name + "); `make test` checks and tests it).\n"
        for f in src:
            text += f"\n### {f}\n\n```vo\n" + open(os.path.join(d, f), encoding="utf-8").read() + "```\n"
        write(page, text, f"examples/{name}/README.md")
        nav.append({name: page})
    return nav


def copied():
    for source, page in (("docs/security.md", "security/index.md"), ("docs/security-review-1.md", "security/review-1.md"),
                         ("docs/roadmap.md", "roadmap.md")):
        write(page, rewrite_links(open(os.path.join(ROOT, source), encoding="utf-8").read(), source, page), source)
    rows = []
    for d in sorted(os.listdir(os.path.join(ROOT, "specs"))):
        spec = os.path.join(ROOT, "specs", d, "spec.md")
        if not re.match(r"\d{3}-", d) or not os.path.exists(spec):
            continue
        title = re.sub(r"^# (Feature Specification: )?", "", open(spec, encoding="utf-8").readline().strip())
        results = os.path.exists(os.path.join(ROOT, "specs", d, "results.md"))
        rows.append(f"| {d[:3]} | [{title}]({REPO}/tree/main/specs/{d}) | "
                    + (f"[results]({REPO}/blob/main/specs/{d}/results.md)" if results else "") + " |")
    write("specs.md", "# Specifications\n\nEvery feature of vo was specified, planned and built through a spec in "
          "`specs/` (spec, plan, tasks, results); the [roadmap](roadmap.md) says what each delivered and what "
          "comes next.\n\n| Spec | Title | Results |\n|---|---|---|\n" + "\n".join(rows) + "\n")


# ---- assembly ----

def assemble():
    shutil.rmtree(SRC, ignore_errors=True)
    shutil.copytree(os.path.join(ROOT, "site", "docs"), SRC)
    os.makedirs(os.path.join(SRC, "assets"), exist_ok=True)
    shutil.copy(os.path.join(ROOT, "docs", "vo.svg"), os.path.join(SRC, "assets", "vo.svg"))
    version = run([VO, "version"]).strip()
    commit = run(["git", "rev-parse", "--short", "HEAD"]).strip()
    ref_nav, err_nav, std_nav, ex_nav = reference(), errors(), stdlib(), examples()
    copied()

    def present(prefix, names):
        return [f"{prefix}/{n}.md" for n in names if os.path.exists(os.path.join(SRC, prefix, n + ".md"))]

    nav = [{"Home": "index.md"},
           {"Getting started": present("start", START)},
           {"Language guide": present("guide", GUIDE)},
           {"Toolchain": present("tools", TOOLS)},
           {"Reference": ref_nav + err_nav},
           {"Standard library": std_nav},
           {"Examples": ex_nav},
           {"Project": [{"Security model": "security/index.md"}, {"Security review 1": "security/review-1.md"},
                        {"Roadmap": "roadmap.md"}, {"Specifications": "specs.md"}]}]
    nav = [n for n in nav if list(n.values())[0]]
    cfg = yaml.safe_load(open(os.path.join(ROOT, "site", "mkdocs.yml"), encoding="utf-8"))
    cfg["nav"] = nav
    cfg["not_in_nav"] = "/errors/[EWRT]*.md\n"
    cfg["copyright"] = f"{version} · commit {commit}"
    cfg.setdefault("extra", {})["version"] = {"vo": version, "commit": commit}
    open(GEN, "w", encoding="utf-8").write(yaml.safe_dump(cfg, sort_keys=False, allow_unicode=True))
    json.dump({"version": version, "commit": commit}, open(os.path.join(SRC, "version.json"), "w"))
    for name in ("llms.txt", "llms-full.txt"):  # placeholders so links to them validate; --agents writes them
        open(os.path.join(SRC, name), "w").write("")


# ---- agents (contracts/agents.md) ----

def nav_pages(nav):
    for item in nav:
        for title, v in item.items() if isinstance(item, dict) else [(None, item)]:
            if isinstance(v, list):
                yield from nav_pages(v)
            else:
                yield v


def strip_front(text):
    return re.sub(r"\A---\n.*?\n---\n\n?", "", text, flags=re.S)


def address(page):
    return page[:-len("index.md")] if page.endswith("index.md") else page[:-3] + "/"


def agents():
    cfg = yaml.safe_load(open(GEN, encoding="utf-8"))
    info = json.load(open(os.path.join(SRC, "version.json")))
    pages = list(nav_pages(cfg["nav"]))
    extra = sorted(os.path.relpath(p, SRC).replace(os.sep, "/") for p in
                   __import__("glob").glob(os.path.join(SRC, "errors", "*.md")) if not p.endswith("index.md"))
    every = pages + [p for p in extra if p not in pages]
    sections, full = [], [f"# vo documentation ({info['version']}, commit {info['commit']})\n"]
    for item in cfg["nav"]:
        (name, entries), = item.items()
        entries = entries if isinstance(entries, list) else [entries]
        lines = []
        for p in nav_pages(entries):
            text = strip_front(open(os.path.join(SRC, p), encoding="utf-8").read())
            title = (re.search(r"^# (.+)$", text, re.M) or [None, p])[1]
            lines.append(f"- [{title}]({p}): {first_sentence(re.sub(r'^# .+$', '', text, count=1, flags=re.M))}")
            if p == "errors/index.md":
                for e in extra:
                    et = strip_front(open(os.path.join(SRC, e), encoding="utf-8").read())
                    lines.append(f"- [{et.splitlines()[0][2:]}]({e}): {first_sentence(et.split(chr(10), 1)[1])}")
        sections.append(f"## {name}\n\n" + "\n".join(lines) + "\n")
    for p in every:
        text = strip_front(open(os.path.join(SRC, p), encoding="utf-8").read())
        dest = os.path.join(OUT, p)
        os.makedirs(os.path.dirname(dest), exist_ok=True)
        open(dest, "w", encoding="utf-8").write(text)
        full.append(f"<!-- /{address(p)} -->\n{text.rstrip()}\n")
    summary = ("vo is a small, statically typed programming language for AI coding agents and the people who "
               "review their code: checked effects with scopes, machine-readable diagnostics with fixes, "
               f"deterministic runs. Version {info['version']} (commit {info['commit']}). Each link below is the "
               "page as Markdown.")
    open(os.path.join(OUT, "llms.txt"), "w", encoding="utf-8").write(f"# vo\n\n> {summary}\n\n" + "\n".join(sections))
    open(os.path.join(OUT, "llms-full.txt"), "w", encoding="utf-8").write("\n".join(full))
    print(f"site: {len(every)} Markdown pages, llms.txt, llms-full.txt")


if __name__ == "__main__":
    if "--agents" in sys.argv:
        agents()
    else:
        assemble()
        print(f"site: assembled {sum(len(f) for _, _, f in os.walk(SRC))} files in {os.path.relpath(SRC, ROOT)}")
