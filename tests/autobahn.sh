#!/bin/sh
# Spec 063 SC-004: std.websocket's two roles against the Autobahn test suite (crossbario/autobahn-testsuite,
# in Docker), sections 1 to 7, 9 and 10 (12 and 13 are compression, which vo does not offer). Not part of
# `make test`: it needs Docker and the image. Prints the number of cases per result for each role and
# exits 1 when a case failed. Reports: build/autobahn/{server,client}/index.html.
set -u
root=$(cd "$(dirname "$0")/.." && pwd)
out="$root/build/autobahn"
image=crossbario/autobahn-testsuite
cases='["1.*", "2.*", "3.*", "4.*", "5.*", "6.*", "7.*", "9.*", "10.*"]'
rm -rf "$out"; mkdir -p "$out/server" "$out/client" "$out/config"
cd "$root/tests/autobahn" || exit 1

# The server role: the suite connects to tests/autobahn/server.vo.
"$root/bin/vo" run server.vo >"$out/server.log" 2>&1 &
server=$!
sleep 1
cat >"$out/config/fuzzingclient.json" <<JSON
{"outdir": "/reports/server", "servers": [{"agent": "vo", "url": "ws://127.0.0.1:9002/echo"}],
 "cases": $cases, "exclude-cases": [], "exclude-agent-cases": {}}
JSON
docker run --rm --network host -v "$out/config:/config" -v "$out:/reports" $image \
    wstest -m fuzzingclient -s /config/fuzzingclient.json >"$out/fuzzingclient.log" 2>&1
kill -TERM $server; wait $server

# The client role: tests/autobahn/client.vo connects to the suite.
cat >"$out/config/fuzzingserver.json" <<JSON
{"url": "ws://127.0.0.1:9001", "outdir": "/reports/client", "cases": $cases, "exclude-cases": [], "exclude-agent-cases": {}}
JSON
docker run --rm --network host -v "$out/config:/config" -v "$out:/reports" --name vo-autobahn $image \
    wstest -m fuzzingserver -s /config/fuzzingserver.json >"$out/fuzzingserver.log" 2>&1 &
sleep 3
"$root/bin/vo" run --timeout 3600 client.vo >"$out/client.log" 2>&1
docker stop vo-autobahn >/dev/null 2>&1

python3 - "$out" <<'PY'
import json, os, sys
out, bad = sys.argv[1], 0
for role in ("server", "client"):
    path = os.path.join(out, role, "index.json")
    if not os.path.exists(path):
        print(f"{role}: no report"); bad = 1; continue
    counts = {}
    for agent, results in json.load(open(path)).items():
        for case, r in sorted(results.items()):
            counts[r["behavior"]] = counts.get(r["behavior"], 0) + 1
            if r["behavior"] == "FAILED" or r.get("behaviorClose") == "FAILED":
                print(f"{role} {case}: {r['behavior']} (close {r.get('behaviorClose')})"); bad = 1
    print(f"{role}: " + ", ".join(f"{k} {v}" for k, v in sorted(counts.items())))
sys.exit(bad)
PY
