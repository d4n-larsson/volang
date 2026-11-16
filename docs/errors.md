# vo-lang Error Catalog

Every diagnostic, runtime error and toolchain error has a code. `vo explain CODE` prints its section.

## E0101 invalid_character

A character that is not part of vo syntax, or a comment that does not stand on its own line.

Fixable: yes

The lexer found a character that no token can start with, such as `$` or `@`, or a `#` comment after code on the same line. Comments must be whole lines. The fix deletes the character or the trailing comment; move the comment to its own line if you want to keep it.

### Example

```vo
fn main() uses console {
    print("hello"); # greet the user
}
```

### Fixed

```vo
fn main() uses console {
    # greet the user
    print("hello");
}
```

## E0102 unterminated_string

A string literal is not closed before the end of its line.

Fixable: yes

Strings must open and close on the same line. The usual cause is a missing closing `"`, which can also cause follow-on parse errors on the next lines. The fix inserts the `"`. Use `\n` for a line break inside a string.

### Example

```vo
fn main() uses console {
    let s = "abc;
    print(s);
}
```

### Fixed

```vo
fn main() uses console {
    let s = "abc";
    print(s);
}
```

## E0103 invalid_escape

A string contains an escape other than \n, \t, \\ or \".

Fixable: no

vo has only four escapes: `\n`, `\t`, `\\` and `\"`. A single backslash before any other character, as in a Windows path, is an error. Write a literal backslash as `\\`.

### Example

```vo
fn main() uses console {
    print("C:\data");
}
```

### Fixed

```vo
fn main() uses console {
    print("C:\\data");
}
```

## E0104 invalid_utf8

The source file contains bytes that are not valid UTF-8.

Fixable: no

Source files must be valid UTF-8. The diagnostic points at the first bad byte, which usually comes from a file saved in another encoding (such as Latin-1) or from binary data pasted into a string. Re-save the file as UTF-8 or remove the byte. The Example cannot be shown as text, so it is a shell command that writes the Latin-1 byte 0xE9 into a string.

### Example

```sh
printf 'fn main() {\n    print("caf\xe9");\n}\n' > main.vo
vo check main.vo
```

### Fixed

```vo
fn main() uses console {
    print("café");
}
```

## E0105 number_out_of_range

A number literal is out of range: an integer above 2^63-1, or a float with more than 15 significant digits.

Fixable: no

Integer literals must fit in a signed 64-bit integer (at most 9223372036854775807), and float literals may have at most 15 significant digits. Use a smaller value, or fewer digits for a float.

### Example

```vo
fn main() uses console {
    let big = 9223372036854775808;
    print(str(big));
}
```

### Fixed

```vo
fn main() uses console {
    let big = 9223372036854775807;
    print(str(big));
}
```

## E0110 unexpected_token

The parser found a token that cannot appear here.

Fixable: sometimes

The parser expected one kind of token and found another. The message names both (`expected`, `found`). When exactly one token fits, the fix inserts it. A common cause is leaving out the braces of an `if`, `while` or `for` body; braces are always required.

### Example

```vo
fn main() {
    let n = 3;
    if n > 0 print("positive");
}
```

### Fixed

```vo
fn main() uses console {
    let n = 3;
    if n > 0 {
        print("positive");
    }
}
```

## E0111 unclosed_delimiter

A `(`, `[` or `{` is never closed.

Fixable: yes

An opening `(`, `[` or `{` has no matching closer before the end of the file. The diagnostic's `related` location points at the opening delimiter. The fix inserts the closer, but check that it belongs where the fix puts it.

### Example

```vo
fn main() {
    if true {
        print("x");
}
```

### Fixed

```vo
fn main() uses console {
    if true {
        print("x");
    }
}
```

## E0112 missing_semicolon

A statement is missing its terminating `;`.

Fixable: yes

Every simple statement (`let`, `var`, `set`, `return`, `break`, `continue` and calls) ends with `;`. The fix inserts it at the end of the statement.

### Example

```vo
fn main() uses console {
    let a = 1
    print(str(a));
}
```

### Fixed

```vo
fn main() uses console {
    let a = 1;
    print(str(a));
}
```

## E0113 trailing_comma

A list of arguments, elements or fields ends with a comma.

Fixable: yes

vo does not allow a comma after the last argument, list element or field. The fix removes it.

### Example

```vo
fn main() uses console {
    print(str(len([1, 2,])));
}
```

### Fixed

```vo
fn main() uses console {
    print(str(len([1, 2])));
}
```

## E0114 statement_not_call

A statement starts with a name but is not a call; assignments need `set`.

Fixable: sometimes

A statement that starts with a name must be a call, such as `f(x);`. Assignment is written `set x = value;`, and an expression whose value is not used, such as `x + 1;`, is not a statement.

### Example

```vo
fn main() uses console {
    var x = 0;
    x = 1;
    print(str(x));
}
```

### Fixed

```vo
fn main() uses console {
    var x = 0;
    set x = 1;
    print(str(x));
}
```

## E0115 nesting_too_deep

The code is nested too deeply to process.

Fixable: no

The source nests expressions or blocks so deeply that the toolchain cannot process it. Real code does not reach this limit; it usually comes from generated code. Split the expression into several `let` bindings.

### Example

```vo
# Illustration only: the real trigger is far deeper nesting
# (about 200,000 levels of parentheses or blocks).
fn main() uses console {
    print(str((((((((((1 + 2) * 3) + 4) * 5) + 6) * 7) + 8) * 9) + 10)));
}
```

### Fixed

```vo
fn main() uses console {
    let a = ((1 + 2) * 3 + 4) * 5;
    let b = ((a + 6) * 7 + 8) * 9 + 10;
    print(str(b));
}
```

## E0116 unknown_annotation

An annotation name is not one vo knows.

Fixable: yes

Annotations come from a closed set; the only one is `@spec`, which links a `test` or `prop` to requirement IDs. The fix replaces a misspelled name (within two letters) with `spec`.

### Example

```vo
fn double(x: int) -> int {
    return x * 2;
}

@spce("FR-001")
test "doubles" {
    expect_eq(double(2), 4);
}
```

### Fixed

```vo
fn double(x: int) -> int {
    return x * 2;
}

@spec("FR-001")
test "doubles" {
    expect_eq(double(2), 4);
}
```

## E0117 misplaced_annotation

An annotation is not on the line right above a `test` or `prop`.

Fixable: no

`@spec` annotates the `test` or `prop` that follows it, with nothing but blank lines in between. On any other item, after a comment or at the end of the file, it is this error: move it above the test that proves the requirement.

### Example

```vo
@spec("FR-001")
fn double(x: int) -> int {
    return x * 2;
}

test "doubles" {
    expect_eq(double(2), 4);
}
```

### Fixed

```vo
fn double(x: int) -> int {
    return x * 2;
}

@spec("FR-001")
test "doubles" {
    expect_eq(double(2), 4);
}
```

## E0118 invalid_spec_id

A `@spec` ID is not a requirement ID, or there is none.

Fixable: no

An ID is letters, digits, `-` and `_`, optionally followed by `/`-separated parts of the same: `FR-003`, `SC-002`, `US1/AC2`. `@spec()` needs at least one.

### Example

```vo
fn double(x: int) -> int {
    return x * 2;
}

@spec("FR 1")
test "doubles" {
    expect_eq(double(2), 4);
}
```

### Fixed

```vo
fn double(x: int) -> int {
    return x * 2;
}

@spec("FR-001")
test "doubles" {
    expect_eq(double(2), 4);
}
```

## E0119 repeated_annotation

An item has more than one `@spec`.

Fixable: yes

Each `test` or `prop` has one `@spec` with all its IDs. The fix merges them into one.

### Example

```vo
fn double(x: int) -> int {
    return x * 2;
}

@spec("FR-001")
@spec("US1/AC2")
test "doubles" {
    expect_eq(double(2), 4);
}
```

### Fixed

```vo
fn double(x: int) -> int {
    return x * 2;
}

@spec("FR-001", "US1/AC2")
test "doubles" {
    expect_eq(double(2), 4);
}
```

## E0201 undefined_name

A name is used but never defined or imported.

Fixable: sometimes

The name is not a local, parameter, top-level item, builtin or import in scope. It is usually a typo or a variable used outside the block that declares it. When a defined name is within two edits of it, the fix replaces it with that name.

### Example

```vo
fn main() uses console {
    let total = 3;
    print(str(totl));
}
```

### Fixed

```vo
fn main() uses console {
    let total = 3;
    print(str(total));
}
```

## E0202 duplicate_definition

Two definitions share the same name.

Fixable: no

Each top-level item (function, record, variant, test) and each field, case and parameter needs a unique name, because vo has no overloading. The diagnostic's `related` location points at the first definition. Rename or remove one of them.

### Example

```vo
fn area(w: int, h: int) -> int {
    return w * h;
}

fn area(side: int) -> int {
    return side * side;
}

fn main() uses console {
    print(str(area(2, 3)));
}
```

### Fixed

```vo
fn area(w: int, h: int) -> int {
    return w * h;
}

fn square_area(side: int) -> int {
    return side * side;
}

fn main() uses console {
    print(str(area(2, 3)));
    print(str(square_area(4)));
}
```

## E0203 shadowed_name

A local name reuses a name that is already visible; vo does not allow shadowing.

Fixable: no

A local declaration (`let`, `var`, a `for` variable or a `case` binding) reuses a name that is already in scope. The diagnostic's `related` location points at the outer definition. Give the inner one a different name, or use `set` on a `var` if you meant to change the outer one.

### Example

```vo
fn main() uses console {
    let n = 1;
    if n > 0 {
        let n = 2;
        print(str(n));
    }
}
```

### Fixed

```vo
fn main() uses console {
    let n = 1;
    if n > 0 {
        let m = 2;
        print(str(m));
    }
}
```

## E0204 missing_import

A name is defined in another file but not imported.

Fixable: yes

The name is not defined in this file, but a top-level item with that name exists in another file under this file's directory. The fix inserts the matching `import` line at the top of the file.

### Example

```vo
# file: helpers.vo
fn greet(name: str) -> str {
    return "hello " + name;
}
```

```vo
# file: main.vo
fn main() uses console {
    print(greet("vo"));
}
```

### Fixed

```vo
# file: helpers.vo
export fn greet(name: str) -> str {
    return "hello " + name;
}
```

```vo
# file: main.vo
import helpers.greet;

fn main() uses console {
    print(greet("vo"));
}
```

## E0205 import_not_found

An import names a file or item that does not exist.

Fixable: no

`import a.b.name;` loads `a/b.vo` relative to the importing file and binds the top-level item `name`. This error means the file does not exist or has no item with that name. Check the path and the spelling; paths cannot use `..`.

### Example

```vo
# file: helpers.vo
fn greet(name: str) -> str {
    return "hello " + name;
}
```

```vo
# file: main.vo
import helper.greet;

fn main() uses console {
    print(greet("vo"));
}
```

### Fixed

```vo
# file: helpers.vo
export fn greet(name: str) -> str {
    return "hello " + name;
}
```

```vo
# file: main.vo
import helpers.greet;

fn main() uses console {
    print(greet("vo"));
}
```

## E0206 import_cycle

Files import each other in a cycle.

Fixable: no

Files cannot import each other in a cycle, directly or through other files. The diagnostic's `related` locations show the chain of imports. Move the shared items into a file that both sides import, so that imports only point one way.

### Example

```vo
# file: main.vo
import a.one;

fn main() uses console {
    print(one());
}
```

```vo
# file: a.vo
import b.two;

export fn one() -> str {
    return "one " + two();
}

export fn base() -> str {
    return "base";
}
```

```vo
# file: b.vo
import a.base;

export fn two() -> str {
    return "two " + base();
}
```

### Fixed

```vo
# file: main.vo
import a.one;

fn main() uses console {
    print(one());
}
```

```vo
# file: a.vo
import b.two;

export fn one() -> str {
    return "one " + two();
}
```

```vo
# file: b.vo
export fn two() -> str {
    return "two " + base();
}

fn base() -> str {
    return "base";
}
```

## E0207 naming_convention

A name breaks the naming convention (UpperCamel for types, snake_case otherwise).

Fixable: yes

Records and variants are UpperCamel (`Point`). Functions, variables, fields and variant cases are snake_case (`total_count`). The fix renames the identifier to the converted name; rename every other use too.

### Example

```vo
fn main() uses console {
    let myVar = 1;
    print(str(myVar));
}
```

### Fixed

```vo
fn main() uses console {
    let my_var = 1;
    print(str(my_var));
}
```

## E0208 assign_to_immutable

A `let` binding is assigned with `set`; only `var` bindings can change.

Fixable: sometimes

`let` bindings cannot be reassigned. The fix changes the declaration to `var`. Lists, maps and records bound with `let` can still be changed through `set xs[i]`, `set m[k]` and `set r.field`; only rebinding the name is forbidden.

### Example

```vo
fn main() uses console {
    let count = 0;
    for i in 0..3 {
        set count = count + i;
    }
    print(str(count));
}
```

### Fixed

```vo
fn main() uses console {
    var count = 0;
    for i in 0..3 {
        set count = count + i;
    }
    print(str(count));
}
```

## E0209 not_exported

An imported item is private to its file.

Fixable: yes

Only items declared with `export` (`export fn`, `export record`, `export variant`) can be imported; any other item can be used only inside its own file. The fix adds `export` to the declaration in the other file. A standard-library or package item without `export` is internal to that module: it has no fix, so use an item `vo doc` lists instead. When several imports need the same item, one diagnostic carries the fix and notes the other imports.

### Example

```vo
# file: helpers.vo
fn greet(name: str) -> str {
    return "hello " + name;
}
```

```vo
# file: main.vo
import helpers.greet;

fn main() uses console {
    print(greet("vo"));
}
```

### Fixed

```vo
# file: helpers.vo
export fn greet(name: str) -> str {
    return "hello " + name;
}
```

```vo
# file: main.vo
import helpers.greet;

fn main() uses console {
    print(greet("vo"));
}
```

## E0210 private_type_exported

An exported item uses a type that is private to its file.

Fixable: yes

The parameters and result of an exported function, the fields of an exported record and the cases of an exported variant can only use types other files can import too: a private record or variant of the same file is not allowed there. The fix adds `export` to that type's declaration.

### Example

```vo
# file: shapes.vo
record Size {
    w: int;
    h: int;
}

export fn area(s: Size) -> int {
    return s.w * s.h;
}
```

```vo
# file: main.vo
import shapes.area;

fn main() uses console {
    print("ready");
}
```

### Fixed

```vo
# file: shapes.vo
export record Size {
    w: int;
    h: int;
}

export fn area(s: Size) -> int {
    return s.w * s.h;
}
```

```vo
# file: main.vo
import shapes.area;

fn main() uses console {
    print("ready");
}
```

## E0211 link_outside_tree

An import reaches a file outside the program's directory through a symbolic link.

Fixable: no

An import loads a file from the importing file's directory (or from a listed package). When that file is a symbolic link, or lies under a linked directory, that leads outside the directory of the program's entry file and outside every listed package, the import is refused: checking or formatting a tree you were given must never read files elsewhere on the machine. Copy the file into the tree, or list its directory as a package with `vo package`.

### Example

```vo
# Illustration only: the trigger is a link, here `leak.vo -> ../secret.vo`,
# which a code block cannot show.
import leak.thing;

fn main() uses console {
    print(thing());
}
```

### Fixed

```vo
# file: leak.vo
export fn thing() -> str {
    return "inside the tree";
}
```

```vo
import leak.thing;

fn main() uses console {
    print(thing());
}
```

## E0301 type_mismatch

An expression has a different type than the one required.

Fixable: sometimes

vo has no implicit conversions, so a value must have exactly the type that is expected. The diagnostic gives `expected` and `found`. Change the value, or convert it with `str(...)`, `int(...)` or `float(...)`; the fix offers the conversion when there is one.

### Example

```vo
fn main() uses console {
    let n: int = "five";
    print(str(n));
}
```

### Fixed

```vo
fn main() uses console {
    let n: int = 5;
    print(str(n));
}
```

## E0302 wrong_argument_count

A function is called with the wrong number of arguments.

Fixable: no

A call must pass exactly as many arguments as the function has parameters, since vo has no default arguments or overloading. Add or remove arguments, or change the function.

### Example

```vo
fn add(a: int, b: int) -> int {
    return a + b;
}

fn main() uses console {
    print(str(add(1, 2, 3)));
}
```

### Fixed

```vo
fn add(a: int, b: int) -> int {
    return a + b;
}

fn main() uses console {
    print(str(add(add(1, 2), 3)));
}
```

## E0303 not_callable

Something that is not a function is called.

Fixable: no

Only functions and builtins can be called. vo has no first-class functions, so a variable, parameter or field can never be called. Check that the name refers to the function you meant.

### Example

```vo
fn main() uses console {
    let n = 1;
    print(str(n(2)));
}
```

### Fixed

```vo
fn main() uses console {
    let n = 1;
    print(str(n + 2));
}
```

## E0304 unknown_field

A record has no field with this name.

Fixable: sometimes

The record type has no field with this name. It is usually a typo; when a field is within two edits of it, the fix replaces it with that field.

### Example

```vo
record Point {
    x: int;
    y: int;
}

fn main() uses console {
    let p = new Point { x: 1, y: 2 };
    print(str(p.xx));
}
```

### Fixed

```vo
record Point {
    x: int;
    y: int;
}

fn main() uses console {
    let p = new Point { x: 1, y: 2 };
    print(str(p.x));
}
```

## E0305 missing_field

A record literal leaves out one or more fields.

Fixable: no

`new Record { ... }` must give a value for every field; vo has no default values. The message lists the missing fields. Add them to the literal.

### Example

```vo
record Point {
    x: int;
    y: int;
}

fn main() uses console {
    let p = new Point { x: 1 };
    print(str(p.x));
}
```

### Fixed

```vo
record Point {
    x: int;
    y: int;
}

fn main() uses console {
    let p = new Point { x: 1, y: 0 };
    print(str(p.x));
}
```

## E0306 empty_list_literal

An empty list literal `[]` has no element type.

Fixable: sometimes

`[]` is not valid because an empty literal has no element type. Create an empty list with `new list[T]`; the fix does this when the type is known from an annotation.

### Example

```vo
fn main() uses console {
    let xs: list[int] = [];
    print(str(len(xs)));
}
```

### Fixed

```vo
fn main() uses console {
    let xs = new list[int];
    print(str(len(xs)));
}
```

## E0307 cannot_infer_type

The type of an expression cannot be worked out; add a type annotation.

Fixable: no

Some expressions, such as `ok(...)` and `err(...)`, need their type from the context: a declared return type, a parameter, or a type annotation. Add an annotation such as `let r: result[int] = ok(1);`.

### Example

```vo
fn main() uses console {
    let r = ok(1);
    match r {
        case ok(_) {
            print("ok");
        }
        case err(_) {
            print("err");
        }
    }
}
```

### Fixed

```vo
fn main() uses console {
    let r: result[int] = ok(1);
    match r {
        case ok(_) {
            print("ok");
        }
        case err(_) {
            print("err");
        }
    }
}
```

## E0308 invalid_operands

An operator is applied to operand types it does not accept.

Fixable: no

The operator does not work on these operand types. Arithmetic needs two `int` or two `float` values, `+` also joins two `str` values, `%` takes `int` only, and `and`, `or` and `not` take `bool`. Convert an operand or use a builtin instead.

### Example

```vo
fn main() uses console {
    print("ab" * 2);
}
```

### Fixed

```vo
fn main() uses console {
    print("ab" + "ab");
}
```

## E0309 invalid_map_key

A map key type is not `int` or `str`.

Fixable: no

Map keys must be `int` or `str`. Convert other keys to one of these, for example a `bool` to `"true"`/`"false"` or a record to a `str` built from its fields.

### Example

```vo
fn main() uses console {
    let m = new map[bool, int];
    print(str(len(m)));
}
```

### Fixed

```vo
fn main() uses console {
    let m = new map[str, int];
    print(str(len(m)));
}
```

## E0310 cannot_infer_type_argument

The type argument of a generic function or variant cannot be worked out.

Fixable: sometimes

Type arguments are never written at calls: they come from the arguments and from the expected type. `let s = empty_stack();` gives neither. Annotate the variable (`let s: Stack[int] = empty_stack();`); when a later call with the variable shows its type, the fix adds that annotation.

### Example

```vo
record Stack[T] {
    items: list[T];
}

fn empty_stack[T]() -> Stack[T] {
    return new Stack[T] { items: new list[T] };
}

fn push_item[T](s: Stack[T], x: T) {
    push(s.items, x);
}

fn main() {
    let s = empty_stack();
    push_item(s, 1);
}
```

### Fixed

```vo
record Stack[T] {
    items: list[T];
}

fn empty_stack[T]() -> Stack[T] {
    return new Stack[T] { items: new list[T] };
}

fn push_item[T](s: Stack[T], x: T) {
    push(s.items, x);
}

fn main() {
    let s: Stack[int] = empty_stack();
    push_item(s, 1);
}
```

## E0311 missing_ordered

A generic function compares or sorts values of a type parameter that is not `ordered`.

Fixable: yes

Only `==` and `!=` work on every type. `<`, `<=`, `>`, `>=`, `min`, `max` and `sort` need a type parameter declared `ordered` (it can then be `int`, `float` or `str`). The error points into the generic function; the fix adds `: ordered`.

### Example

```vo
fn bigger[T](a: T, b: T) -> T {
    if a > b {
        return a;
    }
    return b;
}

fn main() uses console {
    print(str(bigger(1, 2)));
}
```

### Fixed

```vo
fn bigger[T: ordered](a: T, b: T) -> T {
    if a > b {
        return a;
    }
    return b;
}

fn main() uses console {
    print(str(bigger(1, 2)));
}
```

## E0312 wrong_type_argument_count

A generic type is written with the wrong number of type arguments.

Fixable: no

A generic record or variant is always written with its type arguments in a type (`Stack[int]`, `Pair[str, int]`, `new Stack[int] { ... }`); other types and type parameters take none.

### Example

```vo
record Pair[A, B] {
    first: A;
    second: B;
}

fn main() uses console {
    let p = new Pair[int] { first: 1, second: 2 };
    print(str(p.first));
}
```

### Fixed

```vo
record Pair[A, B] {
    first: A;
    second: B;
}

fn main() uses console {
    let p = new Pair[int, int] { first: 1, second: 2 };
    print(str(p.first));
}
```

## E0313 not_ordered

A type that cannot be ordered is used where an order is needed.

Fixable: no

Sorting, `min`, `max`, `<`, `<=`, `>`, `>=`, a `sort_by` key, an `ordered` type parameter and the keys of the ordered collections need an order: `int`, `float`, `str`, or a record whose fields all have one (compared field by field, in declaration order). Lists, maps, variants and functions have no order, so a record with such a field has none either; the message names that field. Sort by one of the ordered fields instead, or give the collection a comparison function.

### Example

```vo
record Task {
    name: str;
    tags: list[str];
}

fn main() uses console {
    let ts = [new Task { name: "b", tags: new list[str] }, new Task { name: "a", tags: new list[str] }];
    sort_by(ts, fn(t: Task) -> Task {
        return t;
    });
    print(ts[0].name);
}
```

### Fixed

```vo
record Task {
    name: str;
    tags: list[str];
}

fn main() uses console {
    let ts = [new Task { name: "b", tags: new list[str] }, new Task { name: "a", tags: new list[str] }];
    sort_by(ts, fn(t: Task) -> str {
        return t.name;
    });
    print(ts[0].name);
}
```

## E0314 task_result_function

The function given to `concurrent_map` returns something that holds a function.

Fixable: no

Each task of `concurrent_map` runs on its own copy of the program, and only its result comes back to the caller, as data. A function cannot come back, so the result type may not contain a function type anywhere: not itself, not in a record field, a variant case, a list or a map. The message names where the function is. Return the data the function would need instead, and build the function after the call.

### Example

```vo
record Job {
    name: str;
    greet: fn(str) -> str;
}

fn make(name: str) -> Job {
    return new Job { name: name, greet: fn(s: str) -> str { return s + "!"; } };
}

fn main() uses console {
    let jobs = concurrent_map(["a", "b"], 2, make);
    print(jobs[0].greet(jobs[0].name));
}
```

### Fixed

```vo
fn make(name: str) -> str {
    return name + "!";
}

fn main() uses console {
    let jobs = concurrent_map(["a", "b"], 2, make);
    print(jobs[0]);
}
```

## E0401 non_exhaustive_match

A `match` does not handle every case.

Fixable: yes

A `match` must have an arm for every case of the variant or `result` it inspects. The message lists the missing cases, and the fix inserts empty arms for them, which you then fill in. A `result` match needs both `ok` and `err`.

### Example

```vo
fn main() uses console {
    match parse_int("4") {
        case ok(v) {
            print(str(v));
        }
    }
}
```

### Fixed

```vo
fn main() uses console {
    match parse_int("4") {
        case ok(v) {
            print(str(v));
        }
        case err(e) {
            print(e);
        }
    }
}
```

## E0402 duplicate_case

A `match` handles the same case twice.

Fixable: yes

Each case may appear only once in a `match`. The second arm could never run. The fix removes it; merge its body into the first arm if it was needed.

### Example

```vo
fn main() uses console {
    match parse_int("1") {
        case ok(v) {
            print(str(v));
        }
        case ok(w) {
            print(str(w));
        }
        case err(e) {
            print(e);
        }
    }
}
```

### Fixed

```vo
fn main() uses console {
    match parse_int("1") {
        case ok(v) {
            print(str(v));
        }
        case err(e) {
            print(e);
        }
    }
}
```

## E0403 missing_return

A function with a return type can reach its end without `return`.

Fixable: no

Every path through a function that declares a return type must end in `return`. This usually happens when only some branches of an `if` return. Add a `return` after the branches or an `else` branch that returns.

### Example

```vo
fn sign(n: int) -> int {
    if n > 0 {
        return 1;
    }
}

fn main() uses console {
    print(str(sign(5)));
}
```

### Fixed

```vo
fn sign(n: int) -> int {
    if n > 0 {
        return 1;
    }
    return 0;
}

fn main() uses console {
    print(str(sign(5)));
}
```

## E0404 unhandled_result

A `result` value is ignored instead of being handled.

Fixable: sometimes

A call that returns a `result` cannot be used as a statement or bound to `_`, because the error would be lost. Propagate it with `try` (the enclosing function must return a `result`) or handle both cases with `match`. When the function returns a `result`, the fix adds `try`.

### Example

```vo
fn main() -> result[void] uses file.write {
    write_file("out.txt", "hello");
    return ok();
}
```

### Fixed

```vo
fn main() -> result[void] uses file.write {
    try write_file("out.txt", "hello");
    return ok();
}
```

## E0405 unused_value

A call returns a value that is ignored.

Fixable: yes

A call whose value is not used cannot be a statement on its own. If you only want the side effect, discard the value explicitly with `let _ = f();`, which is what the fix does. For a `result[T]` with a non-void payload, `try f();` is also rejected; use `let _ = try f();`.

### Example

```vo
fn log(msg: str) -> int uses console {
    print(msg);
    return len(msg);
}

fn main() uses console {
    log("started");
}
```

### Fixed

```vo
fn log(msg: str) -> int uses console {
    print(msg);
    return len(msg);
}

fn main() uses console {
    let _ = log("started");
}
```

## E0406 try_outside_result_fn

`try` is used in a function that does not return a `result`.

Fixable: no

`try` returns early with the `err`, so it can only be used in a function whose return type is a `result`. Change the function to return `result[...]` (for `main`, `result[void]` with `return ok();` at the end), or handle the error with `match`.

### Example

```vo
fn main() uses console {
    let n = try parse_int("12");
    print(str(n));
}
```

### Fixed

```vo
fn main() -> result[void] uses console {
    let n = try parse_int("12");
    print(str(n));
    return ok();
}
```

## E0407 break_outside_loop

`break` or `continue` appears outside a loop.

Fixable: no

`break` and `continue` must be inside a `while` or `for` body in the same function. A loop in a calling function does not count. Move the statement into the loop, or have the function return a value that the loop checks.

### Example

```vo
fn stop_at(i: int) {
    if i == 3 {
        break;
    }
}

fn main() uses console {
    for i in 0..10 {
        stop_at(i);
        print(str(i));
    }
}
```

### Fixed

```vo
fn main() uses console {
    for i in 0..10 {
        if i == 3 {
            break;
        }
        print(str(i));
    }
}
```

## E0408 missing_main

`vo run` needs a `fn main()`.

Fixable: no

`vo run` starts the program at `fn main()`, and the file has none. `vo check` and `vo test` do not need `main`. Add a `main` function, or use `vo test` if the file only holds tests.

### Example

```vo
fn helper() -> int {
    return 1;
}
```

### Fixed

```vo
fn helper() -> int {
    return 1;
}

fn main() uses console {
    print(str(helper()));
}
```

## E0409 invalid_main_signature

`main` must be `fn main()` or `fn main() -> result[void]`.

Fixable: no

`main` takes no parameters and returns nothing or `result[void]`. Read command-line arguments with `args()`, and set the exit status with `exit(code)` or by returning `err(...)` (exit status 1).

### Example

```vo
fn main(name: str) uses console {
    print(name);
}
```

### Fixed

```vo
fn main() uses console {
    for name in args() {
        print(name);
    }
}
```

## E0410 expect_outside_test

`expect` and `expect_eq` can only be used inside a `test` block.

Fixable: no

`expect` and `expect_eq` report test failures, so they are only allowed inside a `test "name" { ... }` or `prop "name" (...) { ... }` block. Move the check into a test, which `vo test` runs. In normal code, check the condition with `if` and report an error.

### Example

```vo
fn double(x: int) -> int {
    return x * 2;
}

fn main() uses console {
    expect_eq(double(2), 4);
    print(str(double(21)));
}
```

### Fixed

```vo
fn double(x: int) -> int {
    return x * 2;
}

test "double works" {
    expect_eq(double(2), 4);
}

fn main() uses console {
    print(str(double(21)));
}
```

## E0411 empty_case_parens

`case NAME()` on anything but the success of a `result[void]`.

Fixable: yes

Empty parentheses in a `case` mean "the success of a `result[void]`", which `ok()` builds and `case ok()` matches. A case that carries a value binds it with `case NAME(v)` or ignores it with `case NAME(_)`; a variant case without a value is written `case NAME`. The fix writes the matching form.

### Example

```vo
fn main() uses console {
    match parse_int("4") {
        case ok() {
            print("a number");
        }
        case err(e) {
            print(e);
        }
    }
}
```

### Fixed

```vo
fn main() uses console {
    match parse_int("4") {
        case ok(_) {
            print("a number");
        }
        case err(e) {
            print(e);
        }
    }
}
```

## E0501 missing_effect

A function or test does something its `uses` clause does not declare.

Fixable: yes

Every function and test declares what it may touch: `uses console, file.read, ...`. A function also needs everything its callees declare, so a declaration tells the truth about everything reachable from it. The message lists every missing effect (including those of callees that are missing their own declarations), and the fix rewrites the clause to declare them all.

### Example

```vo
fn greet(name: str) {
    print("hello " + name);
}

fn main() uses console {
    greet("vo");
}
```

### Fixed

```vo
fn greet(name: str) uses console {
    print("hello " + name);
}

fn main() uses console {
    greet("vo");
}
```

## E0502 function_value_effects

A function value has effects that the expected function type does not allow.

Fixable: sometimes

Effects are part of function types: `fn(int) -> int` is pure, `fn(int) -> int uses console` may print. A value fits a type whose effects cover its own. When the expected type is written in the program (a parameter's type), the fix adds the effects to it; the function that calls the parameter then needs them too (E0501).

### Example

```vo
fn apply(f: fn(int) -> int, x: int) -> int {
    return f(x);
}

fn main() uses console {
    print(str(apply(fn(x: int) -> int {
        print("called");
        return x;
    }, 1)));
}
```

### Fixed

```vo
fn apply(f: fn(int) -> int uses console, x: int) -> int uses console {
    return f(x);
}

fn main() uses console {
    print(str(apply(fn(x: int) -> int {
        print("called");
        return x;
    }, 1)));
}
```

## E0503 scope_too_narrow

A function declares an effect with scopes that do not cover what a callee declares.

Fixable: yes

A scoped declaration such as `file.write("out/")` covers callees that write under `out/`; `env("HOME")` covers callees that read `HOME`. A callee that declares more needs a wider caller. The fix widens the caller's scopes to include the callee's; the related note points at the callee's declaration.

### Example

```vo
fn user() -> result[str] uses env("USER") {
    return env("USER");
}

fn main() uses console, env("HOME") {
    match env("HOME") {
        case ok(h) {
            print(h);
        }
        case err(e) {
            print(e);
        }
    }
    match user() {
        case ok(u) {
            print(u);
        }
        case err(e) {
            print(e);
        }
    }
}
```

### Fixed

```vo
fn user() -> result[str] uses env("USER") {
    return env("USER");
}

fn main() uses console, env("HOME", "USER") {
    match env("HOME") {
        case ok(h) {
            print(h);
        }
        case err(e) {
            print(e);
        }
    }
    match user() {
        case ok(u) {
            print(u);
        }
        case err(e) {
            print(e);
        }
    }
}
```

## E0504 effects_grew

A function or test now has effects that the project's `vo.effects` record does not list.

Fixable: no

`vo.effects` (in the entry file's directory or an ancestor) lists the effects of every function and test. When code gains an effect or a wider scope, `vo check` fails with one E0504 per grown function, with related notes on the calls that brought the effect in. A person approves the change by running `vo effects --update`, which rewrites the record; its diff shows exactly what changed. There is no automatic fix, because approving new capabilities is a review decision.

### Example

```vo
fn main() uses console {
    print("hi");
}
```

```text
# file: vo.effects
main.vo:main pure
```

### Fixed

```vo
fn main() uses console {
    print("hi");
}
```

```text
# file: vo.effects
main.vo:main uses console
```

## E0505 captures_var

An anonymous function uses a `var` from the function around it.

Fixable: yes

Anonymous functions capture the values of the `let` bindings and parameters they use when they are created; a `var` could change afterwards, so capturing it is not allowed. The fix copies the current value into a `let` just before the statement and uses that inside the function.

### Example

```vo
fn main() uses console {
    var total = 1;
    let f = fn() -> int {
        return total;
    };
    print(str(f()));
}
```

### Fixed

```vo
fn main() uses console {
    var total = 1;
    let total_now = total;
    let f = fn() -> int {
        return total_now;
    };
    print(str(f()));
}
```

## E0507 uses_on_anonymous_fn

An anonymous function has a `uses` clause.

Fixable: yes

Anonymous functions get their effects from their body; only named functions and tests declare them. The fix removes the clause.

### Example

```vo
fn main() uses console {
    let f = fn(x: int) uses console {
        print(str(x));
    };
    f(1);
}
```

### Fixed

```vo
fn main() uses console {
    let f = fn(x: int) {
        print(str(x));
    };
    f(1);
}
```

## E0506 invalid_scope

A scope in a `uses` clause is empty, absolute, or leaves the working directory with `..`.

Fixable: no

File scopes are directories relative to the directory the program starts in, such as `"out/"` or `"data/raw/"`; `"./"` is the working directory itself. Environment and network scopes are non-empty names.

### Example

```vo
fn load() -> result[str] uses file.read("../data/") {
    return read_file("../data/a.txt");
}

fn main() uses console, file.read {
    match load() {
        case ok(t) {
            print(t);
        }
        case err(e) {
            print(e);
        }
    }
}
```

### Fixed

```vo
fn load() -> result[str] uses file.read("data/") {
    return read_file("data/a.txt");
}

fn main() uses console, file.read {
    match load() {
        case ok(t) {
            print(t);
        }
        case err(e) {
            print(e);
        }
    }
}
```

## E0509 unknown_effect

A `uses` clause names something that is not an effect.

Fixable: sometimes

The effects are `console`, `input`, `file.read`, `file.write`, `net`, `clock`, `random` and `env`. When the name is within two edits of one of them, the fix replaces it.

### Example

```vo
fn hello() uses consol {
    print("hi");
}

fn main() uses console {
    hello();
}
```

### Fixed

```vo
fn hello() uses console {
    print("hi");
}

fn main() uses console {
    hello();
}
```

## E0510 invalid_effects_record

A line of `vo.effects` cannot be read.

Fixable: no

Each line of the record is `<path>:<name> uses <effects>` or `<path>:<name> pure`, where the effects use the `uses` syntax. Lines starting with `#` are comments. Fix the line, or delete the record and write it again with `vo effects --update`.

### Example

```vo
fn main() uses console {
    print("hi");
}
```

```text
# file: vo.effects
main.vo:main uses consoel
```

### Fixed

```vo
fn main() uses console {
    print("hi");
}
```

```text
# file: vo.effects
main.vo:main uses console
```

## E0511 task_reads_input

The function given to `concurrent_map` reads stdin.

Fixable: no

The tasks of `concurrent_map` run at the same time, so they cannot share stdin: which task would get which line would depend on timing. A function whose effects include `input` cannot run as a task. Read the input before the call and pass the lines or the text to the tasks.

### Example

```vo
fn answer(question: str) -> str uses console, input {
    print(question);
    match read_line() {
        case ok(line) {
            return line;
        }
        case err(_) {
            return "";
        }
    }
}

fn main() uses console, input {
    let answers = concurrent_map(["name?", "age?"], 2, answer);
    print(answers[0]);
}
```

### Fixed

```vo
fn shout(line: str) -> str {
    return upper(line);
}

fn main() uses console, input {
    for s in concurrent_map(split(read_all(), "\n"), 2, shout) {
        print(s);
    }
}
```

## E0601 hole_type_unknown

A hole is in a place whose type is not known.

Fixable: no

`todo(...)` takes the type its context expects, so the context must have one. A `let` without a type, an operand of `==` and the argument of `str(...)` give none. Annotate the binding, or move the hole where a type is expected.

### Example

```vo
fn main() uses console {
    let count = todo("count the words");
    print(str(count));
}
```

### Fixed

```vo
fn main() uses console {
    let count = 3;
    print(str(count));
}
```

## E0602 ungeneratable_type

A property parameter has a type whose values cannot be generated.

Fixable: no

`vo test` generates the arguments of a `prop`, so every parameter type must have values it can build: no function types anywhere inside, and at least one finite value (a record whose fields always lead back to itself has none). Give the parameter a data type, and build the function or the recursive value inside the body.

### Example

```vo
prop "applies" (f: fn(int) -> int, x: int) {
    expect(f(x) == f(x));
}
```

### Fixed

```vo
fn double(x: int) -> int {
    return x * 2;
}

prop "applies" (x: int) {
    assume(x > -1000 and x < 1000);
    expect(double(x) == double(x));
}
```

## E0603 impure_contract

A `requires` or `ensures` condition needs an effect.

Fixable: no

Contracts are checked on every call and return, so they must not print, read input, touch files or call anything that does (including functions passed in with an effect parameter). Contracts never change a function's effects. Compute what the condition needs with pure code, or check the effectful part in the body.

### Example

```vo
fn logged(n: int) -> bool uses console {
    print(str(n));
    return n >= 0;
}

fn half(n: int) -> int
    requires logged(n) {
    return n / 2;
}

fn main() uses console {
    print(str(half(4)));
}
```

### Fixed

```vo
fn half(n: int) -> int
    requires n >= 0 {
    return n / 2;
}

fn main() uses console {
    print(str(half(4)));
}
```

## E0604 assume_outside_prop

`assume` is used outside a property.

Fixable: no

`assume(cond)` discards the current generated input of a `prop`, so it means nothing in a function or a test. In a test, check the condition with `if`; in a function, return early or an error.

### Example

```vo
test "halves" {
    assume(4 % 2 == 0);
    expect_eq(4 / 2, 2);
}
```

### Fixed

```vo
prop "halves" (n: int) {
    assume(n % 2 == 0);
    expect_eq(n / 2 * 2, n);
}
```

## W0101 repeated_spec_id

A `@spec` names the same ID twice.

Fixable: yes

Each ID counts once; the fix removes the repeats.

### Example

```vo
fn double(x: int) -> int {
    return x * 2;
}

@spec("FR-001", "FR-001")
test "doubles" {
    expect_eq(double(2), 4);
}
```

### Fixed

```vo
fn double(x: int) -> int {
    return x * 2;
}

@spec("FR-001")
test "doubles" {
    expect_eq(double(2), 4);
}
```

## W0201 unused_variable

A local variable is never used.

Fixable: no

A local variable or parameter is declared but never read. This is a warning, so the program still runs. Use the value, remove the declaration, or bind it to `_` (`let _ = f();`) to keep a call's side effect. There is no automatic fix because removing the declaration could drop a side effect.

### Example

```vo
fn main() uses console {
    let unused = 1;
    print("done");
}
```

### Fixed

```vo
fn main() uses console {
    print("done");
}
```

## W0401 unreachable_code

A statement can never run.

Fixable: no

The statement comes after a `return`, `break` or `continue` in the same block, so it can never run. This is a warning, so the program still runs. Remove the statement, or move it before the jump.

### Example

```vo
fn main() uses console {
    print("start");
    return;
    print("never");
}
```

### Fixed

```vo
fn main() uses console {
    print("start");
    print("now runs");
}
```

## W0402 void_ok_pattern

The success of a `result[void]` is matched with `case ok(_)` or `case ok`.

Fixable: yes

A `result[void]` succeeds with `ok()`, and its success is matched the same way: `case ok()`. `case ok(_)` and `case ok` still work, so this is a warning; the fix rewrites the pattern to `case ok()`.

### Example

```vo
fn check(n: int) -> result[void] {
    if n < 0 {
        return err("negative");
    }
    return ok();
}

fn main() uses console {
    match check(1) {
        case ok(_) {
            print("fine");
        }
        case err(e) {
            print(e);
        }
    }
}
```

### Fixed

```vo
fn check(n: int) -> result[void] {
    if n < 0 {
        return err("negative");
    }
    return ok();
}

fn main() uses console {
    match check(1) {
        case ok() {
            print("fine");
        }
        case err(e) {
            print(e);
        }
    }
}
```

## W0501 unused_effect

A `uses` clause declares an effect or scope that nothing in the function needs.

Fixable: yes

Declarations are the capabilities a function is granted, so unneeded ones are removed. A scope counts as needed when a callee's scope lies under it, or when a builtin uses the effect.

### Example

```vo
fn add(a: int, b: int) -> int uses file.read {
    return a + b;
}

fn main() uses console {
    print(str(add(1, 2)));
}
```

### Fixed

```vo
fn add(a: int, b: int) -> int {
    return a + b;
}

fn main() uses console {
    print(str(add(1, 2)));
}
```

## W0502 effects_reduced

A function or test has fewer effects than `vo.effects` lists, or was removed.

Fixable: no

Losing a capability is never an error, but the record is out of date: run `vo effects --update` so its diff shows the reduction.

### Example

```vo
fn main() uses console {
    print("hi");
}
```

```text
# file: vo.effects
main.vo:main uses console, clock
```

### Fixed

```vo
fn main() uses console {
    print("hi");
}
```

```text
# file: vo.effects
main.vo:main uses console
```

## W0503 function_renamed

A function in the record is gone and a new one in the same file has the same effects.

Fixable: no

This is reported as a rename, not as a removal plus an addition, because no capability changed. Run `vo effects --update` to record the new name.

### Example

```vo
fn say_hi() uses console {
    print("hi");
}

fn main() uses console {
    say_hi();
}
```

```text
# file: vo.effects
main.vo:greet uses console
main.vo:main uses console
```

### Fixed

```vo
fn say_hi() uses console {
    print("hi");
}

fn main() uses console {
    say_hi();
}
```

```text
# file: vo.effects
main.vo:main uses console
main.vo:say_hi uses console
```

## W0504 no_effects_record

A program in several files has no `vo.effects` record.

Fixable: no

The record lets reviewers see capability changes as a short diff. Create it with `vo effects --update` in the entry file's directory and keep it next to the code. Single-file programs get no warning.

### Example

```vo
# file: geom.vo
export fn area(w: int, h: int) -> int {
    return w * h;
}
```

```vo
# file: main.vo
import geom.area;

fn main() uses console {
    print(str(area(2, 3)));
}
```

### Fixed

```vo
# file: geom.vo
export fn area(w: int, h: int) -> int {
    return w * h;
}
```

```vo
# file: main.vo
import geom.area;

fn main() uses console {
    print(str(area(2, 3)));
}
```

```text
# file: vo.effects
geom.vo:area pure
main.vo:main uses console
```

## W0601 hole

A `todo(...)` hole marks code that is not written yet.

Fixable: no

`todo("description")` stands for a value of the type its context expects, so the rest of the program can be checked before the gap is filled. The report gives that type (`expected`) and the names in scope with their types (`scope`). A statement `todo("...");` ends its path, like `return`, and needs a value of the function's return type. While a function has a hole, W0201 and W0501 are not reported for it. Running into a hole is R0011. Replace the hole with an expression of the expected type.

### Example

```vo
fn area(w: int, h: int) -> int {
    return todo("area of w by h");
}

fn main() uses console {
    print(str(area(2, 3)));
}
```

### Fixed

```vo
fn area(w: int, h: int) -> int {
    return w * h;
}

fn main() uses console {
    print(str(area(2, 3)));
}
```

## E0701 package_changed

A package's files no longer match the hash its manifest pins.

Fixable: no

`vo.packages` (next to the entry file or above) pins each package to a SHA-256 hash of all its `.vo` files: their paths, lengths and bytes. Any changed byte, added, removed or renamed file changes the hash. When a check uses the package and its files hash differently, nothing of it is checked or run: the message gives both hashes. Review what changed in the package, then pin it again with `vo package ENTRY NAME DIRECTORY`, which writes the new hash.

### Example

```vo
import money.format.cents;

fn main() uses console {
    print(cents(1234));
}
```

```vo
# file: shared/money/format.vo
export fn cents(n: int) -> str {
    return str(n / 100) + "." + str(n % 100);
}
```

```text
# file: vo.packages
money shared/money sha256:0000000000000000000000000000000000000000000000000000000000000000
```

### Fixed

```vo
import money.format.cents;

fn main() uses console {
    print(cents(1234));
}
```

```vo
# file: shared/money/format.vo
export fn cents(n: int) -> str {
    return str(n / 100) + "." + str(n % 100);
}
```

```text
# file: vo.packages
money shared/money sha256:b8a83f7d5889555621315d5fb1332ddb9b479351d7dd62ed621b5f52bc74c951
```

## E0702 package_missing

A listed package's directory is missing, unreadable or has no `.vo` files.

Fixable: no

Each line of `vo.packages` names a directory, relative to the manifest. When a check uses the package and the directory cannot be read or holds no `.vo` file, the import fails. Restore the directory, or point the line at the right place with `vo package ENTRY NAME DIRECTORY`.

### Example

```vo
import money.format.cents;

fn main() uses console {
    print(cents(1234));
}
```

```text
# file: vo.packages
money shared/money sha256:0000000000000000000000000000000000000000000000000000000000000000
```

### Fixed

```vo
import money.format.cents;

fn main() uses console {
    print(cents(1234));
}
```

```vo
# file: shared/money/format.vo
export fn cents(n: int) -> str {
    return str(n / 100) + "." + str(n % 100);
}
```

```text
# file: vo.packages
money shared/money sha256:b8a83f7d5889555621315d5fb1332ddb9b479351d7dd62ed621b5f52bc74c951
```

## E0703 bad_manifest

A line of `vo.packages` is not `NAME DIRECTORY sha256:HEX`.

Fixable: no

Each line of `vo.packages` is a package name (an identifier other than `std`), its directory and `sha256:` followed by 64 lowercase hex digits; `#` starts a comment line. `vo package` writes these lines: run it rather than editing the file by hand.

### Example

```vo
import money.format.cents;

fn main() uses console {
    print(cents(1234));
}
```

```vo
# file: shared/money/format.vo
export fn cents(n: int) -> str {
    return str(n / 100) + "." + str(n % 100);
}
```

```text
# file: vo.packages
money shared/money
```

### Fixed

```vo
import money.format.cents;

fn main() uses console {
    print(cents(1234));
}
```

```vo
# file: shared/money/format.vo
export fn cents(n: int) -> str {
    return str(n / 100) + "." + str(n % 100);
}
```

```text
# file: vo.packages
money shared/money sha256:b8a83f7d5889555621315d5fb1332ddb9b479351d7dd62ed621b5f52bc74c951
```

## E0704 duplicate_package

A package name is listed twice in `vo.packages`.

Fixable: no

Every name in `vo.packages` must be unique, so that an import names exactly one directory. Remove one of the lines (`vo package --remove ENTRY NAME` removes an entry).

### Example

```vo
import money.format.cents;

fn main() uses console {
    print(cents(1234));
}
```

```vo
# file: shared/money/format.vo
export fn cents(n: int) -> str {
    return str(n / 100) + "." + str(n % 100);
}
```

```text
# file: vo.packages
money shared/money sha256:b8a83f7d5889555621315d5fb1332ddb9b479351d7dd62ed621b5f52bc74c951
money shared/money sha256:b8a83f7d5889555621315d5fb1332ddb9b479351d7dd62ed621b5f52bc74c951
```

### Fixed

```vo
import money.format.cents;

fn main() uses console {
    print(cents(1234));
}
```

```vo
# file: shared/money/format.vo
export fn cents(n: int) -> str {
    return str(n / 100) + "." + str(n % 100);
}
```

```text
# file: vo.packages
money shared/money sha256:b8a83f7d5889555621315d5fb1332ddb9b479351d7dd62ed621b5f52bc74c951
```

## E0705 package_name_clash

A package name is also a directory next to the importing file.

Fixable: no

`import money.format.cents;` could mean the local file `money/format.vo` or the package `money`. Rather than pick one silently, vo reports the clash. Rename the local directory, or list the package under another name.

### Example

```vo
import money.format.cents;

fn main() uses console {
    print(cents(1234));
}
```

```vo
# file: money/format.vo
fn cents(n: int) -> str {
    return str(n);
}
```

```vo
# file: shared/money/format.vo
export fn cents(n: int) -> str {
    return str(n / 100) + "." + str(n % 100);
}
```

```text
# file: vo.packages
money shared/money sha256:b8a83f7d5889555621315d5fb1332ddb9b479351d7dd62ed621b5f52bc74c951
```

### Fixed

```vo
import cash.format.cents;

fn main() uses console {
    print(cents(1234));
}
```

```vo
# file: money/format.vo
fn cents(n: int) -> str {
    return str(n);
}
```

```vo
# file: shared/money/format.vo
export fn cents(n: int) -> str {
    return str(n / 100) + "." + str(n % 100);
}
```

```text
# file: vo.packages
cash shared/money sha256:b8a83f7d5889555621315d5fb1332ddb9b479351d7dd62ed621b5f52bc74c951
```

## R0001 integer_overflow

An integer operation overflowed the 64-bit range.

Fixable: no

`int` is a signed 64-bit integer, and `+`, `-`, `*`, unary `-`, `/` and `%` stop the program instead of wrapping when the result does not fit (including `-9223372036854775808 / -1`). Check the operands before the operation, or use `float` when large values are expected.

### Example

```vo
fn main() uses console {
    let big = 9223372036854775807;
    print(str(big + 1));
}
```

### Fixed

```vo
fn main() uses console {
    let big = 9223372036854775807;
    if big < 9223372036854775807 {
        print(str(big + 1));
    } else {
        print("too big");
    }
}
```

## R0002 division_by_zero

An integer was divided by zero.

Fixable: no

Integer `/` or `%` with a zero divisor stops the program. Float division by zero does not; it gives an infinity or NaN. Check the divisor before dividing.

### Example

```vo
fn average(total: int, count: int) -> int {
    return total / count;
}

fn main() uses console {
    print(str(average(10, 0)));
}
```

### Fixed

```vo
fn average(total: int, count: int) -> int {
    if count == 0 {
        return 0;
    }
    return total / count;
}

fn main() uses console {
    print(str(average(10, 0)));
}
```

## R0003 index_out_of_bounds

An index or slice bound is outside the list or string.

Fixable: no

`xs[i]` and `set xs[i] = v;` need `0 <= i < len(xs)`, and `slice(x, a, b)` needs `0 <= a <= b <= len(x)`. Indexes start at 0, so the last element is `xs[len(xs) - 1]`. Check the index against `len` first.

### Example

```vo
fn main() uses console {
    let xs = [1, 2, 3];
    print(str(xs[3]));
}
```

### Fixed

```vo
fn main() uses console {
    let xs = [1, 2, 3];
    print(str(xs[len(xs) - 1]));
}
```

## R0004 key_not_found

A map has no entry for the key.

Fixable: no

Reading `m[k]` for a key that is not in the map stops the program. Check with `has(m, k)` first. Writing with `set m[k] = v;` always works.

### Example

```vo
fn main() uses console {
    let m = new map[str, int];
    set m["a"] = 1;
    print(str(m["b"]));
}
```

### Fixed

```vo
fn main() uses console {
    let m = new map[str, int];
    set m["a"] = 1;
    if has(m, "b") {
        print(str(m["b"]));
    } else {
        print("no b");
    }
}
```

## R0005 stack_exhausted

The program recursed too deeply.

Fixable: no

The call stack is full, almost always because a recursive function has no base case or never reaches it. Add or fix the base case, or rewrite the recursion as a loop.

### Example

```vo
fn count_down(n: int) -> int {
    return count_down(n - 1) + 1;
}

fn main() uses console {
    print(str(count_down(10)));
}
```

### Fixed

```vo
fn count_down(n: int) -> int {
    if n <= 0 {
        return 0;
    }
    return count_down(n - 1) + 1;
}

fn main() uses console {
    print(str(count_down(10)));
}
```

## R0006 out_of_memory

The program holds more live data than the memory limit.

Fixable: no

Memory that can no longer be reached is reclaimed automatically, so this error means the data the program still holds passed the limit (512 MiB by default, `--max-memory` to change it). The usual cause is a loop that keeps growing a string, list or map without bound. Limit the loop, keep only the data you still need, or raise the limit. With `--json` the report adds `in_use` and `limit`, in bytes.

### Example

```vo
fn main() {
    var s = "xxxxxxxxxxxxxxxx";
    while true {
        set s = s + s;
    }
}
```

### Fixed

```vo
fn main() uses console {
    var s = "xxxxxxxxxxxxxxxx";
    var n = 0;
    while n < 10 {
        set s = s + s;
        set n = n + 1;
    }
    print(str(len(s)));
}
```

## R0007 invalid_argument

A builtin received an argument it cannot handle.

Fixable: no

A builtin got a value it cannot work with: `int(...)` of NaN or of a float out of range, `split` with an empty separator, or `slice` of a `str` at a position inside a multi-byte character. The message says which. Check the value first. For `slice`, use byte offsets that `find` returned, or work on the `list[str]` from `chars`.

### Example

```vo
fn main() uses console {
    print(slice("café", 0, 4));
}
```

### Fixed

```vo
fn main() uses console {
    print(slice("café", 0, 5));
}
```

## R0008 exit_code_out_of_range

`exit` was called with a status outside 0-63.

Fixable: no

`exit` accepts statuses 0 to 63 only. Statuses 64 and above are reserved for the toolchain's own results (64 usage error, 65 check errors, 70 runtime error, 124 timeout). Use a status from 0 to 63.

### Example

```vo
fn main() uses console {
    print("done");
    exit(64);
}
```

### Fixed

```vo
fn main() uses console {
    print("done");
    exit(0);
}
```

## R0009 timeout

The program ran longer than the time limit.

Fixable: no

The program ran longer than the limit set by `vo run --timeout SECONDS` and was stopped. The usual cause is a loop whose condition never becomes false. Make sure the loop ends, or raise the limit if the work is really that long.

### Example

```vo
fn main() uses console {
    var i = 0;
    var total = 0;
    while i < 10 {
        set total = total + i;
    }
    print(str(total));
}
```

### Fixed

```vo
fn main() uses console {
    var i = 0;
    var total = 0;
    while i < 10 {
        set total = total + i;
        set i = i + 1;
    }
    print(str(total));
}
```

## R0010 capability_denied

The program tried to use an effect, path or variable outside the declaration of the running function.

Fixable: no

File paths and environment names are values, so scopes are also checked when the program runs: the path is resolved (`..`, absolute paths and symbolic links included) and must lie under a declared directory, and an env name must be one of the declared names. Two declarations apply: the running function's, and the root's (`main`'s, or the running test's or property's), so no callee can widen what the root declared. A path containing a NUL byte is always refused. The report names the target, the allowed scopes and the function whose declaration refused it. (A literal target outside the scopes is already E0503 at `vo check`.)

### Example

```vo
fn main() -> result[void] uses console, file.write("out/") {
    let path = "out/../x.txt";
    try write_file(path, "no");
    return ok();
}
```

### Fixed

```vo
fn main() -> result[void] uses file.write("./") {
    try write_file("x.txt", "yes");
    return ok();
}
```

## R0011 hole_reached

The program ran into a `todo(...)` hole.

Fixable: no

A hole checks as any type, but it has no value: running into one stops the program with its description. Write the missing code (W0601 lists every hole and what it needs).

### Example

```vo
fn parse(line: str) -> int {
    todo("parse the header");
}

fn main() uses console {
    print(str(parse("a,b")));
}
```

### Fixed

```vo
fn parse(line: str) -> int {
    return len(split(line, ","));
}

fn main() uses console {
    print(str(parse("a,b")));
}
```

## R0012 precondition_failed

A function was called with arguments its `requires` condition rejects.

Fixable: no

The report points at the call, and the trace starts at the caller: the caller broke the contract. Calls through function values and builtins such as `map` are checked the same way. Check the argument before the call, or fix the code that computed it.

### Example

```vo
fn isqrt(n: int) -> int
    requires n >= 0 {
    var r = 0;
    while (r + 1) * (r + 1) <= n {
        set r = r + 1;
    }
    return r;
}

fn main() uses console {
    print(str(isqrt(-4)));
}
```

### Fixed

```vo
fn isqrt(n: int) -> int
    requires n >= 0 {
    var r = 0;
    while (r + 1) * (r + 1) <= n {
        set r = r + 1;
    }
    return r;
}

fn main() uses console {
    print(str(isqrt(4)));
}
```

## R0013 postcondition_failed

A function returned a value its `ensures` condition rejects.

Fixable: no

The report points at the exit that produced the value (`return`, a `try` that passed an error on, or the closing `}` of a function without a return type), and the trace starts at the function: its body broke the contract. Fix the body, or the condition if it is wrong.

### Example

```vo
fn isqrt(n: int) -> int
    ensures ret * ret <= n and (ret + 1) * (ret + 1) > n {
    var r = 0;
    while (r + 1) * (r + 1) < n {
        set r = r + 1;
    }
    return r;
}

fn main() uses console {
    print(str(isqrt(4)));
}
```

### Fixed

```vo
fn isqrt(n: int) -> int
    ensures ret * ret <= n and (ret + 1) * (ret + 1) > n {
    var r = 0;
    while (r + 1) * (r + 1) <= n {
        set r = r + 1;
    }
    return r;
}

fn main() uses console {
    print(str(isqrt(4)));
}
```

## R0014 replay_mismatch

During `vo replay`, the program asked for an input that is not the next one in the recording.

Fixable: no

`vo replay` feeds the program exactly the inputs of the recorded run, in order: lines of stdin, files, environment variables, the clock. A program changed since the recording that asks for something else at some point (another file, another variable, more lines than were read) cannot be replayed past that point. The message names what the program asked for and what the recording holds. Replay the original program, or record the changed one again.

### Example

```sh
vo run --record r.jsonl main.vo    # main.vo reads config.txt
# main.vo now reads settings.txt first
vo replay r.jsonl
```

### Fixed

```sh
vo run --record r.jsonl main.vo    # record the changed program
vo replay r.jsonl
```

## R0015 task_result_order

A task returned a collection created with its own comparison function.

Fixable: no

Each task of `concurrent_map` sends its result back to the caller as data. An `ordered_map`, `ordered_set` or `priority_queue` made with `ordered_map_by`, `ordered_set_by` or `priority_queue_by` carries its comparison function, which cannot come back (a result type with a function in it is E0314, but this function is not part of the type). The same happens when a type parameter of the result stands for a function type. Return the items instead (`keys`, or a list), and build the collection after the call.

### Example

```vo
import std.collections.ordered_set_by;

fn backwards(n: int) -> ordered_set[int] {
    let s: ordered_set[int] = ordered_set_by(fn(a: int, b: int) -> int { return b - a; });
    push(s, n);
    push(s, n + 1);
    return s;
}

fn main() uses console {
    let sets = concurrent_map([1, 2], 2, backwards);
    print(str(len(sets)));
}
```

### Fixed

```vo
import std.collections.ordered_set_by;

fn items(n: int) -> list[int] {
    return [n, n + 1];
}

fn main() uses console {
    let lists = concurrent_map([1, 2], 2, items);
    let s: ordered_set[int] = ordered_set_by(fn(a: int, b: int) -> int { return b - a; });
    for xs in lists {
        for x in xs {
            push(s, x);
        }
    }
    print(str(len(s)));
}
```

## T0001 usage_error

The command line is not valid.

Fixable: no

The command, a flag or the number of arguments is not valid (exit status 64). The message lists the valid commands or options. This includes an option given to a command it does not belong to (`--trace` only works with `run`, `test` and `replay`, `--record` only with `run`, `--diff` only with `impact`, `--dry-run` only with `edit`), `--trace-limit` without `--trace`, `--seed`, `--clock-ms` or program arguments with `replay` (they come from the recording), and a trace or recording path that is a source file of the program.

### Example

```sh
vo frobnicate main.vo
```

### Fixed

```sh
vo check main.vo
```

## T0002 cannot_read_input

An input file or directory could not be read.

Fixable: no

vo could not open or read a file or directory it was given (exit status 66), usually because the path is misspelled or not readable. Check the path relative to the current directory.

### Example

```sh
vo run mian.vo
```

### Fixed

```sh
vo run main.vo
```

## T0003 internal_error

The toolchain hit an internal error. This is a bug in vo.

Fixable: no

The toolchain reached a state that should be impossible. It exits with status 125. This is always a bug in vo, not in your program. Please report it with the command, the output and the input file that caused it.

### Example

```sh
vo check main.vo
# error[T0003]: internal error
```

### Fixed

```sh
# No fix in your program: report the bug with main.vo and the full output.
vo version
```

## T0004 unknown_target

A query or edit names something that is not in the program.

Fixable: no

`vo query`, `vo impact` and `vo edit` take a target: `name` (a function, record or variant), `Record.field`, `Variant.case`, or `FILE:LINE:COL` (the name at that position). Only the entry file and the files it imports are searched. The message gives the closest name when there is one. Check the spelling, or point at the name with a position.

### Example

```sh
vo query refs main.vo parse_lin
```

### Fixed

```sh
vo query refs main.vo parse_line
```

## T0005 ambiguous_target

A name in a query or edit matches items in several files.

Fixable: no

Two files of a program can each declare an item with the same name when no file imports both. A plain `name` then does not say which one is meant. The report lists every declaration; give the one you mean as `FILE:LINE:COL`.

### Example

```sh
vo query refs main.vo helper
```

### Fixed

```sh
vo query refs main.vo util/a.vo:1:4
```

## T0006 edit_refused

`vo edit` refused an edit, and changed no file.

Fixable: no

Every edit is checked before it is written: the program is checked again as it would be after the edit, and the edit is refused when that adds a check error, or makes a name mean something other than before. The new name of a rename must be an identifier that is not a keyword, a builtin or already used where it would clash; an added field must be new and its default must have its type and be visible at every record literal. When the program already has errors, the edit must leave them as they are. The report gives the reason and the diagnostics the edit would cause. Choose another name or default, or fix the program first.

### Example

```sh
vo edit rename main.vo count total   # main already has a local named total
```

### Fixed

```sh
vo edit rename main.vo count item_count
```

## T0007 diff_mismatch

`vo impact` was given a diff it cannot use.

Fixable: no

`vo impact --diff FILE` reads a unified diff (as written by `git diff` or `diff -u`) of the program's current files: every context and added line must match the file as it is now. The diff is not one of these, or it was made before the files changed again. Make the diff again from the current files.

### Example

```sh
git diff > change.diff
# pricing.vo is edited again
vo impact main.vo --diff change.diff
```

### Fixed

```sh
git diff > change.diff
vo impact main.vo --diff change.diff
```

## T0008 invalid_recording

`vo replay` was given a file that is not a complete recording.

Fixable: no

A recording is the JSON-lines file written by `vo run --record FILE`: a `recording` header, the inputs, and an `end` record. A file that was cut short (for example when the run was killed), edited by hand, or is something else cannot be replayed. Record the run again.

### Example

```sh
vo replay main.vo
```

### Fixed

```sh
vo run --record r.jsonl main.vo
vo replay r.jsonl
```

## T0009 cannot_write_output

A trace or recording file could not be written.

Fixable: no

`--trace FILE` and `--record FILE` write their file next to the program's own output. When the file cannot be created (a missing directory, no permission) or a write fails (a full disk), the run still happens, the error is reported once at the end, and the exit status is 74 unless the run already failed. Choose a path that can be written.

### Example

```sh
vo run --trace out/missing-dir/t.jsonl main.vo
```

### Fixed

```sh
vo run --trace t.jsonl main.vo
```

## T0010 doc_not_found

`vo doc` names something that is not documented.

Fixable: no

`vo doc` takes a builtin (`print`), a module (`std.json`) or an item of a module (`std.json.parse_json`). Items without a description in their module are internal and are not listed. The message gives the closest documented name when there is one. Run `vo doc` with no name for the list of builtins and modules.

### Example

```sh
vo doc std.jsn
```

### Fixed

```sh
vo doc std.json
```
