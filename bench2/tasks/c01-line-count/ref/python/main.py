import sys

name = sys.stdin.readline().rstrip("\n")
try:
    with open("data/" + name) as f:
        text = f.read()
except OSError:
    print("missing")
else:
    print(len(text.splitlines()))
