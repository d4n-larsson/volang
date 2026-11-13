def render(header, rows):
    widths = [max(len(r[c]) for r in [header] + rows) for c in range(len(header))]

    def line(cells):
        return " | ".join(c.ljust(w) for c, w in zip(cells, widths)).rstrip(" ")

    return [line(header), "-+-".join("-" * w for w in widths)] + [line(r) for r in rows]
