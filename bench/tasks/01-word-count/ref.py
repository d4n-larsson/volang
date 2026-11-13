import sys
data = sys.stdin.read()
print(data.count("\n"), len(data.split()))
