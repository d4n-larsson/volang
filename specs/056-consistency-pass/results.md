# Results: Consistency Pass

Implemented 2026-11-25 as vo 0.2.0. Measured with `build/vo` (x86-64) unless said otherwise.

## SC-001

`tests/std_names.py`: 365 exported names in 23 modules, no name exported by two modules, and a program
importing one item of each name checks with no E0202.

## SC-002

`tests/migrate_056.py`: the 11 programs of 8c8db3e (every example and the postgres package) migrate by
the fixes alone and check clean; their test results and replays equal the baseline except one intended
change (`examples/expenses` compares `parse_time`'s old text, CHANGED in the script, T027). E0214 rows
used: `collections.key_at_or_below`, `metrics.add`, `sqlite.get_int`, `get_str`, `json.field`,
`security.Browser.lifetime_s`, `web.clock_ms`, `tcp.close`, `describe`, `send`; E0203 in two examples.
`tests/migrate.py`: 61 of 61 v1 programs, with E0214, E0203 and E0105 (spelling) among the migration
codes and up to three rounds.

## SC-003

`tests/libchange_rows.py`: 49 rows of `src/libchange.c` (11 kinds of rename, remove, option, swap,
field and case), each with a generated program using the old form: every one reports E0214 and checks
clean after its fix; none skipped. Golden cases per kind of fix: `library/rename_import`,
`rename_value`, `rename_both`, `own_name` (rename), `option_lookups` and `lookups` (option),
`worker_timed_out` (case), `units` (rename and field with a factor), `regex/*` (swap), and the removals
through the migrated `time/*` and `web/ready_steps` cases.

## SC-004

`tests/doc_conformance.py` checks every std example (365 items, 0 failures) and fails on any
`err("none")` in std; the FR-005 lookups' signatures return `Option` (`library/option_lookups`).

## SC-005

`run/order_bool_variant` (sorts `bool`s, an enum-like variant, a variant with payloads, and the keys
of a generic `map[K: key, V]`) gives identical output on x86-64, ARM64 and RISC-V (under qemu) and the
instrumented build, as do `library/*`, `lex/number_spellings`, `check/case_parens` and
`run/empty_list_context`; `tests/migrate_056.py --quick` gives 10 programs, 0 problems on each of the three (T040).

## SC-006

`docs/review-2026-11/inconsistencies.md`: every item of section 3 (3.1 to 3.15) ends with what spec
056 did: fixed, declined or deferred, with the reason.

## SC-007

`tests/ref_examples.py`: ~14,990 tokens of 15,000.

## Reference cost

~14,987 tokens before spec 056. The additions (ordered `bool` and variants, `[]` from context, the
`parse_`/`from_` rule, the `timeout_ms` range, literal spellings, E0214 in the code ranges, renamed
names) took it to ~15,052. Moved out, all already in the guide: std.json's `from_json` shapes,
`browser_routes`, the cross-site 403 and recordings holding passwords (`apis.md`). After: ~14,990.

## Deviations

- One table (`src/libchange.c`) gives E0214 for every kind of change, where the plan had two
  mechanisms (renames at the import, retyping at the call).
- Names chosen during implementation: `tcp.send` → `write_all` (`write` is a builtin), `sqlite.get` →
  `value_column` (`column` clashed with parameter names), `replace_all` → `replace_matches` (`replace`
  is `std.text`'s), `http.header` → `response_header` (beside `request_header`).
- `lists.index_of` keeps `-1` like the builtin `find` (FR-005 listed it; the spec's own requirement
  text kept it).
- `workers.with_deadline` clamps a limit outside 0 to 600,000 instead of returning an error: its result
  type has no error case for it.
- std's own locals and parameters that shadowed builtins were renamed (`argv` → `arguments`, `rows` →
  `table`, `env` → `vars`, ...); a parameter name is not part of a call, so no program sees it.
- T023: `fixed_zone` became `offset_zone` (ms): a unit change keeping the name could not be reported by
  `vo check`, so the old name reports E0214 and the fix multiplies by 60000. A non-literal argument becomes
  `e * 60000` (or `(e) * 60000`), not `minutes(e)`: `minutes` may not be imported, or may be a local's name.
- T023: positional parameters keep seconds (`issue_token` and `sign_in_route`'s `lifetime_s`,
  `safe_headers`' `hsts_s`, renamed from `hsts_seconds`): a call passes them by position, so a changed unit
  would migrate silently wrong. Fields (`Cookie.max_age_ms`, `Browser.lifetime_ms`) changed through E0214.
  A read of a changed field is fixed to `(x.max_age_ms / 1000)` so the program's arithmetic keeps its unit.
- T023: the tz records' fields: `starts_s`, `offsets_s`, `std_offset_s`, `dst_offset_s`, `RuleDay.time_s`.
- T024: `float_column` INTEGER conversion is covered by `sqlite/getters` (no separate `float_column_int` case); `sqlite/open_fails` is the open-failure case. Classified errors keep their kind (`constraint: `, `busy: `, `readonly: `, ...); only the generic `error: ` went.
- T026: the error texts follow `tcp_error_text`'s existing style (lowercase, no closing period) so they compose (`"connect: " + text`); tasks.md said "ending with a period". The new functions are at the end of their files so std line numbers in traces stay put.
- T027 changed texts (for the CHANGELOG): `parse_time` → `invalid time: S; DETAIL`; `bytes.from_hex` →
  `invalid hex: S; not hex at position N` / `...; odd number of hex digits`; `from_base64` →
  `invalid base64: S; not base64 at position N` / `...; length is not a multiple of 4`; `parse_json` (and
  everything reading JSON through it) → `invalid json at line L, column C: DETAIL`; `parse_regex` →
  `invalid regex at byte N: DETAIL`; `parse_sql` → `invalid sql: DETAIL` (SQLite gives no position);
  web's body binding reason drops its `not JSON: ` (the json text says it). int, float, decimal and
  `bits.parse_hex` already had the form. sqlite: no `error: ` prefix (T024). `float_column` converts an
  INTEGER; `no column `NAME``/`ambiguous column `NAME`` have backticks.
- T027: the input is quoted whole, as `parse_int` does (a `ponytail:` note in std/bytes.vo).
- T029: three files of the frozen v1 corpus had been rewritten by the builtin-shadowing migration of the repository; restored, they now migrate through E0203's fix.
- T033: `vo fmt` adds `uses` where it can see the argument names effects: an effect parameter of the item
  or an `effects` set of the file (an imported set stays as written; the checker accepts both). std got only
  the `uses` rewrites (std was not canonical before, and a full format would move std line numbers in
  traces); examples and the postgres package were formatted whole.
- T033 (not in tasks): the postgres package mirrors std.sqlite (its parity test), so its readers were
  renamed the same way (`value_column`, `int_column`, `float_column` converting an integer, `text_column`,
  `bytes_column`, and `time_column` for its `get_time`), with backticked column names. The E0214 table
  matches std items only, so the package's users get E0201 with the closest-name fix where one is close.
  Its private `index_of` moved to `values.vo` as `column_index` (`client.vo` stays under 400 lines).
- T032: `vo fmt` cases fed old spellings (`0010`, `1.5E-3`, `2e+2`, `007`) as input; those are now lex
  errors, so the inputs use the one spelling. `E0105` joined tests/migrate.py's MIGRATION (a v1 file has `007`).
- T034: only diagnostics follow the style; error values a program sees (std.web problem reasons such as `expected integer, got "x"`, `workers must be 1 to 64, got N`) keep their text.
- T035 (see Reference cost): ~14,987 tokens before spec 056; the additions (ordered `bool` and variants, `[]` from context, the `parse_`/`from_` rule, `timeout_ms` range, literal spellings, E0214 in the ranges, renamed names) took it to ~15,052; moved to the site (already there): std.json's `from_json` shapes, `browser_routes`, the cross-site 403 and recordings holding passwords (apis.md); after: ~14,990 of 15,000.
- T041: `make test` found the benchmark's vo programs (bench2 references and the refactoring tasks' starters, 28 files) using `args`, `env` and `keys` as locals; migrated with E0203's fix alone (`tools/vo_fix.py --only E0203`), and `tests/gc/baseline.py` gives 731 of 731 reference runs unchanged.
- T041: `make test` passes (golden cases, steps, every script in the list, the examples, `diff_engines.py`:
  1000 programs, 0 disagreements). On the way: `tests/strings.py` generated literals in the old spellings
  (`1E9`, `1e+9`, and ones rounding to zero) and now writes the one spelling; `tests/sqlite_scope.py`
  expects `parse_sql`'s `invalid sql: ` text. `tests/iteration.py`'s speed comparison failed twice on a
  loaded machine and passed in the final run (repeated timings: keys form ~84 ms, two-variable form
  ~77 ms, medians of five).
