LEVELS = ["DEBUG", "INFO", "WARN", "ERROR"]


class Entry:
    def __init__(self, words, seconds):
        self.words = words
        self.level = words[2]
        self.component = words[3][:-1]
        self.message = " ".join(words[4:])
        self.hour = int(words[1][0:2])
        self.seconds = seconds
        self.text = " ".join(words)


def is_digits(s):
    return len(s) > 0 and all("0" <= c <= "9" for c in s)


def is_leap(year):
    return year % 4 == 0 and (year % 100 != 0 or year % 400 == 0)


def days_in(year, month):
    if month == 2:
        return 29 if is_leap(year) else 28
    if month in (4, 6, 9, 11):
        return 30
    return 31


def to_seconds(date, time):
    """Seconds since 0001-01-01 00:00:00 for a date and time word, or -1 if invalid."""
    if len(date) != 10 or date[4] != "-" or date[7] != "-":
        return -1
    if len(time) != 8 or time[2] != ":" or time[5] != ":":
        return -1
    parts = [date[0:4], date[5:7], date[8:10], time[0:2], time[3:5], time[6:8]]
    if not all(is_digits(p) for p in parts):
        return -1
    year, month, day, hour, minute, second = [int(p) for p in parts]
    if year < 1 or month < 1 or month > 12 or day < 1 or day > days_in(year, month):
        return -1
    if hour > 23 or minute > 59 or second > 59:
        return -1
    days = 365 * (year - 1) + (year - 1) // 4 - (year - 1) // 100 + (year - 1) // 400
    for m in range(1, month):
        days += days_in(year, m)
    days += day - 1
    return days * 86400 + hour * 3600 + minute * 60 + second


def parse_entry(line):
    """The Entry of a valid log line, or None."""
    words = [w for w in line.split(" ") if w != ""]
    if len(words) < 4:
        return None
    seconds = to_seconds(words[0], words[1])
    if seconds < 0 or words[2] not in LEVELS:
        return None
    if len(words[3]) < 2 or not words[3].endswith(":"):
        return None
    return Entry(words, seconds)


def entries(lines):
    out = []
    for line in lines:
        e = parse_entry(line)
        if e is not None:
            out.append(e)
    return out
