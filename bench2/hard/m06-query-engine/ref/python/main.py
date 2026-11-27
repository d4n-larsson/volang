import sys

import engine
import render
import syntax


def run(db, line):
    try:
        s = syntax.parse(line)
    except syntax.SyntaxErr:
        return ["error: syntax error"]
    try:
        if s.kind == "select":
            return render.render(*engine.run_select(db, s))
        return [getattr(engine, "run_" + s.kind)(db, s)]
    except engine.SemErr as e:
        return [f"error: {e}"]


def main():
    db, out = {}, []
    for line in sys.stdin.read().split("\n"):
        if line.strip(" "):
            out += run(db, line)
    sys.stdout.write("".join(s + "\n" for s in out))


main()
