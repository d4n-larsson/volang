# The standard library

The standard library is six modules written in vo and built into the toolchain, for JSON, time, text, randomness, HTTP and HTTPS, and the ordered collections, imported a module at a time with `import std.MODULE;`, each item then written `MODULE.NAME`.

## How the library works

The library is small on purpose. Common operations on strings, lists and maps are
[builtins](../reference/builtins.md), always available without an import: `len`, `split`, `join`,
`trim`, `sort`, `keys` and so on. The modules hold everything else that most programs need: reading
and writing JSON, dates and times, text formatting, random choices, HTTP and HTTPS, and the neighbors and
ranges of the ordered collections.

Library modules are imported like your own files, one module per line, and their items are written with
the module's name (`json.parse_json`, `text.pad_left`):

```text
import std.json;
import std.text;
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
import std.text;

fn main() uses console {
    print(text.pad_left("7", 3, "0"));
}
```

```console
$ vo doc std.text.pad_left
fn pad_left(s: str, width: int, fill: str) -> str
    s padded on the left with fill (one character) to width characters.
    use: import std.text; then text.pad_left
    example: pad_left("7", 3, "0") is "007"
$ vo doc std.random.pick
fn pick[T](xs: list[T]) -> result[T] uses random
    A random element of xs; an empty list is an error.
    use: import std.random; then random.pick
    example: pick([7]) == ok(7) is true
```

With `--json`, each item is a `doc` record with `signature`, `params`, `returns`, `effects`,
`description` and `example`. The same pages are on this site: [std.json](../std/json.md),
[std.time](../std/time.md), [std.text](../std/text.md), [std.random](../std/random.md),
[std.http](../std/http.md), [std.tcp](../std/tcp.md), [std.bits](../std/bits.md), [std.decimal](../std/decimal.md), [std.lists](../std/lists.md), [std.workers](../std/workers.md), [std.log](../std/log.md), [std.metrics](../std/metrics.md) and [std.collections](../std/collections.md).

## Effects of library functions

Most library functions are pure. The ones that are not say so in their signature: `sleep` uses `clock`,
`pick` uses `random`, `get` uses `net`. Their effects work like those of builtins:

- the function that calls one declares the effect, with any scope it likes
  ([E0501](../errors/E0501.md) otherwise, with a fix that adds it);
- the call runs with the caller's grant, so `uses net("localhost")` limits `std.http.get` to that
  host at run time ([R0010](../errors/R0010.md) for any other);
- library functions do not appear in `vo.effects`: the functions that call them do, with the effects
  they declare.

A generic library function that takes a function, such as `serve`, has the effects of the function you
pass it, as with [generics](generics.md) and effect parameters.

## Names

Each library name means one thing across the modules, so two imports never clash and an agent can guess
a name from another module's. Some pairs stay apart on purpose. vo has no overloading, so a function
that works on several types has a name per type: `std.collections` has `first` (sets), `first_key`
(maps), `front` (deques) and `peek` (queues), and `between` beside `keys_between`; `std.decimal` has
`total` and `absolute` where lists have `sum` and the builtin `abs`. `std.web`'s `with_cookie` returns a
`result` because it checks the cookie a browser would refuse; `with_header` checks nothing and returns
the answer. A function that reads a value from text is `parse_KIND`, and its error is
`invalid KIND: TEXT`; a function that decodes an encoding into bytes is `from_KIND` (`from_hex`).

## std.json

JSON text as a `Json` variant: `null`, `boolean(bool)`, `integer(int)`, `number(float)`, `text(str)`,
`array(list[Json])` and `object(map[str, Json])`. `parse_json` reads text, `write_json` and
`write_json_pretty` write it. To read a document, use `match` on the variant, or the readers, which
return a `result` so that `try` passes a missing or mistyped field up as an error that names it:
`member`, `at`, `as_text`, `as_int`, `as_array`, `text_field`, `int_field` and others.

```vo
import std.json;

fn main() -> result[void] uses console {
    let doc = try json.parse_json(
        `{"order": 1042, "lines": [{"sku": "tea", "qty": 2}, {"sku": "scone", "qty": 1}]}`
    );
    let counts = new map[str, json.Json];
    var total = 0;
    for line in try json.as_array(try json.member(doc, "lines")) {
        let qty = try json.int_field(line, "qty");
        set counts[try json.text_field(line, "sku")] = json.Json.integer(qty);
        set total = total + qty;
    }
    set counts["total"] = json.Json.integer(total);
    print(try json.write_json_pretty(json.Json.object(counts)));
    match json.parse_json(`{"order": }`) {
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
error: invalid json at line 1, column 11: expected a value, found `}`
```

Object members keep their order, both when reading and when writing. A number that fits an `int` is
`integer`; anything else is `number`. Writing a NaN or an infinite number is an error, since JSON has
neither. Details: [std.json](../std/json.md).

## std.time

Times are `int` milliseconds since 1970-01-01 UTC, which makes arithmetic plain addition: `days`,
`hours`, `minutes` and `seconds` turn a count into milliseconds. `parse_time` reads ISO 8601 text
(a date, or a date and time with `Z` or an offset) and `format_time` writes it in UTC. `from_ms` gives a
`DateTime` record with the calendar fields and the weekday, `to_ms` and `date` go back. The builtin `now_ms()` reads
the clock.

```vo
import std.time;

fn main() -> result[void] uses console, clock {
    let names = ["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"];
    let ordered = try time.parse_time("2026-02-27T16:30:00+01:00");
    let due = ordered + time.days(3) + time.hours(2);
    print("ordered " + time.format_time(ordered));
    print("due     " + time.format_time(due) + " (" + names[time.from_ms(due).weekday - 1] + ")");
    print("now     " + time.format_time(now_ms()));
    return ok();
}
```

```console
$ vo run --clock-ms 1772236800000 main.vo
ordered 2026-02-27T15:30:00Z
due     2026-03-02T17:30:00Z (Mon)
now     2026-02-28T00:00:00Z
```

`now_ms()` needs `clock`, and returns the real time unless the run fixes it with `--clock-ms N`, as here; in
`vo test` it starts at 0 (see [Determinism](determinism.md)). `sleep` and `local_zone` need it too;
everything else in the module is pure.
Details: [std.time](../std/time.md).

`sleep(ms)` waits (needs `clock`): for retries, polling and rate limits. With the real clock it really
waits, without using the processor, and never past `--timeout` ([R0009](../errors/R0009.md)). With a fixed clock, as in every
`vo test` and with `--clock-ms`, it returns at once and moves the clock on, so code that sleeps is tested
instantly and exactly; `vo replay` does not wait either.

```vo
import std.time;

fn main() uses console, clock {
    let start = now_ms();
    var pause = 50;
    for _ in 0..3 {
        time.sleep(pause);
        set pause = pause * 2;
    }
    print("waited " + str(now_ms() - start) + " ms");
}
```

```console
$ vo run --clock-ms 0 main.vo
waited 351 ms
```

### Time zones

A date for a person needs a zone. `load_zone("Europe/Stockholm")` gives a `Zone` from the tz database
that is built into `vo`: it needs no effect and no file on the machine, so the same program prints the
same local time everywhere, in a test and in a container that holds nothing but the program. Names are
exact; an unknown one is an error value, and `zone_names()` lists them all. `offset_zone(ms)` is a
zone that is always that far east of UTC.

`in_zone(z, ms)` is the calendar time a clock in the zone shows, `offset_at` and `abbreviation_at` give
the offset and its name (`CEST`), and `format_in` writes ISO 8601 text with the offset, which
`parse_time` reads back to the same instant.

The other direction is input: a person typed a local date and time. `from_local(z, d)` returns the
instant only when there is exactly one. Twice a year there is not: when the clocks go forward an hour
of local time does not exist (`skipped`, with the instant of the change), and when they go back an hour
happens twice (`repeated`, with both instants). The error type makes the program say what it does
then; nothing is picked silently.

```vo
import std.time;

fn booking(z: time.Zone, month: int, day: int, hour: int, minute: int) -> str {
    let d = new time.DateTime {
        year: 2026,
        month: month,
        day: day,
        hour: hour,
        minute: minute,
        second: 0,
        ms: 0,
        weekday: 0
    };
    match time.from_local(z, d) {
        case ok(ms) {
            return "booked for " + time.format_in(z, ms);
        }
        case err(e) {
            match e {
                case invalid(reason) {
                    return "not a time: " + reason;
                }
                case skipped(change) {
                    return "no such time: the clocks go forward to " + time.format_in(z, change);
                }
                case repeated(r) {
                    return "twice that night: " + time.format_in(z, r.first) + " or " + time.format_in(z, r.second);
                }
            }
        }
    }
}

fn main() -> result[void] uses console {
    let z = try time.load_zone("Europe/Stockholm");
    let sent = try time.parse_time("2026-07-01T12:00:00Z");
    let d = time.in_zone(z, sent);
    print(format("sent {}:{:02} {} on {} July", d.hour, d.minute, time.abbreviation_at(z, sent), d.day));
    print(time.format_in(z, sent));
    print(booking(z, 3, 30, 9, 0));
    print(booking(z, 3, 29, 2, 30));
    print(booking(z, 10, 25, 2, 30));
    return ok();
}
```

```console
$ vo run main.vo
sent 14:00 CEST on 1 July
2026-07-01T14:00:00+02:00
booked for 2026-03-30T09:00:00+02:00
no such time: the clocks go forward to 2026-03-29T03:00:00+02:00
twice that night: 2026-10-25T02:30:00+02:00 or 2026-10-25T02:30:00+01:00
```

`local_zone()` is the zone of the machine the program runs on, for tools that print times for whoever
runs them. It needs `clock`: it reads the `TZ` variable (when that is a name `load_zone` accepts) and
otherwise the machine's `/etc/localtime`, and gives UTC when neither says anything. With a fixed clock,
as here and in every `vo test`, it is UTC, and `vo replay` gives the zone that was recorded.

```vo
import std.time;

fn main() uses console, clock {
    let z = time.local_zone();
    print("it is " + time.format_in(z, now_ms()) + " in " + z.name);
    print("zone data: " + time.tzdata_version());
}
```

```console
$ vo run --clock-ms 1772236800000 main.vo
it is 2026-02-28T00:00:00+00:00 in UTC
zone data: 2026f
```

Zone rules change a few times a year, when a government moves its summer time. The data inside `vo` is
the release that `tzdata_version()` and `vo version` name, and a newer `vo` brings a newer release. A
program that cannot wait reads a compiled zone file of its own (the format of `/usr/share/zoneinfo`)
with `std.fs.read_bytes` and passes it to `zone_from_bytes(name, data)`.

## std.text

Text helpers beyond the builtins: `starts_with`, `ends_with`, `contains`, `replace`, `repeat`,
`pad_left` and `pad_right`, `align_left` and `align_right` for columns, `lines` and `words` for
splitting input. Widths count characters, not bytes, so a column of names with accents still lines up.
The `format` builtin pads the same way in one call: `format("{:<18}{:>6}", page, visits)`.

```vo
import std.text;

fn main() uses console {
    let log = "# visits per page\n/home  120\n/about 7\n\n/blog/first-post 46\n";
    print(text.align_left("page", 18) + text.align_right("visits", 6));
    print(text.repeat("-", 24));
    for line in text.lines(log) {
        let ws = text.words(line);
        if len(ws) != 2 or text.starts_with(line, "#") {
            continue;
        }
        print(text.align_left(text.replace(ws[0], "-", " "), 18) + text.align_right(ws[1], 6));
    }
    print(str(text.contains(log, "/blog")));
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

`char_count(s)` counts characters where `len(s)` counts bytes, and `equal_fold(a, b)` compares without
letter case. The builtins `upper` and `lower` map Latin, Greek and Cyrillic letters (`upper("åäö")` is
`"ÅÄÖ"`); a letter whose upper case is several characters, such as `ß`, stays as it is.

`lines` drops a trailing carriage return and the empty line after a final newline; `words` splits on
any run of spaces, tabs and newlines, so it ignores the double space after `/home`. Details:
[std.text](../std/text.md).

## std.random

`unit` (a float in [0, 1)), `pick` (a random element) and `shuffled` (a shuffled copy). With the builtin
`random_int`, they all need `random`, and they all follow the run's seed: the same `--seed N` gives the
same choices.

```vo
import std.random;

fn main() -> result[void] uses console, random {
    let people = ["ana", "bo", "cy", "di"];
    print("order: " + join(random.shuffled(people), ", "));
    print("on call: " + try random.pick(people));
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
returns a `result` because an empty list has nothing to pick; `shuffled` leaves its argument unchanged.
Details: [std.random](../std/random.md).

## std.http

HTTP and HTTPS. `get` and `post` send a request and return a `Response` with the status, the
headers in order and the body; `response_header` reads a header without regard to case (`none` when it is missing). They need `net`, scoped
to the hosts the program may reach.

```vo
import std.http;
import std.option;

# Print the status and content type of the local status page.
fn main() -> result[void] uses console, net("localhost") {
    let r = try http.get("http://localhost:8000/status");
    print(str(r.status) + " " + option.or_else(http.response_header(r, "content-type"), ""));
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
import std.http;

# Replace one note on an API that wants a token, waiting at most 5 seconds.
fn put_note(token: str, id: int, json: str) -> result[http.Response] uses net("api.example.com") {
    let headers = [
        new http.Header { name: "Authorization", value: "Bearer " + token },
        new http.Header { name: "Content-Type", value: "application/json" }
    ];
    return http.send("PUT", "https://api.example.com/notes/" + str(id), headers, json, 5000);
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
import std.http;

fn answer(req: http.Request) -> http.Response {
    if req.path == "/health" {
        return new http.Response { status: 200, headers: new list[http.Header], body: "ok\n" };
    }
    return new http.Response { status: 404, headers: new list[http.Header], body: "not found\n" };
}

fn main() -> result[void] uses serve("8000") {
    try http.serve("8000", answer);
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
import std.http;
import std.sqlite;

fn open_db() -> result[sqlite.Database[uses file.read, file.write]]
    uses file.read("data/"), file.write("data/") {
    return sqlite.open("data/app.db");
}

fn hit(db: sqlite.Database[uses file.read, file.write], req: http.Request) -> http.Response
    uses file.read("data/"), file.write("data/") {
    let saved: result[int] = sqlite.transaction(db, fn() {
        let c = try sqlite.exec(db, sqlite.sql("insert into hits (path) values (?)"), [sqlite.Value.text(req.path)]);
        return ok(c.last_id);
    });
    match saved {
        case ok(id) {
            return new http.Response {
                status: 200,
                headers: new list[http.Header],
                body: "hit " + str(id) + "\n"
            };
        }
        case err(e) {
            return new http.Response { status: 500, headers: new list[http.Header], body: e + "\n" };
        }
    }
}

fn main() -> result[void] uses file.read("data/"), file.write("data/"), serve("8000") {
    return http.serve_workers("8000", 4, open_db, hit);
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

A body larger than 16 MiB, or one you want while it arrives, is read with `send_stream`: see
[Large inputs](files-and-processes.md#large-inputs).

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
import std.bytes;

fn main() uses console {
    let b = bytes.from_text("hé") + bytes.from_ints([0, 255]);
    print(str(len(b)) + " " + str(b[0]) + " " + bytes.to_hex(b) + " " + bytes.to_base64(b));
    match bytes.to_text(b) {
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

## std.crypto: hashes and secrets

`sha256` and `hmac_sha256` hash `bytes` to 32 bytes, `same_bytes` compares two values without showing
where they differ, `random_bytes` draws secrets from the operating system, and `hash_password` with
`verify_password` store passwords. Text goes in through `std.bytes.from_text`, results come out through
`to_hex` or `to_base64`.

A webhook is checked by computing the signature again and comparing in constant time:

```vo
import std.bytes;
import std.crypto;

fn signed_by(secret: str, body: bytes, signature: bytes) -> bool {
    return crypto.same_bytes(crypto.hmac_sha256(bytes.from_text(secret), body), signature);
}

fn main() uses console {
    let body = bytes.from_text(`{"event":"paid"}`);
    let signature = crypto.hmac_sha256(bytes.from_text("whsec_1"), body);
    print(bytes.to_hex(crypto.sha256(bytes.from_text("abc"))));
    print(str(signed_by("whsec_1", body, signature)) + " " + str(signed_by("whsec_2", body, signature)));
}
```

```output
ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad
true false
```

Secrets need their own effect, `entropy`, so `vo.effects` shows which functions draw them, and a
function that only declares `random` cannot. `random_int` follows the seed and is for simulations and
shuffles; a session token, an API key or a reset code is `random_bytes`:

```vo
import std.bytes;
import std.crypto;

fn main() uses console, entropy {
    let token = bytes.to_hex(crypto.random_bytes(16));
    print(str(len(token)));
    let stored = crypto.hash_password("correct horse");
    print(slice(stored, 0, 21));
    print(str(crypto.verify_password("correct horse", stored)) + " " + str(crypto.verify_password("wrong", stored)));
}
```

```output
32
pbkdf2-sha256$600000$
true false
```

- No option makes `random_bytes` predictable under `vo run` or in a built executable. In `vo test` the
  bytes are the same on every run, so tests stay reproducible. A recording holds the bytes that were
  drawn: keep recordings as private as the secrets.
- `hash_password` returns one line to store: the method (PBKDF2-HMAC-SHA256), its cost, a fresh salt and
  the key, so two hashes of one password differ and old hashes still verify if the cost is raised
  later. It takes about a third of a second on purpose. `verify_password` answers `false` for anything
  that is not such a line.

Details: [std.crypto](../std/crypto.md).

## std.tcp: connections

`std.tcp` gives a program the connection itself, for protocols that are not HTTP: a database, a cache,
mail, a queue. `connect` opens one, `write_all` writes `bytes`, and three reads take them back: `read` (what
has arrived, up to a size), `read_exact` (exactly so many) and `read_until` (through a delimiter, such
as a line end). `connect_tls` does the same over TLS, verified exactly as an `https://` URL is, and
`start_tls` upgrades an open connection, as SMTP and PostgreSQL ask for. `listen` and `accept` are the
server side.

A client for three Redis commands:

```vo
import std.bytes;
import std.tcp;

const redis = "127.0.0.1:6379";
const crlf = "\r\n";

fn other(message: str) -> tcp.TcpError {
    return tcp.TcpError.other(message);
}

# Sends one command as an array of bulk strings and returns the reply's text.
fn command[E: effects](c: tcp.Conn[E], words: list[str]) -> result[str, tcp.TcpError] uses E {
    var request = "*" + str(len(words)) + crlf;
    for w in words {
        set request = request + "$" + str(len(w)) + crlf + w + crlf;
    }
    try tcp.write_all(c, bytes.from_text(request), 1000);
    let head = trim(try map_err(bytes.to_text(try tcp.read_until(c, bytes.from_text(crlf), 1000)), other));
    if slice(head, 0, 1) != "$" {
        return ok(slice(head, 1, len(head)));
    }
    let size = try map_err(parse_int(slice(head, 1, len(head))), other);
    if size < 0 {
        return ok("(nil)");
    }
    return ok(trim(try map_err(bytes.to_text(try tcp.read_exact(c, size + 2, 1000)), other)));
}

fn session() -> result[void, tcp.TcpError] uses console, net(redis) {
    let c = try tcp.connect("127.0.0.1", 6379, 1000);
    print(try command(c, ["PING"]));
    print(try command(c, ["SET", "drink", "tea"]));
    print(try command(c, ["GET", "drink"]));
    tcp.close_conn(c);
    return ok();
}

fn main() uses console, net(redis) {
    match session() {
        case ok() {
        }
        case err(e) {
            print(tcp.tcp_error_text(e));
        }
    }
}
```

- **Scopes.** Connecting needs `net`, and a `net` scope names a host, or one port of it:
  `net("db.internal")` allows every port, `net("db.internal:5432")` only that one. A bare host covers
  its ports, not the other way round, in `vo check`, in `vo.effects` and at run time (R0010, before
  anything is sent). `std.http` obeys the same scopes with the URL's port. Listening needs `serve` with
  the address, written as for `std.http.serve`; port `0` (`serve("0")`) asks the system for a free
  port, which the listener's `port` field then holds.
- **A connection carries its effect.** `connect` returns a `Conn[uses net]` and `accept` a
  `Conn[uses serve]`; a function that takes any connection is generic, `fn ask[E: effects](c: Conn[E])
  uses E`, like `command` above.
- **Every wait has a time limit**, in milliseconds (0 to 600000; 0 is the longest, 600000), given at the
  call. After a read that timed out the connection is still usable and no byte is lost; after a
  `write_all` that timed out, close it.
- **Errors are a variant**, `TcpError`: `refused`, `timed_out`, `closed`, `not_found`, `unreachable`,
  `tls(reason)`, `in_use`, `too_large`, `too_many`, `invalid(what)` and `other(reason)`.
  `tcp_error_text` turns one into a sentence. Using a closed connection is `closed`, never a stopped program.
- **Limits.** One read returns at most 16 MiB (`too_large` when that much arrived without the
  delimiter; the bytes stay readable), and 256 connections and listeners can be open at once.
- **Recording.** `vo run --record` saves how each connection opened and everything it received;
  `vo replay` opens no connection and never waits. A TLS recording holds the decrypted bytes: keep it as
  private as the conversation.
- **Tasks.** A listener or connection opened before `concurrent_map` is open in every task, so a test
  can listen on port `0` and run both ends as two tasks. Use each connection from one task at a time.
- **Not there yet:** UDP, IPv6, TLS for accepted connections, and a pool of workers per connection.

## std.bits: the bits of an int

Checksums, hashes, binary formats and flag sets work on the bits of an integer. vo has no operators
for that: the functions of `std.bits` do it, on the 64-bit two's complement form of an `int`.

| Functions | Result |
|---|---|
| `bit_and(a, b)`, `bit_or(a, b)`, `bit_xor(a, b)`, `bit_not(a)` | bit by bit |
| `shift_left(a, n)` | zeros enter at the bottom; never an overflow error |
| `shift_right(a, n)` | the sign bit is repeated: a negative number stays negative |
| `shift_right_logical(a, n)` | zeros enter at the top |
| `rotate_left(a, n)`, `rotate_right(a, n)` | bits leaving one end enter at the other |
| `wrapping_add`, `wrapping_sub`, `wrapping_mul` | the low 64 bits, where `+`, `-` and `*` stop with R0001 |
| `parse_hex(s)`, `parse_binary(s)` | a `result[int]` from digits alone: no sign, no prefix |

A count `n` outside 0 to 63 is [R0007](../errors/R0007.md); nothing else fails, and every function is
pure. Constants are written in the base they are thought in: `0xFF`, `0b1010`. A hash that relies on
arithmetic wrapping around, here 64-bit FNV-1a:

```vo
import std.bits;
import std.bytes;

fn fnv1a(data: bytes) -> int {
    var h = 0xCBF29CE484222325;
    for i in 0..len(data) {
        set h = bits.wrapping_mul(bits.bit_xor(h, data[i]), 0x100000001B3);
    }
    return h;
}

fn main() uses console {
    print(format("{:x}", bits.shift_right_logical(fnv1a(bytes.from_text("hello")), 32)));
    print(str(bits.bit_and(0b1100, 0b1010)));
}
```

```output
a430d846
8
```

A set of flags in one `int`:

```vo
import std.bits;

const can_read = 0b001;
const can_write = 0b010;
const can_delete = 0b100;

fn main() uses console {
    var rights = bits.bit_or(can_read, can_write);
    print(str(bits.bit_and(rights, can_write) != 0) + " " + str(bits.bit_and(rights, can_delete) != 0));
    set rights = bits.bit_and(rights, bits.bit_not(can_write));
    print(format("{:b}", rights));
}
```

```output
true false
1
```

For a 32-bit algorithm, keep values in 0 to 4294967295 by masking after each step:
`bit_and(a + b, 0xFFFFFFFF)`. Details: [std.bits](../std/bits.md).

## std.decimal: exact amounts

A `float` cannot hold 0.10, so money and other quantities with decimal places are `Decimal` values: a
whole number of units and a number of places (0 to 18). 12.50 is 1250 units at 2 places. Nothing is
rounded unless you say how, and a result that does not fit stops the program with
[R0001](../errors/R0001.md), as `int` arithmetic does.

| Functions | Result |
|---|---|
| `parse_decimal(s)` | a `result[Decimal]` from `"12.50"`; the places written are kept |
| `decimal(units, places)`, `from_int(n)` | a decimal from numbers |
| `decimal_text(d)`, `grouped_text(d)` | `"1290.50"`, `"1,290.50"` |
| `add(a, b)`, `sub(a, b)`, `total(list)` | exact, with the larger number of places |
| `mul(a, b)` | exact; the places add up |
| `div(a, b, places, mode)`, `round_to(d, places, mode)` | rounded to `places` by a `Rounding` |
| `compare(a, b)`, `equal(a, b)`, `is_zero(d)`, `is_negative(d)` | by value: 1.5 equals 1.50 |
| `normalize(d)` | the same value with the fewest places |
| `to_int(d)`, `to_float(d)` | a `result[int]` when whole; the nearest float |
| `split_evenly(d, n)`, `allocate(d, ratios)` | parts that sum to `d` exactly |
| `negate(d)`, `absolute(d)` | the sign |

There is no default rounding: `div` and `round_to` take a `Rounding` (`half_even`, `half_up`, `down`,
`up`, `floor`, `ceiling`), so the rule is in the source where a reviewer reads it. An invoice line with
25% VAT, and the total split three ways without losing a cent:

```vo
import std.decimal;

fn main() -> result[void] uses console {
    let net = try decimal.parse_decimal("1999.90");
    let vat = decimal.round_to(decimal.mul(net, try decimal.parse_decimal("0.25")), 2, decimal.Rounding.half_even);
    let gross = decimal.add(net, vat);
    print(decimal.decimal_text(vat) + " " + decimal.grouped_text(gross));
    for part in try decimal.split_evenly(gross, 3) {
        print(decimal.decimal_text(part));
    }
    return ok();
}
```

```output
499.98 2,499.88
833.30
833.29
833.29
```

The places are part of a decimal: 1.50 prints as `1.50`. So `==`, which compares records field by
field, tells 1.5 from 1.50. Compare values with `equal` or `compare`, and `normalize` a decimal before
using it as a map key:

```vo
import std.decimal;

fn main() uses console {
    let a = decimal.decimal(15, 1);
    let b = decimal.decimal(150, 2);
    print(str(a == b) + " " + str(decimal.equal(a, b)) + " " + str(decimal.normalize(a) == decimal.normalize(b)));
}
```

```output
false true true
```

In JSON, a database or a file, store a decimal as its text (`decimal_text(d)`, read back with
`parse_decimal`), not as a JSON number or a float: the text keeps every digit and the places. When all
amounts have the same places, an integer column of units works too (`decimal(cents, 2)`). Details:
[std.decimal](../std/decimal.md).

## std.lists: small operations on lists

The loops everybody writes, once: `has_item(xs, x)` and `index_of(xs, x)` (-1 when absent),
`reversed(xs)` (a new list), `sort_by_desc(xs, key)` (in place, largest key first; equal keys keep
their order), `min_by(xs, key)` and `max_by(xs, key)` (the first element with the smallest or largest
key; an error for an empty list), `sum(xs)` for a list of `int`, and `group_by(xs, key)`, a map from
each key to the list of its elements.

```vo
import std.lists;

fn main() uses console {
    let words = split("pear fig kiwi apple fig plum", " ");
    print(format("{} {}", lists.has_item(words, "kiwi"), lists.sum(map(words, fn(w) => len(w)))));
    for size, group in lists.group_by(words, fn(w) => len(w)) {
        print(format("{} letters: {}", size, join(group, " ")));
    }
    lists.sort_by_desc(words, fn(w) => len(w));
    for i, w in words {
        print(format("{}. {}", i + 1, w));
    }
    match lists.max_by(words, fn(w) => w) {
        case ok(w) {
            print("last in the alphabet: " + w);
        }
        case err(e) {
            print(e);
        }
    }
}
```

```output
true 23
4 letters: pear kiwi plum
3 letters: fig fig
5 letters: apple
1. apple
2. pear
3. kiwi
4. plum
5. fig
6. fig
last in the alphabet: plum
```

The builtin `sort_by` sorts from smallest to largest; negating a number key reverses it, but a text
key cannot be negated, which is what `sort_by_desc` is for.

## std.workers: processes beside the program

`start_worker` runs a function in a process of its own and returns at once; the program and the
worker exchange typed messages with `tell`, `receive`, `next_message` and `emit`. `with_deadline`
gives one call a time limit. The chapter [Workers](workers.md) shows them.

## std.log and std.metrics: operating a service

`std.log` writes JSON log records (`log_info(message, [field(name, value)])`), redacting names that look
like secrets; `std.metrics` keeps counters, gauges and histograms for a `/metrics` route. Both need the
`log` effect. The chapter [Operating a service](operating.md) shows them with `std.web`'s request steps.

## std.math

Square roots, powers, logarithms, trigonometry, `pi()`, and `floor`, `ceil` and `round` to `int`.
Import each function like any other library item:

```vo
import std.math;

fn main() uses console {
    print(str(math.sqrt(2.0)) + " " + str(math.round(-2.5)));
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
`push`, `pop`, indexing and `for` directly. This module has the rest, a lookup that may find nothing giving an `Option`: the first and last keys, the
neighbors of a key (`key_at_or_below`, `key_after`, ...), ranges (`keys_between`, `between`), set algebra
(`union`, `intersection`, `difference`), the front of a deque (`push_front`, `pop_front`, `front`),
`peek` for a queue, copies, and constructors with your own order (`ordered_map_by(cmp)` and the like).

```vo
import std.collections;
import std.option;

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
    let start = option.or_else(collections.key_at_or_below(shifts, 615), 0);
    print("at 10:15: " + shifts[start]);
    let week1 = new ordered_set[str];
    let week2 = new ordered_set[str];
    for name in ["cy", "ana", "bo"] {
        push(week1, name);
    }
    for name in ["di", "ana"] {
        push(week2, name);
    }
    print("either week: " + names(collections.union(week1, week2)));
    print("first week only: " + names(collections.difference(week1, week2)));
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
import std.regex;

fn main() uses console {
    let id = regex.regex(`^[A-Z]{3}-\d{4}$`);
    print(str(regex.is_match("ABC-1234", id)) + " " + str(regex.is_match("ABC-12", id)));
    let log = regex.regex(`(\d{4})-(\d{2})-(\d{2}) (?<level>ERROR|WARN)`);
    match regex.first_match("2026-11-19 ERROR disk full", log) {
        case some(m) {
            print(m.groups[1] + " " + m.named["level"] + " at byte " + str(m.start));
        }
        case none {
            print("no match");
        }
    }
    print(regex.replace_matches("due 19/11/2026", regex.regex(`(\d{2})/(\d{2})/(\d{4})`), "$3-$2-$1"));
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
import std.sqlite;

fn main() -> result[void] uses console {
    let db = sqlite.open_memory();
    let _ = try sqlite.exec(
        db,
        sqlite.sql("create table people(id integer primary key, name text not null, age integer)"),
        new list[sqlite.Value]
    );
    let added: int = try sqlite.transaction(db, fn() {
        let a = try sqlite.exec(db, sqlite.sql("insert into people(name, age) values (?, ?)"), [
            sqlite.Value.text("Ada"),
            sqlite.Value.integer(36)
        ]);
        let _ = try sqlite.exec(db, sqlite.sql("insert into people(name, age) values (?, ?)"), [
            sqlite.Value.text("Alan"),
            sqlite.Value.integer(41)
        ]);
        return ok(a.last_id);
    });
    print("first id " + str(added));
    for row in try sqlite.query(db, sqlite.sql("select name, age from people where age > ? order by name"), [
        sqlite.Value.integer(30)
    ]) {
        print(try sqlite.text_column(row, "name") + " " + str(try sqlite.int_column(row, "age")));
    }
    match sqlite.exec(db, sqlite.sql("insert into people(age) values (?)"), [sqlite.Value.integer(1)]) {
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
