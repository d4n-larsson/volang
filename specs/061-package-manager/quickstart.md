# Quickstart: Validating the Package Manager

Run from the repository root after `make`. The scenarios use the test git host
(`tests/git_server.py`, research R8), which prints its port; `$S` is `https://127.0.0.1:PORT` and
`--ca-file` with the test authority (as in `tests/cases/packages/fetch/steps`) goes with every fetching command.

```sh
python3 tests/git_server.py tests/cases/packages/graph/repos &   # report 2.1.0 → money 1.4.0 → pads 1.0.2
mkdir -p /tmp/vo061 && cd /tmp/vo061 && printf 'import report.table;\n\nfn main() uses console {\n    print(table.title());\n}\n' > main.vo
```

1. **Add with dependencies (US1)**: `vo package main.vo report $S/report` prints three `added` blocks
   and a total; `vo.packages` has three lines, two `indirect`; `vo run main.vo` prints the title.
2. **Selection (US2)**: in a second project add `ledger` (needs money 1.2.0) then `report`, and in a
   third the other order: `cmp` of the two manifests is silent, money is 1.4.0 although 1.6.0 exists.
   Adding `audit` (needs money 2.0.0) fails with E0707 naming both requirers; `git status` is clean.
3. **Fresh clone (US3)**: `rm -rf packages && vo package --restore main.vo` then `vo check main.vo`
   passes. Delete the `money` line by hand: `vo check` gives E0709 naming `--repair`; `vo package
   --repair main.vo` restores the line. Restart the server with `moved=money@1.4.0`: restore fails
   with E0701 for money.
4. **Updates (US4)**: `vo package --outdated main.vo` lists money 1.6.0 and 2.0.0; `vo package --update
   --dry-run main.vo money` shows the change and writes nothing; without `--dry-run` it moves to 1.6.0
   and shows `gains: net("rates.example.com")`.
5. **Remove (US6)**: `vo package --remove main.vo money` fails (required by report); `--remove main.vo
   report` removes all three.
6. **No network elsewhere (SC-004)**: stop the server; `strace -f -e trace=socket,connect vo check
   main.vo` shows no socket.
7. **Suites**: `python3 tests/package_git.py`, `python3 tests/package_graphs.py`, `make quick
   AREA=packages`, then `make test`.
