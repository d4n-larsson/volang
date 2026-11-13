import sys

from staff import add_employee, log_hours, set_rate, add_deduction
from report import payslip, summary
from money import CURRENCIES


def is_number(s):
    return len(s) > 0 and all("0" <= c <= "9" for c in s)


def run(words, staff):
    cmd = words[0]
    args = words[1:]
    if cmd == "employee":
        if len(args) not in (4, 5) or args[2] not in ("hourly", "salaried") or not is_number(args[3]):
            return ["error: bad command"]
        currency = args[4] if len(args) == 5 else "USD"
        if currency not in CURRENCIES:
            return ["error: unknown currency " + currency]
        return [add_employee(staff, args[0], args[1], args[2], int(args[3]), currency)]
    if cmd == "hours":
        if len(args) != 2 or not is_number(args[1]):
            return ["error: bad command"]
        return [log_hours(staff, args[0], int(args[1]))]
    if cmd == "rate":
        if len(args) != 2 or not is_number(args[1]):
            return ["error: bad command"]
        return [set_rate(staff, args[0], int(args[1]))]
    if cmd == "deduct":
        if len(args) != 3 or not is_number(args[1]):
            return ["error: bad command"]
        return [add_deduction(staff, args[0], int(args[1]), args[2])]
    if cmd == "payslip":
        if len(args) != 1:
            return ["error: bad command"]
        if args[0] not in staff:
            return ["error: unknown employee " + args[0]]
        return payslip(staff[args[0]])
    if cmd == "summary":
        if len(args) != 0:
            return ["error: bad command"]
        return summary(staff)
    return ["error: unknown command " + cmd]


def main():
    staff = {}
    for line in sys.stdin.read().split("\n"):
        words = [w for w in line.split(" ") if w != ""]
        if not words:
            continue
        for out in run(words, staff):
            print(out)


main()
