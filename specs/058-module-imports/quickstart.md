# Quickstart: Validating Module Imports

Run from the repository root after `make`.

## 1. The new form checks and runs

```sh
mkdir -p /tmp/vo058 && cd /tmp/vo058
cat > geometry.vo <<'EOF'
export record Point {
    x: int;
    y: int;
}

export fn dist2(p: Point) -> int {
    return p.x * p.x + p.y * p.y;
}
EOF
cat > main.vo <<'EOF'
import geometry;

fn main() uses console {
    print(str(geometry.dist2(new geometry.Point { x: 3, y: 4 })));
}
EOF
vo run main.vo            # 25
```

## 2. A 0.3 program migrates by its fix

```sh
cat > main.vo <<'EOF'
import geometry.Point;
import geometry.dist2;

fn main() uses console {
    print(str(dist2(new Point { x: 3, y: 4 })));
}
EOF
vo check main.vo          # E0215 ... fix: rewrite lines 1 to 4 (module imports)
bin/vo run --timeout 0 tools/vo_fix.vo -- /tmp/vo058/main.vo
vo run main.vo            # 25, and main.vo is back to the form of step 1
```

## 3. Clashes and shadowing

- `let geometry = 1;` inside `main` → E0203 with the rename fix.
- `import std.text;` and a parameter `text: str` → E0203, fix renames to `text_value`.
- `dist2(p)` bare → E0201, fix `geometry.dist2`.
- `geometry.helper` (private) → E0209, fix adds `export`.
- an unused `import std.json;` → W0203, fix removes it.

## 4. The repository

```sh
make quick AREA=front,query,docs     # between edits
python3 tests/migrate_058.py         # golden cases: old vs new expected outputs, normalized
make test                            # once per change
```

Expected: every example runs with unchanged output; `grep -c '^import' examples/*/*.vo` shows at most 12
per file; `tests/ref_examples.py` stays at or under 15,000 tokens.

## 5. Tools

```sh
cd examples/notes-api
vo query refs main.vo notes.add_note          # declaration, import, qualified calls
vo edit rename main.vo notes.add_note add     # writes all files or none; check clean
vo doc std.json.parse_json                    # ... use: import std.json; then json.parse_json
```
