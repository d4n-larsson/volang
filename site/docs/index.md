# vo

vo is a small, statically typed programming language designed for AI coding agents, and for the people
who review what those agents write.

Agents write code in one shot, read the compiler's output and fix what it reports, often with nobody
watching. vo is built for that loop: every diagnostic is machine-readable and comes with a fix, every
function's type says what it may touch (files, network, environment), runs are deterministic, and the
whole language fits in one reference an agent can read in full.

```vo
fn greeting(name: str) -> str {
    return "hello, " + name;
}

fn main() uses console {
    print(greeting("vo"));
}
```

```output
hello, vo
```

## Where to start

- **[Getting started](start/install.md)**: build the toolchain and write, run and test a first program.
- **[Language guide](guide/basics.md)**: every feature explained with worked examples.
- **[Toolchain](tools/check.md)**: every command, its options and its output.
- **[Reference](reference/index.md)**: the complete language reference, the [error catalog](errors/index.md)
  and the [standard library](std/index.md), generated from the toolchain itself.

## For agents

Every page is also available as plain Markdown: add `.md` to its address. [`llms.txt`](llms.txt) lists
every page with a one-line summary, and [`llms-full.txt`](llms-full.txt) has all of them in one file.
