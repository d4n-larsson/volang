# Inconsistencies in vo (audit of 2026-11-25)

The language, the standard library, the diagnostics and the command line were audited for
inconsistencies after spec 055. Every finding was checked against `build/vo` 0.1.0 or the source lines
cited, and line numbers are those of that day. Items marked **Fixed** were fixed afterwards.

- **Bug:** the docs and the toolchain disagree, or a rule is broken.
- **Doc:** the reference or the catalog is wrong or contradicts itself.
- **Design:** the language or library does one thing two ways and needs a decision.

## 1. Bugs that change what a program does

### 1.1 Loops do not take a consistent snapshot (bug)

**Fixed:** `for x in list` runs as `for _, x in list` (`run/loop_snapshot`); the reference states the rule per collection kind.

`docs/reference.md:383` says a loop sees "what was there when the loop began; additions are not
visited". Line 909 says a `for` over a collection "copies it first".

```vo
let a = [1, 2];
for x in a { if x < 4 { push(a, x + 2); } write(str(x) + " "); }   # 1 2 3 4 5
for _, x in b { ... }                                               # 1 2
```

The one-variable loop visits added elements, so `push` in the body can loop forever. The two-variable
form reads the length once. Across collections:
- `list`, `map` and `ordered_map` are read live: a changed value shows, a removed key is skipped;
- `deque` and `ordered_set` are copied first: they show the old value and visit removed items.

Source: `src/compile.c`, `cfor` (about lines 489–520):
- the one-variable list path takes no snapshot and no length;
- the two-variable path reads `OPC_LEN` once;
- `TY_IS_COLL` types get `OPC_SNAPSHOT`.

### 1.2 `==` is not always by contents (bug)

**Fixed:** only strings and bytes keep the shortcut (`run/eq_by_contents`).

Line 455 says equality is by contents, and that `nan != nan`.

```vo
let xs = [0.0 / 0.0];
let ys = slice(xs, 0, 1);
print(str(xs == xs) + " " + str(xs == ys));   # true false
```

Records behave the same way. The cause is the identity shortcut
`if (type->kind != TY_FLOAT && a == b) return 1;` at `src/interp.c:325`.

### 1.3 Functions can be compared with `==` after all (bug or design)

**Fixed:** `==`, `!=` and `expect_eq` on a type holding a function are E0308 (`check/eq_holds_function`); a generic function given functions stops with R0007.

Line 513 says functions cannot be compared with `==`, and `id == inc` written directly is E0308. All of
these are accepted, though:
- comparing records with a function field;
- `[id] == [id]`;
- a generic `fn same[T](a: T, b: T) -> bool { return a == b; }` called with functions;
- `expect_eq(f, f)`.

The comparison is by identity, so two lambdas with the same code are unequal. The cause is the
`default:` branch (`a == b`) of `values_equal` in `src/interp.c:320`.

### 1.4 A capitalized name breaks parsing before E0207 can fix it (bug)

**Fixed:** such names parse as names, and a lower-case effect set as a set (`check/naming_uses`). Found on the way: E0509's list of effects lacked `entropy` and `log` (fixed).

Line 30 says a wrongly cased name is E0207, with the converted name as its fix. Once the name is used,
parsing fails first:
- `let Total = 3; print(str(Total));` is E0110 "Expected `.`, found `)`";
- `fn Main()` called as `Main();` is E0110;
- `effects storage = console;` is E0110 "Expected `fn`, `record`, ...";
- `fn id[t](x: t)` is accepted at `[t]`, but `t` then fails as a type.

E0207 and its fix appear only when the name is never used.

### 1.5 A generic function cannot sort a map's keys (bug or design)

**Partly fixed:** E0311 no longer offers the fix that does not parse (`generic/order_bounded`). Open: two bounds on a parameter, or ordered `bool` and variants.

`src/parse.c:1162` takes one bound per type parameter, and `key` and `ordered` overlap without nesting:
`ordered` includes `float`, which is not a key, and `key` includes `bool` and variants, which have no
order. So:
- `fn sorted_keys[K: key](m: map[K, int])` calling `sort(keys(m))` is E0311;
- `[K: ordered]` is E0309, and so is `[K: ordered, key]`;
- `new ordered_map[K, int]` with `K: key` is E0313.

E0311's fix inserts `: ordered` and gives `[K: ordered: key]`, which is a parse error (E0110).

### 1.6 Float text (bug)

**Fixed in the reference:** plain decimal is deliberate (`str` gives the fewest digits, without an exponent; `nan`, `inf`, `-inf` are not read back); `parse_float` reads an exponent since spec 042.

- `str(1e300)` prints 302 characters, not the "shortest text" that lines 61 and 873 promise.
- `str(1.0 / 0.0)` is `"inf"`, which `parse_float` rejects.
- `parse_float("1e9")` succeeds, though line 878 allows only digits and `.`.

### 1.7 The command line

**Fixed:** all five. The summary goes only with `--json`, also for `run`; options given to the wrong command are T0001; `vo check` reports E0409; E0408 names `vo build` too; a built executable's errors use `--vo-` names. `./app --vo-effects --vo-seed 3` still ignores the seed.

- **Summary lines.** A missing input prints a raw JSON summary in human mode. `vo check nope.vo` prints
  `error[T0002]: ...` and then `{"v":1,"kind":"summary",...}`; so do `query`, `impact`, `edit`, `doc`,
  `test`, `fmt` and `effects`. `vo run --json nope.vo` prints no summary, although `reference.md:1404`
  says `run` emits one when it stops before the program starts. Source: `src/main.c:194-198`.
- **Options silently ignored.** These exit 0 and ignore the option: `vo check --timeout 5`, `--seed 3`,
  `--filter x`, `--max-memory 5`; `vo run --check`, `--update`, `--filter x`; `vo explain --mem-stats`;
  `vo test --check`. Meanwhile `vo test --workers 2`, `vo run --list` and `vo check --log-level info` are
  T0001. The catalog's T0001 says an option given to the wrong command is a usage error. Source:
  `src/main.c:272-336`; `opt_spec006_check` never restricts these.
- **E0409 is never reported by `vo check`.** `fn main(name: str)` checks clean, while `run` and `build`
  fail with 65. The catalog does not say `check` skips it (it says so for E0408).
- **E0408 names the wrong command.** `vo build` prints "`vo run` needs a `fn main()`."
- **A built program names the toolchain's options.** Its errors use the toolchain's spelling, not its own
  `--vo-` names:
  - `./app --vo-timeout x` prints "`--timeout` needs a whole number of seconds.";
  - `./app --vo-seed -1` prints "`--seed` and `--clock-ms` need a whole number.";
  - `./app --vo-replay r --vo-workers 2` prints "`--workers` only works with `run`.".

  Only a missing value uses the app's spelling ("`--vo-seed` needs a value.", `src/build.c:183`).
  `./app --vo-effects --vo-seed 3` silently ignores the seed (`src/build.c:192-195`).

## 2. Docs that disagree with the toolchain

**Fixed** (all but the example `test` record, which still lacks `id`, `uses` and `cause`). Each was
fixed in the docs to match the toolchain:
- the exit-status table names every T code;
- one statement of the ordered types (including `bytes` and records), to which `min`, `max`, `sort_by`
  and `T: ordered` refer;
- `const` and `effects` are only reserved where they start an item, and constants and effect sets can be
  imported;
- `_` as a `for` variable;
- what an anonymous function cannot have;
- `"cannot read PATH"` for reading a file;
- the catalog: E0111 covers only `{`, the W0501 and W0602 examples, T0001's list, the code ranges,
  E0506 before E0507;
- the JSON kinds and summaries, `--json` with `mcp` and `lsp`, and the `doc` and `package` synopses.

To stay under the token budget, the reference's formatting and mutation details moved to
`site/docs/tools/fmt.md` and the guide.

- **Exit statuses** (`reference.md:1336`). The table says 64 is "usage error (T0001)" and 65 is
  "check errors". But:
  - 64 is also T0004, T0005 and T0007 (`query.c:150`), T0008 (`replay.c:933`), T0010 (`doc.c:725`),
    T0011 (`testrun.c:275,288`) and T0013 (`tls.c:152`);
  - 65 is also T0006 (`edit.c:107`);
  - 66 is T0014;
  - 1 is T0012 (`main.c:668`).
- **What "ordered" means.** The reference says it four different ways:
  - line 454: `<` works on int, float and str only;
  - lines 892, 895 and 518: `min`, `max` and `sort`/`sort_by` keys take int, float or str;
  - line 590: `T: ordered` is int, float, str or a record, without `bytes`;
  - line 64: `bytes` supports `<`.

  The compiler accepts `<`, `sort` and `max` on records and on `bytes`, and `bytes` as a `T: ordered`
  and an `ordered_map` key. It rejects `bool` and variants (`Color.red < Color.green` is E0308), though
  both can be map keys.
- **Keywords** (line 36). `const` and `effects` are not reserved (`let const = 1;`, `fn const()` and
  `let effects = 2;` check). Neither are the collection type names: `fn deque()` and
  `let ordered_map = 1` check, and `new deque[int]` still works beside `fn deque()`. Line 1113 says only
  `fn`, `record` and `variant` can be imported, but an exported `const` and an `effects` set import fine.
- **`_` in loops.** Line 31 says `_` is not allowed as a `for` variable. Lines 371 and 383 show
  `for _ in 0..n` and `for _, v in m`, and both run.
- **Anonymous functions.** Line 506 says they are "the same syntax as `fn`, without a name". `uses` is
  E0507, which is documented. `requires`, `ensures` and type parameters (`fn[T](...)`) are rejected with
  a bare E0110 "Expected `{`", which is not documented.
- **Missing files.** Line 1182 says file operations fail with `"not found"`. `read_file`, `fs.read_bytes`
  (`fs.vo:122`) and `fs.each_line`/`each_chunk` (`fs.vo:166,192`) give `cannot read PATH`; `list_dir`,
  `delete`, `rename` and `process.run` give `not found`.
- **Catalog.**
  - E0111 says "A `(`, `[` or `{` is never closed", but only a missing `}` is E0111 (`parse.c:1542`).
    `print("a"` at the end of a file is E0110 "Expected `)`, found the end of the file."
  - The W0501 and W0602 examples also produce E0501 and exit 65, though warnings "never fail a command".
  - T0001's text (`errors.md:4907`) says `--record` goes only with `run` and `--diff` only with `impact`;
    both also work with `test`.
  - `reference.md:1410` lists E01xx as E0101 to E0120, but E0121 and E0122 exist. E05xx and E06xx are not
    listed. W07xx are structure warnings while E07xx are packages. E0507 is catalogued before E0506.
- **JSON output.**
  - `vo effects --json` emits `kind:"capabilities"` and `vo build --json` emits `kind:"build"`; neither
    kind is in `reference.md:1406`.
  - A summary is documented only for check, test, fmt and run, but effects, query, impact, edit, doc and
    build emit one too (build only on failure).
  - The example `test` record has no `id`, `uses` or `cause`.
  - "`--json` works with every command", but `vo mcp --json` and `vo lsp --json` are T0001.
  - `vo explain --json` gives `"fixable"` as a boolean, so "sometimes" becomes `true` (`data.c:40-47`).
- **Synopsis.**
  - `vo doc [ENTRY] [NAME]` suggests `vo doc main.vo` works; it is T0010.
  - `vo package` accepts `--dir` and `--ca-file`, which neither the synopsis nor the unknown-option list
    (`main.c:342`) shows.
  - The synopsis says `--log-level L`; the error message says `LEVEL`.

## 3. Design decisions to make

### 3.1 Same names in two modules, and no import aliases

These pairs cannot be imported together (E0202 "already defined"):

| Name | Modules |
|---|---|
| `contains` | lists, text |
| `field` | json, log |
| `close` | sqlite, tcp |
| `describe` | tcp, security |
| `add` | decimal, metrics |
| `get`, `get_bytes` | http, sqlite |
| `send` | http, tcp |

Logging a JSON field, or a list check beside a text check, needs a workaround. Either rename one side of
each pair or add `import ... as`.

**Fixed (spec 056):** one side of each pair renamed (`has_item`, `member`, `close_conn`, `tcp_error_text`, `add_to`, `value_column` and the typed readers, `write_all`), through E0214. **Declined:** `import ... as` (decided with the user).

### 3.2 "Nothing found" is reported three ways

`std/option.vo:3-4` says to use `Option` when "nothing there" is a normal answer. In practice:
- `Option`: `web.read_cookie` and `security.current_user`;
- `result`: `http.header` and `request_header` (`err("no header ...")`), and the collections' neighbour
  functions such as `key_after` (`err("none")`);
- `-1`: the builtin `find` and `lists.index_of`.

"Empty" is worded two ways: `pop` and the collections say `err("empty")`; `lists.min_by`/`max_by` and
`random.pick` say `err("empty list")`.

**Fixed (spec 056):** the lookups that may find nothing return `Option` (`http.response_header`, `request_header`, `regex.first_match`, the collections' first/last and neighbour functions), with E0214's fix wrapping old uses in `to_result`. **Declined:** `lists.index_of` keeps `-1` like the builtin `find`; taking an element (`pop`) stays a `result`. **Deferred:** the wording `"empty"` vs `"empty list"` (an error value a program may compare; no diagnostic could migrate it).

### 3.3 Builtins can be shadowed, your own names cannot

`fn len` at top level is E0202, and a local named like a constant, your own function or an imported std
function is E0203. But `let len = 7;`, `let print = 1;` and a parameter named `keys` are accepted; later
calls then fail with "`len` is not a function" (E0303).

**Fixed (spec 056):** a local or parameter named like a builtin is E0203, with a fix renaming it to `NAME_value` (`check/shadow_builtin`).

### 3.4 Expected types and type arguments

- `[]` never takes its type from context: `f([])` and `let xs: list[int] = [];` are E0306. Yet the fix
  knows the type, and `ok(1)`, `Tree.leaf` and `todo()` all take theirs from context.
- Explicit type arguments work only after `new` (`new Pair[B, A] {...}`): `Tree[int].leaf` and
  `mk[int]()` are E0110.

**Fixed (spec 056):** `[]` takes its type from the context (`run/empty_list_context`); with none it is E0306. **Deferred:** explicit type arguments on calls and variant cases (`mk[int]()`): new syntax.

### 3.5 Effect arguments have two spellings

`Db[D]` and `Db[uses D]` both check, and `vo fmt` keeps both. A named effect set must be written with
`uses`, though: `Db[Out]` with `effects Out = console;` is E0327.

**Fixed (spec 056):** `Db[Out]` with a named set checks, and `vo fmt` writes every effect argument it can identify with `uses` (`fmt/effect_arg_uses`, `effect_sets/named_set_arg`).

### 3.6 Cases without a value

`ok()` and `case ok()` take parentheses; `Shape.empty` and `case empty` do not. And the wrong form is
handled unevenly:
- `S.empty()` is a parse error with no fix;
- `case empty()` is E0411 with a fix;
- `case ok` is only W0402.

**Fixed (spec 056):** `S.empty()` is E0411 with a fix removing `()` (`check/case_parens`). **Declined:** `case ok` on a `result[void]` stays W0402 (it matches correctly; the warning's fix gives the one spelling).

### 3.7 Numeric literals

- `0X1F` is E0105, but `1E9`, `1e+9` and `00012` are accepted (and rewritten by `vo fmt`).
- `1e400` is E0105, but `1e-400` silently becomes `0.0`.
- `-9223372036854775808` is E0105, but `-0x8000000000000000` checks and fails at run time with R0001.

**Fixed (spec 056):** `1E9`, `1e+9` and `00012` are E0105 with a fix; `1e-400` and `-0x8000000000000000` are E0105 (`lex/number_spellings`).

### 3.8 Timeouts

- `http.send`: 0 means "only the run's `--timeout`", and the limit bounds the whole request.
- `http.send_stream`: 0 means 600,000 ms, and the limit bounds each wait.
- tcp: 0 is an error (1 to 600,000).
- workers: a `Wait` variant, but `with_deadline(ms, f)` takes milliseconds.

The timeout case is `TcpError.timed_out` but `WorkerError.timeout`.

**Fixed (spec 056):** every `timeout_ms` takes 0 to 600,000, 0 meaning 600,000 (`http.send`'s 0 included: listed in the changelog); `WorkerError.timeout` is `timed_out` through E0214. **Declined:** `with_deadline` keeps milliseconds (it bounds a computation, not a wait) and clamps its range.

### 3.9 Library naming

- **Typed reads.** json has `as_int` and `int_field`; sqlite has `get_int` and `get_str`. The type is
  "text" in json, bytes and `Value.text`, but "str" in `get_str`. `json.as_float` converts an integer;
  `sqlite.get_float` refuses one. Missing names: ``no field `k` `` vs `no column y` vs
  ``no header `X-A` ``.
- **Parsing.** `parse_json`, `parse_time`, `parse_decimal` and `bits.parse_hex`, against
  `bytes.from_hex` and `from_base64`. Hex has two parsers with different errors (`invalid hex: zz` vs
  `not hex at position 0`). Parse errors come in four formats:
  - `invalid int: zz`;
  - `too short` (time);
  - `line 1, column 1: ...` (json);
  - `at byte N: ...` (regex).
- **Text output.** `write_json`, `format_time`, `decimal_text`, `to_hex` and `metrics_text`.
- **The first or smallest item.** `first` (set), `first_key` (map), `front` (deque) and `peek` (queue);
  ranges are `between` vs `keys_between`.
- **Verbs.** `random.shuffle` returns a copy, against the rule in `lists.vo:3` (verbs change in place,
  adjectives return new); it would be `shuffled`. `decimal.total` vs `lists.sum`; `decimal.absolute` vs
  `abs`.
- **`_bytes` and header functions.** `http.serve_bytes_workers` puts `_bytes` in the middle,
  `process.run_with_bytes` puts it last. `http.header(Response)` pairs with `request_header(Request)`.
  `web.with_header(Request) -> Request` but `with_cookie(Response) -> result[Response]`.
- **Error text.** Only `TcpError` has a `describe`; `WorkerError` and `time.LocalError` have no text
  form.

**Fixed (spec 056):** sqlite's readers are `value_column`, `int_column`, `float_column` (converting an integer), `text_column`, `bytes_column`, with ``no column `NAME` ``; parse errors are `invalid KIND: TEXT; DETAIL` or `invalid KIND at POSITION: DETAIL`; `random.shuffle` is `shuffled`; `serve_bytes_workers` is `serve_workers_bytes`; `http.header` is `response_header`; `worker_error_text` and `local_error_text` are new. **Declined** (the guide's "Names" says why): collections' per-type names and `decimal.total`/`absolute` (no overloading; `abs` is a builtin), `with_cookie`'s `result` (it validates), `from_` for decoding beside `parse_` for reading. **Deferred:** the text-output names (`write_json`, `format_time`, `decimal_text`, `to_hex`, `metrics_text`), which do not clash.

### 3.10 Argument order

Text and the builtins take the string first: `find(s, sub)`, `split(s, sep)`, `text.replace(s, old,
new)`. Regex takes the pattern first: `is_match(re, s)`, `replace_all(re, s, r)`, `split_on(re, s)`.
`text.replace` already replaces every occurrence, yet regex calls it `replace_all`.

**Fixed (spec 056):** regex takes the text first, with a fix swapping the arguments; `replace_all` is `replace_matches` (`replace` would clash with `std.text`).

### 3.11 Duplicates

- The clock: `now_ms()`, `std.time.now()` and `std.web.clock_ms()` are the same call.
- Request logging: `web.log_requests` writes text lines, `web.request_logs` writes std.log records.
- Authentication: `web.bearer_auth` and `api_key_auth` return `result[str]`; `security.tokens` and
  `header_key` return `result[User]`. Both put the identity in `X-Identity`, so `current_user` accepts
  both forms.

**Fixed (spec 056):** `time.now` and `web.clock_ms` are removed through E0214 (the fix writes `now_ms()`). **Declined:** `web.log_requests` stays (text lines for a program without `log`), its doc points at `request_logs`. **Deferred:** `web.bearer_auth`/`api_key_auth` beside the `std.security` ways, until a spec deprecates one (no mechanical fix exists).

### 3.12 Units

- Time zone offsets: `fixed_zone(east)` takes minutes, `offset_at` returns milliseconds, and the `Zone`
  fields are seconds.
- Name suffixes: `timeout_ms`, `at_ms`, `lifetime_s`, `max_age_s` and `hsts_seconds`.

**Fixed (spec 056):** `fixed_zone` is `offset_zone` in milliseconds, `Cookie.max_age_ms` and `Browser.lifetime_ms` replace their `_s` fields, and the tz records' fields end in `_s`, all through E0214 with converting fixes. **Declined:** positional parameters keep seconds (`issue_token`'s `lifetime_s`, `safe_headers`' `hsts_s`, renamed from `hsts_seconds`): a changed unit there could not be reported.

### 3.13 Effects of closing a resource

`sqlite.close` declares no effect, `tcp.close uses E`, `close_listener uses serve`, and
`workers.stop_worker` none.

**Fixed (spec 056):** `tcp.close_conn` and `close_listener` need no effect.

### 3.14 sqlite's error prefix

sqlite errors start with `error: ` (`sqlite.vo:59,67`), so `try open(...)` in `main` prints
`error: error: unable to open database file`. No other module prefixes its messages.

**Fixed (spec 056):** a plain sqlite error has no prefix (`sqlite/open_fails`); the kinds (`constraint:`, `busy:`, ...) stay.

### 3.15 Message style

- **Tests:** "Test `prints` checks nothing", "A test named \"same\" already exists", "`test "totals"`
  does not declare console".
- **Packages:** "Package money is listed twice", against "`money` is both a listed package" and "No
  package `money`".
- **Inference:** "Add a type annotation.", "give the variable a type", "annotate the binding".
- **Argument counts:** "takes 4 argument(s), got 1" says "got" where other messages say "found".
- **Backticks:** the escape message lists `\n, \t, …` without backticks, unlike its siblings; so do
  "like 1000 or 1.5e3" and "use @size, @range".
- **Ranges:** written "0 to 63", "0-63" and "from 0 to 65536".
- **Capitals:** R0007 "Invalid pattern at byte 0: The group is not closed" has one mid-sentence.
- **E0110's fields:** they mix category words with token text: the message says "found the end of the
  file", the JSON says "found `end of file`".
- **Repeated annotations:** a repeated `@spec` is E0119 with a fix; a repeated `@size` is E0329 without
  one; a repeated `@allow` is reported only as two W0706.
- **File codes:** "Cannot read the base toolchain" is T0001 (exit 64), while every other unreadable input
  is T0002 (exit 66).

**Fixed (spec 056):** tests are `` `test "NAME"` ``, packages `` `NAME` ``, "found" for "got", ranges "A to B" (checked by `tests/message_style.py`); a repeated annotation is E0119 with a fix for every annotation; an unreadable `--base` is T0002. **Deferred:** the inference wordings, R0007's mid-sentence capital (the regex engine's sentences are shared with E0319) and E0110's mixed fields: rewording only, no code or fix changes.

## Checked and consistent

Nothing to report in these areas:
- catalog names and severities against what the toolchain emits;
- every "Fixable: yes" example offers a fix, and every "no" example none;
- `ret` in `ensures`, nested `set` targets, `result[T]` vs `result[T, str]`, string escapes, comparison
  chaining, and calls through `xs[i](...)`.
