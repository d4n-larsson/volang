import sys
st = []
for line in sys.stdin:
    p = line.split()
    if p[0] == "push":
        st.append(int(p[1]))
    elif p[0] == "pop":
        print(st.pop() if st else "empty")
    elif p[0] == "peek":
        print(st[-1] if st else "empty")
    else:
        print(len(st))
