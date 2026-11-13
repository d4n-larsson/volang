import re
import sys


def get(key):
    try:
        with open("store/" + key) as f:
            return f.read()
    except OSError:
        return None


def put(key, value):
    with open("store/" + key, "w") as f:
        f.write(value)


for line in sys.stdin.read().split("\n"):
    if not line:
        continue
    cmd, key, *rest = line.split(" ", 2)
    arg = rest[0] if rest else ""
    if cmd == "set":
        put(key, arg)
        print("ok")
    elif cmd == "get":
        v = get(key)
        print("none" if v is None else v)
    elif cmd == "add":
        v = get(key)
        v = "0" if v is None else v
        if re.fullmatch(r"-?[0-9]+", v):
            n = int(v) + int(arg)
            put(key, str(n))
            print(n)
        else:
            print("error")
    elif cmd == "copy":
        v = get(key)
        if v is None:
            print("none")
        else:
            put(arg, v)
            print("ok")
