# Quickstart: validating scoped effects through calls

## 1. The probe that failed

```sh
cat > /tmp/dial.vo <<'EOF'
import std.tcp.Conn;
import std.tcp.TcpError;
import std.tcp.connect;
import std.tcp.describe;

fn dial(host: str, port: int) -> result[Conn[uses net], TcpError] uses net {
    return connect(host, port, 1000);
}

fn main() uses console, net("127.0.0.1:1") {
    match dial("127.0.0.1", 1) {
        case ok(_) { print("open"); }
        case err(e) { print(describe(e)); }
    }
    match dial("127.0.0.1", 2) {
        case ok(_) { print("open"); }
        case err(e) { print(describe(e)); }
    }
}
EOF
bin/vo check /tmp/dial.vo      # before: E0503 on main. after: 0 errors
bin/vo run /tmp/dial.vo        # after: "the connection was refused", then R0010 naming 127.0.0.1:2 and `main`, exit 70
```

## 2. Between edits

```sh
make quick AREA=effects        # the new cases in tests/cases/effects, scope_paths.py --count 300, scope_fuzz.py
```

## 3. Once per change

```sh
make test
python3 tests/scope_paths.py --count 2000 --seed 1     # SC-002
python3 tests/ref_examples.py                          # the budget line: not above today's count
for d in examples/*/; do (cd $d && ../../bin/vo effects main.vo); done   # SC-004: no change reported
```

## 4. Spec 049 unblocked

The two-module package probe of `specs/049-postgres-package/research.md` R1 checks and runs with
`net("127.0.0.1:PORT")` in `main` (SC-008).

Details: [contracts/scopes.md](contracts/scopes.md), [data-model.md](data-model.md), [research.md](research.md).
