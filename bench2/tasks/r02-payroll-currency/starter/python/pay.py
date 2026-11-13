OVERTIME_AFTER = 40

# (upper limit of the bracket in cents, rate in basis points); -1 = no limit
BRACKETS = [(100000, 1000), (300000, 2000), (-1, 3000)]


class Pay:
    def __init__(self):
        self.regular_hours = 0
        self.overtime_hours = 0
        self.regular = 0
        self.overtime = 0
        self.gross = 0
        self.tax = 0
        self.deductions = 0
        self.net = 0


def income_tax(gross):
    total_bp = 0
    lower = 0
    for upper, bp in BRACKETS:
        if gross <= lower:
            break
        top = gross
        if upper >= 0 and gross > upper:
            top = upper
        total_bp += (top - lower) * bp
        lower = upper
    return total_bp // 10000


def compute_pay(emp):
    p = Pay()
    if emp.kind == "hourly":
        p.regular_hours = min(emp.hours, OVERTIME_AFTER)
        p.overtime_hours = emp.hours - p.regular_hours
        p.regular = p.regular_hours * emp.amount
        p.overtime = p.overtime_hours * emp.amount * 3 // 2
    else:
        p.regular = emp.amount
    p.gross = p.regular + p.overtime
    p.tax = income_tax(p.gross)
    p.deductions = sum(d.amount for d in emp.deductions)
    p.net = p.gross - p.tax - p.deductions
    return p
