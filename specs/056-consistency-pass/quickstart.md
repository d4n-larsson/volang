# Quickstart: Consistency Pass

Validation scenarios for spec 056. The changes are listed in [contracts/api.md](contracts/api.md), and
the change table in [data-model.md](data-model.md).

## Prerequisites

Run `make` to build `build/vo`. The scripts need Python 3.

## 1. Old code migrates by fixes (US6, SC-002)

```sh
python3 tests/migrate_056.py
```

This copies each example and package as they were before this spec. It then applies the first fix of
every diagnostic (at most 3 rounds), checks the result, and runs the tests.

Expected:
- every program checks clean;
- every test result and output equals the recorded one;
- the report lists the E0214 rows each program needed.

The v1 corpus migration is checked too: `python3 tests/migrate.py` must still report 61 of 61.

## 2. Imports never clash (US1, SC-001)

```sh
python3 tests/std_names.py
```

Expected:
- no exported name is used by two modules;
- a generated file importing every exported item checks without E0202.

## 3. One diagnostic per change (SC-003)

`vo check --json` on `tests/cases/library/*.vo` gives one E0214 per old form, each with the fix of
contracts/api.md. The fixed files check clean (`tests/apply_fixes.py`). `python3 tests/libchange_rows.py`
generates a program for every row of the change table and requires the same.

## 4. Lookups give Option (US2, SC-004)

`tests/cases/library/lookups.vo` exercises `response_header`, `key_after` on an empty map, `index_of` and
`first_match`, showing `none` and `some`. Doc conformance shows that no std function returns
`err("none")`.

## 5. Order (US4, SC-005)

`tests/cases/run/order_bool_variant.vo` sorts these, giving the same output on x86-64, ARM64 and RISC-V:
- `[true, false, true]`;
- a variant `Level { low; mid; high; }`;
- `Shape` values with payloads;
- the keys of a map inside `fn sorted_keys[K: key]`.

## 6. Builtins are not hidden (US3)

`tests/cases/check/shadow_builtin.vo` has a parameter `keys`, a local `len` and a binding `min`. Expected:
each is E0203 with a fix to `keys_value` and so on, and the file checks clean after the fixes.

## 7. Unchanged

`make test` passes, and the reference token check stays at or under 15,000. The audit file marks every
section 3 item fixed, declined or deferred.
