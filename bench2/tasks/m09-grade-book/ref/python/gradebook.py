def rdiv(x, y):
    return (2 * x + y) // (2 * y)


class GradeBook:
    def __init__(self):
        self.assignments = {}  # name -> (weight, max), in declaration order
        self.students = []
        self.scores = {}  # (student, assignment) -> points
        self.drop = 0

    def apply(self, p):
        """False if the line breaks a rule."""
        if p[0] == "assignment":
            if p[1] in self.assignments:
                return False
            self.assignments[p[1]] = (p[2], p[3])
        elif p[0] == "student":
            if p[1] in self.students:
                return False
            self.students.append(p[1])
        elif p[0] == "score":
            if p[1] not in self.students or p[2] not in self.assignments or p[3] > self.assignments[p[2]][1]:
                return False
            self.scores[(p[1], p[2])] = p[3]
        else:
            self.drop = p[1]
        return True

    def bp(self, s, a):
        return self.scores.get((s, a), 0) * 10000 // self.assignments[a][1]

    def final(self, s):
        names = list(self.assignments)
        if not names:
            return 0
        k = min(self.drop, len(names) - 1)
        order = sorted(range(len(names)), key=lambda i: (self.bp(s, names[i]), i))
        kept = [names[i] for i in sorted(order[k:])]
        w = sum(self.assignments[a][0] for a in kept)
        return rdiv(sum(self.assignments[a][0] * self.bp(s, a) for a in kept), w)
