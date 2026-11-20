# Contract: Syntax and Meaning (spec 032)

## The type

```text
result[T]          the error is a str (unchanged)
result[T, E]       the error is an E
```

`result[T, str]` is the same type as `result[T]`. `E` is any type but `void`, a function type or a type
containing one (E0334).

## Building and reading

```vo
variant LoadError {
    missing(str);
    malformed(int);
}

fn load(text: str) -> result[int, LoadError] {
    if text == "" {
        return err(LoadError.missing("config.txt"));
    }
    match parse_int(text) {
        case ok(n) {
            return ok(n);
        }
        case err(_) {
            return err(LoadError.malformed(len(text)));
        }
    }
}

fn describe(e: LoadError) -> str {
    match e {
        case missing(path) {
            return "no file " + path;
        }
        case malformed(n) {
            return "bad number of " + str(n) + " characters";
        }
    }
}

fn main() -> result[void] uses console {
    let n = try map_err(load("12"), describe);
    print(str(n));
    match load("") {
        case ok(_) {
            print("loaded");
        }
        case err(e) {
            print(describe(e));
        }
    }
    return ok();
}
```

```output
12
no file config.txt
```

## try

`try r` in a function returning `result[U, F]` needs `r: result[T, F]`; it gives `T` or returns `r`'s
error. Any other error type is E0412. `main` and tests return `result[void]`, so a typed error reaches
them only through `map_err`.

## map_err

`map_err(r: result[T, E1], f: fn(E1) -> E2) -> result[T, E2]`: an `ok` unchanged, `err(e)` becomes
`err(f(e))`. `f` is any function value (a named function or an anonymous one); its effects become the
caller's. Like the other builtins, `map_err` itself is not a value.

## Formatting

`result[T, E]` is printed as written, with `, ` between the types. The guide writes `result[T]` for
message errors.
