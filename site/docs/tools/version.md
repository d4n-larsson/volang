# vo version

`vo version` prints the toolchain's version and the version of its JSON output schema.

## Synopsis

```text
vo version [--json]
```

The version is a semantic version (`0.1.0`). The schema version is the `"v"` of every JSON record the
toolchain prints; it is 1, and within version 1 fields and record kinds are only ever added, never removed
or renamed (see [JSON output](json-output.md#versions)).

## Options

| Option | Meaning |
|---|---|
| `--json` | Print one `version` record. |

## Output

Text: `vo VERSION schema N`. JSON: `{"v":1,"kind":"version","version":"0.1.0","schema":1}`.

## Exit statuses

| Status | When |
|---|---|
| 0 | Printed. |
| 64 | Usage error, such as an unknown option ([T0001](../errors/T0001.md)). |

## Example

```vo
fn main() uses console {
    print("hello");
}
```

```console
$ vo version
vo 0.1.0 schema 1
$ vo version --json
{"v":1,"kind":"version","version":"0.1.0","schema":1}
```

Include the output of `vo version` when you report a bug, with the command and the input that caused it.
