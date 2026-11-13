from book import unknown_student, unknown_assignment


def rounded_div(num, den):
    """num / den rounded half up (num >= 0, den > 0)."""
    return (num + den // 2) // den


def tenths(x):
    """Formats a non-negative number of tenths: 875 -> 87.5."""
    return str(x // 10) + "." + str(x % 10)


def percent_tenths(points, max_points):
    """points out of max_points as tenths of a percent."""
    return rounded_div(points * 1000, max_points)


def weighted_average(book, student):
    """The student's weighted average in tenths of a percent, or None without scores."""
    total = 0
    weight = 0
    for a in book.assignments.values():
        if a.name in student.scores:
            total += a.weight * percent_tenths(student.scores[a.name], a.max_points)
            weight += a.weight
    if weight == 0:
        return None
    return rounded_div(total, weight)


def letter(avg):
    """The letter grade for an average in tenths of a percent."""
    if avg >= 900:
        return "A"
    if avg >= 800:
        return "B"
    if avg >= 700:
        return "C"
    if avg >= 600:
        return "D"
    return "F"


def assignment_stats(book, name):
    if name not in book.assignments:
        return [unknown_assignment(name)]
    points = [s.scores[name] for s in book.students.values() if name in s.scores]
    if not points:
        return [name + " no scores"]
    mean = rounded_div(10 * sum(points), len(points))
    return [name + " count " + str(len(points)) + " min " + str(min(points)) + " max " + str(max(points))
            + " mean " + tenths(mean)]


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


def class_report(book):
    graded = []
    ungraded = []
    for student in book.students.values():
        avg = weighted_average(book, student)
        if avg is None:
            ungraded.append(student.name)
        else:
            graded.append((avg, student.name))
    graded.sort(key=lambda g: (-g[0], g[1]))
    out = []
    for avg, name in graded:
        rank = 1 + sum(1 for other, _ in graded if other > avg)
        out.append(str(rank) + ". " + name + " " + tenths(avg) + " " + letter(avg))
    for name in sorted(ungraded):
        out.append("-. " + name + " no scores")
    if graded:
        out.append("class average " + tenths(rounded_div(sum(avg for avg, _ in graded), len(graded))))
    else:
        out.append("class average none")
    return out
