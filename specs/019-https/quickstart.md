# Quickstart: validating spec 019

Prerequisites: `make` (builds `bin/vo` with the vendored library), Python 3, the `openssl` command;
`make tls-deps` installs the pinned `tlslite-ng` into `build/py/` for the second implementation.
No internet is needed by any test.

## 1. Cryptography self-test (FR-012, FR-013)

```sh
make tls-selftest     # library self-tests natively and under qemu (ARM64, RISC-V); identical output, all pass
```

## 2. HTTPS against a local server (US1)

```sh
python3 tests/https_server.py --cert tests/tls/certs/a.test --port 8443 &    # serves fixed responses
cat > /tmp/h.vo <<'X'
import std.http.get;
fn main() -> result[void] uses console, net("127.0.0.1") {
    let r = try get("https://127.0.0.1:8443/hello");
    print(str(r.status) + " " + r.body);
    return ok();
}
X
vo run /tmp/h.vo                                          # err: the certificate of 127.0.0.1:8443 is not trusted
vo run --ca-file tests/tls/certs/test-ca.pem /tmp/h.vo    # 200 hello
```

## 3. Each verification failure (US2) and the option (US3)

`python3 tests/steps.py tests/cases/https_steps` — one case per message of contracts/library.md
(expired, not yet valid, wrong host, self-signed, untrusted authority, missing intermediate, TLS 1.0
only server, tamper proxy), plus `--ca-file` accepted, T0002 and T0013.

## 4. Record and replay (US4)

The step case `https_steps/record_replay` records against the local server, stops it, replays
identically; `tests/cross_replay.py` includes it (identical on all three platforms).

## 5. Interop, fuzz, speed

```sh
python3 tests/https_interop.py   # OpenSSL (TLS 1.3, TLS 1.2 each suite) and tlslite-ng: all succeed
python3 tests/tls_fuzz.py        # 1,000 hostile handshakes: err or ok, no crash, no hang, no R0006
python3 tests/https_scale.py     # handshake + request < 100 ms; 10 MB over HTTPS >= half of HTTP speed
make test && make gate && make instrumented && make ARCH=aarch64 test && make ARCH=riscv64 test
```
