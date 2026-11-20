# The standard library

The standard library is six modules written in vo and built into the toolchain, for JSON, time, text, randomness, HTTP and HTTPS, and the ordered collections, imported one item at a time with `import std.MODULE.NAME;`.

## How the library works

The library is small on purpose. Common operations on strings, lists and maps are
[builtins](../reference/builtins.md), always available without an import: `len`, `split`, `join`,
`trim`, `sort`, `keys` and so on. The modules hold everything else that most programs need: reading
and writing JSON, dates and times, text formatting, random choices, HTTP and HTTPS, and the neighbors and
ranges of the ordered collections.

Library items are imported like items of your own files, one name per line:

```text
import std.json.parse_json;
import std.text.pad_left;
```

Nothing has to be installed: the modules are compiled into `vo`. They are ordinary vo code, so their
functions have signatures, generics and effects like yours.

The rules are in [Standard library and packages](../reference/standard-library-and-packages.md).

## Finding what is there

The reference does not list the library's items. `vo doc` does, generated from the library itself, so
it always matches the toolchain you have:

- `vo doc` lists every builtin and every module;
- `vo doc std.text` lists one module's items with a one-line description each;
- `vo doc std.text.pad_left` prints one item: its exact signature, its effects and an example.

```vo
import std.text.pad_left;

fn main() uses console {
    print(pad_left("7", 3, "0"));
}
```

```console
$ vo doc std.text.pad_left
fn pad_left(s: str, width: int, fill: str) -> str
    s padded on the left with fill (one character) to width characters.
    example: pad_left("7", 3, "0") is "007"
$ vo doc std.random.pick
fn pick[T](xs: list[T]) -> result[T] uses random
    A random element of xs; an empty list is an error.
    example: pick([7]) == ok(7) is true
```

With `--json`, each item is a `doc` record with `signature`, `params`, `returns`, `effects`,
`description` and `example`. The same pages are on this site: [std.json](../std/json.md),
[std.time](../std/time.md), [std.text](../std/text.md), [std.random](../std/random.md),
[std.http](../std/http.md) and [std.collections](../std/collections.md).

## Effects of library functions

Most library functions are pure. The ones that are not say so in their signature: `now` uses `clock`,
`pick` uses `random`, `get` uses `net`. Their effects work like those of builtins:

- the function that calls one declares the effect, with any scope it likes
  ([E0501](../errors/E0501.md) otherwise, with a fix that adds it);
- the call runs with the caller's grant, so `uses net("localhost")` limits `std.http.get` to that
  host at run time ([R0010](../errors/R0010.md) for any other);
- library functions do not appear in `vo.effects`: the functions that call them do, with the effects
  they declare.

A generic library function that takes a function, such as `serve`, has the effects of the function you
pass it, as with [generics](generics.md) and effect parameters.

## std.json

JSON text as a `Json` variant: `null`, `boolean(bool)`, `integer(int)`, `number(float)`, `text(str)`,
`array(list[Json])` and `object(map[str, Json])`. `parse_json` reads text, `write_json` and
`write_json_pretty` write it. To read a document, use `match` on the variant, or the readers, which
return a `result` so that `try` passes a missing or mistyped field up as an error that names it:
`field`, `at`, `as_text`, `as_int`, `as_array`, `text_field`, `int_field` and others.

```vo
import std.json.Json;
import std.json.as_array;
import std.json.field;
import std.json.int_field;
import std.json.parse_json;
import std.json.text_field;
import std.json.write_json_pretty;

fn main() -> result[void] uses console {
    let doc = try parse_json(
        "{\"order\": 1042, \"lines\": [{\"sku\": \"tea\", \"qty\": 2}, {\"sku\": \"scone\", \"qty\": 1}]}"
    );
    let counts = new map[str, Json];
    var total = 0;
    for line in try as_array(try field(doc, "lines")) {
        let qty = try int_field(line, "qty");
        set counts[try text_field(line, "sku")] = Json.integer(qty);
        set total = total + qty;
    }
    set counts["total"] = Json.integer(total);
    print(try write_json_pretty(Json.object(counts)));
    match parse_json("{\"order\": }") {
        case ok(_) {
            print("parsed");
        }
        case err(e) {
            print("error: " + e);
        }
    }
    return ok();
}
```

```output
{
  "tea": 2,
  "scone": 1,
  "total": 3
}
error: line 1, column 11: expected a value, found `}`
```

Object members keep their order, both when reading and when writing. A number that fits an `int` is
`integer`; anything else is `number`. Writing a NaN or an infinite number is an error, since JSON has
neither. Details: [std.json](../std/json.md).

## std.time

Times are `int` milliseconds since 1970-01-01 UTC, which makes arithmetic plain addition: `days`,
`hours`, `minutes` and `seconds` turn a count into milliseconds. `parse_time` reads ISO 8601 text
(a date, or a date and time with `Z` or an offset) and `format_time` writes it in UTC. `from_ms` gives a
`DateTime` record with the calendar fields and the weekday, `to_ms` and `date` go back. `now` reads the
clock.

```vo
import std.time.days;
import std.time.format_time;
import std.time.from_ms;
import std.time.hours;
import std.time.now;
import std.time.parse_time;

fn main() -> result[void] uses console, clock {
    let names = ["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"];
    let ordered = try parse_time("2026-02-27T16:30:00+01:00");
    let due = ordered + days(3) + hours(2);
    print("ordered " + format_time(ordered));
    print("due     " + format_time(due) + " (" + names[from_ms(due).weekday - 1] + ")");
    print("now     " + format_time(now()));
    return ok();
}
```

```console
$ vo run --clock-ms 1772236800000 main.vo
ordered 2026-02-27T15:30:00Z
due     2026-03-02T17:30:00Z (Mon)
now     2026-02-28T00:00:00Z
```

`now` needs `clock`, and returns the real time unless the run fixes it with `--clock-ms N`, as here; in
`vo test` it starts at 0 (see [Determinism](determinism.md)). Everything else in the module is pure.
Details: [std.time](../std/time.md).


`sleep(ms)` waits (needs `clock`): for retries, polling and rate limits. With the real clock it really
waits, without using the processor, and never past `--timeout` ([R0009](../errors/R0009.md)). With a fixed clock, as in every
`vo test` and with `--clock-ms`, it returns at once and moves the clock on, so code that sleeps is tested
instantly and exactly; `vo replay` does not wait either.

```vo
import std.time.now;
import std.time.sleep;

fn main() uses console, clock {
    let start = now();
    var pause = 50;
    for _ in 0..3 {
        sleep(pause);
        set pause = pause * 2;
    }
    print("waited " + str(now() - start) + " ms");
}
```

```console
$ vo run --clock-ms 0 main.vo
waited 351 ms
```

## std.text

Text helpers beyond the builtins: `starts_with`, `ends_with`, `contains`, `replace`, `repeat`,
`pad_left` and `pad_right`, `align_left` and `align_right` for columns, `lines` and `words` for
splitting input. Widths count characters, not bytes, so a column of names with accents still lines up.
The `format` builtin pads the same way in one call: `format("{:<18}{:>6}", page, visits)`.

```vo
import std.text.align_left;
import std.text.align_right;
import std.text.contains;
import std.text.lines;
import std.text.repeat;
import std.text.replace;
import std.text.starts_with;
import std.text.words;

fn main() uses console {
    let log = "# visits per page\n/home  120\n/about 7\n\n/blog/first-post 46\n";
    print(align_left("page", 18) + align_right("visits", 6));
    print(repeat("-", 24));
    for line in lines(log) {
        let ws = words(line);
        if len(ws) != 2 or starts_with(line, "#") {
            continue;
        }
        print(align_left(replace(ws[0], "-", " "), 18) + align_right(ws[1], 6));
    }
    print(str(contains(log, "/blog")));
}
```

```output
page              visits
------------------------
/home                120
/about                 7
/blog/first post      46
true
```

`lines` drops a trailing carriage return and the empty line after a final newline; `words` splits on
any run of spaces, tabs and newlines, so it ignores the double space after `/home`. Details:
[std.text](../std/text.md).

## std.random

`unit` (a float in [0, 1)), `pick` (a random element) and `shuffle` (a shuffled copy). With the builtin
`random_int`, they all need `random`, and they all follow the run's seed: the same `--seed N` gives the
same choices.

```vo
import std.random.pick;
import std.random.shuffle;

fn main() -> result[void] uses console, random {
    let people = ["ana", "bo", "cy", "di"];
    print("order: " + join(shuffle(people), ", "));
    print("on call: " + try pick(people));
    print("die: " + str(random_int(1, 6)));
    return ok();
}
```

```console
$ vo run --seed 7 main.vo
order: cy, bo, di, ana
on call: bo
die: 2
```

Without `--seed`, `vo run` picks a seed of its own, so the output changes from run to run. `pick`
returns a `result` because an empty list has nothing to pick; `shuffle` leaves its argument unchanged.
Details: [std.random](../std/random.md).

## std.http

HTTP and HTTPS. `get` and `post` send a request and return a `Response` with the status, the
headers in order and the body; `header` reads a header without regard to case. They need `net`, scoped
to the hosts the program may reach.

```vo
import std.http.get;
import std.http.header;

# Print the status and content type of the local status page.
fn main() -> result[void] uses console, net("localhost") {
    let r = try get("http://localhost:8000/status");
    print(str(r.status) + " " + try header(r, "content-type"));
    print(r.body);
    return ok();
}
```

```console
$ vo effects main.vo
main uses console, net("localhost")

main.vo:main uses console, net("localhost")
0 errors, 0 warnings
```

A failed connection is an `err`, like any other failure. A request to a host outside the `net` scope
stops the program with [R0010](../errors/R0010.md) before anything is sent.

`send(method, url, headers, body, timeout_ms)` is the general form: any method, request headers such
as `Authorization`, and a timeout of the request's own (`0` for none: the run's `--timeout` still
applies). When the timeout passes, the result is `err("the request to HOST:PORT timed out")` and the
program goes on.

```vo
import std.http.Header;
import std.http.Response;
import std.http.send;

# Replace one note on an API that wants a token, waiting at most 5 seconds.
fn put_note(token: str, id: int, json: str) -> result[Response] uses net("api.example.com") {
    let headers = [
        new Header { name: "Authorization", value: "Bearer " + token },
        new Header { name: "Content-Type", value: "application/json" }
    ];
    return send("PUT", "https://api.example.com/notes/" + str(id), headers, json, 5000);
}

fn main() uses console {
    print("ready");
}
```

```output
ready
```

`send` sets `Host`, `Connection` and `Content-Length` itself and refuses them, and refuses a header
name or value that could start another line, so text from outside cannot add headers to a request.

An `https://` URL works the same way, on port 443 unless the URL names another. The request is sent
only after the server proves it is the host in the URL: its certificate must lead to an authority in
the machine's trust store, be valid now, and name the host. TLS 1.3 and 1.2 are accepted, with modern
ciphers only. Anything else is an `err` that says what is wrong, and nothing turns verification off:

| `err` | What to do |
|---|---|
| `the certificate of HOST:PORT is not trusted` | an authority the machine does not trust (or the server left out an intermediate): for a private or test authority, run with `--ca-file` |
| `the certificate of HOST:PORT is self-signed and not trusted` | give the certificate itself with `--ca-file`, or use a real authority |
| `the certificate of HOST:PORT is for another host` | the URL's host is not in the certificate: use the name it was issued for |
| `the certificate of HOST:PORT has expired` / `is not valid yet` | the server's certificate (or the machine's clock) is wrong |
| `HOST:PORT offers no TLS version or algorithm that vo accepts` | the server only speaks old TLS or weak ciphers |
| `the connection to HOST:PORT was tampered with` | something on the network changed the data |
| `the TLS handshake with HOST:PORT failed` | the server broke the protocol |
| `no trusted certificate authorities found (looked in ...)` | the machine has no trust store: install one, or use `--ca-file` |

`vo run --ca-file FILE` (and `vo test`) trusts the authorities in a PEM file for that run, in addition
to the machine's store: a company's internal authority, or the test authority of a local HTTPS server.
The person running the program decides; the program cannot. A recording made with `--record` keeps the
requests and responses, marked as HTTPS, and never any keys; `vo replay` needs neither the network nor
the authorities.

`serve(address, handle)` reads many requests at once and calls `handle` for one at a time, in the order
they arrive complete (see [Concurrency](concurrency.md)), with each `Request`,
sending back the `Response` it returns, until the program receives Ctrl-C or SIGTERM. The caller
declares `serve` with the address (`"8000"` for this machine only, `"0.0.0.0:8000"` for the network)
and the handler's effects:

```vo
import std.http.Header;
import std.http.Request;
import std.http.Response;
import std.http.serve;

fn answer(req: Request) -> Response {
    if req.path == "/health" {
        return new Response { status: 200, headers: new list[Header], body: "ok\n" };
    }
    return new Response { status: 404, headers: new list[Header], body: "not found\n" };
}

fn main() -> result[void] uses serve("8000") {
    try serve("8000", answer);
    return ok();
}
```

```console
$ vo effects main.vo
main uses serve("8000")

main.vo:answer pure
main.vo:main uses serve("8000")
0 errors, 0 warnings
```

The handler here is pure: it can only compute a response from the request. A handler that stops with a
runtime error answers 500 and the server goes on; `--timeout` limits each handler call, not the server.

`serve_workers(address, workers, setup, handle)` answers up to `workers` requests at once, so a slow
request (a large query, a call to another service) no longer delays the others. Each worker is a
separate process: it runs `setup` once, keeps its result (a database connection, say), and calls
`handle` with it for one request at a time. Nothing in memory is shared between requests, so keep state
in a database and change it inside a `transaction`; reading a file, changing it and writing it back
loses updates when two requests do it at once.

```vo
import std.http.Header;
import std.http.Request;
import std.http.Response;
import std.http.serve_workers;
import std.sqlite.Database;
import std.sqlite.Value;
import std.sqlite.exec;
import std.sqlite.open;
import std.sqlite.sql;
import std.sqlite.transaction;

fn open_db() -> result[Database[uses file.read, file.write]]
    uses file.read("data/"), file.write("data/") {
    return open("data/app.db");
}

fn hit(db: Database[uses file.read, file.write], req: Request) -> Response
    uses file.read("data/"), file.write("data/") {
    let saved = transaction(db, fn() -> result[int] {
        let c = try exec(db, sql("insert into hits (path) values (?)"), [Value.text(req.path)]);
        return ok(c.last_id);
    });
    match saved {
        case ok(id) {
            return new Response {
                status: 200,
                headers: new list[Header],
                body: "hit " + str(id) + "\n"
            };
        }
        case err(e) {
            return new Response { status: 500, headers: new list[Header], body: e + "\n" };
        }
    }
}

fn main() -> result[void] uses file.read("data/"), file.write("data/"), serve("8000") {
    return serve_workers("8000", 4, open_db, hit);
}
```

`vo run --workers N main.vo` overrides the count. A handler that fails answers 500 and its worker goes
on; a worker that dies is replaced; Ctrl-C or SIGTERM answers the requests already read, then
`serve_workers` returns `ok()`. A recording keeps each request's inputs together, and `vo replay` runs
the requests one at a time in one process, with the same responses. For an API, `std.web` adds routes,
typed input, JSON answers and error bodies on top: see [Building APIs](apis.md).

A server can be recorded and replayed like any program (see [Determinism](determinism.md)). Details:
[std.http](../std/http.md); the [notes-api](../examples/notes-api.md) and
[status-board](../examples/status-board.md) examples are complete servers.

## std.fs and std.process

`std.fs` lists directories and looks at paths (`file.read`), appends and creates directories
(`file.write`), and deletes and renames (`file.delete`); `std.process` runs other programs to their end
with no shell (`process`). Both have their own page: [Files and processes](files-and-processes.md).

## std.bytes: binary data

`bytes` is the type for data that is not text: images, archives, hashes, binary protocols. It has no
literal; make values with `std.bytes`, or read them (`std.fs.read_bytes`, `std.bytes.read_stdin`,
`std.process.run_bytes`, `std.http.get_bytes`). `len`, `b[i]` (an `int` from 0 to 255), `slice`, `+`,
`==` and `<` work on it; `bytes` and `str` never mix without a conversion.

```vo
import std.bytes.from_text;
import std.bytes.from_ints;
import std.bytes.to_hex;
import std.bytes.to_base64;
import std.bytes.to_text;

fn main() uses console {
    let b = from_text("hé") + from_ints([0, 255]);
    print(str(len(b)) + " " + str(b[0]) + " " + to_hex(b) + " " + to_base64(b));
    match to_text(b) {
        case ok(s) {
            print(s);
        }
        case err(e) {
            print(e);
        }
    }
}
```

```output
5 104 68c3a900ff aMOpAP8=
not UTF-8 at byte 4
```

- **Text is always text.** Every `str` is valid UTF-8: text read from files, standard input,
  arguments, the environment, other programs and HTTP is checked when it is read. `read_file` of a file
  that is not UTF-8 returns `err("not UTF-8 at byte N (std.fs.read_bytes reads any bytes)")`;
  `read_all` and `args`, which return no `result`, stop with [R0007](../errors/R0007.md). Read such data as `bytes`.
- **Hex and base64**: `to_hex`/`from_hex` and `to_base64`/`from_base64` (RFC 4648, padded); decoding
  returns an `err` naming the first invalid position.
- **Every bytes function has a text twin**: `read_bytes`, `write_bytes`, `append_bytes` (`std.fs`),
  `run_bytes`, `run_with_bytes` (`std.process`), `get_bytes`, `post_bytes`, `serve_bytes` (`std.http`),
  with the same effects and scopes.

## std.math

Square roots, powers, logarithms, trigonometry, `pi()`, and `floor`, `ceil` and `round` to `int`.
Import each function like any other library item:

```vo
import std.math.sqrt;
import std.math.round;

fn main() uses console {
    print(str(sqrt(2.0)) + " " + str(round(-2.5)));
}
```

```output
1.4142135623730951 -3
```

- **Correctly rounded**: every float result is the double nearest to the exact value, so a program prints
  the same digits on x86-64, ARM64 and RISC-V. The functions come from CORE-MATH, a library of proved
  correctly rounded functions, built into `vo`.
- **Out of domain stops the program**: `sqrt` of a negative number, `log` of zero or less, `asin` or
  `acos` outside -1 to 1, `sin`, `cos` or `tan` of an infinity, and `pow` of a negative number to a
  fraction are runtime error R0007 at the call, never a silent `nan`. Check the value first when it can
  be out of range. A `nan` argument (from `0.0 / 0.0`) passes through as `nan`.
- **Overflow is not an error**: `exp(1000.0)` is `inf` and `exp(-1000.0)` is `0.0`, as with `*` and `/`.
- **Rounding to int**: `round` takes halves away from zero (`round(2.5)` is 3, `round(-2.5)` is -3).
  `floor`, `ceil` and `round` stop with R0007 for `nan`, an infinity, or a result outside the `int`
  range, like `int(f)`.
- Angles are in radians; `atan2(y, x)` takes `y` first, as in C.

`vo doc std.math` lists every function with its domain and an example.

## std.collections

The builtin collections `ordered_map`, `ordered_set`, `priority_queue` and `deque` support `len`, `has`,
`push`, `pop`, indexing and `for` directly. This module has the rest: the first and last keys, the
neighbors of a key (`key_at_or_below`, `key_after`, ...), ranges (`keys_between`, `between`), set algebra
(`union`, `intersection`, `difference`), the front of a deque (`push_front`, `pop_front`, `front`),
`peek` for a queue, copies, and constructors with your own order (`ordered_map_by(cmp)` and the like).

```vo
import std.collections.difference;
import std.collections.key_at_or_below;
import std.collections.union;

fn names(s: ordered_set[str]) -> str {
    let out = new list[str];
    for x in s {
        push(out, x);
    }
    return join(out, " ");
}

fn main() -> result[void] uses console {
    # Who is on call: each shift starts at a minute of the day.
    let shifts = new ordered_map[int, str];
    set shifts[0] = "ana";
    set shifts[480] = "bo";
    set shifts[960] = "cy";
    let start = try key_at_or_below(shifts, 615);
    print("at 10:15: " + shifts[start]);
    let week1 = new ordered_set[str];
    let week2 = new ordered_set[str];
    for name in ["cy", "ana", "bo"] {
        push(week1, name);
    }
    for name in ["di", "ana"] {
        push(week2, name);
    }
    print("either week: " + names(union(week1, week2)));
    print("first week only: " + names(difference(week1, week2)));
    return ok();
}
```

```output
at 10:15: bo
either week: ana bo cy di
first week only: bo cy
```

The neighbor functions return a `result` because there may be no such key. Details:
[std.collections](../std/collections.md) and the [collections](collections.md) chapter; the
[help-desk example](../examples/helpdesk.md) uses all four collections.

## std.regex

Regular expressions with the syntax agents know from Python and JavaScript: classes, `\d \w \s \b`,
greedy and lazy quantifiers, numbered and named groups `(?<name>...)`, alternation, and the flags
`(?i)`, `(?m)`, `(?s)`. Every pattern runs in time linear in the text, so a pattern a user types cannot
hang the program; lookaround and backreferences, which need backtracking, are not supported. A pattern
is a vo string, so its backslashes are doubled: the pattern `\d+` is written `"\\d+"`.

```vo
import std.regex.regex;
import std.regex.is_match;
import std.regex.first_match;
import std.regex.replace_all;

fn main() uses console {
    let id = regex("^[A-Z]{3}-\\d{4}$");
    print(str(is_match(id, "ABC-1234")) + " " + str(is_match(id, "ABC-12")));
    let log = regex("(\\d{4})-(\\d{2})-(\\d{2}) (?<level>ERROR|WARN)");
    match first_match(log, "2026-11-19 ERROR disk full") {
        case ok(m) {
            print(m.groups[1] + " " + m.named["level"] + " at byte " + str(m.start));
        }
        case err(e) {
            print(e);
        }
    }
    print(replace_all(regex("(\\d{2})/(\\d{2})/(\\d{4})"), "due 19/11/2026", "$3-$2-$1"));
}
```

```output
true false
2026 ERROR at byte 0
due 2026-11-19
```

`regex("...")` takes a string literal, and `vo check` checks the pattern: a group that is not closed, a
lookahead or Python's `(?P<name>...)` is reported at its place in the pattern, with a fix where there is
one. For a pattern built at run time, `parse_regex(s)` returns `err` instead. Every operation searches
anywhere in the text: anchor with `^` and `$`. Details: [std.regex](../std/regex.md).

## std.sqlite

SQLite databases, built into vo: a database file with `open` (`file.read` and `file.write`) or
`open_readonly` (`file.read`), or one in memory with `open_memory` (no effect, so `vo test` can use real
SQL with no setup). A statement is a literal, `sql("...")`, which `vo check` parses: a syntax error, a
second statement or a missing `?` is reported at its place. Values are always `?` parameters, given as a
list of `Value`, so a value can never become SQL. For SQL built at run time, `parse_sql(s)` returns `err`
instead.

```vo
import std.sqlite.Value;
import std.sqlite.exec;
import std.sqlite.get_int;
import std.sqlite.get_str;
import std.sqlite.open_memory;
import std.sqlite.query;
import std.sqlite.sql;
import std.sqlite.transaction;

fn main() -> result[void] uses console {
    let db = open_memory();
    let _ = try exec(
        db,
        sql("create table people(id integer primary key, name text not null, age integer)"),
        new list[Value]
    );
    let added = try transaction(db, fn() -> result[int] {
        let a = try exec(db, sql("insert into people(name, age) values (?, ?)"), [
            Value.text("Ada"),
            Value.integer(36)
        ]);
        let _ = try exec(db, sql("insert into people(name, age) values (?, ?)"), [
            Value.text("Alan"),
            Value.integer(41)
        ]);
        return ok(a.last_id);
    });
    print("first id " + str(added));
    for row in try query(db, sql("select name, age from people where age > ? order by name"), [
        Value.integer(30)
    ]) {
        print(try get_str(row, "name") + " " + str(try get_int(row, "age")));
    }
    match exec(db, sql("insert into people(age) values (?)"), [Value.integer(1)]) {
        case ok(_) {
            print("added");
        }
        case err(e) {
            print(e);
        }
    }
    return ok();
}
```

```output
first id 1
Ada 36
Alan 41
constraint: NOT NULL constraint failed: people.name
```

A database's type carries its effects: `open` gives a `Database[uses file.read, file.write]`, and a
function that queries one declares those effects, wherever the database came from (a parameter, a record
field, a list). `transaction(db, f)` commits when `f` returns `ok` and rolls back when it returns `err`.
SQL that reads the clock or randomness (`CURRENT_TIMESTAMP`, `random()`) is refused, so results are the
same on every run and platform; compute those values in vo and pass them. `vo run --record` records each
call's results, and `vo replay` answers from them without the file. Details: [std.sqlite](../std/sqlite.md).

## What is not in the library

There is no serving over HTTPS (run a vo server behind a proxy for that), no threads, no long-running or interactive child
processes, no file permissions, links or modification times, and no way to call code in another
language. A program that needs such a thing gets it from outside, for example by running a tool with
`std.process`, or by talking HTTP to a local service. Shared vo code that is not in the library comes in as a
[package](modules-and-packages.md#packages).
