# Read-only views of the plan.

import graph
import schedule
import workdays
from fmt import table, dep_text, id_list, percent


def list_lines(plan):
    if len(plan.tasks) == 0:
        return ["no tasks"]
    out = []
    for t in graph.ordered(plan):
        size = "ms" if t.duration == 0 else str(t.duration) + "d"
        out.append(t.tid + " " + size + " " + t.name)
    return out


def schedule_lines(plan):
    if len(plan.tasks) == 0:
        return ["no tasks"]
    order, slots, end = schedule.compute(plan)
    rows = []
    for t in order:
        s = slots[t.tid]
        rows.append([t.tid, str(s.start), str(s.finish), str(s.lstart), str(s.lfinish), str(s.slack)])
    out = table(["task", "start", "finish", "lstart", "lfinish", "slack"], rows)
    out.append("end " + str(end))
    return out


def critical_line(plan):
    if len(plan.tasks) == 0:
        return "no tasks"
    order, slots, end = schedule.compute(plan)
    ids = [t.tid for t in order if slots[t.tid].slack == 0]
    return "critical: " + " ".join(ids) + " (" + str(schedule.length(plan, end)) + " working days)"


def show_lines(plan, tid):
    t = plan.tasks[tid]
    order, slots, end = schedule.compute(plan)
    s = slots[tid]
    out = ["task " + tid + ": " + t.name]
    if t.duration == 0:
        out.append("  milestone")
    else:
        out.append("  duration " + str(t.duration) + " days")
    out.append("  after: " + id_list([dep_text(d) for d in t.preds]))
    out.append("  before: " + id_list(graph.successors(plan, tid)))
    if t.not_before > 0:
        out.append("  not before day " + str(t.not_before))
    out.append("  start " + str(s.start) + " finish " + str(s.finish) + " slack " + str(s.slack))
    if t.duration > 0:
        out.append("  done " + str(t.done) + "%")
    return out


def planned_days(plan, task, slot, day):
    """How many of the task's working days come before `day`."""
    return min(task.duration, workdays.count(plan, slot.start, day))


def status_lines(plan, day):
    if len(plan.tasks) == 0:
        return ["no tasks"]
    order, slots, end = schedule.compute(plan)
    out = ["status on day " + str(day)]
    total_planned = 0
    total_done = 0
    for t in order:
        if t.duration == 0:
            continue
        planned = planned_days(plan, t, slots[t.tid], day)
        done = t.duration * t.done // 100
        total_planned += planned
        total_done += done
        if done > planned:
            word = "ahead"
        elif done < planned:
            word = "behind"
        else:
            word = "on track"
        out.append("  " + t.tid + ": planned " + str(planned) + " done " + str(done) + " " + word)
    out.append("total planned " + str(total_planned) + " done " + str(total_done) + " "
               + percent(total_done, total_planned))
    return out


def end_line(plan):
    if len(plan.tasks) == 0:
        return "no tasks"
    order, slots, end = schedule.compute(plan)
    return "project finishes on day " + str(end) + " (" + workdays.weekday(end) + ")"
