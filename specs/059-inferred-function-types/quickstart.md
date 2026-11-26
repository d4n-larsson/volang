# Quickstart: Validating Inferred Function Types

Run from the repository root after `make`.

## 1. Lambdas without types check and run (US1)

```sh
mkdir -p /tmp/vo059 && cd /tmp/vo059
cat > main.vo <<'EOF'
record Person {
    name: str;
    age: int;
}

fn make_adder(n: int) -> fn(int) -> int {
    return fn(x) => x + n;
}

fn main() uses console {
    let people = [new Person { name: "cy", age: 30 }, new Person { name: "al", age: 25 }];
    sort_by(people, fn(p) => p.age);
    print(join(map(people, fn(p) => p.name), ","));
    let total = fold([1, 2, 3], 0, fn(acc, x) => acc + x);
    print(str(make_adder(total)(4)));
}
EOF
vo run main.vo                         # al,cy  then 10
vo query type-at main.vo main.vo:12:24 # p: Person
```

## 2. A wrong body names the expected type

`let f: fn(Person) -> int = fn(p) => p.name;` gives E0301 at `p.name`, expected `int`, found `str`.

## 3. 0.4 code migrates by its fixes (US2)

```sh
cat > old.vo <<'EOF'
fn main() uses console {
    let xs = [1, 2, 3];
    print(str(fold(map(xs, fn(x: int) -> int { return x * 2; }), 0, fn(a: int, x: int) -> int { return a + x; })));
    let f = fn(x: int) -> int { return x + 1; };
    print(str(f(1)));
}
EOF
vo check --json old.vo   # E0342 (x3) and E0413 (x3), each with a fix
# from the repository root: applies fixes, up to 4 rounds, until clean
bin/vo run --timeout 0 tools/vo_fix.vo -- /tmp/vo059/old.vo
vo run /tmp/vo059/old.vo # 12  then 2, as before
```

After the fixes: `map(xs, fn(x) => x * 2)`, `fn(a, x) => a + x`, `let f: fn(int) -> int = fn(x) => x + 1;`.

## 4. No expected type (E0343)

`let f = fn(x) => x;` gives E0343 with no fix; `let f: fn(int) -> int = fn(x) => x;` checks.

## 5. Formatting (US3)

`vo fmt` turns `fn(p) { return p.age; }` into `fn(p) => p.age` on one line.

## 6. Whole-repository checks

```sh
python3 tests/migrate_059.py   # golden cases migrated by fixes: identical output and test results
make test                      # SC-002, SC-004, SC-005 (reference budget)
python3 tests/ref_examples.py  # reference tokens: no growth
```
