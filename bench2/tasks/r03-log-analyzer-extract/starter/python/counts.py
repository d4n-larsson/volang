LEVELS = ["DEBUG", "INFO", "WARN", "ERROR"]


def is_digits(s):
    return len(s) > 0 and all("0" <= c <= "9" for c in s)


def month_length(year, month):
    if month == 2:
        if year % 4 == 0 and (year % 100 != 0 or year % 400 == 0):
            return 29
        return 28
    if month in (4, 6, 9, 11):
        return 30
    return 31


def valid_date(word):
    if len(word) != 10 or word[4] != "-" or word[7] != "-":
        return False
    y, m, d = word[0:4], word[5:7], word[8:10]
    if not (is_digits(y) and is_digits(m) and is_digits(d)):
        return False
    year, month, day = int(y), int(m), int(d)
    return year >= 1 and 1 <= month <= 12 and 1 <= day <= month_length(year, month)


def valid_time(word):
    if len(word) != 8 or word[2] != ":" or word[5] != ":":
        return False
    h, m, s = word[0:2], word[3:5], word[6:8]
    if not (is_digits(h) and is_digits(m) and is_digits(s)):
        return False
    return int(h) <= 23 and int(m) <= 59 and int(s) <= 59


def parse_record(line):
    """(level, component) of a valid log line, or None."""
    words = [w for w in line.split(" ") if w != ""]
    if len(words) < 4:
        return None
    if not valid_date(words[0]) or not valid_time(words[1]):
        return None
    if words[2] not in LEVELS:
        return None
    comp = words[3]
    if len(comp) < 2 or not comp.endswith(":"):
        return None
    return words[2], comp[:-1]


def stats(lines):
    valid = sum(1 for line in lines if parse_record(line) is not None)
    return ["lines " + str(len(lines)) + " valid " + str(valid) + " invalid " + str(len(lines) - valid)]


def levels(lines):
    counts = {lv: 0 for lv in LEVELS}
    for line in lines:
        rec = parse_record(line)
        if rec is not None:
            counts[rec[0]] += 1
    return [lv + " " + str(counts[lv]) for lv in LEVELS]


def errors(lines):
    per = {}
    for line in lines:
        rec = parse_record(line)
        if rec is not None and rec[0] == "ERROR":
            per[rec[1]] = per.get(rec[1], 0) + 1
    if not per:
        return ["no errors"]
    names = sorted(per)
    names.sort(key=lambda c: -per[c])
    return [c + " " + str(per[c]) for c in names]
