#!/usr/bin/env python3
"""Spec 017 (A4, SC-003): a process swapping a directory for a symbolic link while std.fs works must never
make an operation reach outside the scope. A helper flips `data/sub` between a real directory (inside)
and a link to `outside/` (outside) while a vo program appends, creates directories, renames, deletes and
lists below `data/sub` in a loop; exit 1 if anything in `outside/` was created, changed, deleted or
listed.

  fs_race.py [VO] [RUNS]   (RUNS default 300)
"""
import os, shutil, subprocess, sys, tempfile, threading

VO = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else
                     os.path.join(os.path.dirname(__file__), "..", "..", "build", "vo"))
RUNS = int(sys.argv[2]) if len(sys.argv) > 2 else 300
PROGRAM = """import std.fs;

fn main() uses console, file.read("data/"), file.write("data/"), file.delete("data/") {
    var i = 0;
    while i < 200 {
        match fs.append_file("data/sub/victim.txt", "x") {
            case ok() {
            }
            case err(_) {
            }
        }
        match fs.make_dir("data/sub/made") {
            case ok() {
            }
            case err(_) {
            }
        }
        match fs.rename("data/sub/keep.txt", "data/sub/moved.txt") {
            case ok() {
            }
            case err(_) {
            }
        }
        match fs.delete("data/sub/keep.txt") {
            case ok() {
            }
            case err(_) {
            }
        }
        match fs.list_dir("data/sub") {
            case ok(entries) {
                for e in entries {
                    if e.name == "SECRET" {
                        print("LISTED");
                    }
                }
            }
            case err(_) {
            }
        }
        set i = i + 1;
    }
}
"""
d = tempfile.mkdtemp()
os.makedirs(os.path.join(d, "data", "real"))
os.makedirs(os.path.join(d, "outside"))
for name in ("SECRET", "keep.txt", "victim.txt"):
    open(os.path.join(d, "outside", name), "w").write("untouched")
open(os.path.join(d, "main.vo"), "w").write(PROGRAM)
os.symlink("real", os.path.join(d, "data", "sub"))  # a link inside the scope: allowed, and flipped below
stop = False


def flip():
    targets = ["real", os.path.join(d, "outside")]
    i = 0
    while not stop:
        tmp = os.path.join(d, "data", ".tmp")
        try:
            os.unlink(tmp)
        except FileNotFoundError:
            pass
        os.symlink(targets[i & 1], tmp)
        os.replace(tmp, os.path.join(d, "data", "sub"))
        i += 1


def outside_state():
    o = os.path.join(d, "outside")
    return sorted((n, open(os.path.join(o, n)).read() if os.path.isfile(os.path.join(o, n)) else "dir")
                  for n in os.listdir(o))


before = outside_state()
t = threading.Thread(target=flip)
t.start()
listed = 0
try:
    for _ in range(RUNS):
        r = subprocess.run([VO, "run", "--timeout", "60", "main.vo"], cwd=d, capture_output=True, text=True)
        listed += r.stdout.count("LISTED")
finally:
    stop = True
    t.join()
after = outside_state()
ran = os.path.exists(os.path.join(d, "data", "real", "victim.txt"))  # the operations did work inside
shutil.rmtree(d)
changed = before != after or not ran
print(f"fs_race: {RUNS} runs, {listed} listings outside the scope, outside/ "
      f"{'CHANGED: ' + repr(after) if before != after else 'unchanged'}"
      f"{'' if ran else ', but nothing was appended inside the scope (the test did not run)'}")
sys.exit(1 if listed or changed else 0)
