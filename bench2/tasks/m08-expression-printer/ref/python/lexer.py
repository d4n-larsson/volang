class SyntaxErr(Exception):
    pass


def tokenize(line):
    toks, i = [], 0
    while i < len(line):
        c = line[i]
        if c == " ":
            i += 1
        elif c in "0123456789" or "a" <= c <= "z":
            j = i
            kind = "num" if c in "0123456789" else "name"
            while j < len(line) and ((kind == "num" and line[j] in "0123456789") or (kind == "name" and "a" <= line[j] <= "z")):
                j += 1
            toks.append((kind, line[i:j]))
            i = j
        elif c in "+-*/%^()=":
            toks.append(("op", c))
            i += 1
        else:
            raise SyntaxErr()
    return toks
