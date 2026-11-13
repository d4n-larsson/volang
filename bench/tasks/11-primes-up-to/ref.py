n = int(input())
sieve = [True] * (n + 1)
primes = []
for i in range(2, n + 1):
    if sieve[i]:
        primes.append(i)
        for j in range(i * i, n + 1, i):
            sieve[j] = False
print(" ".join(map(str, primes)))
