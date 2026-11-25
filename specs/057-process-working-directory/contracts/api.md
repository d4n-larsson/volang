# Contract: std.process with a working directory (vo 0.3.0)

## Functions

```vo
export fn run(program: str, arguments: list[str]) -> result[Output] uses process
export fn run_bytes(program: str, arguments: list[str]) -> result[BytesOutput] uses process
export fn run_with(program: str, arguments: list[str], input: str, vars: list[str], dir: str)
    -> result[Output] uses process, file.read
export fn run_with_bytes(program: str, arguments: list[str], input: bytes, vars: list[str], dir: str)
    -> result[BytesOutput] uses process, file.read
export fn run_lines[E: effects](program: str, arguments: list[str], input: str, vars: list[str], dir: str,
    f: fn(str) -> result[bool] uses E) -> result[Output] uses process, file.read, E
```

`run` and `run_bytes` start the program in the start directory and need no `file.read`. Each `vo doc`
entry keeps a runnable example; the `_with` forms' examples pass `"./"`.

## Results

| Situation | Result |
|---|---|
| program ran | `ok(Output)` (any exit code), unchanged |
| directory missing | `err("directory not found")` |
| directory is not a directory | `err("not a directory")` |
| directory cannot be entered | `err("directory not accessible")` |
| program missing | `err("not found")`, unchanged |
| directory outside the `file.read` scopes (`"./"` is always allowed) | R0010 (exit 70) |
| directory empty or holding NUL | R0007 |

## Diagnostics

| Code | When | Fix |
|---|---|---|
| E0214 (existing, new rows) | a call of `run_with`, `run_with_bytes` or `run_lines` with the 0.2 arguments | insert `, "./"` after the variables |
| E0501 (existing, fix changed) | the function lacks `file.read` (or another file effect) and every need for it is a literal target | add the effect scoped to those targets (data-model.md) |
| E0503 (existing, new targets) | a literal directory outside the declared `file.read` scopes | none (unchanged) |

E0214's message: "`std.process.run_with` takes a working directory after the variables since vo 0.3.0:
`"./"` is the start directory."

## Recording

A run event may carry `"dir": "<as written>"`; it is absent for `"./"`. Replay requires the same directory
(R0014 otherwise). Recordings made by vo 0.2 replay unchanged against programs migrated with `"./"`.

## Reference text (docs/reference.md, std.process paragraph)

"`run_with` adds input text, `NAME=value` variables and a working directory (inside a `file.read`
scope)."
