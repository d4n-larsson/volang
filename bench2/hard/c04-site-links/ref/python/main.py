import os
import sys


def name_ok(s):
    return 1 <= len(s) <= 30 and all("a" <= c <= "z" or "0" <= c <= "9" or c == "-" for c in s)


def slug(text):
    out = []
    for c in text:
        if "A" <= c <= "Z":
            out.append(c.lower())
        elif "a" <= c <= "z" or "0" <= c <= "9" or c == "-":
            out.append(c)
        elif c == " ":
            out.append("-")
    return "".join(out)


def scan(text):
    """(anchors, [(line number, target)]) of a page."""
    anchors, links, seen, code = set(), [], {}, False
    for i, line in enumerate(text.split("\n")):
        if line.startswith("```"):
            code = not code
            continue
        if code:
            continue
        hashes = len(line) - len(line.lstrip("#"))
        if 1 <= hashes <= 3 and line[hashes:hashes + 1] == " ":
            s = slug(line[hashes + 1:].strip(" "))
            if s:
                k = seen.get(s, 0)
                anchors.add(s if k == 0 else f"{s}-{k}")
                seen[s] = k + 1
        pos = 0
        while True:
            j = line.find("](", pos)
            if j < 0:
                break
            k = line.find(")", j + 2)
            target = line[j + 2:k] if k >= 0 else ""
            if k < 0 or target == "" or " " in target:
                pos = j + 2
                continue
            links.append((i + 1, target))
            pos = k
    return anchors, links


def resolve(page, path):
    parts = [] if path.startswith("/") else page.split("/")[:-1]
    for p in path.split("/"):
        if p in ("", "."):
            continue
        if p == "..":
            if not parts:
                return None
            parts.pop()
        else:
            parts.append(p)
    return "/".join(parts)


def report(site):
    root = "pages/" + site
    files = set()
    for d, _, names in os.walk(root):
        for n in names:
            files.add(os.path.relpath(os.path.join(d, n), root))
    pages = sorted(f for f in files if f.endswith(".md"))
    info = {}
    for p in pages:
        with open(root + "/" + p) as f:
            info[p] = scan(f.read())
    problems, edges = [], {p: [] for p in pages}
    internal = external = 0
    for p in pages:
        anchors, links = info[p]
        for no, t in links:
            if t.startswith("http://") or t.startswith("https://"):
                external += 1
                continue
            if t.startswith("mailto:"):
                continue
            internal += 1
            if t.startswith("#"):
                if t[1:] and t[1:] not in anchors:
                    problems.append(f"{p}:{no}: missing anchor {t}")
                continue
            path, _, anchor = t.partition("#")
            target = resolve(p, path)
            if target is None:
                problems.append(f"{p}:{no}: outside {t}")
            elif target not in files:
                problems.append(f"{p}:{no}: missing file {target}")
            elif target in info:
                edges[p].append(target)
                if anchor and anchor not in info[target][0]:
                    problems.append(f"{p}:{no}: missing anchor {target}#{anchor}")
    reached = set()
    todo = ["index.md"] if "index.md" in info else []
    while todo:
        p = todo.pop()
        if p not in reached:
            reached.add(p)
            todo.extend(edges[p])
    orphans = [p for p in pages if p not in reached]
    for line in problems:
        print(line)
    for p in orphans:
        print("orphan " + p)
    print(f"pages {len(pages)} links {internal} external {external} problems {len(problems)} orphans {len(orphans)}")


def main():
    for site in sys.stdin.read().split("\n"):
        if site == "":
            continue
        print("== " + site)
        if not name_ok(site):
            print("error: bad site name")
        elif not os.path.isdir("pages/" + site):
            print("error: no such site")
        else:
            report(site)


main()
