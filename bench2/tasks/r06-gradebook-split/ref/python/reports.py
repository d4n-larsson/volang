from book import unknown_student
from stats import rounded_div, tenths, percent_tenths, weighted_average
from letters import LETTER_ORDER, letter


def card(book, name):
    if name not in book.students:
        return [unknown_student(name)]
    student = book.students[name]
    avg = weighted_average(book, student)
    if avg is None:
        out = [name + " no scores"]
    else:
        out = [name + " average " + tenths(avg) + " " + letter(avg)]
    for a in book.assignments.values():
        if a.name in student.scores:
            p = student.scores[a.name]
            out.append("  " + a.name + " " + str(p) + "/" + str(a.max_points) + " "
                       + tenths(percent_tenths(p, a.max_points)))
        else:
            out.append("  " + a.name + " missing")
    return out


def graded_students(book):
    """(average, name) of every student with scores, in enrollment order."""
    graded = []
    for student in book.students.values():
        avg = weighted_average(book, student)
        if avg is not None:
            graded.append((avg, student.name))
    return graded


def class_report(book):
    graded = graded_students(book)
    graded.sort(key=lambda g: (-g[0], g[1]))
    out = []
    for avg, name in graded:
        rank = 1 + sum(1 for other, _ in graded if other > avg)
        out.append(str(rank) + ". " + name + " " + tenths(avg) + " " + letter(avg))
    ungraded = [s.name for s in book.students.values() if weighted_average(book, s) is None]
    for name in sorted(ungraded):
        out.append("-. " + name + " no scores")
    if graded:
        out.append("class average " + tenths(rounded_div(sum(avg for avg, _ in graded), len(graded))))
    else:
        out.append("class average none")
    return out


def distribution(book):
    by_letter = {}
    for avg, name in graded_students(book):
        by_letter.setdefault(letter(avg), []).append(name)
    if not by_letter:
        return ["no grades"]
    out = []
    for grade in LETTER_ORDER:
        if grade in by_letter:
            names = sorted(by_letter[grade])
            out.append(grade + " " + str(len(names)) + " " + " ".join(names))
    return out
