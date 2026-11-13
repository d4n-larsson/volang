from money import CURRENCIES, format_money
from pay import compute_pay
from staff import sorted_staff


def payslip(emp):
    p = compute_pay(emp)
    out = ["payslip " + emp.emp_id + " " + emp.name + " (" + emp.kind + ")"]
    if emp.kind == "hourly":
        out.append("  regular " + str(p.regular_hours) + "h " + format_money(p.regular, emp.currency))
        if p.overtime_hours > 0:
            out.append("  overtime " + str(p.overtime_hours) + "h " + format_money(p.overtime, emp.currency))
    else:
        out.append("  salary " + format_money(p.regular, emp.currency))
    out.append("  gross " + format_money(p.gross, emp.currency))
    out.append("  tax " + format_money(p.tax, emp.currency))
    for d in emp.deductions:
        out.append("  deduction " + d.label + " " + format_money(d.amount, emp.currency))
    out.append("  net " + format_money(p.net, emp.currency))
    return out


class Totals:
    def __init__(self):
        self.gross = 0
        self.tax = 0
        self.deductions = 0
        self.net = 0


def total_line(t, currency):
    return ("total gross " + format_money(t.gross, currency) + " tax " + format_money(t.tax, currency)
            + " deductions " + format_money(t.deductions, currency) + " net " + format_money(t.net, currency))


def summary(staff):
    emps = sorted_staff(staff)
    if not emps:
        return ["summary: no employees"]
    out = ["summary: " + str(len(emps)) + " employees"]
    totals = {}
    for emp in emps:
        p = compute_pay(emp)
        out.append("  " + emp.emp_id + " " + emp.name + " gross " + format_money(p.gross, emp.currency)
                   + " net " + format_money(p.net, emp.currency))
        t = totals.setdefault(emp.currency, Totals())
        t.gross += p.gross
        t.tax += p.tax
        t.deductions += p.deductions
        t.net += p.net
    for currency in CURRENCIES:
        if currency in totals:
            out.append(total_line(totals[currency], currency))
    return out
