# job-queue

Three workers take jobs from the program. Each job has 300 ms; the answers are printed as they arrive,
and a job that takes too long is given up without stopping the others. Exit status 2 when a job was
given up.

```
../../bin/vo run main.vo
../../bin/vo test main.vo                      # the work and the deadlines, without a worker
../../bin/vo replay recordings/demo.jsonl      # a saved run: no worker is started
```

What it shows:

- **Workers that stay.** `start_worker(serve_jobs)` returns at once with a handle; the function runs in
  a process of its own, taking a job with `next_message` and answering with `emit` until it is stopped.
  `concurrent_map` would wait for all seven jobs; here the program goes on between answers.
- **Whichever answers first.** `receive_first(workers, Wait.at_most(ms))` gives the first answer from
  any worker and says which one sent it, or `timeout` when the nearest deadline passes first.
- **A deadline that ends the work.** `stop_worker` ends a worker whatever it is doing, here in the
  middle of counting primes, and a new one takes its seat. The fourth job would take minutes; the run
  takes a third of a second.
- **Typed messages, nothing shared.** The handle is a `Worker[Job, Done]`: jobs go in, answers come
  out, both copied. A worker has no access to the program's values after it is started.
- **Effects stay visible.** Starting a worker is not an effect; what the worker's function does is, and
  the function that starts it must declare it. `vo.effects` shows `serve_jobs pure`: these workers
  only compute.
- **Tests without workers.** The work (`primes_below`) and the arithmetic of the deadlines
  (`wait_until_due`) are pure functions in `jobs.vo`.
- **Replay.** The order of the answers depends on timing, so two runs can print the jobs in another
  order. A recording holds what the program received from its workers; replaying it prints the same
  lines without starting a process.

For a single call with a time limit there is `with_deadline(ms, f)`.
