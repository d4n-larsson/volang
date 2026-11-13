import sys

for name in sys.stdin.read().split("\n"):
    if not name:
        continue
    print("== " + name)
    try:
        with open(f"in/{name}.txt") as f:
            text = f.read()
    except OSError:
        print("missing")
        continue
    best = {}
    for line in text.split("\n"):
        if line:
            player, score = line.split(" ")
            best[player] = max(best.get(player, int(score)), int(score))
    rows = sorted(best.items(), key=lambda kv: (-kv[1], kv[0]))
    out = []
    for i, (player, score) in enumerate(rows):
        rank = i + 1 if i == 0 or rows[i - 1][1] != score else rank
        out.append(f"{rank} {player} {score}\n")
    board = "".join(out)
    sys.stdout.write(board)
    with open(f"out/{name}.txt", "w") as f:
        f.write(board)
    print(f"published out/{name}.txt")
