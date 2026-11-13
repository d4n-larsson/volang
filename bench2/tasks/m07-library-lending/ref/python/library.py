from commands import money


class Library:
    def __init__(self):
        self.copies, self.shelf, self.queue = {}, {}, {}
        self.limit, self.owes, self.loans = {}, {}, {}  # loans: id -> {isbn: due}

    def unknown(self, m, isbn):
        if m not in self.limit:
            return f"error: no member {m}"
        if isbn is not None and isbn not in self.copies:
            return f"error: no book {isbn}"
        return None

    def refusal(self, m, isbn):
        """The first of borrow checks 2-4 that applies, or None."""
        if self.owes[m] > 500:
            return f"{m} blocked: owes {money(self.owes[m])}"
        if isbn in self.loans[m]:
            return f"{m} already has {isbn}"
        if len(self.loans[m]) >= self.limit[m]:
            return f"{m} at limit"
        return None

    def lend(self, m, isbn, day):
        self.loans[m][isbn] = day + 14
        self.shelf[isbn] -= 1

    def serve(self, isbn, day, out):
        q = self.queue[isbn]
        while self.shelf[isbn] > 0 and q:
            m = q.pop(0)
            if self.refusal(m, isbn):
                out.append(f"{m} skipped for {isbn}")
            else:
                self.lend(m, isbn, day)
                out.append(f"{isbn} -> {m} (due {day + 14})")

    def run(self, day, cmd, a, b, out):
        if cmd == "book":
            if a not in self.copies:
                self.copies[a], self.shelf[a], self.queue[a] = 0, 0, []
            self.copies[a] += b
            self.shelf[a] += b
            out.append(f"book {a}: {self.copies[a]} copies")
            self.serve(a, day, out)
        elif cmd == "member":
            if a in self.limit:
                out.append(f"error: member {a} exists")
            else:
                self.limit[a], self.owes[a], self.loans[a] = b, 0, {}
                out.append(f"member {a} joined")
        elif cmd == "pay":
            if self.unknown(a, None):
                out.append(self.unknown(a, None))
            elif b > self.owes[a]:
                out.append(f"error: {a} owes only {money(self.owes[a])}")
            else:
                self.owes[a] -= b
                out.append(f"{a} pays {money(b)}, owes {money(self.owes[a])}")
        elif self.unknown(a, b):
            out.append(self.unknown(a, b))
        elif cmd == "borrow":
            r = self.refusal(a, b)
            if r:
                out.append(r)
            elif self.shelf[b] == 0:
                if a in self.queue[b]:
                    out.append(f"{a} already waiting for {b}")
                else:
                    self.queue[b].append(a)
                    out.append(f"{a} waits for {b} (position {len(self.queue[b])})")
            else:
                self.lend(a, b, day)
                out.append(f"{a} borrows {b} (due {day + 14})")
        else:
            if b not in self.loans[a]:
                out.append(f"error: {a} does not have {b}")
                return
            late = day - self.loans[a].pop(b)
            self.shelf[b] += 1
            if late > 0:
                fine = min(late * 25, 1000)
                self.owes[a] += fine
                out.append(f"{a} returns {b} late {late} days, fine {money(fine)}")
            else:
                out.append(f"{a} returns {b}")
            self.serve(b, day, out)
