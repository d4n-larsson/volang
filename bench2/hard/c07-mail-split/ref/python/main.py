import os
import sys

LOWER = "abcdefghijklmnopqrstuvwxyz"
DIGITS = "0123456789"


def lines_of(text):
    ls = text.split("\n")
    if ls and ls[-1] == "":
        ls.pop()
    return ls


def ascii_lower(s):
    return "".join(chr(ord(c) + 32) if "A" <= c <= "Z" else c for c in s)


def valid_spool(s):
    return 1 <= len(s) <= 20 and all(c in LOWER or c in DIGITS or c == "-" for c in s)


def valid_user(u):
    return 1 <= len(u) <= 32 and not u.startswith(".") and all(c in LOWER or c in DIGITS or c in "._-" for c in u)


def messages(text):
    """lists of lines, one per message"""
    out = []
    for line in lines_of(text):
        if line.startswith("From "):
            out.append([line])
        elif out:
            out[-1].append(line)
    return out


def headers(msg):
    """[(lowercase name, value)] in order"""
    hs = []
    current = None                      # index into hs, or None after an ignored line
    for line in msg[1:]:
        if line == "":
            break
        if line[0] in " \t":
            if current is not None:
                name, value = hs[current]
                hs[current] = (name, value + " " + line.lstrip(" \t"))
            continue
        if ":" not in line:
            current = None
            continue
        name, value = line.split(":", 1)
        hs.append((ascii_lower(name), value))
        current = len(hs) - 1
    return [(n, v.strip(" \t")) for n, v in hs]


def first(hs, name):
    for n, v in hs:
        if n == name:
            return v
    return ""


def addresses(hs):
    out = []
    for n, v in hs:
        if n not in ("to", "cc"):
            continue
        for item in v.split(","):
            item = item.strip(" \t")
            if item == "":
                continue
            if "<" in item:
                rest = item[item.index("<") + 1:]
                if ">" in rest:
                    rest = rest[:rest.index(">")]
                item = rest
            out.append(item)
    return out


def main():
    lines = sys.stdin.read().split("\n")
    domain = lines[0].split(" ", 1)[1]
    quota = int(lines[1].split(" ", 1)[1])
    boxes = {}                          # user -> [texts]
    sizes = {}
    seen_ids = set()
    counts = {"deliver": 0, "bounce": 0, "relay": 0, "dup": 0}
    for s in lines[2:]:
        if s == "":
            continue
        if not valid_spool(s):
            print(f"skip {s}: bad spool name")
            continue
        path = f"spool/{s}.mbox"
        if not os.path.isfile(path):
            print(f"skip {s}: no such spool")
            continue
        with open(path) as f:
            text = f.read()
        for number, msg in enumerate(messages(text), 1):
            hs = headers(msg)
            mid = first(hs, "message-id") or f"{s}#{number}"
            if mid in seen_ids:
                print(f"dup {mid}")
                counts["dup"] += 1
                continue
            seen_ids.add(mid)
            print(f"message {mid}: {first(hs, 'subject') or '(no subject)'}")
            body = "".join(l + "\n" for l in msg)
            size = len(body.encode())
            handled = set()
            for addr in addresses(hs):
                key = ascii_lower(addr)
                if key in handled:
                    continue
                handled.add(key)
                if addr.count("@") != 1:
                    print(f"bounce {mid} {addr}: bad address")
                    counts["bounce"] += 1
                    continue
                local, dom = addr.split("@")
                if ascii_lower(dom) != domain:
                    print(f"relay {mid} {addr}")
                    counts["relay"] += 1
                    continue
                user = ascii_lower(local)
                if not valid_user(user):
                    print(f"bounce {mid} {addr}: invalid user")
                    counts["bounce"] += 1
                    continue
                if sizes.get(user, 0) + size > quota:
                    print(f"bounce {mid} {addr}: over quota")
                    counts["bounce"] += 1
                    continue
                print(f"deliver {mid} {user}")
                counts["deliver"] += 1
                boxes.setdefault(user, []).append(body)
                sizes[user] = sizes.get(user, 0) + size
    print("== mailboxes")
    for user in sorted(boxes):
        content = "".join(boxes[user])
        with open(f"out/{user}.mbox", "w") as f:
            f.write(content)
        print(f"wrote out/{user}.mbox {len(boxes[user])} {len(content.encode())}")
    print(f"total {counts['deliver']} delivered, {counts['bounce']} bounced, "
          f"{counts['relay']} relayed, {counts['dup']} duplicates")


main()
