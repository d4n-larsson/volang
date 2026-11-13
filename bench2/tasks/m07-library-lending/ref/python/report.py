from commands import money


def report(lib):
    out = ["members:"]
    for m in sorted(lib.limit):
        out.append(f"{m} loans {','.join(sorted(lib.loans[m])) or '-'} owes {money(lib.owes[m])}")
    out.append("books:")
    for b in sorted(lib.copies):
        out.append(f"{b} {lib.shelf[b]}/{lib.copies[b]} queue {','.join(lib.queue[b]) or '-'}")
    return out
