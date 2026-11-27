# A text chart of the schedule.

import schedule
import workdays
from fmt import pad_right

MAX_COLUMNS = 60


def cell(plan, task, slot, day):
    if task.duration == 0:
        if day == slot.start:
            return "*"
        return "."
    last = workdays.after(plan, slot.start, task.duration - 1)
    if slot.start <= day <= last:
        if workdays.is_working(plan, day):
            return "#"
        return ":"
    return "."


def chart(plan, first, last):
    order, slots, end = schedule.compute(plan)
    width = max(len(t.tid) for t in order)
    ruler = "".join(str(day % 10) for day in range(first, last + 1))
    out = [pad_right("", width) + " " + ruler]
    for t in order:
        row = "".join(cell(plan, t, slots[t.tid], day) for day in range(first, last + 1))
        out.append(pad_right(t.tid, width) + " " + row)
    return out


def gantt(plan, args):
    """`gantt` shows days 0 to the project's finish (at most MAX_COLUMNS of them);
    `gantt FROM TO` the days FROM to TO."""
    if len(plan.tasks) == 0:
        return ["no tasks"]
    if len(args) == 0:
        order, slots, end = schedule.compute(plan)
        return chart(plan, 0, min(end, MAX_COLUMNS - 1))
    first, last = int(args[0]), int(args[1])
    if first > last:
        return ["error: bad range"]
    if last - first + 1 > MAX_COLUMNS:
        return ["error: range too wide"]
    return chart(plan, first, last)
