# release-check

Is this checkout ready to release? Runs three git checks (everything committed, no whitespace errors,
the last commit), prints a report, and writes it to `out/report.txt`, replacing the previous one. Exit
status 1 when a check fails, so a script can use it.

![What main is granted. It may run git, with the effect process("git"), and gets the exit code and the
output back, with no shell. It may write and delete out/report.txt, with file.write("out/") and
file.delete("out/"). Running any other program, such as make, is not granted and stops with
R0010.](diagram.svg)

## Run it

```sh
../../bin/vo run src/main.vo
../../bin/vo replay recordings/demo.jsonl      # a saved run: no git needed, nothing written
```

## How it works

### Running a program without a shell

`std.process.run("git", args)` runs git with its arguments as they are (no shell) and returns the exit
code and output; a non-zero exit is a value the program looks at, not an error.

<!-- code: src/main.vo fn git -->

### The grant names the program

`main` declares `process("git")`, so this program can start git and nothing else: `run("make", ...)`
would stop with R0010. A started program is not confined (git can do what you can), so grant the
programs you would run by hand.

Deleting is its own effect. Replacing the report needs `file.delete("out/")` besides
`file.write("out/")`; `vo.effects` shows both, so a reviewer sees that `main` may delete files under
`out/` and nowhere else.

<!-- code: src/main.vo fn main -->

### Logic stays testable

`report` is pure and tested without running anything (`vo test src/main.vo`).

<!-- code: src/main.vo fn report -->

### Record and replay

The saved run replays with the same output and exit status without git and without touching `out/`:
program outputs and file results come from the recording.
