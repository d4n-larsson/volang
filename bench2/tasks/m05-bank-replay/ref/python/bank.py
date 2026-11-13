FEE = 250


class Bank:
    def __init__(self):
        self.balance, self.limit, self.closed = {}, {}, set()
        self.fees = self.interest = 0

    def exists(self, a):
        return a in self.balance and a not in self.closed

    def apply(self, cmd, args):
        """None on success, else the rejection reason."""
        if cmd == "open":
            if args[0] in self.balance:
                return f"account {args[0]} exists"
            self.balance[args[0]], self.limit[args[0]] = 0, args[1]
            return None
        if cmd == "month":
            for a in self.balance:
                if self.exists(a) and self.balance[a] < 0:
                    i = (-self.balance[a] * 15 + 999) // 1000
                    self.balance[a] -= i
                    self.interest += i
            return None
        names = args[:2] if cmd == "transfer" else args[:1]
        for a in names:
            if not self.exists(a):
                return f"no account {a}"
        a, b = self.balance, args[0]
        if cmd == "deposit":
            a[b] += args[1]
        elif cmd == "withdraw":
            if a[b] - args[1] < -self.limit[b]:
                return "insufficient funds"
            a[b] -= args[1]
            if a[b] < 0:
                a[b] -= FEE
                self.fees += FEE
        elif cmd == "transfer":
            if args[0] == args[1]:
                return "same account"
            if a[b] - args[2] < 0:
                return "insufficient funds"
            a[b] -= args[2]
            a[args[1]] += args[2]
        elif cmd == "close":
            if a[b] != 0:
                return "balance not zero"
            self.closed.add(b)
        return None
