# Quickstart: validating Regular Expressions

Prerequisites: `make` (builds `build/vo`), Python 3 for the differential test. Expected results are in
[contracts/library.md](contracts/library.md) and [contracts/pattern.md](contracts/pattern.md);
diagnostics in [contracts/diagnostics.md](contracts/diagnostics.md).

## 1. Matching, groups, replace, split (Stories 1, 3, 5)

```sh
cat > /tmp/r.vo <<'EOF'
import std.regex.regex;
import std.regex.is_match;
import std.regex.first_match;
import std.regex.all_matches;
import std.regex.replace_all;
import std.regex.split_on;

fn main() uses console {
    let id = regex("^[A-Z]{3}-\\d{4}$");
    print(str(is_match(id, "ABC-1234")) + " " + str(is_match(id, "ABC-12345")));
    let log = regex("(\\d{4})-(\\d{2})-(\\d{2}) \\S+ (?<level>ERROR|WARN)");
    match first_match(log, "2026-11-19 12:04:55 ERROR disk full (sda1)") {
        case ok(m) {
            print(m.groups[1] + " " + m.named["level"] + " " + str(m.end));
        }
        case err(e) {
            print(e);
        }
    }
    print(replace_all(regex("(\\d{2})/(\\d{2})/(\\d{4})"), "19/11/2026", "$3-$2-$1"));
    print(str(len(all_matches(regex("#\\w+"), "#vo is #fast, #vo"))));
    print(join(split_on(regex("\\s*,\\s*"), "a , b,c"), "|"));
    print(replace_all(regex("x*"), "abxd", "-"));
    print(str(is_match(regex("(?i)error"), "Error")));
}
EOF
bin/vo run /tmp/r.vo
```

Expected:

```text
true false
2026 ERROR 25
2026-11-19
3
a|b|c
-a-b--d-
true
```

## 2. Mistakes at check time (Story 2)

A file with `regex("\d+")`, `regex("(a")`, `regex("(?=x)")`, `regex("(?P<y>a)")`, `regex(t)` and
`replace_all(regex("(a)"), s, "\\1 $2")`: `vo check --json` reports, in one run, E0103 (fix `\\d`),
E0319, E0320, E0320 (fix `(?<`), E0321, E0322 (fix `$1`) and E0322 ("no group 2"). `tests/apply_fixes.py`
applies each fix and re-checks.

## 3. Safety (Story 4)

```sh
python3 tests/regex_speed.py    # (a+)+$ etc. on 100,000 characters < 1 s, linear growth; 10 MB of logs < 2 s
```

`parse_regex` of a 20,000-byte pattern returns `err("Pattern is longer than 10000 bytes.")`.

## 4. Suites

```sh
make quick AREA=regex                                   # golden cases, fixes, catalog, doc examples
python3 tests/regex_diff.py                             # SC-001: 100,000 pairs against Python's re
python3 tests/regex_diff.py --cross build/vo build-aarch64/vo build-riscv64/vo
make test                                               # once, to complete the change
```

## 5. Docs

```sh
bin/vo doc std.regex                 # every item, with examples
python3 tests/ref_examples.py        # budget: at most +150 tokens
bin/vo explain E0320
```
