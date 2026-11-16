# url-check

Checks a list of URLs at the same time and prints one line per URL, in the order given. Exit status 2
when one is down, so a script can use it.

```
../../bin/vo run main.vo -- http://localhost:8000/health http://localhost:8001/health
../../bin/vo replay recordings/demo.jsonl      # a saved run: no network needed
```

What it shows:

- **`concurrent_map(urls, 8, check)`** runs `check` on each URL as a separate task, at most 8 at a time,
  and returns the results in the order of the URLs. The run takes about as long as the slowest URL; the
  saved demo has two URLs that take 500 and 400 ms and finishes in about 0.5 s.
- **One line changes, the effects do not.** `sequential.vo` is the same program with `map`.
  `concurrent_map` is not a capability, so `vo.effects` is the same for both: a reviewer sees no new
  effect when a loop becomes concurrent.
- **Tasks share nothing.** Each task works on its own copy of what it is given; only its `Check` comes
  back. Output and errors come in the order of the URLs, whatever order the answers arrive in, and the
  recording replays exactly.
- **The grant still holds.** `check` declares `net("localhost")`; a URL on another host stops the program
  with R0010, as in the sequential version.
