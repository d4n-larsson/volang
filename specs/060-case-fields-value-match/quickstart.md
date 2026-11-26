# Quickstart: Validating Case Fields and Value Matches

Run from the repository root after `make`.

## 1. A case with fields (US1)

```sh
mkdir -p /tmp/vo060 && cd /tmp/vo060
cat > main.vo <<'VO'
import std.json;

variant LoadError {
    missing(str);
    malformed {
        path: str;
        line: int;
        reason: str;
    }
    empty;
}

fn describe(e: LoadError) -> str {
    match e {
        case missing(p) {
            return "no " + p;
        }
        case malformed(m) {
            return format("{}:{}: {}", m.path, m.line, m.reason);
        }
        case empty {
            return "empty";
        }
    }
}

fn main() uses console {
    let e = LoadError.malformed { path: "a.txt", line: 3, reason: "bad number" };
    print(describe(e));
    print(json.write_json(json.to_json(e)));
}
VO
vo run main.vo   # a.txt:3: bad number, then {"malformed":{"path":"a.txt","line":3,"reason":"bad number"}}
```

Then: a missing field (E0305), `set m.line = 4;` in the arm (E0344), `missing { path: str; }` (E0216).

## 2. A value match (US2)

```sh
cat > status.vo <<'VO'
const limit = 429;

fn text(status: int) -> str {
    match status {
        case 200 {
            return "ok";
        }
        case 404 {
            return "not found";
        }
        case limit {
            return "slow down";
        }
        else {
            return "other";
        }
    }
}

fn main() uses console {
    print(text(404) + " " + text(429) + " " + text(500));
}
VO
vo run status.vo   # not found slow down other
```

Then: without `else` (E0414, fix inserts it), `case 200` twice (E0402), `case "200"` (E0301), `match` on a
`float` (E0345), `else` on a variant match (E0415).

## 3. Whole-repository checks

```sh
make test                     # includes JSON equality of the migrated payloads (FR-005)
python3 tests/ref_examples.py # reference words: no growth
```
