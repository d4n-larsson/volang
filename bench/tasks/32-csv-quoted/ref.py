import sys
for line in sys.stdin:
    s = line.rstrip("\n")
    fields, cur, i, q = [], "", 0, False
    while i < len(s):
        c = s[i]
        if q:
            if c == '"' and i + 1 < len(s) and s[i + 1] == '"':
                cur += '"'
                i += 1
            elif c == '"':
                q = False
            else:
                cur += c
        elif c == '"':
            q = True
        elif c == ",":
            fields.append(cur)
            cur = ""
        else:
            cur += c
        i += 1
    fields.append(cur)
    print("".join("[" + f + "]" for f in fields))
