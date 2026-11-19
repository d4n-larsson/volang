# Quickstart: validating SQLite Databases

Prerequisites: `make` (builds `build/vo` with the vendored SQLite); cross builds for SC-001. Expected
behavior is in [contracts/library.md](contracts/library.md); diagnostics in
[contracts/diagnostics.md](contracts/diagnostics.md).

## 1. Store and query (Story 1)

```sh
mkdir -p /tmp/inv/data && cd /tmp/inv
cat > main.vo <<'EOF'
import std.sqlite.open;
import std.sqlite.sql;
import std.sqlite.exec;
import std.sqlite.query;
import std.sqlite.get_str;
import std.sqlite.get_int;
import std.sqlite.Value;

fn main() -> result[void] uses console, file.read("data/"), file.write("data/") {
    let db = try open("data/inventory.db");
    let _ = try exec(db, sql("create table if not exists items(name text primary key, qty int not null)"), []);
    let _ = try exec(db, sql("insert or replace into items values (?, ?)"), [Value.text("bolt"), Value.int(3)]);
    let _ = try exec(db, sql("insert or replace into items values (?, ?)"), [Value.text("nut"), Value.int(40)]);
    for row in try query(db, sql("select name, qty from items where qty < ? order by name"), [Value.int(10)]) {
        print(try get_str(row, "name") + " " + str(try get_int(row, "qty")));
    }
    return ok();
}
EOF
vo run main.vo        # bolt 3; data/inventory.db exists afterwards
```

## 2. Mistakes at check time (Story 2)

A file with `sql("selec * from items")`, `sql(name + "x")`, `sql("create table a(x); create table
b(y)")`, `sql("select * from t where id = :id")`, `exec(db, sql("insert into t values (?, ?)"), [a, b,
c])`, `sql("select random()")` and `open(":memory:")`: `vo check --json` reports E0323 (at `selec`),
E0324, E0323 (two statements, with the split fix), E0325 (fix `?`), E0325 (2 and 3), E0323 (random)
and E0326 (fix `open_memory()`) in one run.

## 3. Tests (Story 3)

`vo test` on a file whose tests use `open_memory()`: no effect declared, passes 20 times identically.

## 4. Transactions, scope, replay (Stories 4 and 5)

```sh
make quick AREA=sqlite                     # golden and step cases
python3 tests/sqlite_scope.py              # ATTACH / VACUUM INTO / pragmas never leave the grant
vo run --record rec.jsonl main.vo && rm data/inventory.db && vo replay rec.jsonl   # same output
```

## 5. Suites

```sh
python3 tests/sqlite_corpus.py --cross build/vo build-aarch64/vo build-riscv64/vo   # SC-001
python3 tests/sqlite_fuzz.py                                                       # SC-007
python3 tests/sqlite_speed.py                                                      # SC-002
make test
ls -l build/vo                                                                     # SC-008: +1.5 MB at most
```
