import sys

from interp import Interpreter, RunError
from parser import ParseError, parse_program


def main():
    out = []
    try:
        top, funcs = parse_program(sys.stdin.read())
    except ParseError as e:
        sys.stdout.write(f"parse error at line {e.line}: {e.msg}\n")
        return
    try:
        Interpreter(funcs, out).run(top)
    except RunError as e:
        out.append(f"error at line {e.line}: {e.msg}")
    sys.stdout.write("".join(l + "\n" for l in out))


main()
