LEVEL_NAMES = ("DEBUG", "INFO", "WARN", "ERROR")


def numeric(s):
    return len(s) > 0 and all("0" <= c <= "9" for c in s)


def leap_year(y):
    return y % 4 == 0 and (y % 100 != 0 or y % 400 == 0)


def check_date(word):
    if len(word) != 10 or word[4] != "-" or word[7] != "-":
        return False
    if not numeric(word[0:4]) or not numeric(word[5:7]) or not numeric(word[8:10]):
        return False
    y = int(word[0:4])
    m = int(word[5:7])
    d = int(word[8:10])
    if y < 1 or m < 1 or m > 12 or d < 1:
        return False
    lengths = [31, 29 if leap_year(y) else 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31]
    return d <= lengths[m - 1]


def check_time(word):
    if len(word) != 8 or word[2] != ":" or word[5] != ":":
        return False
    if not numeric(word[0:2]) or not numeric(word[3:5]) or not numeric(word[6:8]):
        return False
    return int(word[0:2]) < 24 and int(word[3:5]) < 60 and int(word[6:8]) < 60


def split_entry(line):
    """The words of a valid log line, or None."""
    words = [w for w in line.split(" ") if w != ""]
    if len(words) < 4 or not check_date(words[0]) or not check_time(words[1]):
        return None
    if words[2] not in LEVEL_NAMES or len(words[3]) < 2 or not words[3].endswith(":"):
        return None
    return words


def grep(lines, word):
    out = []
    for line in lines:
        words = split_entry(line)
        if words is not None and word in " ".join(words[4:]):
            out.append(" ".join(words))
    return out + [str(len(out)) + " matches"]
