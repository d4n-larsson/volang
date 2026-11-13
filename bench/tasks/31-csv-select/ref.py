lines = open(0).read().split("\n")[:-1]
want = lines[0].split(",")
head = lines[1].split(",")
missing = [w for w in want if w not in head]
if missing:
    print("unknown column: " + missing[0])
else:
    idx = [head.index(w) for w in want]
    print(",".join(want))
    for row in lines[2:]:
        f = row.split(",")
        print(",".join(f[i] for i in idx))
