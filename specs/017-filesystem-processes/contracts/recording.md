# Contract: Recording Events

Additions to `specs/006-query-edit-trace/contracts/recording.md`, schema 1 (event kinds added, none
changed; recordings made before this spec replay as before).

| kind | Fields |
|---|---|
| `info` | `path` (as the program passed it), `result` `"ok"` with `value` (`m`, `f<size>`, `d`, `o`), `"err"` with `value`, or `"denied"` |
| `list_dir` | `path`, `result` `"ok"` with `value` (the entries: see data-model.md), `"err"` with `value`, or `"denied"` |
| `append_file`, `make_dir`, `delete` | `path`, `result` `"ok"`, `"err"` with `value`, or `"denied"` (the appended text is not recorded) |
| `rename` | `path` (from), `to`, `result` `"ok"`, `"err"` with `value`, or `"denied"` |
| `run` | `program`, `args` (list of str), `result` `"ok"` with `value` (`<code>\n<stdout length>\n<stdout><stderr>`), `"err"` with `value`, or `"denied"` (input and environment are not recorded) |

```json
{"v":1,"kind":"list_dir","path":"notes","result":"ok","value":"f12 a.txt/d old"}
{"v":1,"kind":"run","program":"git","args":["status","--porcelain"],"result":"ok","value":"0\n3\n M a"}
{"v":1,"kind":"delete","path":"tmp/x","result":"err","value":"not found"}
```

## Replay

- Every path and program is checked against the grant first (R0010), lexically as for `read_file`
  (spec 013: replay never looks at the file system); a recorded `denied` repeats R0010.
- The next event must have the same kind and `path` (and `to` for `rename`; `program` and `args` for
  `run`); otherwise R0014, naming what the program asked for ("lists the directory `notes`", "runs
  `git status --porcelain`") and what the recording has next.
- Nothing is read, changed or started; the recorded result is returned.
- Mismatch texts (d_/n_ tables): `info` "looks at `$1`" / "a look at `$1`"; `list_dir` "lists the
  directory `$1`" / "a listing of `$1`"; `append_file` "appends to `$1`" / "an append to `$1`";
  `make_dir` "creates the directory `$1`" / "a directory created at `$1`"; `delete` "deletes `$1`" /
  "a delete of `$1`"; `rename` "renames `$1`" / "a rename of `$1`"; `run` "runs `$1`" / "a run of `$1`"
  (`$1` for `run`: the program and its arguments, space-separated).
