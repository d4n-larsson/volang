# Quickstart: Validate vo-lang v2

Prerequisites, build and the full test gate are as in `specs/001-vo-lang-core/quickstart.md` (`make`, then `make test`). The v2 gate adds `tests/migrate.py` and `tests/scope_fuzz.py`.

## US1: Effects are part of every function's type

```sh
bin/vo check --json tests/cases/effects/missing_effect.vo
```
Expect E0501 naming the function and `console`, with a fix inserting ` uses console`. Applying the fix makes `check` exit 0.

```sh
bin/vo run tests/cases/effects/scope_escape.vo
```
`main` declares `file.write("out/")` and the program writes `../x`. Expect R0010 with `target` and `allowed`, and exit status 70.

## US2: Review capability changes

```sh
cp -r tests/cases/effects/record /tmp/p && cd /tmp/p && rm vo.effects   # the case ships an outdated record
vo effects --update main.vo            # creates vo.effects (vo = the repo's bin/vo)
cp vo.effects vo.effects.before
# edit report.vo: save_report now also prints (add `print(...)` and `console` to its uses)
vo check --json main.vo                # E0504 for save_report, with a note on the call that brought it in
vo effects --update main.vo            # prints exactly that change: `+ report.vo:save_report: console`
diff vo.effects.before vo.effects      # only save_report's line differs
```

The 21 recorded change sets in `tests/effect_changes/` do this automatically: `python3 tests/record_changes.py` (SC-004).

## US3: Functions as values

```sh
bin/vo run tests/cases/func/sort_by_field.vo    # records sorted by age, stable on ties
bin/vo check --json tests/cases/func/capture_var.vo   # E0505 with the copy-to-let fix
```

## US4: Generics

```sh
bin/vo run tests/cases/generic/stack.vo          # Stack[int] and Stack[str] in one program
bin/vo check --json tests/cases/generic/needs_ordered.vo   # E0311 with the `: ordered` fix
```

## US5: Migration

```sh
python3 tests/migrate.py    # every v1 golden program and benchmark solution: one round of fixes, identical output
```

## US6: Benchmark v2

```sh
bench2/run_agent.sh --lang vo
bench2/run_agent.sh --lang python
python3 bench2/summarize.py            # SC-001 ceiling check, SC-002 comparison, SC-003 capability detection
bench2/run_agent.sh --lang vo --set features && python3 bench2/summarize.py --set features   # SC-006
```

## Reference budget

```sh
python3 tests/ref_examples.py          # the whole reference, v1 plus v2, at most 15,000 tokens; every v2 construct has a runnable example
```
