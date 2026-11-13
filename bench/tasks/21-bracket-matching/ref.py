import sys
P = {")": "(", "]": "[", "}": "{"}
for line in sys.stdin:
    st, ok = [], True
    for c in line:
        if c in "([{":
            st.append(c)
        elif c in P:
            if not st or st.pop() != P[c]:
                ok = False
                break
    print("yes" if ok and not st else "no")
