# Quickstart: validating `vo lsp`

```sh
make
python3 tests/lsp.py                  # every feature against its command, robustness (in make test)
python3 tests/lsp.py --session 5000   # SC-004: memory over a long session
python3 tests/lsp.py --time           # SC-003: diagnostics and hover latency
python3 tests/mcp.py                  # the shared code moved out of src/mcp.c
tests/run.sh --stress tests/cases/run/oom_small.vo tests/cases/map_keys   # no new static buffers
VO_BIN=build-aarch64/vo python3 tests/lsp.py
VO_BIN=build-riscv64/vo python3 tests/lsp.py
```

## By hand in an editor (SC-005)

With `vo` on the PATH, follow `site/docs/tools/editors.md` for one editor, open
`examples/expenses/main.vo`, and check: a deliberate type error is underlined with its code and offers
its fix; hover on a function shows its signature and comment; rename of a function changes the other
files; format restores canonical form. See [the contract](contracts/lsp.md) for what is offered.
