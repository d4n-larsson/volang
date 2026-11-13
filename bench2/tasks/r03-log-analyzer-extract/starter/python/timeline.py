class Stamp:
    def __init__(self, seconds, hour, text):
        self.seconds = seconds
        self.hour = hour
        self.text = text


def digits_only(s):
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
    if not all(digits_only(p) for p in parts):
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
    return days * 86400 + hour * 3600 + minute * 60


def parse_stamp(line):
    """Stamp of a valid log line, or None."""
    words = [w for w in line.split(" ") if w != ""]
    if len(words) < 4:
        return None
    seconds = to_seconds(words[0], words[1])
    if seconds < 0:
        return None
    if words[2] not in ("DEBUG", "INFO", "WARN", "ERROR"):
        return None
    if len(words[3]) < 2 or words[3][-1] != ":":
        return None
    return Stamp(seconds, int(words[1][0:2]), " ".join(words))


def stamps(lines):
    out = []
    for line in lines:
        st = parse_stamp(line)
        if st is not None:
            out.append(st)
    return out


def busiest(lines):
    per_hour = [0] * 24
    found = stamps(lines)
    if not found:
        return ["no entries"]
    for st in found:
        per_hour[st.hour] += 1
    best = 0
    for h in range(24):
        if per_hour[h] > per_hour[best]:
            best = h
    label = str(best)
    if len(label) < 2:
        label = "0" + label
    return ["busiest hour " + label + " with " + str(per_hour[best]) + " entries"]


def in_range(lines, args):
    lo = to_seconds(args[0], args[1])
    hi = to_seconds(args[2], args[3])
    if lo < 0 or hi < 0:
        return ["error: bad time"]
    out = [st.text for st in stamps(lines) if lo <= st.seconds <= hi]
    return out + [str(len(out)) + " entries in range"]


def span(lines):
    found = stamps(lines)
    if not found:
        return ["no entries"]
    first = min(st.seconds for st in found)
    last = max(st.seconds for st in found)
    return ["span " + str(last - first) + " seconds"]
