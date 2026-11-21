# Quickstart: checking `std.tcp`

```sh
make                          # build/vo
make quick AREA=tcp           # the scripted peers and programs, x86-64
python3 tests/tcp.py          # the same, all groups; --group NAME for one; --time for SC-005
VO_BIN=$PWD/build-aarch64/vo python3 tests/tcp.py   # after make ARCH=aarch64
VO_BIN=$PWD/build-riscv64/vo python3 tests/tcp.py   # after make ARCH=riscv64
tests/run.sh tcp effects      # golden cases: arguments, use after close, scopes with ports
make test                     # once, at the end
```

By hand:

```sh
python3 tests/tcp_server.py echo &          # prints its port: use it for 6399 below
cat > /tmp/ping.vo <<'VO'
import std.bytes.from_text;
import std.tcp.connect;
import std.tcp.send;
import std.tcp.read_until;
import std.tcp.close;
import std.tcp.describe;

fn main() uses console, net("127.0.0.1:6399") {
    match connect("127.0.0.1", 6399, 1000) {
        case ok(c) {
            let _ = send(c, from_text("PING\n"), 1000);
            match read_until(c, from_text("\n"), 1000) {
                case ok(line) { print(str(len(line))); }
                case err(e) { print(describe(e)); }
            }
            close(c);
        }
        case err(e) { print(describe(e)); }
    }
}
VO
build/vo run /tmp/ping.vo                      # the length of the reply line
build/vo run --record /tmp/ping.rec /tmp/ping.vo
kill %1; build/vo replay /tmp/ping.rec         # 7, with no server
```

Expected outcomes are the acceptance scenarios of [spec.md](spec.md); the items and errors are in
[contracts/tcp.md](contracts/tcp.md). (The exact spelling of bytes literals and of `match` on a typed
result follows the reference; the example in the site page is the one the suite runs.)
