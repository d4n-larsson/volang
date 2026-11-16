# Contract: Pages for Agents

## `/llms.txt` (llmstxt.org format)

```text
# vo

> vo is a small, statically typed programming language for AI coding agents and the people who review
> their code: checked effects, machine-readable diagnostics with fixes, deterministic runs. Version
> <vo version> (<commit>).

## Getting started
- [Install](start/install.md): build the toolchain on Linux x86-64, ARM64 or RISC-V.
...
## Language guide
- [Effects](guide/effects.md): ...
## Toolchain guide
## Reference
## Error catalog
## Standard library
## Examples
```

Every page appears once, with its title and its summary sentence, linking to its Markdown copy.

## Markdown copies

`<address>.md` for every page (`/guide/effects/` → `/guide/effects.md`): the page's assembled Markdown,
without navigation, scripts or styling; links point to other pages' `.md` copies.

## `/llms-full.txt`

The version line, then every page's Markdown in navigation order, each starting with its address as a
comment line (`<!-- /guide/effects/ -->`).
