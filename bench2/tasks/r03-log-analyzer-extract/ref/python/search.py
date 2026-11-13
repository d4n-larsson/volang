from logline import entries


def grep(lines, word):
    out = [e.text for e in entries(lines) if word in e.message]
    return out + [str(len(out)) + " matches"]
