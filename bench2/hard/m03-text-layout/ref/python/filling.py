"""Line filling: paragraphs into aligned lines, list markers and headings."""


def fill(words, a):
    """Lines (each a list of words) of at most `a` characters."""
    lines, cur, length = [], [], 0
    for w in words:
        if cur and length + 1 + len(w) <= a:
            cur.append(w)
            length += 1 + len(w)
        elif not cur and len(w) <= a:
            cur, length = [w], len(w)
        elif len(w) <= a:
            lines.append(cur)
            cur, length = [w], len(w)
        else:
            if cur:
                lines.append(cur)
            while len(w) > a:
                lines.append([w[:a]])
                w = w[a:]
            cur, length = [w], len(w)
    if cur:
        lines.append(cur)
    return lines


def align(line, a, how, last):
    text = " ".join(line)
    gap = a - len(text)
    if how == "right":
        return " " * gap + text
    if how == "center":
        return " " * (gap // 2) + text
    if how == "justify" and not last and len(line) >= 2:
        g = len(line) - 1
        out = line[0]
        for i, w in enumerate(line[1:]):
            out += " " * (1 + gap // g + (1 if i < gap % g else 0)) + w
        return out
    return text


def paragraph(words, st):
    """The output lines of a paragraph under the current settings and lists."""
    if st.lists:
        lst = st.lists[-1]
        lst.count += 1
        if lst.kind == "number":
            marker = str(lst.count) + "."
        else:
            marker = "*-+"[lst.depth - 1]
        m = st.indent + 2 * (lst.depth - 1)
        t = m + len(marker) + 1
        first = " " * m + marker + " "
    else:
        t = st.indent
        first = " " * t
    a = st.width - t
    lines = fill(words, a)
    out = []
    for i, line in enumerate(lines):
        prefix = first if i == 0 else " " * t
        out.append((prefix + align(line, a, st.align, i == len(lines) - 1)).rstrip(" "))
    return out


def heading(text, st):
    text = text[:st.width - st.indent].rstrip(" ")
    pad = " " * st.indent
    return [pad + text, pad + "=" * len(text)]
