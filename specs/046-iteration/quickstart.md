# Quickstart: Iteration (spec 046)

```vo
import std.lists.sort_by_desc;

fn main() uses console {
    let counts = new map[str, int];
    for w in split("b a b c b a", " ") {
        set counts[w] = get_or(counts, w, 0) + 1;
    }
    let words = keys(counts);
    sort_by_desc(words, fn(w: str) -> int { return counts[w]; });
    for i, w in words {
        print(format("{}. {} {}", i + 1, w, counts[w]));
    }
    for word, n in counts {
        print(word + ": " + str(n));
    }
}
```

## Validate

```sh
make quick AREA=iteration                    # golden cases, doc examples, tests/iteration.py
python3 tests/iteration.py --group loops     # SC-001: 1,000 generated programs against the model
python3 tests/iteration.py --group lists     # SC-004
python3 tests/iteration.py --group speed     # SC-002
python3 tests/grammars.py                    # SC-005
python3 tests/ref_examples.py                # the reference under 15,000 tokens
grep -rn "in keys(" -A1 std examples | grep "let .* = .*\[" # SC-003: nothing
make site-check
python3 tools/progress.py run test           # once; then read build/progress/test.log
```
