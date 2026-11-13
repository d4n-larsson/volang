def two_digits(n):
    s = str(n)
    if len(s) < 2:
        s = "0" + s
    return s


def format_money(cents):
    if cents < 0:
        return "-" + format_money(-cents)
    return "$" + str(cents // 100) + "." + two_digits(cents % 100)
