#!/usr/bin/env python3
"""Spec 013 (H4): a process swapping a symbolic link between the scope check and the open must never
make a scoped read reach outside the scope. Runs the program RUNS times while a helper flips
`data/link` between `real.txt` (inside) and the secret (outside); exit 1 on any secret read.

  race.py [VO] [RUNS]   (RUNS default 1000)
"""
import os, subprocess, sys, tempfile, threading

VO = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else
                     os.path.join(os.path.dirname(__file__), "..", "..", "build", "vo"))
RUNS = int(sys.argv[2]) if len(sys.argv) > 2 else 1000
PROGRAM = """fn main() uses console, file.read("data/") {
    var i = 0;
    while i < 2000 {
        match read_file("data/link") {
            case ok(s) {
                if s == "TOPSECRET" {
                    print("SECRET");
                }
            }
            case err(e) {
            }
        }
        set i = i + 1;
    }
}
"""
d = tempfile.mkdtemp()
os.mkdir(os.path.join(d, "data"))
open(os.path.join(d, "data", "real.txt"), "w").write("benign")
open(os.path.join(d, "secret.txt"), "w").write("TOPSECRET")
open(os.path.join(d, "main.vo"), "w").write(PROGRAM)
os.symlink("real.txt", os.path.join(d, "data", "link"))
subprocess.run([VO, "effects", "--update", "main.vo"], cwd=d, capture_output=True)
stop = False

def flip():
    targets = ["real.txt", os.path.join(d, "secret.txt")]
    i = 0
    while not stop:
        tmp = os.path.join(d, "data", ".tmp")
        try:
            os.unlink(tmp)
        except FileNotFoundError:
            pass
        os.symlink(targets[i & 1], tmp)
        os.replace(tmp, os.path.join(d, "data", "link"))
        i += 1

t = threading.Thread(target=flip)
t.start()
won = 0
try:
    for _ in range(RUNS):
        r = subprocess.run([VO, "run", "main.vo"], cwd=d, capture_output=True, text=True)
        won += r.stdout.count("SECRET")
finally:
    stop = True
    t.join()
    subprocess.run(["rm", "-rf", d])
print(f"race: {RUNS} runs, {won} reads outside the scope")
sys.exit(1 if won else 0)
