import sys

WS = " \t"


def is_start(c):
    return c.isascii() and (c.isalpha() or c == "_")


def is_name(c):
    return c.isascii() and (c.isalnum() or c == "_")


class Err(Exception):
    def __init__(self, col, msg):
        self.col, self.msg = col, msg


def variable(s, i, env):
    """s[i] is '$'. Returns (value or None for a literal '$', next index)."""
    if i + 1 < len(s) and s[i + 1] == "{":
        j = i + 2
        if j < len(s) and is_start(s[j]):
            while j < len(s) and is_name(s[j]):
                j += 1
            if j < len(s) and s[j] == "}":
                return env.get(s[i + 2:j], ""), j + 1
        raise Err(i + 1, "bad substitution")
    if i + 1 < len(s) and is_start(s[i + 1]):
        j = i + 1
        while j < len(s) and is_name(s[j]):
            j += 1
        return env.get(s[i + 1:j], ""), j
    return None, i + 1


def split_line(s, env):
    words, cur, started = [], [], False

    def end():
        nonlocal cur, started
        if started:
            words.append("".join(cur))
        cur, started = [], False

    i = 0
    sep = True                      # at the start, or right after an unquoted space or tab
    while i < len(s):
        c = s[i]
        was_sep, sep = sep, False
        if c in WS:
            end()
            sep = True
            i += 1
        elif c == "#" and was_sep:
            break
        elif c == "\\":
            if i + 1 == len(s):
                raise Err(i + 1, "dangling backslash")
            cur.append(s[i + 1])
            started = True
            i += 2
        elif c == "'":
            j = s.find("'", i + 1)
            if j < 0:
                raise Err(i + 1, "unclosed '")
            cur.append(s[i + 1:j])
            started = True
            i = j + 1
        elif c == '"':
            open_col = i + 1
            started = True
            i += 1
            while True:
                if i == len(s):
                    raise Err(open_col, 'unclosed "')
                c = s[i]
                if c == '"':
                    i += 1
                    break
                if c == "\\" and i + 1 < len(s) and s[i + 1] in '"\\$':
                    cur.append(s[i + 1])
                    i += 2
                elif c == "$":
                    v, i = variable(s, i, env)
                    cur.append("$" if v is None else v)
                else:
                    cur.append(c)
                    i += 1
        elif c == "$":
            v, i = variable(s, i, env)
            if v is None:
                cur.append("$")
                started = True
                continue
            k = 0
            while k < len(v):
                if v[k] in WS:
                    end()
                    while k < len(v) and v[k] in WS:
                        k += 1
                else:
                    cur.append(v[k])
                    started = True
                    k += 1
        else:
            cur.append(c)
            started = True
            i += 1
    end()
    return words


def main():
    data = sys.stdin.buffer.read().decode("utf-8")
    lines = data.split("\n")
    if data.endswith("\n"):
        lines.pop()
    env, out = {}, []
    k = 0
    while lines[k] != "%%":
        name, _, value = lines[k].partition("=")
        env[name] = value
        k += 1
    for line in lines[k + 1:]:
        try:
            ws = split_line(line, env)
            out.append(" ".join([str(len(ws))] + ["[" + w + "]" for w in ws]))
        except Err as e:
            out.append(f"error at column {e.col}: {e.msg}")
    sys.stdout.write("".join(l + "\n" for l in out))


main()
