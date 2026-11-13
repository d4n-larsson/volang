# Summary order of the currencies.
CURRENCIES = ["USD", "EUR", "JPY"]


def two_digits(n):
    s = str(n)
    if len(s) < 2:
        s = "0" + s
    return s


def format_money(amount, currency):
    if amount < 0:
        return "-" + format_money(-amount, currency)
    if currency == "JPY":
        return "JPY " + str(amount)
    text = str(amount // 100) + "." + two_digits(amount % 100)
    if currency == "EUR":
        return text + " EUR"
    return "$" + text
