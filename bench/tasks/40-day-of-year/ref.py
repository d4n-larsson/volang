import sys
for line in sys.stdin:
    s = line.rstrip("\n")
    ok = len(s) == 10 and s[4] == "-" and s[7] == "-" and (s[:4] + s[5:7] + s[8:]).isdigit()
    if ok:
        y, m, d = int(s[:4]), int(s[5:7]), int(s[8:])
        leap = y % 4 == 0 and (y % 100 != 0 or y % 400 == 0)
        days = [31, 29 if leap else 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31]
        ok = 1 <= y and 1 <= m <= 12 and 1 <= d <= days[m - 1]
    print(sum(days[:m - 1]) + d if ok else "invalid")
