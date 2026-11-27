# Forward and backward passes over the dependency graph.

import graph
import workdays
from model import Slot


def forward(plan, order):
    slots = {}
    for t in order:
        start = t.not_before
        for d in t.preds:
            start = max(start, workdays.after(plan, slots[d.pred].finish, d.lag))
        start = workdays.first_working(plan, start)
        finish = start
        if t.duration > 0:
            finish = workdays.after(plan, workdays.after(plan, start, t.duration - 1), 1)
        slots[t.tid] = Slot(start, finish)
    return slots


def project_end(slots):
    end = 0
    for s in slots.values():
        end = max(end, s.finish)
    return end


def backward(plan, order, slots, end):
    for t in reversed(order):
        lfinish = end
        for sid in graph.successors(plan, t.tid):
            for d in plan.tasks[sid].preds:
                if d.pred == t.tid:
                    lfinish = min(lfinish, workdays.before(plan, slots[sid].lstart, d.lag))
        s = slots[t.tid]
        s.lfinish = lfinish
        s.lstart = workdays.before(plan, lfinish, t.duration)
        s.slack = workdays.count(plan, s.start, s.lstart)


def compute(plan):
    """(order, slots, end): the tasks in schedule order, a Slot per task ID, the project's finish."""
    order = graph.topo_order(plan)
    slots = forward(plan, order)
    end = project_end(slots)
    backward(plan, order, slots, end)
    return order, slots, end


def length(plan, end):
    """Working days from the project start to its finish."""
    return workdays.count(plan, 0, end)
