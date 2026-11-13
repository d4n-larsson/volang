FREE_MINUTES = 30
HOURLY = 250
DAILY_CAP = 2000


def money(cents):
    frac = str(cents % 100)
    if len(frac) < 2:
        frac = "0" + frac
    return str(cents // 100) + "." + frac


def fee(minutes):
    """The fee in cents for a stay of `minutes`."""
    if minutes <= FREE_MINUTES:
        return 0
    days = minutes // 1440
    hours = (minutes % 1440 + 59) // 60
    return days * DAILY_CAP + min(DAILY_CAP, hours * HOURLY)


def rates():
    return ["car: first " + str(FREE_MINUTES) + " min free, " + money(HOURLY) + " per hour, "
            + money(DAILY_CAP) + " per day"]
