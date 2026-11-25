#!/usr/bin/env python3
"""Spec 045: std.security's tokens against an independent JWT (HMAC-SHA256) written with Python's
standard library, what its outputs must never contain, and its costs.

  web_security.py                 every group (part of `make test`)
  web_security.py --group NAME    one group: tokens, timing, speed, cookies, origin

tokens  1,000 tokens issued by the module verify here, 1,000 issued here are accepted there with the
        same name and roles (SC-002); 10,000 damaged tokens are all refused (SC-003); validly signed
        tokens of the wrong shape are refused with reason `content`; the sign_in golden case prints no
        password, key or token outside the sign-in route's own answer (FR-019)
timing  Basic: refusing an unknown name takes as long as refusing a wrong password, medians within
        10% (SC-004); skipped on CI and under emulation
speed   a token-authenticated request costs at most 100 microseconds more than an unguarded one
        (SC-005); reported everywhere, checked on x86-64 outside CI
cookies spec 052: read_cookie on 10,000 generated Cookie headers against Python's http.cookies where
        the header is well formed and against a model of the contract where it is not
origin  spec 052: same_origin on every combination of method, Sec-Fetch-Site, Origin, Host and the
        origin setting from small sets, against a model of the contract

Spec 052 also extends tokens (the browser case prints no password, cookie value or session key outside
the answers that set the cookie: SC-008) and timing (the sign-in form, SC-006).
"""
import base64
import hashlib
import hmac
import http.cookies
import json
import os
import random
import re
import statistics
import subprocess
import sys
import tempfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VO = os.path.join(ROOT, "bin", "vo")
SLOW = 20 if any(a in os.environ.get("VO_BIN", "") for a in ("aarch64", "riscv64")) else 1
N = 1000 if SLOW == 1 else 100
KEY = b"0123456789abcdef0123456789abcdef"
OTHER = b"fedcba9876543210fedcba9876543210"
NOW = 1_700_000_000

failures = []
cases = 0

DRIVER = """import std.bytes.from_text;
import std.fs.each_line;
import std.json.from_json;
import std.json.parse_json;
import std.json.to_json;
import std.json.write_json;
import std.security.User;
import std.security.check_token;
import std.security.issue_token;

fn answer(f: list[str]) -> result[str] {
    let key = from_text("0123456789abcdef0123456789abcdef");
    if f[0] == "issue" {
        let roles: list[str] = try from_json(try parse_json(f[4]));
        let name: str = try from_json(try parse_json(f[3]));
        return issue_token(key, new User { name: name, roles: roles }, try parse_int(f[1]), try parse_int(f[2]));
    }
    match check_token([key], try parse_int(f[1]), f[2]) {
        case ok(user) {
            return ok("ok " + try write_json(to_json(user)));
        }
        case err(e) {
            return ok("err " + e);
        }
    }
}

fn main() -> result[void] uses console, file.read("./") {
    try each_line("cases.txt", fn(line: str) -> result[bool] {
        print(try answer(split(line, "\\t")));
        return ok(true);
    });
    return ok();
}
"""

TIMING = """import std.bytes.from_text;
import std.bytes.to_base64;
import std.option.Option;
import std.security.Access;
import std.security.Account;
import std.security.Way;
import std.security.basic;
import std.security.policy;
import std.security.rule;
import std.security.secure;
import std.http.Response;
import std.web.App;
import std.web.ErrorStatus;
import std.web.NoInput;
import std.web.Route;
import std.web.handle_request;
import std.web.json_reply;
import std.web.on_get;
import std.web.request;
import std.web.with_header;

fn find_account(n: int, name: str) -> result[Option[Account]] {
    if name == "col" {
        return ok(Option.some(new Account { name: "col", password_hash: "pbkdf2-sha256$600000$UzaawpluQd/9+wjqxK7k5g==$T77DFP1a//H/wGSMrDAzacBRzngTQcTM4Nma4jsUNWw=", roles: new list[str], enabled: true }));
    }
    return ok(Option.none);
}

fn ran(n: int, input: NoInput) -> result[Response] {
    return ok(json_reply(200, "ran"));
}

fn main() uses console, clock {
    let routes: list[Route[int, uses clock]] = [on_get("/me", ran)];
    let ways: list[Way[int, uses clock]] = [basic(find_account)];
    let app = new App { routes: routes, steps: [secure(policy([rule("/", Access.signed_in)], ways, fn() -> int { return now_ms(); }))], errors: new list[ErrorStatus] };
    for _ in 0..7 {
        for pair in ["col:wrong", "eve:wrong"] {
            let t0 = now_ms();
            let resp = handle_request(app, 0, with_header(request("GET", "/me", ""), "Authorization", "Basic " + to_base64(from_text(pair))));
            print(format("{} {} {} {}", pair, now_ms() - t0, resp.status, resp.body));
        }
    }
}
"""

SPEED = """import std.bytes.from_text;
import std.http.Response;
import std.security.Access;
import std.security.User;
import std.security.Way;
import std.security.current_user;
import std.security.issue_token;
import std.security.policy;
import std.security.rule;
import std.security.secure;
import std.security.tokens;
import std.web.App;
import std.web.ErrorStatus;
import std.web.Route;
import std.web.Step;
import std.web.handle_request;
import std.web.json_reply;
import std.web.on_get;
import std.web.request;
import std.web.with_header;

record Caller {
    @header("X-Identity")
    @default("")
    identity: str;
}

fn me(n: int, input: Caller) -> result[Response] {
    match current_user(input.identity) {
        case some(user) {
            return ok(json_reply(200, user.name));
        }
        case none {
            return ok(json_reply(200, "nobody"));
        }
    }
}

fn main() -> result[void] uses console, clock {
    let key = from_text("0123456789abcdef0123456789abcdef");
    let routes: list[Route[int, uses clock]] = [on_get("/me", me)];
    let ways: list[Way[int, uses clock]] = [tokens([key])];
    let rules = [rule("/health", Access.anyone), rule("/admin", Access.any_role(["admin"])), rule("/", Access.signed_in)];
    let plain = new App { routes: routes, steps: new list[Step[int, uses clock]], errors: new list[ErrorStatus] };
    let app = new App { routes: routes, steps: [secure(policy(rules, ways, fn() -> int { return now_ms(); }))], errors: new list[ErrorStatus] };
    let token = try issue_token(key, new User { name: "ada", roles: ["admin", "user"] }, now_ms(), 3600);
    let r = with_header(request("GET", "/me", ""), "Authorization", "Bearer " + token);
    var bad = 0;
    let t0 = now_ms();
    for _ in 0..10000 {
        if handle_request(plain, 0, r).status != 200 {
            set bad = bad + 1;
        }
    }
    let t1 = now_ms();
    for _ in 0..10000 {
        if handle_request(app, 0, r).body != "\\"ada\\"" {
            set bad = bad + 1;
        }
    }
    print(format("{} {} {}", t1 - t0, now_ms() - t1, bad));
    return ok();
}
"""

COOKIES = """import std.bytes.from_hex;
import std.bytes.to_text;
import std.fs.each_line;
import std.web.read_cookie;

fn answer(line: str) -> result[str] {
    let f = split(line, " ");
    match read_cookie(try to_text(try from_hex(f[0])), try to_text(try from_hex(f[1]))) {
        case some(v) {
            return ok("some " + v);
        }
        case none {
            return ok("none");
        }
    }
}

fn main() -> result[void] uses console, file.read("./") {
    try each_line("cases.txt", fn(line: str) -> result[bool] {
        print(try answer(line));
        return ok(true);
    });
    return ok();
}
"""

ORIGIN = """import std.fs.each_line;
import std.security.same_origin;

fn main() -> result[void] uses console, file.read("./") {
    try each_line("cases.txt", fn(line: str) -> result[bool] {
        let f = split(line, "|");
        print(str(same_origin(f[0], f[1], f[2], f[3], f[4])));
        return ok(true);
    });
    return ok();
}
"""

FORM_TIMING = """import std.bytes.from_text;
import std.http.Header;
import std.http.Response;
import std.option.Option;
import std.security.Access;
import std.security.Account;
import std.security.Browser;
import std.security.Session;
import std.security.browser;
import std.security.browser_routes;
import std.security.policy;
import std.security.rule;
import std.security.secure;
import std.web.App;
import std.web.ErrorStatus;
import std.web.handle_request;
import std.web.request;

fn find_account(n: int, name: str) -> result[Option[Account]] {
    if name == "col" {
        return ok(Option.some(new Account { name: "col", password_hash: "pbkdf2-sha256$600000$UzaawpluQd/9+wjqxK7k5g==$T77DFP1a//H/wGSMrDAzacBRzngTQcTM4Nma4jsUNWw=", roles: new list[str], enabled: true }));
    }
    return ok(Option.none);
}

fn main() uses console, clock {
    let b = new Browser[int, uses clock] {
        find_account: find_account,
        keep: fn(n: int, s: Session) -> result[void] { return ok(); },
        find: fn(n: int, key: str) -> result[Option[Session]] { return ok(Option.none); },
        end: fn(n: int, key: str) -> result[void] { return ok(); },
        random: fn(k: int) -> bytes { return from_text("0123456789abcdef"); },
        sign_in: "/sign-in",
        sign_out: "/sign-out",
        lifetime_ms: 3600000,
        origin: ""
    };
    let p = policy([rule("/", Access.anyone)], [browser(b)], fn() -> int { return now_ms(); });
    let app = new App { routes: browser_routes(b, p), steps: [secure(p)], errors: new list[ErrorStatus] };
    for _ in 0..7 {
        for pair in ["col:wrong", "eve:wrong"] {
            let f = split(pair, ":");
            let r = request("POST", "/sign-in", "");
            set r.body = "name=" + f[0] + "&password=" + f[1];
            push(r.headers, new Header { name: "Content-Type", value: "application/x-www-form-urlencoded" });
            push(r.headers, new Header { name: "Sec-Fetch-Site", value: "same-origin" });
            let t0 = now_ms();
            let resp = handle_request(app, 0, r);
            print(format("{} {} {} {}", pair, now_ms() - t0, resp.status, resp.body));
        }
    }
}
"""


def case(name, ok, detail=""):
    global cases
    cases += 1
    if not ok:
        failures.append(name)
        print(f"FAIL {name}: {detail}")


def b64(b):
    return base64.urlsafe_b64encode(b).rstrip(b"=").decode()


def unb64(s):
    return base64.urlsafe_b64decode(s + "=" * (-len(s) % 4))


def sign(key, head, body):
    return b64(hmac.new(key, f"{head}.{body}".encode(), hashlib.sha256).digest())


def py_issue(key, claims, header=None, compact=True):
    sep = (",", ":") if compact else (", ", ": ")
    head = b64(json.dumps(header or {"alg": "HS256", "typ": "JWT"}, separators=sep).encode())
    body = b64(json.dumps(claims, separators=sep, ensure_ascii=False).encode())
    return f"{head}.{body}.{sign(key, head, body)}"


def py_verify(key, token):
    head, body, sig = token.split(".")
    assert hmac.compare_digest(sign(key, head, body), sig), "signature"
    assert json.loads(unb64(head)) == {"alg": "HS256", "typ": "JWT"}, "header"
    return json.loads(unb64(body))


def rand_name(rng):
    alphabet = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789._-@ \"\\/åéλж日"
    return "".join(rng.choice(alphabet) for _ in range(rng.randint(1, 24)))


def rand_user(rng):
    return rand_name(rng), [rand_name(rng) for _ in range(rng.randint(0, 4))]


def run_driver(d, lines):
    with open(os.path.join(d, "main.vo"), "w") as f:
        f.write(DRIVER)
    with open(os.path.join(d, "cases.txt"), "w") as f:
        f.write("".join(line + "\n" for line in lines))
    r = subprocess.run([VO, "run", "--timeout", str(300 * SLOW), "main.vo"], cwd=d, capture_output=True, text=True, timeout=600 * SLOW)
    out = r.stdout.split("\n")[:-1]
    if r.returncode != 0 or len(out) != len(lines):
        case("tokens/the driver runs", False, f"exit {r.returncode}, {len(out)} lines for {len(lines)}: {r.stderr[:400]}")
        return None
    return out


def damaged(rng, n):
    """(token, the reason it must be refused with or None for any, what was done)."""
    chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"
    out = []
    for i in range(n):
        name, roles = rand_user(rng)
        claims = {"sub": name, "roles": roles, "iat": NOW - 10, "exp": NOW + 600}
        good = py_issue(KEY, claims)
        kind = i % 10
        if kind < 4:
            at = rng.randrange(len(good))
            c = rng.choice([x for x in chars + ".=+/ " if x != good[at]])
            out.append((good[:at] + c + good[at + 1:], None, f"character {at} changed"))
        elif kind == 4:
            alg = rng.choice(["none", "None", "HS384", "HS512", "RS256", "hs256", ""])
            head = {"alg": alg, "typ": "JWT"}
            if rng.random() < 0.5:
                # Unsigned, as the classic attack sends it.
                h = b64(json.dumps(head).encode())
                out.append((f"{h}.{good.split('.')[1]}.", "signature", f"alg {alg!r}, no signature"))
            else:
                out.append((py_issue(KEY, claims, head), "signing method", f"alg {alg!r}, signed"))
        elif kind == 5:
            out.append((py_issue(OTHER, claims), "signature", "another key"))
        elif kind == 6:
            claims["exp"] = NOW - rng.randint(0, 100000)
            out.append((py_issue(KEY, claims), "expired", "expired"))
        elif kind == 7:
            claims["nbf"] = NOW + rng.randint(1, 100000)
            out.append((py_issue(KEY, claims), "not yet valid", "not yet valid"))
        elif kind == 8:
            parts = good.split(".")
            del parts[rng.randrange(3)]
            out.append((".".join(parts), "not a token", "a part removed"))
        else:
            out.append((good + "." + rng.choice(["", "x", good.split(".")[2]]), "not a token", "a part added"))
    return out


def group_tokens(d):
    rng = random.Random(45)
    # The module's tokens verify here.
    users = [rand_user(rng) for _ in range(N)]
    lines = [f"issue\t{NOW * 1000 + i}\t{60 + i}\t{json.dumps(n, ensure_ascii=False)}\t{json.dumps(r, ensure_ascii=False)}"
             for i, (n, r) in enumerate(users)]
    # Tokens issued here are accepted there.
    theirs = []
    for i in range(N):
        n, r = rand_user(rng)
        claims = {"sub": n, "roles": r, "iat": NOW - i, "exp": NOW + 1 + i}
        if i % 3 == 0:
            claims = {"iss": "elsewhere", "exp": NOW + 1 + i, "roles": r, "aud": ["a", "b"], "sub": n, "jti": i}
        if i % 5 == 0:
            claims["nbf"] = NOW - (i % 2)
        theirs.append((n, r, py_issue(KEY, claims, compact=i % 2 == 0)))
    lines += [f"check\t{NOW * 1000 + 999}\t{t}" for _, _, t in theirs]
    bad = damaged(rng, 10 * N)
    lines += [f"check\t{NOW * 1000}\t{t}" for t, _, _ in bad]
    shapes = [
        ("no sub", {"roles": [], "exp": NOW + 9}), ("sub not a text", {"sub": 7, "roles": [], "exp": NOW + 9}),
        ("empty sub", {"sub": "", "roles": [], "exp": NOW + 9}), ("no roles", {"sub": "a", "exp": NOW + 9}),
        ("roles of numbers", {"sub": "a", "roles": [1], "exp": NOW + 9}), ("roles a text", {"sub": "a", "roles": "admin", "exp": NOW + 9}),
        ("no exp", {"sub": "a", "roles": []}), ("exp a text", {"sub": "a", "roles": [], "exp": "x"}),
        ("exp a fraction", {"sub": "a", "roles": [], "exp": NOW + 9.5}), ("exp null", {"sub": "a", "roles": [], "exp": None}),
        ("nbf a text", {"sub": "a", "roles": [], "exp": NOW + 9, "nbf": "x"}),
        ("claims a list", []), ("claims a text", "x"),
    ]
    lines += [f"check\t{NOW * 1000}\t{py_issue(KEY, c)}" for _, c in shapes]
    head = b64(b'{"alg":"HS256","typ":"JWT"}')
    raw = b64(b"not json")
    lines.append(f"check\t{NOW * 1000}\t{head}.{raw}.{sign(KEY, head, raw)}")
    out = run_driver(d, lines)
    if out is None:
        return
    wrong = []
    for i, (n, r) in enumerate(users):
        try:
            c = py_verify(KEY, out[i])
            want = {"sub": n, "roles": r, "iat": NOW, "exp": NOW + 60 + i}
            if c != want:
                wrong.append(f"{out[i]}: {c} want {want}")
        except Exception as e:  # noqa: BLE001
            wrong.append(f"{out[i][:60]}: {e}")
    case(f"tokens/{N} issued by the module verify here (SC-002)", not wrong, f"{len(wrong)}: {wrong[:2]}")
    at = N
    wrong = []
    for i, (n, r, t) in enumerate(theirs):
        want = "ok " + json.dumps({"name": n, "roles": r}, separators=(",", ":"), ensure_ascii=False)
        if out[at + i] != want:
            wrong.append(f"{out[at + i]!r} want {want!r}")
    case(f"tokens/{N} issued here are accepted by the module (SC-002)", not wrong, f"{len(wrong)}: {wrong[:2]}")
    at += N
    wrong = []
    for i, (t, reason, what) in enumerate(bad):
        got = out[at + i]
        if not got.startswith("err unauthorized: ") or (reason and got != "err unauthorized: " + reason):
            wrong.append(f"{what}: {got!r} for {t}")
    case(f"tokens/{len(bad)} damaged tokens are refused (SC-003)", not wrong, f"{len(wrong)}: {wrong[:3]}")
    at += len(bad)
    for i, (what, _) in enumerate(shapes + [("claims not JSON", None)]):
        case(f"tokens/validly signed, {what}: content", out[at + i] == "err unauthorized: content", out[at + i])

    # FR-019: what the sign_in golden case prints.
    r = subprocess.run([VO, "run", os.path.join(ROOT, "tests", "cases", "web_security", "sign_in.vo")],
                       capture_output=True, text=True, timeout=600 * SLOW)
    text = r.stdout
    token_lines = [line for line in text.split("\n") if line.startswith("sign in -> 200 ")]
    ok = r.returncode == 0 and len(token_lines) == 1
    case("secrets/the sign_in case runs and issues one token", ok, f"exit {r.returncode} {r.stderr[:300]}")
    if ok:
        token = json.loads(token_lines[0][len("sign in -> 200 "):])["token"]
        rest = text.replace(token_lines[0], "")
        secrets = ["passwd", "pa:ss", "nope", KEY.decode(), OTHER.decode(), token, token.split(".")[2],
                   base64.b64encode(b"ada:passwd").decode(), base64.b64encode(b"ada:nope").decode()]
        found = [s for s in secrets if s in rest]
        case("secrets/no password, key or token in an event or another answer (FR-019)", not found, str(found))
        case("secrets/events were printed", rest.count("  event ") >= 10, str(rest.count("  event ")))

    # Spec 052 SC-008: the browser case. Cookie values appear only in the answers that set them.
    r = subprocess.run([VO, "run", os.path.join(ROOT, "tests", "cases", "web_security", "browser.vo")],
                       capture_output=True, text=True, timeout=600 * SLOW)
    values = sorted(set(re.findall(r"__Host-session=([0-9a-f]+);", r.stdout)))
    case("secrets/the browser case runs and sets session cookies", r.returncode == 0 and len(values) >= 3,
         f"exit {r.returncode}, {len(values)} values {r.stderr[:300]}")
    rest = re.sub(r"Set-Cookie=[^\n]*", "", r.stdout)
    secrets = ["passwd", "nope"] + values + [hashlib.sha256(v.encode()).hexdigest() for v in values]
    found = [x for x in secrets if x in rest]
    case("secrets/no password, cookie value or session key outside Set-Cookie (SC-008)", not found, str(found))
    case("secrets/the browser case printed its events", rest.count("event: ") >= 8, str(rest.count("event: ")))


def measure(d, name, program, what, status):
    """Medians of refusing a wrong password and an unknown name in program; True when within 10%."""
    with open(os.path.join(d, "main.vo"), "w") as f:
        f.write(program)
    # A loaded machine spreads the samples (one run here gave 498 to 868 ms for the same work), so the
    # measurement is repeated up to three times; a real difference (no password work for an unknown
    # name) fails all three.
    for attempt in range(3):
        r = subprocess.run([VO, "run", "main.vo"], cwd=d, capture_output=True, text=True, timeout=300)
        rows = [line.split(" ", 3) for line in r.stdout.split("\n")[:-1]]
        if r.returncode != 0 or len(rows) != 14:
            case(f"timing/{name}: the program runs", False, f"exit {r.returncode} {r.stderr[:300]}")
            return
        known = [int(x[1]) for x in rows if x[0] == "col:wrong"]
        unknown = [int(x[1]) for x in rows if x[0] == "eve:wrong"]
        mk, mu = statistics.median(known), statistics.median(unknown)
        print(f"web_security: {name}: refusing a wrong password {mk} ms (samples {min(known)}-{max(known)}), "
              f"an unknown name {mu} ms (samples {min(unknown)}-{max(unknown)})")
        if abs(mk - mu) <= 0.1 * max(mk, mu):
            break
    case(f"timing/{name}: unknown name within 10% of wrong password ({what})", abs(mk - mu) <= 0.1 * max(mk, mu), f"{mk} against {mu}")
    case(f"timing/{name}: the password work is done", mu >= 20, f"{mu} ms")
    case(f"timing/{name}: the answers are identical", len({(x[2], x[3]) for x in rows}) == 1 and rows[0][2] == status, str(rows[:2]))


def group_timing(d):
    if SLOW != 1 or os.environ.get("CI"):
        print("web_security: timing skipped (CI or emulation)")
        return
    measure(d, "Basic", TIMING, "SC-004", "401")
    measure(d, "sign-in form", FORM_TIMING, "spec 052 SC-006", "303")


def group_speed(d):
    with open(os.path.join(d, "main.vo"), "w") as f:
        f.write(SPEED)
    best = None
    for _ in range(3):
        r = subprocess.run([VO, "run", "--timeout", str(120 * SLOW), "main.vo"], cwd=d, capture_output=True, text=True, timeout=300 * SLOW)
        parts = r.stdout.split()
        if r.returncode != 0 or len(parts) != 3 or parts[2] != "0":
            case("speed/the program runs and every request is answered", False, f"exit {r.returncode} {r.stdout[:80]!r} {r.stderr[:300]}")
            return
        added = (int(parts[1]) - int(parts[0])) / 10
        best = added if best is None else min(best, added)
    print(f"web_security: a token-authenticated request costs {best:.1f} microseconds more (bound 100 on x86-64)")
    if SLOW == 1 and not os.environ.get("CI"):
        case("speed/at most 100 microseconds added per request (SC-005)", best <= 100, f"{best}")


def cookie_strings(rng):
    """(header, name, well formed) triples: half from valid names and values, half with junk."""
    token = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789!#$%&'*+-.^_`|~"
    octets = token + "()/:<=>?@[]{}"
    junk = octets + ' ;,="\\t'
    out = []
    for i in range(10000):
        names = [("".join(rng.choice(token) for _ in range(rng.randint(1, 8)))) for _ in range(rng.randint(0, 5))]
        if i % 2 == 0:
            pairs = []
            seen = set()
            for n in names:
                if n.lower() in seen:
                    continue
                seen.add(n.lower())
                pairs.append(n + "=" + "".join(rng.choice(octets) for _ in range(rng.randint(0, 10))))
            header = "; ".join(pairs)
            out.append((header, rng.choice(names) if names and rng.random() < 0.8 else "zz", True))
        else:
            parts = [rng.choice([n + "=" + "".join(rng.choice(junk) for _ in range(rng.randint(0, 8))), n, "", " " * rng.randint(1, 3),
                                 n + "=\"" + "".join(rng.choice(octets) for _ in range(rng.randint(0, 5))) + "\""])
                     for n in names]
            out.append((rng.choice([";", "; ", " ;"]).join(parts), rng.choice(names) if names else "a", False))
    return out


def cookie_model(header, name):
    for part in header.split(";"):
        pair = part.strip(" \t")
        at = pair.find("=")
        if at > 0 and pair[:at] == name:
            v = pair[at + 1:]
            if len(v) >= 2 and v[0] == '"' and v[-1] == '"':
                return v[1:-1]
            return v
    return None


def group_cookies(d):
    rng = random.Random(52)
    rows = cookie_strings(rng)
    with open(os.path.join(d, "main.vo"), "w") as f:
        f.write(COOKIES)
    with open(os.path.join(d, "cases.txt"), "w") as f:
        f.write("".join(f"{h.encode().hex()} {n.encode().hex()}\n" for h, n, _ in rows))
    r = subprocess.run([VO, "run", "--timeout", str(300 * SLOW), "main.vo"], cwd=d, capture_output=True, text=True, timeout=600 * SLOW)
    out = r.stdout.split("\n")[:-1]
    if r.returncode != 0 or len(out) != len(rows):
        case("cookies/the driver runs", False, f"exit {r.returncode}, {len(out)} lines for {len(rows)}: {r.stderr[:400]}")
        return
    model_wrong, lib_wrong, lib_checked = [], [], 0
    for (h, n, well), got in zip(rows, out):
        want = cookie_model(h, n)
        want_text = "none" if want is None else "some " + want
        if got != want_text:
            model_wrong.append(f"{h!r} {n!r}: {got!r} want {want_text!r}")
        if well:
            c = http.cookies.SimpleCookie()
            try:
                c.load(h)
            except http.cookies.CookieError:
                continue
            if len(c) != h.count("=") or (h and len(c) == 0):
                continue  # Python's parser stopped at something it does not accept
            lib_checked += 1
            lib = "some " + c[n].value if n in c else "none"
            if got != lib:
                lib_wrong.append(f"{h!r} {n!r}: {got!r}, http.cookies {lib!r}")
    case(f"cookies/{len(rows)} headers read as the contract says", not model_wrong, f"{len(model_wrong)}: {model_wrong[:3]}")
    case(f"cookies/{lib_checked} well-formed headers read as Python's http.cookies reads them", not lib_wrong and lib_checked >= 2000,
         f"{len(lib_wrong)}: {lib_wrong[:3]}")


def origin_model(method, site, origin, host, setting):
    if method in ("GET", "HEAD", "OPTIONS"):
        return True
    if site:
        return site in ("same-origin", "none")
    if not origin or origin == "null":
        return False
    if setting:
        return origin.lower() == setting.lower()
    if "://" not in origin or not host:
        return False
    return origin.split("://", 1)[1].lower() == host.lower()


def group_origin(d):
    import itertools
    combos = list(itertools.product(
        ["GET", "HEAD", "OPTIONS", "POST", "PUT", "PATCH", "DELETE"],
        ["", "same-origin", "same-site", "cross-site", "none", "SAME-ORIGIN"],
        ["", "null", "http://a.example", "http://a.example:8080", "https://a.example", "http://b.example", "HTTP://A.EXAMPLE", "a.example"],
        ["", "a.example", "a.example:8080", "b.example", "A.example"],
        ["", "https://a.example", "http://a.example:8080"]))
    with open(os.path.join(d, "main.vo"), "w") as f:
        f.write(ORIGIN)
    with open(os.path.join(d, "cases.txt"), "w") as f:
        f.write("".join("|".join(c) + "\n" for c in combos))
    r = subprocess.run([VO, "run", "--timeout", str(300 * SLOW), "main.vo"], cwd=d, capture_output=True, text=True, timeout=600 * SLOW)
    out = r.stdout.split("\n")[:-1]
    if r.returncode != 0 or len(out) != len(combos):
        case("origin/the driver runs", False, f"exit {r.returncode}, {len(out)} lines for {len(combos)}: {r.stderr[:400]}")
        return
    wrong = [f"{c}: {got}" for c, got in zip(combos, out) if got != str(origin_model(*c)).lower()]
    allowed = sum(1 for got in out if got == "true")
    case(f"origin/{len(combos)} combinations decided as the contract says ({allowed} allowed)", not wrong, f"{len(wrong)}: {wrong[:3]}")


GROUPS = {"tokens": group_tokens, "timing": group_timing, "speed": group_speed, "cookies": group_cookies,
          "origin": group_origin}


def main():
    args = sys.argv[1:]
    names = [args[args.index("--group") + 1]] if "--group" in args else list(GROUPS)
    tmp = os.path.join(ROOT, "build", "tmp")
    os.makedirs(tmp, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=tmp) as base:
        for n in names:
            sub = os.path.join(base, n)
            os.makedirs(sub)
            GROUPS[n](sub)
    print(f"web_security: {len(names)} groups, {cases} cases, {len(failures)} failures")
    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
