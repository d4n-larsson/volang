check = 0
for rnd in range(12000):
    parts = []
    for i in range(60):
        parts.append("item" + str(i * rnd % 97))
    line = ",".join(parts)
    back = line.split(",")
    hits = 0
    for p in back:
        if p.find("7") >= 0:
            hits = hits + 1
    check = check + hits + len(line)
print(check)
