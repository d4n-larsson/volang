# Quickstart: Workers (spec 047)

```vo
import std.workers.Link;
import std.workers.Wait;
import std.workers.next_message;
import std.workers.emit;
import std.workers.receive;
import std.workers.start_worker;
import std.workers.stop_worker;
import std.workers.tell;
import std.workers.with_deadline;

fn doubler(link: Link[int, int]) {
    while true {
        match next_message(link, Wait.forever) {
            case ok(n) {
                match emit(link, n * 2) {
                    case ok() {
                    }
                    case err(_) {
                        return;
                    }
                }
            }
            case err(_) {
                return;
            }
        }
    }
}

fn main() -> result[void] uses console {
    let w = try map_err(start_worker(doubler), fn(e: WorkerError) -> str { return "no worker"; });
    ...
}
```

## Validate

```sh
make quick AREA=workers                     # golden cases, doc examples, tests/workers.py
tests/run.sh tests/cases/workers
python3 tests/workers.py --group exchange   # SC-001
python3 tests/workers.py --group leaks      # SC-002
python3 tests/workers.py --group deadline   # SC-003
python3 tests/workers.py --group speed      # SC-004
python3 tests/workers.py --group replay     # SC-005
(cd examples/job-queue && ../../build/vo test main.vo)
python3 tests/ref_examples.py               # the reference under 15,000 tokens
make site-check
python3 tools/progress.py run test          # once; then read build/progress/test.log
```
