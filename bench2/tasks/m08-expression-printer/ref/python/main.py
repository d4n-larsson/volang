import sys

from evaluator import EvalErr, evaluate
from lexer import SyntaxErr, tokenize
from exprparser import parse_line
from printer import show


def main():
    env, out = {}, []
    for line in sys.stdin.read().split("\n"):
        if line.strip(" ") == "":
            continue
        try:
            name, e = parse_line(tokenize(line))
        except SyntaxErr:
            out.append("error: syntax")
            continue
        text = (f"{name} = " if name else "") + show(e)
        try:
            v = evaluate(e, env)
            if name:
                env[name] = v
            out.append(f"{text} => {v}")
        except EvalErr as err:
            out.append(f"{text} => error: {err}")
    sys.stdout.write("".join(l + "\n" for l in out))


main()
