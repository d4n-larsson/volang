"""Splits one line into tokens."""

KEYWORDS = {"let", "set", "print", "if", "elif", "else", "while", "end", "func", "return",
            "break", "continue", "and", "or", "not", "true", "false"}
OPS2 = ("==", "!=", "<=", ">=")
OPS1 = "<>=+-*/%(),"


class SyntaxErr(Exception):
    pass


def is_word_char(c):
    return "a" <= c <= "z" or "0" <= c <= "9" or c == "_"


def tokenize(line):
    """A list of (kind, text) with kind in int, str, kw, name, op."""
    toks, i, n = [], 0, len(line)
    while i < n:
        c = line[i]
        if c == " ":
            i += 1
        elif c == "#":
            break
        elif "0" <= c <= "9":
            j = i
            while j < n and "0" <= line[j] <= "9":
                j += 1
            if j - i > 18:
                raise SyntaxErr()
            toks.append(("int", line[i:j]))
            i = j
        elif "a" <= c <= "z" or c == "_":
            j = i
            while j < n and is_word_char(line[j]):
                j += 1
            w = line[i:j]
            toks.append(("kw" if w in KEYWORDS else "name", w))
            i = j
        elif c == '"':
            j = line.find('"', i + 1)
            if j < 0:
                raise SyntaxErr()
            toks.append(("str", line[i + 1:j]))
            i = j + 1
        elif line[i:i + 2] in OPS2:
            toks.append(("op", line[i:i + 2]))
            i += 2
        elif c in OPS1:
            toks.append(("op", c))
            i += 1
        else:
            raise SyntaxErr()
    return toks
