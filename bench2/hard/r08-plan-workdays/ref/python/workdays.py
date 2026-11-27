# The working-day calendar: day 0 is a Monday; Saturdays and Sundays are off unless changed.

WEEKDAYS = ["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"]


def weekday(day):
    return WEEKDAYS[day % 7]


def is_working(plan, day):
    if day in plan.days:
        return plan.days[day]
    return day % 7 < 5


def first_working(plan, day):
    while not is_working(plan, day):
        day += 1
    return day


def after(plan, day, n):
    """The n-th working day after `day` (`day` itself when n is 0)."""
    while n > 0:
        day += 1
        if is_working(plan, day):
            n -= 1
    return day


def before(plan, day, n):
    """The n-th working day before `day` (`day` itself when n is 0)."""
    while n > 0:
        day -= 1
        if is_working(plan, day):
            n -= 1
    return day


def count(plan, first, stop):
    """Working days from `first` to `stop` - 1."""
    return sum(1 for d in range(first, stop) if is_working(plan, d))


def holiday(plan, day):
    if not is_working(plan, day):
        return "error: day " + str(day) + " is already off"
    plan.days[day] = False
    return "day " + str(day) + " is now off"


def workday(plan, day):
    if is_working(plan, day):
        return "error: day " + str(day) + " is already a working day"
    plan.days[day] = True
    return "day " + str(day) + " is now a working day"


def cal_lines(plan, first, last):
    if first > last:
        return ["error: bad range"]
    off = [str(d) for d in range(first, last + 1) if not is_working(plan, d)]
    working = last - first + 1 - len(off)
    out = ["days " + str(first) + "-" + str(last) + ": " + str(working) + " working, " + str(len(off)) + " off"]
    if len(off) > 0:
        out.append("off: " + " ".join(off))
    return out
