# Quickstart: validating spec 017

Prerequisites: `make` (x86-64), and for the cross checks `make ARCH=aarch64` / `make ARCH=riscv64` with
qemu-user (`make check-tools`). Behavior is in [contracts/language.md](contracts/language.md); codes in
[contracts/codes.md](contracts/codes.md); events in [contracts/recording.md](contracts/recording.md).

## 1. Directories (US1)

```sh
mkdir -p /tmp/q17/data/old && cd /tmp/q17 && printf 'x' > data/b.txt && printf 'yy' > data/a.txt
cat > list.vo <<'EOF'
import std.fs.list_dir;

fn main() -> result[void] uses console, file.read("data/") {
    for e in try list_dir("data") {
        print(e.name);
    }
    return ok();
}
EOF
vo run list.vo            # a.txt, b.txt, old (byte order), the same on every run
```

Expected: three lines in that order; changing the scope to `file.read("other/")` gives E0503 at check
time; a computed path outside `data/` gives R0010 (exit 70).

## 2. Changing files (US2)

A program with `uses file.write("out/"), file.delete("out/")` appends to `out/log.txt` twice,
`make_dir("out/2026-11")`, renames a file inside `out/`, and deletes it: each step's listing printed.
Then: `delete("out")` is R0010 (the scope itself), renaming onto an existing name is
`err("already exists")`, deleting a non-empty directory is `err("not empty")`. The same program with
only `file.write("out/")` does not check: E0501 naming `file.delete`.

## 3. Running programs (US3)

```sh
cat > run.vo <<'EOF'
import std.process.run;

fn main() -> result[void] uses console, process("echo") {
    let out = try run("echo", ["a b", "$(rm -rf x)"]);
    write(out.stdout);
    print(str(out.code));
    return ok();
}
EOF
vo run run.vo             # "a b $(rm -rf x)" then 0: no shell
```

Then: `run("ls", ...)` with this grant is R0010; `uses process` alone is E0512; a script that sleeps 5 s
under `--timeout 1` gives R0009 and leaves no process (`pgrep -f` finds none); a script printing 17 MiB
gives `err("output is larger than 16 MiB")`.

## 4. Review (US4)

`vo effects --update` on a program, then add a `run("make", ...)` call with `process("git", "make")`:
`vo check` fails with E0504 naming the function, `process("make")` and the call.

## 5. Determinism and replay (US5)

```sh
vo run --record r.jsonl list.vo && rm -rf data && vo replay r.jsonl   # same output, no file read
python3 tests/cross_replay.py                                        # every platform pair identical
```

## 6. Gates

```sh
make test && make gate                 # includes the new cases in the determinism check
make instrumented                      # ASan/UBSan over the suite
python3 tests/fs_scale.py              # SC-004: 100,000 entries < 1 s; 1,000 runs < 5 s; no leaks
make ARCH=aarch64 test && make ARCH=riscv64 test
```

Record the figures in `specs/017-filesystem-processes/results.md`.
