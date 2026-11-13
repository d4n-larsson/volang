from money import format_money
from pay import compute_pay
from staff import sorted_staff


def payslip(emp):
    p = compute_pay(emp)
    out = ["payslip " + emp.emp_id + " " + emp.name + " (" + emp.kind + ")"]
    if emp.kind == "hourly":
        out.append("  regular " + str(p.regular_hours) + "h " + format_money(p.regular))
        if p.overtime_hours > 0:
            out.append("  overtime " + str(p.overtime_hours) + "h " + format_money(p.overtime))
    else:
        out.append("  salary " + format_money(p.regular))
    out.append("  gross " + format_money(p.gross))
    out.append("  tax " + format_money(p.tax))
    for d in emp.deductions:
        out.append("  deduction " + d.label + " " + format_money(d.amount))
    out.append("  net " + format_money(p.net))
    return out


def total_line(gross, tax, deductions, net):
    return ("total gross " + format_money(gross) + " tax " + format_money(tax)
            + " deductions " + format_money(deductions) + " net " + format_money(net))


def summary(staff):
    emps = sorted_staff(staff)
    if not emps:
        return ["summary: no employees"]
    out = ["summary: " + str(len(emps)) + " employees"]
    gross = tax = deductions = net = 0
    for emp in emps:
        p = compute_pay(emp)
        out.append("  " + emp.emp_id + " " + emp.name + " gross " + format_money(p.gross)
                   + " net " + format_money(p.net))
        gross += p.gross
        tax += p.tax
        deductions += p.deductions
        net += p.net
    out.append(total_line(gross, tax, deductions, net))
    return out
