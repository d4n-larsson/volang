# Build scheduler

Read a build description from standard input, check it, and simulate building it with a fixed number of parallel workers, like `make -j`. Print the problems found, a log of what happens during the build, and a summary.

## Program structure

Split the program into at least three source files: reading and checking the input lines, the dependency graph (needed targets, cycles and priorities), and the build simulation each in their own file, plus the entry file that reads standard input, drives them and prints the output.

## Input

Lines are numbered from 1; every line counts. A line is split into fields at runs of spaces; leading and trailing spaces are ignored; a line without fields is ignored.

- A *name* is one or more characters, each a lowercase ASCII letter `a`-`z`, a digit or `_`.
- A *number* is one to four ASCII digits (leading zeros allowed); its value is the decimal value of the digits.

The first field selects the line's kind:

| Line | Meaning |
|---|---|
| `target <name> <duration> <dep> ...` | defines a target that takes `duration` time units to build and depends on zero or more other targets |
| `workers <n>` | the number of workers, 1 to 9 (default 1) |
| `retries <n>` | how many times a failed target is tried again, 0 to 9 (default 0) |
| `backoff <n>` | how long a failed target waits before it may start again, 0 to 1000 (default 0) |
| `timeout <n>` | the longest an attempt may run, 0 to 1000; 0 means no limit (default 0) |
| `fail <name> <attempt>` | the given attempt of the target fails; attempts are counted from 1, and `attempt` is 1 to 10 |
| `pool <pool> <size>` | defines a pool (a name) that lets at most `size` attempts of its targets run at the same time, 1 to 9 |
| `use <name> <pool>` | puts the target into the pool |
| `goal <name> ...` | one or more targets to build |

A line is *malformed* if its first field is none of these words, if it has the wrong number of fields (`target` needs at least 3 fields, `goal` at least 2, `workers`, `retries`, `backoff` and `timeout` exactly 2, `fail`, `pool` and `use` exactly 3), if a field that must be a name or number is not one, or if a number is outside its range. A target's duration must be 1 to 1000. A malformed line is reported (see Output) and otherwise ignored.

`workers`, `retries`, `backoff` and `timeout` may appear any number of times; the last well-formed one counts. `goal` lines add up. `fail` lines add up; repeating one has no further effect. Pool names and target names are separate: a pool may have the same name as a target.

## Checking

The checks below use only well-formed lines.

1. **Duplicate targets.** The first `target` line for a name defines it. A later `target` line with the same name is reported as a duplicate and ignored completely (its dependencies are not checked).
2. **Duplicate pools.** The first `pool` line for a name defines the pool. A later `pool` line with the same name is reported as a duplicate and ignored.
3. **Unknown names.** A target name is *defined* if some well-formed `target` line anywhere in the input defines it (earlier or later); a pool name is defined if some well-formed `pool` line anywhere in the input defines it. On a defining `target` line, each dependency that is not defined is reported, once per distinct name, in the order of first appearance on the line. A dependency listed several times counts once. A `fail`, `use` or `goal` line naming an undefined target is reported; for a `goal` line, each undefined name is reported once, in order of first appearance on the line, and the defined names on the line still count as goals. A `use` line naming an undefined pool is reported (after the target, if both are undefined). A `fail` or `use` line with an undefined name has no other effect. When several `use` lines with defined names name the same target, the last one counts: a target is in at most one pool.
4. **Cycles.** Target `a` *reaches* target `b` if `a` depends on `b`, or `a` depends on some target that reaches `b` (only defined dependencies count). A target is *cyclic* if it reaches itself (this includes a target that depends on itself). Two cyclic targets are in the same *cycle group* if each reaches the other.

A target is *broken* if it has an undefined dependency or is cyclic. These checks cover all defined targets, needed or not.

## Needed and buildable targets

If there is no well-formed `goal` line, every defined target is a goal. Otherwise the goals are the defined names on the `goal` lines (possibly none). The *needed* targets are the goals and every target a goal reaches.

A needed target is *blocked* if it is broken or reaches a broken target. The other needed targets are *buildable*; only they take part in the build.

The *priority* of a buildable target is its duration plus the largest priority among the buildable targets that depend on it directly (plus 0 if there are none). Priorities are fixed before the build starts and never change.

## The build

Time is an integer that starts at 0. The workers are numbered from 1. Each worker runs at most one attempt at a time. The attempts are numbered per target: the first start of a target is attempt 1, its next start attempt 2, and so on. An attempt of target `t` that starts at time `s`:

- *times out* if the timeout is not 0 and `duration(t)` is greater than the timeout. It ends at `s + timeout`.
- otherwise *fails* if there is a `fail <t> <a>` line for its number `a`. It ends at `s + duration(t)`.
- otherwise *succeeds*. It ends at `s + duration(t)`.

The *length* of an attempt is its end minus its start.

A buildable target becomes *ready* at the moment all of its dependencies have succeeded (at time 0 if it has none). A ready target waits in the *ready set* until a worker takes it.

At each moment when something happens, the steps below run in this order:

1. Every attempt ending at this moment ends, in order of worker number. Its worker becomes free.
   - On success the target has *succeeded*: print `<time> done <target> w<worker>`.
   - On failure print `<time> fail <target> w<worker> #<attempt>`; on a timeout print `<time> timeout <target> w<worker> #<attempt>`. In both cases, if the target has made fewer than `retries + 1` attempts, it becomes ready again at `time + backoff`. Otherwise it has *failed*: print `<time> give up <target>`, then for every buildable target that reaches it and has not been skipped before, in byte order (ASCII order) of name, print `<time> skip <name>`. A skipped target never becomes ready.
2. Every target becoming ready at this moment (because its last dependency succeeded in step 1, or because its backoff ends now) joins the ready set. With backoff 0 a failed target therefore joins the ready set at the same moment it failed.
3. A target in the ready set *may start* unless it is in a pool and the number of running attempts of that pool's targets equals the pool's size. While there is a free worker and some target in the ready set may start, the target with the highest priority among those that may start (ties: the smaller name in byte order) leaves the ready set and starts on the free worker with the smallest number: print `<time> start <target> w<worker> #<attempt>`. A target that may not start stays in the ready set.

The build ends when no attempt is running and no target is in the ready set or waiting for its backoff to end.

## Output

Print, in this order:

1. The problems, one per line, sorted by line number; problems on the same line in the order rule 3 gives:
   - `line <N>: malformed`
   - `line <N>: duplicate target <name>`
   - `line <N>: duplicate pool <pool>`
   - `line <N>: unknown dependency <dep> of <name>`
   - `line <N>: unknown target <name>` (from a `fail`, `use` or `goal` line)
   - `line <N>: unknown pool <pool>` (from a `use` line)
2. One line `cycle: <names>` per cycle group, its names in byte order separated by single spaces. The groups are ordered by their first name in byte order.
3. The build log described above.
4. The summary:
   - `makespan <T>`: the time at which the last attempt ended, or 0 if no attempt started.
   - `utilization <P>%`: the sum of the lengths of all attempts (failed and timed-out ones included) times 100, divided by `workers * makespan`, rounded down to an integer; `0%` if the makespan is 0.
   - For each worker in order: `w<k> busy <B> attempts <A>`, where `B` is the sum of the lengths of the attempts it ran and `A` their number.
   - `built: <names>` in the order the targets succeeded.
   - `failed: <names>` in byte order.
   - `skipped: <names>` in byte order.
   - `blocked: <names>` in byte order (needed targets that are blocked).

   In these four lists the names are separated by single spaces; an empty list is written `-` (for example `failed: -`).

Every output line ends with a newline.
