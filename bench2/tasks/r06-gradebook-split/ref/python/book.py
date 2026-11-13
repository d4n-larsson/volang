class Assignment:
    def __init__(self, name, weight, max_points):
        self.name = name
        self.weight = weight
        self.max_points = max_points


class Student:
    def __init__(self, name):
        self.name = name
        self.scores = {}


class Book:
    def __init__(self):
        self.assignments = {}
        self.students = {}


def add_assignment(book, name, weight, max_points):
    if name in book.assignments:
        return "error: assignment " + name + " exists"
    book.assignments[name] = Assignment(name, weight, max_points)
    return "assignment " + name + " added"


def enroll(book, name):
    if name in book.students:
        return "error: student " + name + " exists"
    book.students[name] = Student(name)
    return "student " + name + " enrolled"


def unknown_student(name):
    return "error: unknown student " + name


def unknown_assignment(name):
    return "error: unknown assignment " + name


def set_score(book, student, assignment, points):
    if student not in book.students:
        return unknown_student(student)
    if assignment not in book.assignments:
        return unknown_assignment(assignment)
    a = book.assignments[assignment]
    if points > a.max_points:
        return "error: " + str(points) + " above max " + str(a.max_points) + " for " + assignment
    book.students[student].scores[assignment] = points
    return student + " " + assignment + " " + str(points) + "/" + str(a.max_points)
