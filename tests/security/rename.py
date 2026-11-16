#!/usr/bin/env python3
"""Spec 013 (H3): renaming a running program's working directory and leaving a decoy at the old
path must not redirect its scoped reads outside the scope. RUNS times: start a program reading
`data/x` in a loop, move its directory, put a decoy `data/x` at the old path and make the real
`data/x` a link to the secret; exit 1 if the secret is ever read.

  rename.py [VO] [RUNS]   (RUNS default 1000)
"""
import os, subprocess, sys, tempfile, time

VO = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else
                     os.path.join(os.path.dirname(__file__), "..", "..", "build", "vo"))
RUNS = int(sys.argv[2]) if len(sys.argv) > 2 else 1000
PROGRAM = """fn main() uses console, file.read("data/") {
    var i = 0;
    while i < 400000 {
        match read_file("data/x") {
            case ok(s) {
                if s == "TOPSECRET" {
                    print("SECRET");
                    return;
                }
            }
            case err(e) {
            }
        }
        set i = i + 1;
    }
}
"""
won = 0
for run in range(RUNS):
    top = tempfile.mkdtemp()
    proj = os.path.join(top, "proj")
    os.makedirs(os.path.join(proj, "data"))
    open(os.path.join(proj, "data", "x"), "w").write("benign")
    open(os.path.join(top, "secret.txt"), "w").write("TOPSECRET")
    open(os.path.join(proj, "main.vo"), "w").write(PROGRAM)
    subprocess.run([VO, "effects", "--update", "main.vo"], cwd=proj, capture_output=True)
    p = subprocess.Popen([VO, "run", "--timeout", "30", "main.vo"], cwd=proj, stdout=subprocess.PIPE,
                         stderr=subprocess.DEVNULL, text=True)
    time.sleep(0.02)
    real = os.path.join(top, "proj.real")
    os.rename(proj, real)  # the program's working directory moves away
    os.makedirs(os.path.join(proj, "data"))
    open(os.path.join(proj, "data", "x"), "w").write("decoy")  # what the old path text resolves to
    os.symlink(os.path.join(top, "secret.txt"), os.path.join(real, "data", "x.new"))
    os.replace(os.path.join(real, "data", "x.new"), os.path.join(real, "data", "x"))
    try:
        out, _ = p.communicate(timeout=60)
    except subprocess.TimeoutExpired:
        p.kill()
        out, _ = p.communicate()
    won += out.count("SECRET")
    subprocess.run(["rm", "-rf", top])
print(f"rename: {RUNS} runs, {won} reads outside the scope")
sys.exit(1 if won else 0)
