import os
import sys

LOWER = "abcdefghijklmnopqrstuvwxyz"
DIGITS = "0123456789"


def resolve(start_parts, path):
    """The normalized parts, or None when the path is outside."""
    if path.startswith("/"):
        return None
    parts = list(start_parts)
    for p in path.split("/"):
        if p == "" or p == ".":
            continue
        if p == "..":
            if not parts:
                return None
            parts.pop()
        else:
            parts.append(p)
    return parts


def chars_ok(s, extra):
    return s != "" and all(c in LOWER or c in DIGITS or c in extra for c in s)


class Root:
    def __init__(self):
        self.values = {}     # full key -> (value, file, line)
        self.stack = []
        self.files = 0
        self.errors = 0

    def error(self, name, no, msg):
        print(f"{name}:{no}: {msg}")
        self.errors += 1

    def expand(self, name, no, value):
        out, i = [], 0
        while True:
            j = value.find("${", i)
            if j < 0:
                out.append(value[i:])
                break
            k = value.find("}", j + 2)
            if k < 0:
                out.append(value[i:])
                break
            out.append(value[i:j])
            key = value[j + 2:k]
            if key in self.values:
                out.append(self.values[key][0])
            else:
                self.error(name, no, "undefined ${" + key + "}")
            i = k + 1
        return "".join(out)

    def read(self, parts):
        name = "/".join(parts)
        with open("conf/" + name) as f:
            text = f.read()
        self.files += 1
        self.stack.append(name)
        section = ""
        for i, raw in enumerate(text.split("\n")):
            no = i + 1
            line = raw.strip(" ")
            if line == "" or line.startswith("#"):
                continue
            if line.startswith("include? ") or line.startswith("include "):
                optional = line.startswith("include? ")
                path = line[len("include? " if optional else "include "):].strip(" ")
                target = resolve(parts[:-1], path)
                if target is None:
                    self.error(name, no, "include outside conf: " + path)
                    continue
                tname = "/".join(target)
                if tname in self.stack:
                    self.error(name, no, "include cycle: " + " -> ".join(self.stack + [tname]))
                    continue
                if not os.path.isfile("conf/" + tname):
                    if not optional:
                        self.error(name, no, "missing include: " + tname)
                    continue
                self.read(target)
                continue
            if line.startswith("[") and line.endswith("]"):
                s = line[1:-1]
                if s == "" or chars_ok(s, "_"):
                    section = s
                else:
                    self.error(name, no, "bad section")
                continue
            if "=" in line:
                key, value = line.split("=", 1)
                key, value = key.strip(" "), value.strip(" ")
                if not chars_ok(key, "_."):
                    self.error(name, no, "bad key")
                    continue
                full = section + "." + key if section else key
                self.values[full] = (self.expand(name, no, value), name, no)
                continue
            self.error(name, no, "syntax error")
        self.stack.pop()


def main():
    for root in sys.stdin.read().split("\n"):
        if root == "":
            continue
        print("== " + root)
        parts = resolve([], root)
        if parts is None:
            print("error: outside conf")
            continue
        if not os.path.isfile("conf/" + "/".join(parts)):
            print("error: not found")
            continue
        r = Root()
        r.read(parts)
        for k in sorted(r.values):
            v, f, no = r.values[k]
            print(f"{k}={v} ({f}:{no})")
        print(f"files {r.files} errors {r.errors}")


main()
