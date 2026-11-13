from book import unknown_assignment


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


def assignment_stats(book, name):
    if name not in book.assignments:
        return [unknown_assignment(name)]
    points = [s.scores[name] for s in book.students.values() if name in s.scores]
    if not points:
        return [name + " no scores"]
    mean = rounded_div(10 * sum(points), len(points))
    return [name + " count " + str(len(points)) + " min " + str(min(points)) + " max " + str(max(points))
            + " mean " + tenths(mean)]
