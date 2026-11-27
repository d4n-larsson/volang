"""Exchange rates and conversion to the base currency."""

from fmt import money
from model import BASE


def rate_text(rate):
    """Ten-thousandths as `X.YYYY`."""
    frac = str(rate % 10000)
    while len(frac) < 4:
        frac = "0" + frac
    return str(rate // 10000) + "." + frac


def set_rate(ledger, currency, day, rate):
    if currency == BASE:
        return "error: " + BASE + " is the base currency"
    ledger.rates.setdefault(currency, {})[day] = rate
    return "rate " + currency + " from day " + str(day) + ": " + rate_text(rate)


def rate_on(ledger, currency, day):
    """The rate of the currency on the day (the one with the greatest day not after it), or 0."""
    best_day = -1
    best = 0
    for d, r in ledger.rates.get(currency, {}).items():
        if d <= day and d > best_day:
            best_day = d
            best = r
    return best


def latest_rate(ledger, currency):
    """The rate with the greatest day, or 0."""
    days = ledger.rates.get(currency, {})
    if not days:
        return 0
    return days[max(days)]


def to_base(amount, rate):
    """amount * rate / 10000, rounded half away from zero."""
    if amount < 0:
        return -to_base(-amount, rate)
    return (amount * rate + 5000) // 10000


def native_text(amount, currency):
    """An amount in a currency: a foreign one carries its code."""
    if currency == BASE:
        return money(amount)
    return money(amount) + " " + currency
