# Contract: `std.crypto` (spec 035)

```text
export fn sha256(b: bytes) -> bytes                              # 32 bytes
export fn hmac_sha256(key: bytes, message: bytes) -> bytes       # 32 bytes
export fn same_bytes(a: bytes, b: bytes) -> bool                 # == in constant time
export fn random_bytes(n: int) -> bytes uses entropy             # 0 <= n <= 65536, else R0007
export fn hash_password(password: str) -> str uses entropy       # "pbkdf2-sha256$600000$SALT$KEY"
export fn verify_password(password: str, stored: str) -> bool    # false for anything malformed
```

Examples:

```text
import std.bytes.from_text;
import std.bytes.to_hex;
import std.crypto.hmac_sha256;
import std.crypto.random_bytes;
import std.crypto.same_bytes;
import std.crypto.sha256;

fn token() -> str uses entropy {
    return to_hex(random_bytes(16));
}

fn signed_by(secret: str, body: bytes, signature: bytes) -> bool {
    return same_bytes(hmac_sha256(from_text(secret), body), signature);
}
```

`to_hex(sha256(from_text("abc")))` is
`ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad`.

## The `entropy` effect

- Declared like any effect: `uses entropy`. No scopes. Not covered by `random`.
- `vo run` and built executables: the operating system's random source; no option changes it.
- `vo test`: reproducible bytes, from the test's seed.
- `vo run --record`: the bytes are in the recording; `vo replay` returns them.
- The random source failing: the program stops with R0007 (exit 70); never a predictable fallback.
