# Printing times, spans and durations.

from model import MINUTES_PER_DAY


def two(n):
    if n < 10:
        return "0" + str(n)
    return str(n)


def day_of(t):
    return t // MINUTES_PER_DAY


def clock(t):
    """`HH:MM` of a time (its time of day)."""
    m = t % MINUTES_PER_DAY
    return two(m // 60) + ":" + two(m % 60)


def moment(t):
    return str(day_of(t)) + " " + clock(t)


def span(start, end):
    """`D HH:MM-HH:MM`, or `D HH:MM-E HH:MM` when the end is on another day."""
    if day_of(start) == day_of(end):
        return moment(start) + "-" + clock(end)
    return moment(start) + "-" + moment(end)


def zone(offset):
    """`UTC+HH:MM` or `UTC-HH:MM`."""
    sign = "+"
    if offset < 0:
        sign = "-"
        offset = -offset
    return "UTC" + sign + two(offset // 60) + ":" + two(offset % 60)


def duration(minutes):
    return str(minutes // 60) + "h" + two(minutes % 60) + "m"


def count(n, one, many):
    if n == 1:
        return "1 " + one
    return str(n) + " " + many


def joined(items, empty):
    if len(items) == 0:
        return empty
    return ", ".join(items)
