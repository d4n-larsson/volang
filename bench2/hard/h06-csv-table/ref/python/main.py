import re
import sys


class Err(Exception):
    pass


def parse(s):
    records, rec = [], []
    i, line, col0 = 0, 1, 0          # col0: index of the first character of the current line
    n = len(s)

    def pos(k):
        return f"line {line}, column {k - col0 + 1}"

    def end_record():
        nonlocal rec
        records.append(rec)
        if len(rec) != len(records[0]):
            raise Err(f"error: record {len(records)} has {len(rec)} fields, expected {len(records[0])}")
        rec = []

    if n == 0:
        return records
    while True:
        # start of a field at i
        if i < n and s[i] == '"':
            oline, ocol = line, i - col0 + 1
            buf = []
            i += 1
            while True:
                if i == n:
                    raise Err(f"error: line {oline}, column {ocol}: unclosed quote")
                c = s[i]
                if c == '"':
                    if i + 1 < n and s[i + 1] == '"':
                        buf.append('"')
                        i += 2
                        continue
                    i += 1
                    break
                if c == "\n":
                    line += 1
                    col0 = i + 1
                buf.append(c)
                i += 1
            if i < n and s[i] not in ",\n":
                raise Err(f"error: {pos(i)}: character after closing quote")
            rec.append("".join(buf))
        else:
            j = i
            while j < n and s[j] not in ',\n':
                if s[j] == '"':
                    raise Err(f"error: {pos(j)}: quote in unquoted field")
                j += 1
            rec.append(s[i:j])
            i = j
        # i is at ',', '\n' or the end
        if i == n:
            end_record()
            return records
        if s[i] == ",":
            i += 1
            continue
        i += 1
        line += 1
        col0 = i
        end_record()
        if i == n:
            return records


def main():
    s = sys.stdin.buffer.read().decode("utf-8")
    try:
        records = parse(s)
    except Err as e:
        print(e)
        return
    if not records:
        return
    shown = [[c.replace("\\", "\\\\").replace("\n", "\\n") for c in r] for r in records]
    m = len(records[0])
    widths = [max(len(r[k]) for r in shown) for k in range(m)]
    numeric = [len(records) > 1 and all(re.fullmatch(r"-?[0-9]+", r[k]) for r in records[1:]) for k in range(m)]

    def fmt(r):
        cells = []
        for k, c in enumerate(r):
            if numeric[k]:
                cells.append(c.rjust(widths[k]))
            elif k == m - 1:
                cells.append(c)
            else:
                cells.append(c.ljust(widths[k]))
        return " | ".join(cells)

    out = [fmt(shown[0]), "-+-".join("-" * w for w in widths)] + [fmt(r) for r in shown[1:]]
    sys.stdout.write("".join(l + "\n" for l in out))


main()
