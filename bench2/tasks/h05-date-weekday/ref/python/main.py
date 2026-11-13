import datetime
import re
import sys

DAYS = ["Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"]


def answer(s):
    if not re.fullmatch(r"[0-9]{4}-[0-9]{2}-[0-9]{2}", s):
        return "invalid"
    try:
        d = datetime.date(int(s[:4]), int(s[5:7]), int(s[8:]))
    except ValueError:
        return "invalid"
    return f"{DAYS[d.weekday()]} {d.timetuple().tm_yday}"


text = sys.stdin.read()
lines = text.split("\n")
if text.endswith("\n"):
    lines.pop()
for line in lines if text else []:
    print(answer(line))
