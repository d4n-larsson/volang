# Contract: scopes through calls

## Language rule (reference, Effects)

> A caller's scopes must cover its callees' scopes (E0503); a callee's effect without scopes takes its
> callers'.
>
> Paths and names are also checked at run time, after resolving `..`, absolute paths and symbolic
> links, against every scoped declaration on the call path: one outside is R0010 (exit 70).

Applies to `file.read`, `file.write`, `file.delete`, `net`, `env`, `serve`. `process` always names
its programs (E0512). In `main`, a test and a property an effect without scopes means every target.

## Check time

| Program | Before | After |
|---|---|---|
| `fn f() uses net("a")` calls `fn g(h: str) uses net` | E0503, fix `uses net` | no diagnostic |
| `fn f()` (no `net`) calls `g` | E0501, fix adds `net` | the same |
| `fn f() uses net("a")` calls `fn g() uses net("b")` | E0503, fix `uses net("a", "b")` | the same |
| `fn f() uses file.read("a/")` calls `read_file("b/x")` (a literal) | E0503 | the same |
| `fn f() uses net("a")` calls `connect("b", ...)` (a literal host) | no diagnostic (R0010 when run) | the same |
| `fn f() uses net("a")` calls `fn g() uses net`, which calls `fn h() uses net("b")` | E0503 on `g`'s callers | E0503 on `f` (what `g` needs reaches `f` through plain calls) |
| `fn f() uses net("a")` passes `connect` to `fn run[N: effects](d: fn(...) -> ... uses N) uses N` | E0503 | no diagnostic |
| a value of type `fn() uses net` where `fn() uses net("a")` is expected | E0502 | the same |
| `uses process` | E0512 | the same |

Diagnostics keep their codes, messages and JSON fields. The fix of E0503 adds scopes.

## Run time

| Path (root first) | Target | Result |
|---|---|---|
| `main net("a")` → `g net` | `a` | allowed |
| `main net("a")` → `g net` | `b` | R0010: `main` declares net("a") |
| `main net("a","b")` → `f net("a")` → `g net` | `b` | R0010: `f` declares net("a") |
| `main net("a","b")` → `f net("a")` → anonymous → generic → `g net` | `b` | R0010: `f` declares net("a") |
| `main net` → `g net` | anything | allowed |
| `main file.read("data/")` → `g file.read` | `data/../x`, `/etc/x`, a link out of `data/` | R0010 |

R0010's message and JSON are unchanged: the attempted effect and target, and the function whose
declaration refused it. When several refuse, the one nearest the operation is named.

## Unchanged

`vo.effects`, `vo effects`, `vo package` output, `vo doc`, hover, recordings, replay format, the
schema version.
