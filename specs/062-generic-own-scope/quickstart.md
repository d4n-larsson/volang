# Quickstart: A Generic Function's Own Scope

Prerequisite: `make` (builds `build/vo`).

## 1. The finding

```sh
mkdir -p /tmp/q062/a /tmp/q062/b && cd /tmp/q062
echo inside > a/x.txt; echo other > b/y.txt
```

`main.vo` is the program in the spec's Background (`g` and `h` declare `file.read("a/")`; `main`
declares `a/` and `b/`).

```sh
vo run main.vo
```

Expected: `inside other`, then R0010 naming `g` at the second call. On 0.7.0 the second line is
`other -` and R0010 comes only at `h`.

## 2. The cases

```sh
tests/run.sh tests/cases/effect_sets tests/cases/security      # generic_own_*, generic_scope_*
python3 tests/scope_fuzz.py --iterations 300 --seed 1          # the model against vo, with generics
```

Expected: all pass. The cases P1 to P5 and L1 to L3 of [research.md](research.md) R4 are
`generic_own_p1` … `generic_scope_l3`.

## 3. Nothing else moved

```sh
make test              # every existing case unchanged (SC-002)
make security-check    # the race and server scripts (SC-004)
python3 tests/speed.py --baseline <a build before the change> --new build/vo
```

Expected: no changed outputs; ratios as before. The gate (`make gate`) covers the three architectures,
the stress build and the RISC-V speed limit (SC-006).

## 4. Reading a refusal

`vo run --json main.vo` prints the R0010 record; `declared_by` is `g` and `allowed` is `["a/"]`. See
[contracts/call-path.md](contracts/call-path.md).
