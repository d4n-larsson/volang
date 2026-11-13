import sys

import tokens
from dictionary import Dictionary
from machine import VmError, run_builtin, show


def run_line(line, stack, words, out):
    toks = tokens.tokens(line)
    i = 0
    while i < len(toks):
        t = toks[i]
        if t == ":":
            if i + 1 >= len(toks) or ";" not in toks[i + 2:]:
                raise VmError("unfinished definition")
            end = toks.index(";", i + 2)
            words.define(toks[i + 1], toks[i + 2:end])
            i = end + 1
            continue
        for item in words.expand(t):
            if isinstance(item, int):
                stack.append(item)
            else:
                run_builtin(stack, item, out)
        i += 1


def main():
    stack, words, out = [], Dictionary(), []
    for line in sys.stdin.read().splitlines():
        try:
            run_line(line, stack, words, out)
        except VmError as e:
            out.append(f"error: {e}")
            stack.clear()
    out.append(show(stack))
    sys.stdout.write("".join(l + "\n" for l in out))


main()
