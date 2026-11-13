from money import format_money


class Deduction:
    def __init__(self, label, amount):
        self.label = label
        self.amount = amount


class Employee:
    def __init__(self, emp_id, name, kind, amount):
        self.emp_id = emp_id
        self.name = name
        # "hourly" (amount = cents per hour) or "salaried" (amount = salary in cents)
        self.kind = kind
        self.amount = amount
        self.hours = 0
        self.deductions = []


def pay_text(emp):
    if emp.kind == "hourly":
        return "hourly " + format_money(emp.amount) + "/h"
    return "salaried " + format_money(emp.amount)


def add_employee(staff, emp_id, name, kind, amount):
    if emp_id in staff:
        return "error: employee " + emp_id + " exists"
    emp = Employee(emp_id, name, kind, amount)
    staff[emp_id] = emp
    return "employee " + emp_id + " added: " + pay_text(emp)


def log_hours(staff, emp_id, hours):
    emp = staff.get(emp_id)
    if emp is None:
        return "error: unknown employee " + emp_id
    if emp.kind != "hourly":
        return "error: " + emp_id + " is salaried"
    emp.hours += hours
    return emp_id + " hours " + str(emp.hours)


def set_rate(staff, emp_id, amount):
    emp = staff.get(emp_id)
    if emp is None:
        return "error: unknown employee " + emp_id
    emp.amount = amount
    if emp.kind == "hourly":
        return emp_id + " rate " + format_money(amount) + "/h"
    return emp_id + " salary " + format_money(amount)


def add_deduction(staff, emp_id, amount, label):
    emp = staff.get(emp_id)
    if emp is None:
        return "error: unknown employee " + emp_id
    emp.deductions.append(Deduction(label, amount))
    return emp_id + " deduction " + label + " " + format_money(amount)


def sorted_staff(staff):
    return [staff[k] for k in sorted(staff)]
