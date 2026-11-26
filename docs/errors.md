# vo-lang Error Catalog

Every diagnostic, runtime error and toolchain error has a code. `vo explain CODE` prints its section.

Messages follow one style (`tests/message_style.py` checks it): code, names and paths are in backticks;
a test is `` `test "NAME"` ``, a package `` `NAME` ``; what was there is "found" (not "got"); a range is
"A to B". The code, the span and the fixes are the interface; a message's wording may change.

## E0101 invalid_character

A character that is not part of vo syntax, or a comment that does not stand on its own line.

Fixable: sometimes

The lexer found a character that no token can start with, such as `$` or `@`, or a `#` comment after code on the same line. Comments must be whole lines. The fix deletes the character or the trailing comment; move the comment to its own line if you want to keep it. The bit operators of other languages (`&`, `|`, `^`, `~`, `<<`, `>>`) are reported without a fix: import the functions of `std.bits` (`bit_and`, `shift_left`, ...).

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

A quoted string must open and close on the same line. The usual cause is a missing closing `"`, which can also cause follow-on parse errors on the next lines. The fix inserts the `"`. Use `\n` for a line break inside a quoted string, or a raw string between backticks, which may span lines. A raw string that is never closed runs to the end of the file and is reported at its opening backtick, without a fix.

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

A string contains an escape other than \n, \t, \r, \\, \" or \u{HEX}, or a malformed \u{HEX}.

Fixable: sometimes

A quoted string has six escapes: `\n`, `\t`, `\r`, `\\`, `\"` and `\u{HEX}` (1 to 6 hexadecimal digits naming a Unicode character, such as `\u{e5}` or `\u{1b}`). A single backslash before any other character, as in a Windows path or a regular expression, is an error; the fix doubles the backslash, and text full of backslashes is better written as a raw string between backticks: `` `\d+` ``. A `\u` must have braces, name a character (not a surrogate, at most 10FFFF), and not name one that has a short escape: `\u{a}` is written `\n`. A NUL is `\u{0}`.

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

A number literal is out of range or malformed: an integer above 2^63-1, a hex or binary literal without digits or with more than 64 bits, an upper-case `0X` or `0B`, a float with more than 17 significant digits or too large, an exponent without digits, letters after the number, a second spelling (`1E9`, `1e+9`, `007`), a float that rounds to zero (`1e-400`), or `-0x8000000000000000` (its negation overflows).

Fixable: sometimes

Integer literals must fit in a signed 64-bit integer (at most 9223372036854775807). Float literals may have at most 17 significant digits (a `float` holds no more), an optional exponent with digits (`1e9`, `1.5e-3`), and a value below about 1.8e308. A hex literal is `0x` and 1 to 16 hex digits, a binary literal `0b` and 1 to 64 digits, each a 64-bit pattern (`0xFFFFFFFFFFFFFFFF` is -1); the prefix is lower case (the fix lowers `0X` and `0B`). A number has no suffix, no `_` separators and no leading zeros: `10px`, `2em`, `1_000` and `007` are errors. The exponent is a lower-case `e` without `+` (the fix writes `1e9` for `1E9` and `1e+9`, and `7` for `007`). A float that is not zero but rounds to zero is an error, as one too large is: write `0.0`.

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

A `{` is never closed.

Fixable: yes

An opening `{` has no matching `}` before the end of the file. The diagnostic's `related` location points at the opening brace. The fix inserts the `}`, but check that it belongs where the fix puts it. An unclosed `(` or `[` is E0110 instead: the parser expects its closer where the expression ends.

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

Annotations come from a closed set: `@spec`, which links a `test` or `prop` to requirement IDs, and `@allow`, which accepts a structure warning (or W0506) on a `fn`, `test` or `prop`. The fix replaces a misspelled name (within two letters) with the closer of the two.

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

An annotation is not on the line right above the item it annotates (`@spec`: a `test` or `prop`; `@allow`: a `fn`, `test` or `prop`).

Fixable: no

`@spec` annotates the `test` or `prop` that follows it, and `@allow` the `fn`, `test` or `prop`, with nothing but blank lines or other annotations in between. On any other item, after a comment or at the end of the file, it is this error: move it above the test that proves the requirement.

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

An annotation is repeated: more than one `@spec` on an item, the same code in two `@allow`s, or a field annotation given twice.

Fixable: yes

Each `test` or `prop` has one `@spec` with all its IDs: the fix merges them into one. A repeated `@allow` or field annotation (`@size`, `@range`, ...) is removed by the fix: keep the one you mean.

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

## E0120 f_string

A string is written with an `f` prefix, as in Python.

Fixable: sometimes

vo has no f-strings and no interpolation. Use `format` with `{}` placeholders and pass the values as arguments, after the template: `format("hi {}", name)`. Options stay after a colon: `{:.2f}`. When every hole is a name or a field access, the fix writes the `format` call.

### Example

```vo
fn main() uses console {
    let name = "al";
    print(f"hi {name}");
}
```

### Fixed

```vo
fn main() uses console {
    let name = "al";
    print(format("hi {}", name));
}
```

## E0121 invalid_allow

An `@allow` annotation does not have a structure warning code and a reason.

Fixable: no

`@allow("W0703", "why it stays")` accepts one structure warning (W0701 to W0705) or W0506 on the function, test or property right below it. The reason is required: it tells the next reader why the code stays as it is. Other warnings and errors cannot be allowed: fix them.

### Example

```vo
@allow("W0703")
fn double(x: int) -> int {
    return x * 2;
}

fn main() uses console {
    print(str(double(2)));
}
```

### Fixed

```vo
fn double(x: int) -> int {
    return x * 2;
}

fn main() uses console {
    print(str(double(2)));
}
```

## E0122 const_in_function

A constant is declared inside a function.

Fixable: yes

Constants are items: `const name = value;` goes at the top level of a file, next to functions and records. Inside a function a value is named with `let`; the fix replaces `const` with `let`.

### Example

```vo
fn main() uses console {
    const greeting = "hi";
    print(greeting);
}
```

### Fixed

```vo
fn main() uses console {
    let greeting = "hi";
    print(greeting);
}
```

## E0201 undefined_name

A name is used but never defined or imported.

Fixable: sometimes

The name is not a local, parameter, top-level item, builtin or import in scope. It is usually a typo or a variable used outside the block that declares it. When a defined name is within two edits of it, the fix replaces it with that name. With module imports (since vo 0.4.0): an item of an imported module used without the module's name (`parse_json` after `import std.json;`) gets the fix `json.parse_json`; `json.nope` names the module that has no such item; a builtin written with a module name (`lists.len`) gets the fix `len`; and a module name used on its own is not a value.

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

Fixable: sometimes

Each top-level item (function, record, variant, test) and each field, case and parameter needs a unique name, because vo has no overloading. The diagnostic's `related` location points at the first definition. Rename or remove one of them. Module names count too (since vo 0.4.0): two imports that end in the same name, or a module named like an item of the file, cannot be told apart (there is no `import ... as`: rename a file); the same import written twice gets the fix that removes the second.

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

Fixable: sometimes

A local declaration (`let`, `var`, a parameter, a `for` variable or a `case` binding) reuses a name that is already in scope. The diagnostic's `related` location points at the outer definition. Give the inner one a different name, or use `set` on a `var` if you meant to change the outer one. A builtin's name (`len`, `min`, `lower`, ...) is not available for a local either (since vo 0.2.0), nor the name of a module the file imports (since vo 0.4.0: `text` with `import std.text;`): the fix renames the local to `NAME_value`, and each use reports the same rename.

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
import helpers;

fn main() uses console {
    print(helpers.greet("vo"));
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
import helpers;

fn main() uses console {
    print(helpers.greet("vo"));
}
```

## E0206 import_cycle

Files import each other in a cycle.

Fixable: no

Files cannot import each other in a cycle, directly or through other files. The diagnostic's `related` locations show the chain of imports. Move the shared items into a file that both sides import, so that imports only point one way.

### Example

```vo
# file: main.vo
import a;

fn main() uses console {
    print(a.one());
}
```

```vo
# file: a.vo
import b;

export fn one() -> str {
    return "one " + b.two();
}

export fn base() -> str {
    return "base";
}
```

```vo
# file: b.vo
import a;

export fn two() -> str {
    return "two " + a.base();
}
```

### Fixed

```vo
# file: main.vo
import a;

fn main() uses console {
    print(a.one());
}
```

```vo
# file: a.vo
import b;

export fn one() -> str {
    return "one " + b.two();
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

`let` bindings cannot be reassigned. The fix changes the declaration to `var`. Parameters and constants cannot be assigned either (no fix: copy the value into a `var`). Lists, maps and records bound with `let` can still be changed through `set xs[i]`, `set m[k]` and `set r.field`; only rebinding the name is forbidden.

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
import helpers;

fn main() uses console {
    print(helpers.greet("vo"));
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
import helpers;

fn main() uses console {
    print(helpers.greet("vo"));
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
import shapes;

fn main() uses console {
    print(str(shapes.area(new shapes.Size { w: 2, h: 3 })));
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
import shapes;

fn main() uses console {
    print(str(shapes.area(new shapes.Size { w: 2, h: 3 })));
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

## E0212 const_not_constant

A constant's value uses something other than literals, constants and operators.

Fixable: no

The checker works out a constant's value once, before the program runs, so the value can only be made of literals, other constants, parentheses and operators (`+ - * / %`, comparisons, `and`, `or`, `not`). Calls (even `len` or `str`), variables, lists, records and anything else are not constants: compute the value in a function, or write it out.

### Example

```vo
const words = len("one two");

fn main() uses console {
    print(str(words));
}
```

### Fixed

```vo
const words = 7;

fn main() uses console {
    print(str(words));
}
```

## E0213 const_cycle

Constants are defined from each other in a cycle.

Fixable: no

A constant may use other constants, but not itself, directly or through others: the message names the chain (`low -> high -> low`). Give one of them a value that does not depend on the other.

### Example

```vo
const low = high - 10;
const high = low + 10;

fn main() uses console {
    print(str(low) + " " + str(high));
}
```

### Fixed

```vo
const low = 0;
const high = low + 10;

fn main() uses console {
    print(str(low) + " " + str(high));
}
```

## E0214 library_changed

A standard library item was renamed, removed or changed in vo 0.2.0 or 0.3.0.

Fixable: sometimes

Before 1.0 a minor version may change the library, and every change comes with this error and, where one
correction keeps the program's behavior, its fix (`docs/stability.md`). Each use of an old name (`lists.contains`) is reported; the fix gives the new name
(`lists.has_item`), and an import of a changed item in the 0.3 form is reported too and keeps checking as
if the new name were written. A removed item's fix writes its replacement (`now_ms()` for `std.time.now`). A
lookup that now returns an `Option` is wrapped back into the old `result` (`to_result(CALL, "none")`), and
a function that now takes the text first gets its arguments swapped. A function that gained an argument
gets it inserted with the value that keeps the old behavior (`"./"`, the start directory, for the working
directory of `std.process.run_with`, `run_with_bytes` and `run_lines` in 0.3.0). Apply the fixes of
`vo check --json` to migrate a program; the changelog lists every change.

### Example

```vo
import std.lists;

fn main() uses console {
    print(str(lists.contains([1, 2], 2)));
}
```

### Fixed

```vo
import std.lists;

fn main() uses console {
    print(str(lists.has_item([1, 2], 2)));
}
```

## E0215 single_name_import

An import names one item, the form before vo 0.4.0.

Fixable: yes

An import names a module, and its items are used with the module's name: `import std.json;`, then `json.parse_json` and `json.Json`. The form `import std.json.parse_json;` was replaced in vo 0.4.0. The diagnostic appears once per file, at its first such import; the fix rewrites the file from its first import to the last use it changes, as one edit: the imports become one per module, every use of an item they bound is written with the module's name, and a local named like one of those modules is renamed `NAME_value`. When an import names a std item changed in vo 0.2.0 (E0214), apply the E0214 fixes first; this diagnostic has its fix in the next check.

### Example

```vo
import std.text.repeat;

fn main() uses console {
    print(repeat("ab", 2));
}
```

### Fixed

```vo
import std.text;

fn main() uses console {
    print(text.repeat("ab", 2));
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

An empty list literal `[]` has no type to take from its context.

Fixable: no

`[]` takes its type from the context: an annotation, a parameter, a return type or a field (since vo 0.2.0). With none, write `new list[T]`.

### Example

```vo
fn main() uses console {
    let xs = [];
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

A map key type contains a part that cannot be compared as a key.

Fixable: sometimes

Map keys are `int`, `bool`, `str`, `bytes`, and records and variants made of those. A `float` (NaN is not equal to itself), a list, a map or another collection (they change in place), a function or a `result` cannot be a key, alone or inside a record or variant; the message names the part. A type parameter used as a key must be declared `K: key`; the fix adds `: key` to a parameter declared without a constraint.

### Example

```vo
fn main() uses console {
    let m = new map[float, int];
    set m[0.5] = 1;
    print(str(len(m)));
}
```

### Fixed

```vo
fn main() uses console {
    let m = new map[int, int];
    set m[50] = 1;
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

Only `==` and `!=` work on every type. `<`, `<=`, `>`, `>=`, `min`, `max` and `sort` need a type parameter declared `ordered` (it can then be any ordered type: `int`, `float`, `str`, `bool`, `bytes`, and records and variants of ordered parts). A `key` type parameter is ordered too. The error points into the generic function; the fix adds `: ordered`.

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

Sorting, `min`, `max`, `<`, `<=`, `>`, `>=`, a `sort_by` key, an `ordered` type parameter and the keys of the ordered collections need an order: `int`, `float`, `str`, or a record whose fields all have one (compared field by field, in declaration order). Lists, maps, variants and functions have no order, so a record with such a field has none either; the message names that field. Sort by one of the ordered fields instead, or give the collection a comparison function. In a generic function, a `sort_by` key may have the type of a type parameter declared `ordered` (`C: ordered`); one without that constraint has no order.

### Example

```vo
record Task {
    name: str;
    tags: list[str];
}

fn main() uses console {
    let ts = [new Task { name: "b", tags: new list[str] }, new Task { name: "a", tags: new list[str] }];
    sort_by(ts, fn(t) => t);
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
    sort_by(ts, fn(t) => t.name);
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
    return new Job { name: name, greet: fn(s) => s + "!" };
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

## E0315 format_template_not_literal

The template of `format` is not a string literal.

Fixable: no

`format` checks every placeholder against its argument when the program is checked, so the template must be written in the call as a string literal, not built at run time or passed in a variable. Write the template in the call. To choose between templates, choose between calls.

### Example

```vo
fn main() uses console {
    let template = "{} items";
    print(format(template, 3));
}
```

### Fixed

```vo
fn main() uses console {
    print(format("{} items", 3));
}
```

## E0316 format_argument_count

The template of `format` has a different number of placeholders than there are arguments after it.

Fixable: sometimes

Each `{}` or `{:spec}` takes the next argument after the template, in order, so their numbers must be equal. Add or remove placeholders or arguments. A template written with C or Go verbs (`%d`, `%s`, `%.2f`, `%x`) has no placeholders; when every verb has a vo form, the fix rewrites them: `%d` and `%s` become `{}`, `%.2f` becomes `{:.2f}`, `%5s` becomes `{:>5}`, `%%` becomes `%`.

### Example

```vo
fn main() uses console {
    let n = 3;
    let price = 2.5;
    print(format("%d items, %.2f each", n, price));
}
```

### Fixed

```vo
fn main() uses console {
    let n = 3;
    let price = 2.5;
    print(format("{} items, {:.2f} each", n, price));
}
```

## E0317 format_placeholder_invalid

A placeholder or brace in a `format` template is not one vo accepts.

Fixable: sometimes

A placeholder is `{}` or `{:spec}`, with the spec's options in this order, all optional: an alignment `<`, `>` or `^`, `+`, `0` (pad with zeros), a width, `,` (thousands), `.Nf` (N decimals) and `x` or `b` (hex or binary). Write a brace as `{{` or `}}`. Placeholders take the arguments in order: there are no indexes (`{0}`) or names (`{name}`); pass the value as an argument. Precision needs `f` (`{:.2f}`), and type letters such as `d` and `s` are not used: the argument's type decides how it prints. The fix rewrites indexes in order, names with no arguments, `{:.2}`, `{:d}` and `{:0>5}`.

### Example

```vo
fn main() uses console {
    let x = 3.14159;
    print(format("{0}: {1:.2}", "pi", x));
}
```

### Fixed

```vo
fn main() uses console {
    let x = 3.14159;
    print(format("{}: {:.2f}", "pi", x));
}
```

## E0318 format_option_type

An option of a placeholder does not apply to its argument's type.

Fixable: sometimes

`.Nf` applies to `float` values, `x` and `b` to `int` values, and `+`, `0` and `,` to numbers; widths and alignments apply to every type. vo never converts implicitly: convert the argument, or change the option. For `.Nf` on an `int`, the fix converts the argument with `float(...)`.

### Example

```vo
fn main() uses console {
    let total = 12;
    print(format("{:.2f}", total));
}
```

### Fixed

```vo
fn main() uses console {
    let total = 12;
    print(format("{:.2f}", float(total)));
}
```

## E0319 regex_pattern_invalid

The pattern of `regex(...)` has a syntax error or is too large.

Fixable: no

The diagnostic points at the part of the pattern that is wrong: a group or class that is not closed, a quantifier with nothing before it, a reversed range or repetition, a `{` that does not start a repetition, a group name used twice. Patterns are limited (10,000 bytes, repetition counts up to 1,000, 20,000 instructions after repetitions, groups nested 200 deep) so that every pattern compiles and matches in bounded time and memory. Remember that the pattern is a vo string: a backslash is written `\\`.

### Example

```vo
import std.regex;

fn main() uses console {
    print(regex.regex("(\\d+").pattern);
}
```

### Fixed

```vo
import std.regex;

fn main() uses console {
    print(regex.regex("(\\d+)").pattern);
}
```

## E0320 regex_unsupported

The pattern uses a feature that vo's regular expressions do not have.

Fixable: sometimes

Every vo pattern runs in time linear in the text, so features that need backtracking are not available: backreferences (`\1`), lookahead and lookbehind (`(?=...)`, `(?<=...)`), atomic groups and possessive quantifiers. Match more, capture the parts, and check the rest in code. Unicode classes (`\p{L}`), anchors other than `^` and `$`, and comments are not supported either. Named groups are written `(?<name>...)`; the fix rewrites Python's `(?P<name>...)`.

### Example

```vo
import std.regex;

fn main() uses console {
    print(regex.regex("(?P<year>\\d{4})").pattern);
}
```

### Fixed

```vo
import std.regex;

fn main() uses console {
    print(regex.regex("(?<year>\\d{4})").pattern);
}
```

## E0321 regex_not_literal

`regex` is called with something other than a string literal.

Fixable: no

`regex` checks its pattern when the program is checked, so the pattern must be written in the call. For a pattern built at run time, such as one a user types, use `parse_regex(s)`, which returns a `result`: `err` when the pattern is not valid.

### Example

```vo
import std.regex;

fn main() uses console {
    let digits = "\\d+";
    print(regex.regex(digits).pattern);
}
```

### Fixed

```vo
import std.regex;

fn main() -> result[void] uses console {
    let digits = "\\d+";
    let re = try regex.parse_regex(digits);
    print(re.pattern);
    return ok();
}
```

## E0322 regex_replacement_invalid

The replacement text of `replace_matches` refers to a group in a way vo does not accept.

Fixable: sometimes

In a replacement, `$1` or `${1}` is group 1, `${name}` a named group and `$$` a dollar sign; any other `$` is an error, and so is a group the pattern does not have. Python's `\1` and `\g<name>` are not references in vo: the fix rewrites them to `$1` and `${name}`.

### Example

```vo
import std.regex;

fn main() uses console {
    print(regex.replace_matches("al@vo", regex.regex("(\\w+)@(\\w+)"), "\\2 at \\1"));
}
```

### Fixed

```vo
import std.regex;

fn main() uses console {
    print(regex.replace_matches("al@vo", regex.regex("(\\w+)@(\\w+)"), "$2 at $1"));
}
```

## E0323 sql_invalid

The SQL of `sql(...)` is not one valid statement that vo can run.

Fixable: no

`vo check` prepares every statement written with `sql("...")` against an empty database, so a syntax error, an incomplete statement or an unknown token is reported at its place. Each `sql(...)` holds exactly one statement: split a script into one `exec` per statement. SQL may not read the clock or randomness behind the program's back (`random()`, `randomblob()`, `CURRENT_TIMESTAMP`, `'now'`): compute the value in vo (`std.time`, `std.random`) and pass it as a `?` parameter. Three pragmas that would put files elsewhere or change the journal are refused. Table and column names are not checked here: the database is only there when the program runs.

### Example

```vo
import std.sqlite;

fn main() uses console {
    print(sqlite.sql("selec name from items").text);
}
```

### Fixed

```vo
import std.sqlite;

fn main() uses console {
    print(sqlite.sql("select name from items").text);
}
```

## E0324 sql_not_literal

`sql` is called with something other than a string literal.

Fixable: no

A statement is written as a literal, and values go in as `?` parameters, never into the text: SQL built with `+` is how SQL injection happens. Write the statement in the call with `?` where the values go, and pass them in the parameter list. For SQL that really must be built at run time, `parse_sql(s)` returns a `result`.

### Example

```vo
import std.sqlite;

fn main() -> result[void] uses console {
    let name = "al";
    let rows = try sqlite.query(sqlite.open_memory(), sqlite.sql("select '" + name + "' as n"), new list[sqlite.Value]);
    print(str(len(rows)));
    return ok();
}
```

### Fixed

```vo
import std.sqlite;

fn main() -> result[void] uses console {
    let name = "al";
    let rows = try sqlite.query(sqlite.open_memory(), sqlite.sql("select ? as n"), [sqlite.Value.text(name)]);
    print(str(len(rows)));
    return ok();
}
```

## E0325 sql_parameters

The parameters of a statement are written in another form, or the list has the wrong length.

Fixable: sometimes

Parameters are `?`, filled from the list in order: `:name`, `@name`, `$name` and `?1` are other spellings of the same thing. When each is used once (numbered ones in order), the fix rewrites them to `?`. A parameter list written in the call must have as many values as the statement has `?`.

### Example

```vo
import std.sqlite;

fn main() uses console {
    print(sqlite.sql("select * from items where id = :id").text);
}
```

### Fixed

```vo
import std.sqlite;

fn main() uses console {
    print(sqlite.sql("select * from items where id = ?").text);
}
```

## E0326 sqlite_memory_path

An in-memory database is opened by its SQLite file name, `":memory:"`.

Fixable: yes

An in-memory database needs no file and no effect: open it with `open_memory()`, which returns a `Database` with no effects, so tests that use it declare none.

### Example

```vo
import std.sqlite;

fn main() -> result[void] uses console, file.read, file.write {
    let db = try sqlite.open(":memory:");
    print(str(db.id));
    return ok();
}
```

### Fixed

```vo
import std.sqlite;

fn main() -> result[void] uses console {
    let db = sqlite.open_memory();
    print(str(db.id));
    return ok();
}
```

## E0327 effect_argument_syntax

Effects in a type are written in the wrong place or form.

Fixable: sometimes

A record or variant can take an effect parameter (`record Database[E: effects]`). In a type, its effects come last in the brackets, after `uses`, as in a function type: `Database[uses file.read, file.write]`. Without them the type has no effects (`Database`). The fix adds the missing `uses`. A type names effects without scopes (`file.read`, not `file.read("data/")`): the scopes belong to the grant and are checked where the value is used; the fix removes them. Code that takes any such value is generic over its effects: `fn save[D: effects](db: Database[D]) uses D` (`Database[uses D]` is the same type) takes a file database and an in-memory one alike.

### Example

```vo
record Conn[E: effects] {
    id: int;
}

fn ping(c: Conn[console]) -> int {
    return c.id;
}

fn main() uses console {
    print(str(ping(new Conn[uses console] { id: 1 })));
}
```

### Fixed

```vo
record Conn[E: effects] {
    id: int;
}

fn ping(c: Conn[uses console]) -> int {
    return c.id;
}

fn main() uses console {
    print(str(ping(new Conn[uses console] { id: 1 })));
}
```

## E0328 not_json

A type bound to a `json` type parameter has a part with no JSON form.

Fixable: sometimes

`to_json`, `from_json` (std.json) and std.web convert values by their type: `int`, `float`, `bool`, `str`, `bytes`, lists, `map[str, V]`, and records and variants made of them. A function, a map with `int` keys, an ordered collection, a `result` or a value with effects (a `Database`) has no JSON form; the message names the path to it (`items[].on_save`). Keep such parts out of the record you convert: convert a record of just the data. A type parameter passed on needs the constraint itself, `T: json`; the fix adds it.

### Example

```vo
import std.json;

fn convert[T](v: T) -> json.Json {
    return json.to_json(v);
}
```

### Fixed

```vo
import std.json;

fn convert[T: json](v: T) -> json.Json {
    return json.to_json(v);
}
```

## E0329 bad_field_annotation

A record field's annotation is unknown, misplaced or does not fit the field.

Fixable: sometimes

Annotations on the lines above a record field limit what `from_json` (and std.web) accepts for it: `@size(min, max)` for a `str` (characters), list or map; `@range(min, max)` for an `int` or `float`, with literals of the field's type; `@pattern("re")` for a `str`; `@default(v)` for a scalar, an enum-like variant (`Priority.normal`) or a list (`[]` only), which makes the field optional and must keep the field's other limits; `@header("Name")` for a scalar field of a std.web input record. Each at most once per field (a repeat is E0119); a variant case takes none, and an annotation must have a field below it. A misspelled name gets the nearest one as a fix, a whole number in a `float` range gets `.0`. A field of another type (a record) cannot have a default: make it a list of at most one item to make it optional.

### Example

```vo
record NewNote {
    @sise(1, 200)
    text: str;
}
```

### Fixed

```vo
record NewNote {
    @size(1, 200)
    text: str;
}
```

## E0330 bad_route_pattern

A std.web route pattern is not a literal, is malformed, or names a variable its operation cannot take.

Fixable: no

A route pattern is a string literal (so it can be checked): `/`, then segments separated by `/`, each either letters, digits, `-`, `_`, `.` and `~`, or a variable `{name}` that is a whole segment; no empty segment and no `/` at the end (except the pattern `/`). Each variable appears once, and the operation's input record has a field of that name whose type a path can give: `str`, `int`, `float`, `bool` or an enum-like variant.

### Example

```vo
import std.http;
import std.web;

record NoteId {
    id: int;
}

fn show(n: int, input: NoteId) -> result[http.Response] {
    return ok(web.no_content());
}

fn routes() -> list[web.Route[int]] {
    return [web.on_get("/notes/{note}", show)];
}
```

### Fixed

```vo
import std.http;
import std.web;

record NoteId {
    id: int;
}

fn show(n: int, input: NoteId) -> result[http.Response] {
    return ok(web.no_content());
}

fn routes() -> list[web.Route[int]] {
    return [web.on_get("/notes/{id}", show)];
}
```

## E0331 route_conflict

Two routes of one table and one method match the same paths.

Fixable: no

A request goes to the route whose pattern matches; where two patterns differ, a literal segment beats a variable (`/notes/latest` before `/notes/{id}`). Two routes of the same method whose patterns have the same literal segments and variables in the same places (`/notes/{id}` and `/notes/{name}`) match the same requests, so the second could never answer. Make the patterns differ, or remove one. Tables built in code, not written in one list, are checked when the application is served.

### Example

```vo
import std.http;
import std.web;

fn a(n: int, input: web.NoInput) -> result[http.Response] {
    return ok(web.no_content());
}

fn routes() -> list[web.Route[int]] {
    return [web.on_get("/notes", a), web.on_get("/notes", a)];
}
```

### Fixed

```vo
import std.http;
import std.web;

fn a(n: int, input: web.NoInput) -> result[http.Response] {
    return ok(web.no_content());
}

fn routes() -> list[web.Route[int]] {
    return [web.on_get("/notes", a), web.on_get("/notes/all", a)];
}
```

## E0332 const_type

A constant's type is not `int`, `float`, `bool` or `str`.

Fixable: no

Constants hold values that cannot change: `int`, `float`, `bool` and `str`. Lists, maps and records can be changed in place, so they are not constants; build them in a function.

### Example

```vo
const sizes: list[int] = 3;

fn main() uses console {
    print(str(len(sizes)));
}
```

### Fixed

```vo
fn sizes() -> list[int] {
    return [1, 2, 3];
}

fn main() uses console {
    print(str(len(sizes())));
}
```

## E0333 const_arithmetic

A constant's value overflows `int` or divides by zero.

Fixable: no

What would stop a run with R0001 or R0002 stops the check when it is a constant's value: the checker works it out with the same rules. Change the value or the expression.

### Example

```vo
const per_page = 0;
const pages = 100 / per_page;

fn main() uses console {
    print(str(pages));
}
```

### Fixed

```vo
const per_page = 20;
const pages = 100 / per_page;

fn main() uses console {
    print(str(pages));
}
```

## E0334 bad_error_type

The error type of a `result` cannot be read or printed.

Fixable: no

`result[T, E]` fails with a value of type `E` that callers match, compare and print: a variant of the ways to fail, a record, `str` or another value. `void` (nothing to read), a function type or a type that contains one cannot be an error. Use a variant, or `result[T]` for a message.

### Example

```vo
fn check(n: int) -> result[int, void] {
    return ok(n);
}

fn main() uses console {
    print(str(check(1) == ok(1)));
}
```

### Fixed

```vo
variant CheckError {
    negative;
}

fn check(n: int) -> result[int, CheckError] {
    if n < 0 {
        return err(CheckError.negative);
    }
    return ok(n);
}

fn main() uses console {
    print(str(check(1) == ok(1)));
}
```

## E0335 html_template_not_literal

The template of `html` is not a string literal or a constant.

Fixable: no

Retired: 2026-11-23, spec 053. `std.html` was removed from the core: vo is for backend services and
renders no HTML. No version of vo reports this code, and no other code will use it.

## E0336 html_template_hole

A hole of an `html` template is not `{}`, a brace is not doubled, or the number of holes and values differ.

Fixable: no

Retired: 2026-11-23, spec 053. `std.html` was removed from the core: vo is for backend services and
renders no HTML. No version of vo reports this code, and no other code will use it.

## E0337 html_unbalanced

The tags of an `html` template do not nest, an element is left open, or a void element has an end tag.

Fixable: no

Retired: 2026-11-23, spec 053. `std.html` was removed from the core: vo is for backend services and
renders no HTML. No version of vo reports this code, and no other code will use it.

## E0338 html_unsafe_hole

A hole of an `html` template is in a place where no escaping can make a value safe.

Fixable: sometimes

Retired: 2026-11-23, spec 053. `std.html` was removed from the core: vo is for backend services and
renders no HTML. No version of vo reports this code, and no other code will use it.

## E0339 html_markup_in_attribute

A value of an `html` template does not fit its hole: markup in an attribute, or a URL that is not text.

Fixable: no

Retired: 2026-11-23, spec 053. `std.html` was removed from the core: vo is for backend services and
renders no HTML. No version of vo reports this code, and no other code will use it.

## E0340 html_opaque

An `Html` value is built with `new`, or its field is read or set, outside `std.html`.

Fixable: no

Retired: 2026-11-23, spec 053. `std.html` was removed from the core: vo is for backend services and
renders no HTML. No version of vo reports this code, and no other code will use it.

## E0341 bad_metric_name

A literal metric or label name is not one the metrics text format allows, or a label is repeated.

Fixable: sometimes

A metric name (the first argument of `counter`, `gauge` or `histogram`) is letters, digits, `_` and `:`, not starting with a digit; a label name is letters, digits and `_`, not starting with a digit, and not starting with `__` (reserved for the scraper); each label is named once. The error points at the first character that breaks the rule. The fix replaces each character the rule does not allow with `_`, the format's own convention. A label starting with `__` or a repeated label has no fix: the right name is the author's choice. Names that are not literals are checked when the program runs.

### Example

```vo
import std.metrics;

fn requests() -> metrics.Metric {
    return metrics.counter("http-requests", "Requests answered.", new list[str]);
}

fn main() {
    let _ = requests();
}
```

### Fixed

```vo
import std.metrics;

fn requests() -> metrics.Metric {
    return metrics.counter("http_requests", "Requests answered.", new list[str]);
}

fn main() {
    let _ = requests();
}
```

## E0342 lambda_types_written

An anonymous function writes its parameter or return types.

Fixable: sometimes

An anonymous function takes its types from where it is used (spec 059): the parameter of the function it is passed to, the type of the binding, field or list it goes into, or the return type it is returned as. It writes its parameters' names only, `fn(p)`, and no `-> T`. Named functions write every type, as before. The fix removes the types when the place gives the same ones; when the function is the whole value of a `let` or `var` without a type, it moves them onto the binding (`let f: fn(int) -> int = fn(x) ...`). A written type that differs from the place's is reported as the mismatch it is (E0301), so a fix never changes what a program means. Where the place cannot give a type (a call whose type parameter appears only in the function's own parameters), there is no fix: pass a named function, or bind the function with its type first.

### Example

```vo
fn main() uses console {
    let words = ["pear", "fig"];
    print(join(map(words, fn(w: str) -> str {
        return w + "!";
    }), ","));
}
```

### Fixed

```vo
fn main() uses console {
    let words = ["pear", "fig"];
    print(join(map(words, fn(w) => w + "!"), ","));
}
```

## E0343 lambda_type_unknown

An anonymous function has parameters but nothing to take their types from.

Fixable: no

An anonymous function writes only its parameters' names; their types come from where it is used: the parameter of the function it is passed to, the type of the binding, field or list it goes into, or the return type it is returned as (spec 059). `let f = fn(x) => x + 1;` gives it none, and neither does a call whose type parameter appears only in the function's own parameters (`on[I](f: fn(I) -> int)` called with `fn(x) => 1`). Give the binding a function type, or pass a named function, whose parameters are written. An anonymous function without parameters takes its return type from its body and needs no context.

### Example

```vo
fn main() uses console {
    let inc = fn(x) => x + 1;
    print(str(inc(1)));
}
```

### Fixed

```vo
fn main() uses console {
    let inc: fn(int) -> int = fn(x) => x + 1;
    print(str(inc(1)));
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

`vo run` and `vo build` need a `fn main()`.

Fixable: no

`vo run` and `vo build` start the program at `fn main()`, and the file has none. `vo check` and `vo test` do not need `main` (but report E0409 for one with the wrong signature). Add a `main` function, or use `vo test` if the file only holds tests.

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

`case NAME()` on anything but the success of a `result[void]`, or `Type.case()` for a case without a value.

Fixable: yes

Empty parentheses in a `case` mean "the success of a `result[void]`", which `ok()` builds and `case ok()` matches. A case that carries a value binds it with `case NAME(v)` or ignores it with `case NAME(_)`; a variant case without a value is written `case NAME`, and built with `Type.case` (`Shape.empty`, not `Shape.empty()`). The fix writes the matching form.

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

## E0412 try_error_mismatch

`try` on a `result` whose error type differs from the function's.

Fixable: no

`try` passes an error up unchanged, so the error types must be the same: `try` in a function returning `result[T, LoadError]` takes a `result[U, LoadError]`. An error of another type is converted where it crosses the boundary, with `map_err(r, f)`; `main` and tests fail with a message (`result[void]`), so a typed error is converted to a `str` before it reaches them.

### Example

```vo
variant LoadError {
    missing(str);
}

fn load(path: str) -> result[int, LoadError] {
    if path == "" {
        return err(LoadError.missing("config"));
    }
    return ok(1);
}

fn main() -> result[void] uses console {
    print(str(try load("a")));
    return ok();
}
```

### Fixed

```vo
variant LoadError {
    missing(str);
}

fn load(path: str) -> result[int, LoadError] {
    if path == "" {
        return err(LoadError.missing("config"));
    }
    return ok(1);
}

fn describe(e: LoadError) -> str {
    match e {
        case missing(what) {
            return "missing " + what;
        }
    }
}

fn main() -> result[void] uses console {
    print(str(try map_err(load("a"), describe)));
    return ok();
}
```

## E0413 lambda_single_return

An anonymous function's body only returns a value, written as a block.

Fixable: yes

A body that is a single `return expr;` has one form: `fn(p) => expr` (spec 059). The block form is kept for every other body, including one that returns nothing and one that holds a comment. The fix replaces the block with `=> expr`, and `vo fmt` does the same.

### Example

```vo
fn main() uses console {
    let words = ["pear", "fig"];
    print(join(map(words, fn(w) {
        return w + "!";
    }), ","));
}
```

### Fixed

```vo
fn main() uses console {
    let words = ["pear", "fig"];
    print(join(map(words, fn(w) => w + "!"), ","));
}
```

## E0501 missing_effect

A function or test does something its `uses` clause does not declare.

Fixable: yes

Every function and test declares what it may touch: `uses console, file.read, ...`. A function also needs everything its callees declare, so a declaration tells the truth about everything reachable from it. The message lists every missing effect (including those of callees that are missing their own declarations), and the fix rewrites the clause to declare them all. A file effect needed only for literal paths is declared with those paths as scopes (a file's directory, a directory as written: `read_file("data/a.txt")` gives `file.read("data/")`); any other need declares it without scopes.

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
    print(str(apply(fn(x) {
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
    print(str(apply(fn(x) {
        print("called");
        return x;
    }, 1)));
}
```

## E0503 scope_too_narrow

A function declares an effect with scopes that do not cover what a callee declares.

Fixable: yes

A scoped declaration such as `file.write("out/")` covers callees that write under `out/`; `env("HOME")` covers callees that read `HOME`. A callee that declares other scopes needs a wider caller. A callee that declares the effect without scopes (a function that is given its path, host or name) needs none: the caller's scopes apply to it when the program runs (R0010). The fix adds the callee's scopes to the caller's; the related note points at the callee's declaration.

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
    let f = fn() => total;
    print(str(f()));
}
```

### Fixed

```vo
fn main() uses console {
    var total = 1;
    let total_now = total;
    let f = fn() => total_now;
    print(str(f()));
}
```

## E0506 invalid_scope

A scope in a `uses` clause is empty, absolute, or leaves the working directory with `..`.

Fixable: no

File scopes are directories relative to the directory the program starts in, such as `"out/"` or `"data/raw/"`; `"./"` is the working directory itself. Environment and network scopes are non-empty names. Program scopes (`process`) are a name found through `PATH`, or a relative or absolute path to the program, without `..` and not ending in `/`.

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
    let f: fn(int) uses console = fn(x) {
        print(str(x));
    };
    f(1);
}
```

## E0509 unknown_effect

A `uses` clause names something that is not an effect.

Fixable: sometimes

The effects are `console`, `input`, `file.read`, `file.write`, `file.delete`, `net`, `clock`, `random`, `env`, `serve` and `process`. When the name is within two edits of one of them, the fix replaces it. A capitalized name must be an effect parameter or an effect set in scope (`effects Storage = ...;`, declared in the file or imported); the fix replaces it with the nearest one.

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

## E0512 process_unscoped

`uses process` does not name the programs it may run.

Fixable: no

A `process` grant without scopes would allow running every program on the machine, and a started program can do anything the user running `vo` can. Name each program the code runs: a name found through `PATH` (`"git"`), or a relative or absolute path (`"tools/gen"`, `"/usr/bin/git"`). Which programs to allow is the reviewer's decision, so there is no automatic fix.

### Example

```vo
import std.process;

fn main() -> result[void] uses console, process {
    let out = try process.run("git", ["--version"]);
    print(out.stdout);
    return ok();
}
```

### Fixed

```vo
import std.process;

fn main() -> result[void] uses console, process("git") {
    let out = try process.run("git", ["--version"]);
    print(out.stdout);
    return ok();
}
```

## E0513 effect_set_cycle

Effect sets include each other in a cycle.

Fixable: no

A set may name other sets (`effects App = console, Storage;`), but not itself, directly or through others: the message names the chain (`Files -> Storage -> Files`). Write the shared effects in one set and name it from the others.

### Example

```vo
effects Files = file.read("data/"), Storage;
effects Storage = file.write("data/"), Files;

fn main() uses console, Storage {
    print(str(write_file("data/note.txt", "hi") == ok()));
}
```

### Fixed

```vo
effects Files = file.read("data/");
effects Storage = file.write("data/"), Files;

fn main() uses console, Storage {
    print(str(write_file("data/note.txt", "hi") == ok()));
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

## W0102 raw_string_not_needed

A raw string is used for text that needs no backticks.

Fixable: yes

A raw string, between backticks, takes its text as written: no escapes, and line breaks allowed. It is for text with backslashes, quotes or several lines: a regular expression, JSON, SQL. Text with none of these is written in quotes, so that each text has one spelling. The fix, and `vo fmt`, rewrite it.

### Example

```vo
fn main() uses console {
    print(`hello`);
}
```

### Fixed

```vo
fn main() uses console {
    print("hello");
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

## W0202 unused_constant

A constant that is not exported is never used.

Fixable: yes

A private constant that no function, test, other constant, effect scope or annotation of its file refers to is left over from an edit; the fix removes its declaration. Exported constants are not reported: other files may import them.

### Example

```vo
const limit = 10;
const spare = 3;

fn main() uses console {
    print(str(limit));
}
```

### Fixed

```vo
const limit = 10;

fn main() uses console {
    print(str(limit));
}
```

## W0203 unused_import

A module is imported but nothing of it is used.

Fixable: yes

No name of the file is written with the module's name (`json.parse_json`, `json.Json`, an effect set or a constant of it), so the import is left over from an edit; the fix removes the line. The list of imports then says which modules a file reaches (since vo 0.4.0).

### Example

```vo
import std.json;
import std.text;

fn main() uses console {
    print(text.repeat("ab", 2));
}
```

### Fixed

```vo
import std.text;

fn main() uses console {
    print(text.repeat("ab", 2));
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

## W0403 for_reads_entry

A loop reads the entry of its collection first thing, where a loop with two variables gives both.

Fixable: yes

`for k, v in m` gives each key of a map with its value, and `for i, x in xs` each index of a list with its element. A loop over `keys(m)` that begins with `let v = m[k];`, or over `0..len(xs)` that begins with `let x = xs[i];`, is that loop written out, so the fix rewrites it. Both forms visit what the collection had when the loop began. The warning is given only when the map or list is a `let` or a parameter and the read is the first statement.

### Example

```vo
fn main() uses console {
    let prices = new map[str, int];
    set prices["tea"] = 3;
    for name in keys(prices) {
        let price = prices[name];
        print(format("{}: {}", name, price));
    }
}
```

### Fixed

```vo
fn main() uses console {
    let prices = new map[str, int];
    set prices["tea"] = 3;
    for name, price in prices {
        print(format("{}: {}", name, price));
    }
}
```

## W0501 unused_effect

A `uses` clause declares an effect or scope that nothing in the function needs.

Fixable: yes

Declarations are the capabilities a function is granted, so unneeded ones are removed. A scope counts as needed when a callee's scope lies under it, or when a builtin uses the effect.

### Example

```vo
fn add(a: int, b: int) -> int uses console {
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
import geom;

fn main() uses console {
    print(str(geom.area(2, 3)));
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
import geom;

fn main() uses console {
    print(str(geom.area(2, 3)));
}
```

```text
# file: vo.effects
geom.vo:area pure
main.vo:main uses console
```

## W0505 unused_effect_set

An effect set that is not exported is named by no clause.

Fixable: yes

A set (`effects Storage = ...;`) is a name for effects a program repeats. One that no `uses` clause, type or other set of its file names is left over from an edit; the fix removes its declaration. Exported sets are not reported: other files may import them.

### Example

```vo
effects Storage = file.read("data/"), file.write("data/");
effects Spare = clock;

fn main() uses console, Storage {
    print(str(write_file("data/note.txt", "hi") == ok()));
}
```

### Fixed

```vo
effects Storage = file.read("data/"), file.write("data/");

fn main() uses console, Storage {
    print(str(write_file("data/note.txt", "hi") == ok()));
}
```

## W0506 secret_logged

A log record's field takes its value from a name that looks like a secret.

Fixable: yes

`field(NAME, VALUE)` in a `log_debug`, `log_info`, `log_warn` or `log_error` call, where VALUE is a variable, parameter or field access whose last name is secret-like: one of its words (split at `_`) is password, passwd, secret, token, key, authorization, cookie or session. The runtime writes such a field's value as `[redacted]` only when the field's own name is secret-like too, so the value can reach the log under another name; the fix removes the field. Values computed from a secret (`hash(pw)`) are not followed. `@allow("W0506", "reason")` accepts a name that is not a secret.

### Example

```vo
import std.log;

fn login(user: str, password: str) uses log {
    log.log_info("login", [log.field("user", user), log.field("pw", password)]);
}

fn main() uses log {
    login("ada", "hunter2");
}
```

### Fixed

```vo
import std.log;

fn login(user: str, password: str) uses log {
    log.log_info("login", [log.field("user", user)]);
}

fn main() uses log {
    login("ada", "hunter2");
}
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

## W0602 test_checks_nothing

A test or property has no `expect` or `expect_eq`, so it can only fail by a runtime error.

Fixable: no

A test proves something only through its checks. One that calls code and prints the result passes whatever the result is, so a wrong change goes unnoticed. Add an `expect` or `expect_eq` that states what must hold. A test that calls a function with `ensures` is not reported (the contract checks each result), and a test with a hole (`todo`) is reported as W0601 instead.

### Example

```vo
fn double(x: int) -> int {
    return x * 2;
}

fn main() uses console {
    print(str(double(2)));
}

test "doubles" {
    let _ = double(4);
}
```

### Fixed

```vo
fn double(x: int) -> int {
    return x * 2;
}

fn main() uses console {
    print(str(double(2)));
}

test "doubles" {
    expect_eq(double(4), 8);
}
```

## W0603 constant_check

This check always has the same outcome: it does not depend on the program.

Fixable: no

`expect(true)`, `expect(1 < 2)` or `expect_eq(3, 3)` compares literals with literals, so it passes (or fails) whatever the program does. A literal is a number, string or bool constant, or a list, record or case built only of literals. Check a value the program computes instead.

### Example

```vo
fn double(x: int) -> int {
    return x * 2;
}

fn main() uses console {
    print(str(double(2)));
}

test "doubles" {
    expect_eq(double(4), 8);
    expect_eq(8, 8);
}
```

### Fixed

```vo
fn double(x: int) -> int {
    return x * 2;
}

fn main() uses console {
    print(str(double(2)));
}

test "doubles" {
    expect_eq(double(4), 8);
    expect_eq(double(-4), -8);
}
```

## E0701 package_changed

A package's files no longer match the hash its manifest pins.

Fixable: no

`vo.packages` (next to the entry file or above) pins each package to a SHA-256 hash of all its `.vo` files: their paths, lengths and bytes. Any changed byte, added, removed or renamed file changes the hash. When a check uses the package and its files hash differently, nothing of it is checked or run: the message gives both hashes. Review what changed in the package, then pin it again with `vo package ENTRY NAME DIRECTORY`, which writes the new hash.

### Example

```vo
import money.format;

fn main() uses console {
    print(format.cents(1234));
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
import money.format;

fn main() uses console {
    print(format.cents(1234));
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
import money.format;

fn main() uses console {
    print(format.cents(1234));
}
```

```text
# file: vo.packages
money shared/money sha256:0000000000000000000000000000000000000000000000000000000000000000
```

### Fixed

```vo
import money.format;

fn main() uses console {
    print(format.cents(1234));
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
import money.format;

fn main() uses console {
    print(format.cents(1234));
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
import money.format;

fn main() uses console {
    print(format.cents(1234));
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
import money.format;

fn main() uses console {
    print(format.cents(1234));
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
import money.format;

fn main() uses console {
    print(format.cents(1234));
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

`import money.format;` could mean the local file `money/format.vo` or the package `money`. Rather than pick one silently, vo reports the clash. Rename the local directory, or list the package under another name.

### Example

```vo
import money.format;

fn main() uses console {
    print(format.cents(1234));
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
import cash.format;

fn main() uses console {
    print(format.cents(1234));
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

## E0706 package_fetch_failed

A package could not be fetched, or its archive was refused.

Fixable: no

`vo package ENTRY NAME URL` downloads an archive over HTTPS, unpacks its `.vo` files and pins them. It fails with this code, and changes nothing, when the download fails (the connection, the certificate, a status other than 200, more than five redirects or one to `http://`, more than 16 MiB, more than 60 seconds) or when the archive is not acceptable: damaged or cut short, not a tar archive, holding a link, a device or a path that leaves the package (`..`, an absolute path), or over a limit (64 MiB unpacked, 16 MiB per file, 10,000 entries). The message says which. Nothing of a refused archive is written, and `vo check`, `vo run`, `vo test` and `vo build` never fetch anything.

### Example

```text
$ vo package main.vo money https://example.com/money-1.2.0.tar.gz
error[E0706]: Cannot fetch package money from https://example.com/money-1.2.0.tar.gz: the server answered 404.
```

### Fixed

```text
$ vo package main.vo money https://example.com/archive/money-1.2.0.tar.gz
added money packages/money sha256:1e5b240df8daf715941d8af2629ab6404c7f9d1d369e493f0e9eb2d01fc162c5 from https://example.com/archive/money-1.2.0.tar.gz
  effects: none
```

Check the URL in a browser: it must download a `.tar.gz` (or `.tar`) archive directly. For a repository on a forge, use the archive link of a tag or commit, not of a branch, so that the pinned content does not change.

## W0701 effectful_logic

A function with effects has 15 or more statements that use no effect: its logic belongs in pure functions.

Fixable: no

Keep a program's decisions and computations in pure functions, and the functions with effects (`main`, the ones that read, write, print or fetch) thin: they gather input, call the pure core, and act on its result. Pure functions are the ones a test can call with no `uses` clause, no files and no grant. The warning counts the statements, at every depth, whose own expressions use no effect, and points at the longest stretches of them: move those into a function without `uses`, and call it. Tests and properties are not held to this rule.

### Example

```vo
fn main() uses console {
    let prices = [120, 80, 45];
    let currency = "EUR";
    var total = 0;
    for p in prices {
        set total = total + p;
    }
    let count = len(prices);
    let average = total / count;
    var discount = 0;
    if total > 200 {
        set discount = total / 10;
    }
    let net = total - discount;
    let tax = net * 25 / 100;
    let gross = net + tax;
    let rounded = gross / 10 * 10;
    let label = "total: " + str(rounded) + " " + currency;
    print(label);
    print("average: " + str(average));
}
```

### Fixed

```vo
record Summary {
    label: str;
    average: int;
}

fn summarize(prices: list[int]) -> Summary {
    var total = 0;
    for p in prices {
        set total = total + p;
    }
    var discount = 0;
    if total > 200 {
        set discount = total / 10;
    }
    let net = total - discount;
    let gross = net + net * 25 / 100;
    return new Summary { label: "total: " + str(gross / 10 * 10) + " EUR", average: total / len(prices) };
}

fn main() uses console {
    let s = summarize([120, 80, 45]);
    print(s.label);
    print("average: " + str(s.average));
}

test "summarize" {
    expect_eq(summarize([100, 100]).label, "total: 250 EUR");
}
```

## W0702 file_too_long

A file has more than 400 code lines: split it into modules, one concept each.

Fixable: no

A module (a file) holds one concept: the money arithmetic, the parser, the report. Code lines are lines that are neither blank nor only a comment. A file past 400 of them does several jobs; split it and import what each part needs (`import money;`, then `money.cents`). When a file must stay whole (one module per import path), `@allow("W0702", "reason")` on its first function, test or property accepts it.

### Example

```vo
# (the next line, 400 times: helper_1 to helper_400)
fn helper_N() -> int { return 1; }

fn main() uses console {
    print(str(helper_1()));
}
```

### Fixed

```vo
# file: helpers_a.vo
# (the next line, 200 times: helper_1 to helper_200)
export fn helper_N() -> int { return 1; }
```

```vo
# file: helpers_b.vo
# (the next line, 200 times: helper_201 to helper_400)
export fn helper_N() -> int { return 1; }
```

```vo
import helpers_a;

fn main() uses console {
    print(str(helpers_a.helper_1()));
}
```

## W0703 function_too_long

A function, test or property has more than 50 statements: split it into functions that each do one thing.

Fixable: no

Statements are counted at every depth, those of anonymous functions included. A long function usually does several things in a row: give each its own function with a name that says what it does. `@allow("W0703", "reason")` accepts a function that reads best whole (a scanner, a table of cases).

### Example

```vo
# (the next line, 50 times: step_1 to step_50)
fn step_N(x: int) -> int { return x + 1; }

fn pipeline(x: int) -> int {
    var v = x;
    # (the next line, 50 times: step_1 to step_50)
    set v = step_N(v);
    return v;
}

fn main() uses console {
    print(str(pipeline(0)));
}
```

### Fixed

```vo
# (the next line, 50 times: step_1 to step_50)
fn step_N(x: int) -> int { return x + 1; }

fn first_half(x: int) -> int {
    var v = x;
    # (the next line, 25 times: step_1 to step_25)
    set v = step_N(v);
    return v;
}

fn second_half(x: int) -> int {
    var v = x;
    # (the next line, 25 times: step_26 to step_50)
    set v = step_N(v);
    return v;
}

fn main() uses console {
    print(str(second_half(first_half(0))));
}
```

## W0704 duplicate_code

Six or more consecutive statements repeat earlier ones, up to local names and literal values.

Fixable: no

Copied code drifts apart: a fix in one copy misses the others. Two runs of statements of one block count as copies when they are equal after renaming their local variables and parameters and replacing their literals; calls, fields, case names and operators must match. The later copy is reported, with the first as a related location. Make one function and pass what differs as parameters. Functions that differ only in their effects (one takes `Database[uses file.read, file.write]`, the other `Database`) are one function with an effect parameter, `[E: effects]`. Tests may repeat themselves and are not reported.

### Example

```vo
fn price(base: int) -> int {
    let tax = base * 25 / 100;
    let fee = 3;
    var total = base + tax;
    set total = total + fee;
    if total > 1000 {
        set total = total - 10;
    }
    return total;
}

fn cost(amount: int) -> int {
    let vat = amount * 12 / 100;
    let charge = 5;
    var sum = amount + vat;
    set sum = sum + charge;
    if sum > 500 {
        set sum = sum - 7;
    }
    return sum;
}

fn main() uses console {
    print(str(price(100) + cost(100)));
}
```

### Fixed

```vo
fn with_charges(base: int, percent: int, fee: int, cap: int, rebate: int) -> int {
    let tax = base * percent / 100;
    var total = base + tax + fee;
    if total > cap {
        set total = total - rebate;
    }
    return total;
}

fn main() uses console {
    print(str(with_charges(100, 25, 3, 1000, 10) + with_charges(100, 12, 5, 500, 7)));
}
```

## W0705 test_elsewhere

A test calls only functions of one other file: move it into that file, next to the code it tests.

Fixable: no

A test belongs next to the code it tests, so that reading, changing and moving that code takes its tests along. `vo test main.vo` runs the tests of every file the program loads, so a test in `money.vo` runs with the others. A test that calls functions of two or more files (or of its own file) tests how they work together and is not reported.

### Example

```vo
# file: money.vo
export fn cents(units: int) -> int {
    return units * 100;
}
```

```vo
import money;

fn main() uses console {
    print(str(money.cents(2)));
}

test "cents of units" {
    expect_eq(money.cents(3), 300);
}
```

### Fixed

```vo
# file: money.vo
export fn cents(units: int) -> int {
    return units * 100;
}

test "cents of units" {
    expect_eq(cents(3), 300);
}
```

```vo
import money;

fn main() uses console {
    print(str(money.cents(2)));
}
```

## W0706 unused_allow

An `@allow` accepts a structure warning that is not there.

Fixable: yes

`@allow("W07NN", "reason")` accepts one structure warning on the function, test or property below it. When the code no longer has that warning, the annotation only misleads: the fix removes its line.

### Example

```vo
@allow("W0703", "one scanner, read top to bottom")
fn double(x: int) -> int {
    return x * 2;
}

fn main() uses console {
    print(str(double(2)));
}
```

### Fixed

```vo
fn double(x: int) -> int {
    return x * 2;
}

fn main() uses console {
    print(str(double(2)));
}
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

A builtin got a value it cannot work with: `int(...)` of NaN or of a float out of range, `split` with an empty separator, `slice` of a `str` at a position inside a multi-byte character, an integer outside 0 to 255 for `std.bytes.from_ints`, text that is not UTF-8 where no `result` can say so (`read_all`, `args`), or a count outside 0 to 65536 for `std.crypto.random_bytes` (which also stops here if the system's random source fails: it never continues with other bytes). The message says which. Check the value first. For `slice`, use byte offsets that `find` returned, or work on the `list[str]` from `chars`. For input that may not be text, read it as `bytes` (`std.bytes.read_stdin`, `std.fs.read_bytes`) and convert with `std.bytes.to_text`.

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

File paths and environment names are values, so scopes are also checked when the program runs: the path is resolved (`..`, absolute paths and symbolic links included) and must lie under a declared directory, and an env name must be one of the declared names. Every declaration on the call path that has scopes for the effect applies, from the running function out to the root (`main`, or the running test or property): a function that declares the effect without scopes is held to its callers' scopes, so no callee can widen what its callers or the root declared. A path containing a NUL byte is always refused. The report names the target, the allowed scopes and the function whose declaration refused it (the one nearest the operation when several do). (A literal target outside the scopes is already E0503 at `vo check`.)

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

`vo replay` feeds the program exactly the inputs of the recorded run, in order: lines of stdin, files, environment variables, the clock. A program changed since the recording that asks for something else at some point (another file, another variable, more lines than were read) cannot be replayed past that point. The message names what the program asked for and what the recording holds. Replay the original program, or record the changed one again. In `vo test --replay DIR` (spec 054) it fails only the test that asked, also when the test sends another HTTP body or other bytes over TCP, or has no recording; the message gives the command that records that test again.

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

A task returned a collection created with its own comparison function, or a worker's message holds one or a function.

Fixable: no

Each task of `concurrent_map` sends its result back to the caller as data. An `ordered_map`, `ordered_set` or `priority_queue` made with `ordered_map_by`, `ordered_set_by` or `priority_queue_by` carries its comparison function, which cannot come back (a result type with a function in it is E0314, but this function is not part of the type). The same happens when a type parameter of the result stands for a function type. Return the items instead (`keys`, or a list), and build the collection after the call.

The messages of a worker (`std.workers`) cross between processes in the same way: `start_worker` stops with this error when a message type holds a function, and `tell` or `emit` when a message holds a collection with its own comparison function.

### Example

```vo
import std.collections;

fn backwards(n: int) -> ordered_set[int] {
    let s: ordered_set[int] = collections.ordered_set_by(fn(a, b) => b - a);
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
import std.collections;

fn items(n: int) -> list[int] {
    return [n, n + 1];
}

fn main() uses console {
    let lists = concurrent_map([1, 2], 2, items);
    let s: ordered_set[int] = collections.ordered_set_by(fn(a, b) => b - a);
    for xs in lists {
        for x in xs {
            push(s, x);
        }
    }
    print(str(len(s)));
}
```

## R0016 metric_conflict

A metric was used in a way that contradicts its first use.

Fixable: no

A metric's name has one kind, help text and label list for the whole program, and every worker process shares it: a second use of the name as another kind, with another help text or other labels is this error. So is giving a series fewer or more label values than the metric has labels, and adding a negative amount to a counter, which only goes up (use a gauge for a value that goes down). Define each metric once, in one function or constant place, and use that value everywhere.

### Example

```vo
import std.metrics;

fn main() uses log {
    metrics.add_to(metrics.counter("jobs_total", "Jobs run.", new list[str]), new list[str], 1);
    metrics.add_to(metrics.gauge("jobs_total", "Jobs run.", new list[str]), new list[str], 1);
}
```

### Fixed

```vo
import std.metrics;

fn main() uses log {
    metrics.add_to(metrics.counter("jobs_total", "Jobs run.", new list[str]), new list[str], 1);
    metrics.add_to(metrics.gauge("jobs_running", "Jobs running.", new list[str]), new list[str], 1);
}
```

## T0001 usage_error

The command line is not valid.

Fixable: no

The command, a flag or the number of arguments is not valid (exit status 64). The message lists the valid commands or options. This includes an option given to a command it does not belong to (`--trace` only works with `run`, `test` and `replay`, `--record` only with `run` and `test`, `--diff` only with `impact` and `test`, `--dry-run` only with `edit`, `--filter` only with `test`), `--trace-limit` without `--trace`, `--seed`, `--clock-ms` or program arguments with `replay` (they come from the recording), and a trace or recording path that is a source file of the program.

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

A trace, a recording or a built executable could not be written.

Fixable: no

`--trace FILE` and `--record FILE` write their file next to the program's own output. When the file cannot be created (a missing directory, no permission) or a write fails (a full disk), the run still happens, the error is reported once at the end, and the exit status is 74 unless the run already failed. Choose a path that can be written. `vo build` reports it the same way when its output cannot be written, and leaves nothing behind.

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

## T0011 unknown_test

`vo test --test ID` names no test, or `--spec ID` is named by no test.

Fixable: no

A test's identifier is `FILE::NAME`, with FILE as reports print it (`main.vo::parses`). `--spec ID` selects the tests whose `@spec` names `ID` or an ID starting with `ID/`. The message lists up to three closest identifiers. Nothing runs. Run `vo test --list` for every identifier.

### Example

```sh
vo test --test main.vo::parse main.vo
```

### Fixed

```sh
vo test --test main.vo::parses main.vo
```

## T0012 mutation_needs_passing_tests

`vo test --mutate` found selected tests that do not pass without changes.

Fixable: no

Mutation testing changes the code and checks whether a test notices, so it needs a baseline where every selected test passes: a test that already fails would "notice" every change. The failing tests are reported first, and no change is tried. Fix the failing tests (or narrow the selection with `--filter`, `--test` or `--spec`), then run `--mutate` again.

### Example

```sh
vo test --mutate main.vo
```

### Fixed

```sh
vo test main.vo
vo test --mutate main.vo
```

## T0013 invalid_certificate_file

`--ca-file` names a file that holds no certificate vo can read.

Fixable: no

`vo run --ca-file FILE` and `vo test --ca-file FILE` trust the certificate authorities in FILE for that run, in addition to the machine's trust store, so that HTTPS to a server with a private or test authority can be verified. FILE must be PEM: one or more blocks from `-----BEGIN CERTIFICATE-----` to `-----END CERTIFICATE-----` (a private key, a certificate request or an empty file is not). The file is read before the program runs. Give the authority's certificate, not the server's key.

### Example

```sh
vo run --ca-file server.key main.vo
```

### Fixed

```sh
vo run --ca-file company-ca.pem main.vo
```

## T0014 damaged_executable

A program built with `vo build` cannot start because its program part is not intact.

Fixable: no

An executable from `vo build` carries the program's files and their hash after the toolchain. When bytes of that part were changed after the build (a bad copy, a patched file), the hash does not match and nothing runs (exit status 66). Build it again or copy it again. A file whose end is cut off has lost the part altogether: it starts as the plain `vo` toolchain and reports a usage error (T0001) instead.

### Example

```sh
printf x | dd of=app bs=1 seek=10300000 conv=notrunc && ./app
```

### Fixed

```sh
vo build main.vo -o app && ./app
```
