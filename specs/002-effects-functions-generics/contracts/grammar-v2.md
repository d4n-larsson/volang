# Contract: Grammar and Builtins Added in v2

This is a delta to `specs/001-vo-lang-core/contracts/grammar.md`. Everything not listed here is unchanged.

## New keywords

`uses` is the only new keyword. `ordered` and `effects` are recognized only after `:` in a type-parameter list, and effect names (`console input file net clock random env`) only inside a `uses` clause, so v1 identifiers with those names keep working.

## Syntax (EBNF delta)

```ebnf
func      = "fn" ident [ tparams ] "(" [ param { "," param } ] ")" [ "->" type ] [ uses ] block ;
test      = "test" string [ uses ] block ;
record    = "record" Type [ tparams ] "{" field { field } "}" ;
variant   = "variant" Type [ tparams ] "{" case { case } "}" ;
tparams   = "[" tparam { "," tparam } "]" ;
tparam    = Type [ ":" ( "ordered" | "effects" ) ] ;
uses      = "uses" effect { "," effect } ;
effect    = ename [ "(" string { "," string } ")" ]       (* scopes *)
          | Type ;                                      (* an effect parameter, E *)
ename     = "console" | "input" | "file" "." ( "read" | "write" ) | "net" | "clock" | "random" | "env" ;

type      = …v1… | Type [ "[" type { "," type } "]" ]     (* generic instance: Stack[int] *)
          | "fn" "(" [ type { "," type } ] ")" [ "->" type ] [ uses ] ;

primary   = …v1…
          | "fn" "(" [ param { "," param } ] ")" [ "->" type ] block      (* anonymous function *)
          | "new" Type [ "[" type { "," type } "]" ] "{" finit { "," finit } "}" ;
postfix   = primary { "." ident | "[" expr "]" | "(" [ expr { "," expr } ] ")" } ;  (* call on any expression *)
```

- **Anonymous functions** never have a `uses` clause; their effects are inferred from the body. Writing one is E0507, with a removal fix. As for named functions, `-> type` is required when a value is returned (FR-017); there is no shorter form.
- **Type arguments** are never written in call expressions. They are inferred from the arguments and the expected type. A type parameter that cannot be inferred is E0310, with an annotation fix.
- **Canonical form** (`vo fmt`): effects in the order `console, input, file.read, file.write, net, clock, random, env`, then effect parameters; scope strings sorted; type parameters in declaration order.

## Statements

- An expression statement may be any call: `name(…)`, as in v1, or a call on another expression (`make_adder(1)(2);`, `handlers[0](x);`, `r.on_save(x);`). The first token is still an identifier, so statements stay identifiable by their first token.
- `fn` at the start of a statement inside a block is a parse error (E0110 "functions are declared at top level; use `let f = fn(…) …;` for a local function"), with no fix.

## Semantics

- **Purity**: a function or test without `uses` is pure.
- **Requirements**: a function's requirement is the union of its builtin effects, its callees' declared effects, the effects of the function values it calls, and the effects of its anonymous functions' bodies. Its declaration must cover that (research §3).
- **Scopes**: file scopes are directories relative to the working directory the program starts in, written with a trailing `/` (it is added if missing). `env` scopes are exact variable names. `net` scopes are hosts.
- **Grant**: what `main` declares (for `run`) or what the test declares (for `test`). Effectful builtins check the innermost named function's declaration at run time (R0010).
- **Function values**:
  - A name referring to a function is a value.
  - A function value fits a function type when the parameter and return types are equal and the value's effects are a subset of the type's.
  - Capturing a `var` is E0505.
  - Captured `let` values and parameters are copied when the anonymous function is created.
- **Generics**:
  - Unconstrained type parameters support only `==` and `!=`.
  - `ordered` type parameters also support `< <= > >=`, `min`, `max` and key-less `sort`. They are instantiated only with `int`, `float` or `str`.
  - `effects` parameters appear only in `uses` clauses and function types.

## New builtins

| Builtin | Signature | Effects |
|---|---|---|
| sort_by | `(xs: list[T], key: fn(T) -> K uses E)` with `K` ordered | E (in place, stable) |
| map | `(xs: list[T], f: fn(T) -> U uses E) -> list[U]` | E |
| filter | `(xs: list[T], f: fn(T) -> bool uses E) -> list[T]` | E |
| fold | `(xs: list[T], init: A, f: fn(A, T) -> A uses E) -> A` | E |
| any, all | `(xs: list[T], f: fn(T) -> bool uses E) -> bool` | E (short-circuit) |
| env | `(name: str) -> result[str]` | env (`err("not set")` when missing) |
| random_int | `(lo: int, hi: int) -> int` | random (inclusive range; `lo > hi` is R0007) |

## Effects of v1 builtins

| Builtins | Effect |
|---|---|
| print, write, eprint | console |
| read_line, read_all | input |
| read_file | file.read |
| write_file | file.write |
| now_ms | clock |
| args, exit, everything else | pure |
