import sys
sec, conf = "", {}
for line in sys.stdin:
    s = line.strip()
    if not s or s.startswith("#"):
        continue
    if s.startswith("["):
        sec = s[1:-1] + "."
        continue
    k, v = s.split("=", 1)
    conf[sec + k.strip()] = v.strip()
for k in sorted(conf):
    print(k + "=" + conf[k])
