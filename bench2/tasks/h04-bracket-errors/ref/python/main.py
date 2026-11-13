import sys

CLOSE = {"(": ")", "[": "]", "{": "}"}


def check(s):
    stack, quote = [], 0
    for i, c in enumerate(s, 1):
        if quote:
            if c == '"':
                quote = 0
            continue
        if c == '"':
            quote = i
        elif c in CLOSE:
            stack.append((c, i))
        elif c in ")]}":
            if not stack:
                return f"error at column {i}: unexpected {c}"
            o, _ = stack.pop()
            if CLOSE[o] != c:
                return f"error at column {i}: expected {CLOSE[o]} but found {c}"
    if quote:
        return f'error at column {quote}: unclosed "'
    if stack:
        return f"error at column {stack[0][1]}: unclosed {stack[0][0]}"
    return "ok"


text = sys.stdin.read()
lines = text.split("\n")
if text.endswith("\n"):
    lines.pop()
for line in lines if text else []:
    print(check(line))
