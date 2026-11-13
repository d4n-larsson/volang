from tokens import LIMIT


class VmError(Exception):
    pass


BUILTINS = {"+": 2, "-": 2, "*": 2, "/": 2, "%": 2, "neg": 1, "abs": 1, "dup": 1, "drop": 1,
            "swap": 2, "over": 2, "rot": 3, "clear": 0, ".": 1, ".s": 0}
NAMED = {"neg", "abs", "dup", "drop", "swap", "over", "rot", "clear"}


def checked(v):
    if not -LIMIT <= v <= LIMIT:
        raise VmError("overflow")
    return v


def show(stack):
    return " ".join([f"<{len(stack)}>"] + [str(v) for v in stack])


def run_builtin(stack, w, out):
    if len(stack) < BUILTINS[w]:
        raise VmError("stack underflow")
    if w in ("+", "-", "*", "/", "%"):
        b = stack.pop()
        a = stack.pop()
        if w in ("/", "%") and b == 0:
            raise VmError("division by zero")
        if w == "+":
            r = a + b
        elif w == "-":
            r = a - b
        elif w == "*":
            r = a * b
        else:
            q = abs(a) // abs(b)
            if (a < 0) != (b < 0):
                q = -q
            r = q if w == "/" else a - b * q
        stack.append(checked(r))
    elif w == "neg":
        stack.append(-stack.pop())
    elif w == "abs":
        stack.append(abs(stack.pop()))
    elif w == "dup":
        stack.append(stack[-1])
    elif w == "drop":
        stack.pop()
    elif w == "swap":
        stack[-1], stack[-2] = stack[-2], stack[-1]
    elif w == "over":
        stack.append(stack[-2])
    elif w == "rot":
        stack.append(stack.pop(-3))
    elif w == "clear":
        stack.clear()
    elif w == ".":
        out.append(str(stack.pop()))
    elif w == ".s":
        out.append(show(stack))
