"""Commands that look at the text without changing it."""
import view


def find_text(buf, text):
    hits = [n for n in range(1, buf.size() + 1) if text in buf.line(n)]
    if len(hits) == 0:
        return ["not found"]
    buf.cur = hits[0]
    return ["found at " + ", ".join(str(n) for n in hits)]


def count(buf):
    n_words = 0
    n_bytes = 0
    for line in buf.lines:
        n_words += len([w for w in line.split(" ") if w != ""])
        n_bytes += len(line) + 1
    return [view.plural(buf.size(), "line") + ", " + view.plural(n_words, "word") + ", "
            + view.plural(n_bytes, "byte")]
