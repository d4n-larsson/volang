import sys
lines = sys.stdin.read().split("\n")
k = int(lines[0])
for line in lines[1:-1]:
    out = []
    for c in line:
        if "a" <= c <= "z":
            c = chr((ord(c) - 97 + k) % 26 + 97)
        elif "A" <= c <= "Z":
            c = chr((ord(c) - 65 + k) % 26 + 65)
        out.append(c)
    print("".join(out))
