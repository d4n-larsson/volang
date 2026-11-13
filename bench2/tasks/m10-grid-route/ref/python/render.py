def draw(rows, cells):
    grid = [list(r) for r in rows]
    for r, c in cells:
        if grid[r][c] not in "SG123456789":
            grid[r][c] = "*"
    return ["".join(g) for g in grid]
