#!/usr/bin/env python3
"""Spec 023 SC-003: `vo query type-at --json` with documentation answers in under 100 ms on a
2,000-line two-file program (median of 5), so an editor can ask on every hover. In `make gate`."""
import os, statistics, subprocess, sys, tempfile, time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.environ.get("VO_BIN", os.path.join(ROOT, "build", "vo"))


def main():
    with tempfile.TemporaryDirectory() as d:
        lib = []
        for i in range(330):  # 6 lines each: about 2,000 lines
            lib += [f"# Adds {i} to n, documented for the hover check.",
                    f"#     add{i}(1) is {i + 1}",
                    f"export fn add{i}(n: int) -> int {{",
                    f"    return n + {i};",
                    "}", ""]
        open(os.path.join(d, "lib.vo"), "w").write("\n".join(lib))
        imports = "".join(f"import lib.add{i};\n" for i in range(330))
        calls = "".join(f"    set t = add{i}(t);\n" for i in range(330))
        main_vo = imports + "\nfn main() uses console {\n    var t = 0;\n" + calls + "    print(str(t));\n}\n"
        open(os.path.join(d, "main.vo"), "w").write(main_vo)
        line = main_vo.split("\n").index("    set t = add165(t);") + 1
        subprocess.run([VO, "effects", "--update", "main.vo"], cwd=d, capture_output=True)
        times, out = [], ""
        for _ in range(5):
            t = time.monotonic()
            p = subprocess.run([VO, "query", "type-at", "--json", "main.vo", f"main.vo:{line}:13"], cwd=d,
                               capture_output=True, text=True)
            times.append(time.monotonic() - t)
            out = p.stdout
        ms = statistics.median(times) * 1000
        lines = len(main_vo.split("\n")) + len(lib)
        ok = '"doc":{' in out and "Adds 165" in out and ms < 100
        print(f"hover_speed: type-at with documentation on {lines} lines: {ms:.0f} ms (limit 100 ms)"
              + ("" if ok else f"\nFAIL {out[:300]}"))
        return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
