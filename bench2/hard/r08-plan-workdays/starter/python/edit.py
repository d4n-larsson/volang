# Commands that change the plan.

import graph
from model import Dep, Task


def unknown(tid):
    return "error: unknown task " + tid


def add_task(plan, tid, duration, name):
    if tid in plan.tasks:
        return "error: task " + tid + " exists"
    plan.tasks[tid] = Task(tid, name, duration, plan.next_seq)
    plan.next_seq += 1
    if duration == 0:
        return "added milestone " + tid
    return "added task " + tid


def rename(plan, tid, name):
    if tid not in plan.tasks:
        return unknown(tid)
    plan.tasks[tid].name = name
    return "task " + tid + " renamed"


def add_dep(plan, tid, pred, lag):
    if tid not in plan.tasks:
        return unknown(tid)
    if pred not in plan.tasks:
        return unknown(pred)
    if tid == pred:
        return "error: task " + tid + " cannot depend on itself"
    t = plan.tasks[tid]
    if graph.depends_on(t, pred):
        return "error: " + tid + " already depends on " + pred
    cycle = graph.cycle_with(plan, tid, pred)
    if cycle is not None:
        return "error: cycle " + " -> ".join(cycle)
    t.preds.append(Dep(pred, lag))
    if lag > 0:
        return tid + " after " + pred + "+" + str(lag)
    return tid + " after " + pred


def remove_dep(plan, tid, pred):
    if tid not in plan.tasks:
        return unknown(tid)
    if pred not in plan.tasks:
        return unknown(pred)
    t = plan.tasks[tid]
    if not graph.depends_on(t, pred):
        return "error: " + tid + " does not depend on " + pred
    t.preds = [d for d in t.preds if d.pred != pred]
    return tid + " no longer after " + pred


def remove_task(plan, tid):
    if tid not in plan.tasks:
        return unknown(tid)
    needed = graph.successors(plan, tid)
    if len(needed) > 0:
        return "error: " + tid + " is needed by " + ", ".join(needed)
    del plan.tasks[tid]
    return "removed " + tid


def set_duration(plan, tid, duration):
    if tid not in plan.tasks:
        return unknown(tid)
    plan.tasks[tid].duration = duration
    return "task " + tid + " duration " + str(duration)


def set_not_before(plan, tid, day):
    if tid not in plan.tasks:
        return unknown(tid)
    plan.tasks[tid].not_before = day
    return "task " + tid + " starts no earlier than day " + str(day)


def set_progress(plan, tid, pct):
    if tid not in plan.tasks:
        return unknown(tid)
    t = plan.tasks[tid]
    if t.duration == 0:
        return "error: " + tid + " is a milestone"
    t.done = pct
    return "task " + tid + " " + str(pct) + "% done"
