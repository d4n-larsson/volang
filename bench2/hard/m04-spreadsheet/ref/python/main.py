import sys

import formula
import render
import sheet as sheetmod


def cell_name(s):
    r = formula.parse_ref(s)
    if r is None or r[0] or r[2]:
        return None
    return (r[1], r[3])


def run(line, sh, out):
    fields = [f for f in line.split(" ") if f]
    cmd, n = fields[0], len(fields)
    if cmd == "set" and n >= 3:
        cell = cell_name(fields[1])
        if cell is None:
            return False
        rest = line[len(cmd):].lstrip(" ")[len(fields[1]):].strip(" ")
        sh.put(cell, sheetmod.content_of(rest))
        return True
    if cmd == "copy" and n == 3:
        a, b = cell_name(fields[1]), cell_name(fields[2])
        if a is None or b is None:
            return False
        c = sh.cells.get(a)
        if c is not None and c[0] == "formula" and c[2] is not None:
            text = "=" + formula.shift(c[1][1:], b[0] - a[0], b[1] - a[1])
            c = ("formula", text, formula.parse(text[1:]))
        sh.put(b, c)
        return True
    if n != 2:
        return False
    if cmd == "show":
        ends = fields[1].split(":")
        if len(ends) > 2:
            return False
        cs = [cell_name(x) for x in ends]
        if None in cs:
            return False
        a, b = cs[0], cs[-1]
        out.extend(render.grid(sh, min(a[0], b[0]), min(a[1], b[1]), max(a[0], b[0]), max(a[1], b[1])))
        return True
    cell = cell_name(fields[1])
    if cell is None:
        return False
    if cmd == "clear":
        sh.put(cell, None)
    elif cmd == "print":
        out.append(render.print_line(cell, sh.value(cell)))
    elif cmd == "formula":
        out.append(render.formula_line(cell, sh.cells.get(cell)))
    elif cmd == "deps":
        out.append(render.deps_line(cell, sh.dependents(cell)))
    else:
        return False
    return True


def main():
    sh = sheetmod.Sheet()
    out = []
    for no, raw in enumerate(sys.stdin.read().split("\n"), 1):
        line = raw.strip(" ")
        if line and not run(line, sh, out):
            out.append(f"line {no}: invalid command")
    sys.stdout.write("".join(s + "\n" for s in out))


main()
