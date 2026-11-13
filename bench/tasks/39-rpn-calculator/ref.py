import sys
for line in sys.stdin:
    st, bad = [], False
    for t in line.split():
        if t in ("+", "-", "*", "/"):
            if len(st) < 2:
                bad = True
                break
            b, a = st.pop(), st.pop()
            if t == "+":
                st.append(a + b)
            elif t == "-":
                st.append(a - b)
            elif t == "*":
                st.append(a * b)
            elif b == 0:
                bad = True
                break
            else:
                q = abs(a) // abs(b)
                st.append(q if (a < 0) == (b < 0) else -q)
        else:
            st.append(int(t))
    print("error" if bad or len(st) != 1 else st[0])
