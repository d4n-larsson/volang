import sys

import csvline
from join import join
from layout import render


def table(lines):
    """(header, rows, skipped)."""
    lines = [l for l in lines if l != ""]
    header = csvline.parse(lines[0])
    rows, skipped = [], 0
    for l in lines[1:]:
        f = csvline.parse(l)
        if f is None or len(f) != len(header):
            skipped += 1
        else:
            rows.append(f)
    return header, rows, skipped


def main():
    lines = sys.stdin.read().split("\n")
    if lines[-1] == "":
        lines.pop()
    kind, key = lines[0].split(" ", 1)
    sep = lines.index("---")
    ha, ra, sa = table(lines[1:sep])
    hb, rb, sb = table(lines[sep + 1:])
    if key not in ha:
        out = [f"error: no column {key} in A"]
    elif key not in hb:
        out = [f"error: no column {key} in B"]
    else:
        header, rows = join(kind, key, ha, ra, hb, rb)
        out = render(header, rows) + [f"rows: {len(rows)}, skipped: {sa + sb}"]
    sys.stdout.write("".join(l + "\n" for l in out))


main()
