import sys

from book import Book, add_assignment, enroll, set_score
from stats import assignment_stats
from reports import card, class_report, distribution


def is_number(s):
    return len(s) > 0 and all("0" <= c <= "9" for c in s)


def run(words, book):
    cmd = words[0]
    args = words[1:]
    bad = ["error: bad command"]
    if cmd == "assignment":
        if len(args) != 3 or not is_number(args[1]) or not is_number(args[2]):
            return bad
        weight = int(args[1])
        max_points = int(args[2])
        if weight < 1 or weight > 100 or max_points < 1:
            return bad
        return [add_assignment(book, args[0], weight, max_points)]
    if cmd == "enroll":
        if len(args) != 1:
            return bad
        return [enroll(book, args[0])]
    if cmd == "score":
        if len(args) != 3 or not is_number(args[2]):
            return bad
        return [set_score(book, args[0], args[1], int(args[2]))]
    if cmd == "card":
        if len(args) != 1:
            return bad
        return card(book, args[0])
    if cmd == "stats":
        if len(args) != 1:
            return bad
        return assignment_stats(book, args[0])
    if cmd == "distribution":
        if len(args) != 0:
            return bad
        return distribution(book)
    if cmd == "report":
        if len(args) != 0:
            return bad
        return class_report(book)
    return ["error: unknown command " + cmd]


def main():
    book = Book()
    for line in sys.stdin.read().split("\n"):
        words = [w for w in line.split(" ") if w != ""]
        if not words:
            continue
        for out in run(words, book):
            print(out)


main()
