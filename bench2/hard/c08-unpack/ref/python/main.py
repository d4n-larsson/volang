import os
import sys

NAME_CHARS = set("abcdefghijklmnopqrstuvwxyz0123456789-")
DIGITS = set("0123456789")


def is_digits(s):
    return s != "" and all(c in DIGITS for c in s)


def normalize(path):
    """The kept parts, or None when the path leaves the destination."""
    if path.startswith("/"):
        return None
    kept = []
    for part in path.split("/"):
        if part in ("", "."):
            continue
        if part == "..":
            if not kept:
                return None
            kept.pop()
        else:
            kept.append(part)
    return kept


def checksum(data):
    h = 7
    for b in data:
        h = (h * 31 + b) % 1000000007
    return h


def unpack(name, text):
    lines = text.split("\n")
    if text.endswith("\n"):
        lines.pop()
    files, dirs = {}, set()      # normalized path -> content; directories of the tree
    dest = "out/" + name + "/"
    entries, i = 0, 1
    while True:
        if i >= len(lines):
            print("error: truncated")
            break
        line, k = lines[i], i + 1
        f = line.split(" ")
        kind = f[0]
        ok = (kind == "F" and len(f) == 3 and is_digits(f[2])) or (kind == "D" and len(f) == 2) \
            or (kind == "L" and len(f) == 4 and f[2] == "->") or (kind == "END" and len(f) == 2 and is_digits(f[1]))
        if not ok:
            print(f"error: line {k}: bad entry")
            break
        if kind == "END":
            if int(f[1]) != entries:
                print(f"warning: END says {f[1]}, found {entries}")
            break
        entries += 1
        path = f[1]
        content = None
        if kind == "F":
            n = int(f[2])
            if i + 1 + n > len(lines):
                print("error: truncated")
                break
            content = "".join(l + "\n" for l in lines[i + 1:i + 1 + n])
            i += n
        i += 1
        if kind == "L":
            print(f"skip {path}: link")
            continue
        parts = normalize(path)
        if parts is None:
            print(f"skip {path}: outside")
            continue
        if not parts:
            print(f"skip {path}: empty")
            continue
        norm = "/".join(parts)
        prefixes = ["/".join(parts[:j]) for j in range(1, len(parts))]
        if any(p in files for p in prefixes):
            print(f"skip {path}: parent is a file")
            continue
        if kind == "F":
            if norm in dirs:
                print(f"skip {path}: is a directory")
                continue
            data = content.encode()
            verb = "replace" if norm in files else "file"
            os.makedirs(os.path.dirname(dest + norm), exist_ok=True)
            with open(dest + norm, "wb") as out:
                out.write(data)
            files[norm] = data
            dirs.update(prefixes)
            print(f"{verb} {norm} {len(data)}")
        else:
            if norm in files:
                print(f"skip {path}: is a file")
                continue
            os.makedirs(dest + norm, exist_ok=True)
            dirs.update(prefixes)
            dirs.add(norm)
            print(f"dir {norm}")
    for p in sorted(files):
        print(f"= {p} {len(files[p])} {checksum(files[p])}")
    print(f"total {len(files)} files, {len(dirs)} dirs, {sum(len(d) for d in files.values())} bytes")


def main():
    seen = set()
    for name in sys.stdin.read().split("\n"):
        if name == "":
            continue
        print("== " + name)
        before = name in seen
        seen.add(name)
        if not (1 <= len(name) <= 20 and all(c in NAME_CHARS for c in name)):
            print("bad name")
        elif before:
            print("duplicate")
        elif not os.path.isfile(f"incoming/{name}.pack"):
            print("not found")
        else:
            with open(f"incoming/{name}.pack") as f:
                text = f.read()
            if text.split("\n")[0] != "PACK 1":
                print("not a pack")
            else:
                unpack(name, text)


main()
