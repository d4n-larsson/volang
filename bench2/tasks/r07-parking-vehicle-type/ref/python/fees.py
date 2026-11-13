class Rate:
    def __init__(self, free_minutes, hourly, daily_cap):
        self.free_minutes = free_minutes
        self.hourly = hourly
        self.daily_cap = daily_cap


VEHICLE_TYPES = ["car", "motorcycle", "truck"]
RATES = {
    "car": Rate(30, 250, 2000),
    "motorcycle": Rate(60, 100, 800),
    "truck": Rate(0, 600, 4500),
}


def money(cents):
    frac = str(cents % 100)
    if len(frac) < 2:
        frac = "0" + frac
    return str(cents // 100) + "." + frac


def fee(kind, minutes):
    """The fee in cents for a stay of `minutes` by a vehicle of type `kind`."""
    rate = RATES[kind]
    if minutes <= rate.free_minutes:
        return 0
    days = minutes // 1440
    hours = (minutes % 1440 + 59) // 60
    return days * rate.daily_cap + min(rate.daily_cap, hours * rate.hourly)


def rates():
    out = []
    for kind in VEHICLE_TYPES:
        rate = RATES[kind]
        if rate.free_minutes > 0:
            free = "first " + str(rate.free_minutes) + " min free"
        else:
            free = "no free period"
        line = kind + ": " + free + ", " + money(rate.hourly) + " per hour, " + money(rate.daily_cap) + " per day"
        if kind == "truck":
            line += ", level 1 only"
        out.append(line)
    return out
