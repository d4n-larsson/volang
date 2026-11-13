def hanoi(n, a, c, b):
    if n == 0:
        return
    hanoi(n - 1, a, b, c)
    print(n, a, c)
    hanoi(n - 1, b, c, a)
hanoi(int(input()), "A", "C", "B")
