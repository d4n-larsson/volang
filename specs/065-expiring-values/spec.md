# Feature Specification: Expiring values (a value that cannot be read without the time)

**Feature Branch**: `065-expiring-values`

**Created**: 2026-11-28

**Status**: Implemented (2026-11-28; not released)

**Input**: User description: "expiring values in the standard library: a generic type expiring[T] that
holds a value together with the moment it stops being valid. The only way to read the value takes the
current time and returns a result, so the caller must handle the expired case. For access tokens, cached
responses, sensor readings and temporary locks. No new syntax and no change to the type system; reading
the clock stays under the existing effect so recorded requests replay the same way."

Background: a backend service holds things that are only good for a while: an access token another
system issued for 30 minutes, a response it cached for 10 seconds, a reading from a sensor, a lock it
took for one minute. Today a vo program keeps such a thing as a value and, beside it, a number: the
time it stops being good. Nothing connects the two. A function that receives the token can send it
without looking at the number, and the checker has nothing to say; the mistake shows up later as a 401
from the other system, or as stale data that nobody notices.

The idea came in as a "temporal type system" (`type AccessToken = String valid_for 30.minutes`, with the
compiler refusing the use of an expired token). Decided with the user on 2026-11-28: not as a type-system
feature. Whether a token has expired depends on the clock when the program runs, so no checker can decide
it. What the checker can decide is whether the program **asked**: if the value can only be read by
presenting a time, and the answer is a result, then vo's existing rule that a result must be handled does
the rest. That needs a type in the standard library and nothing else.

What this spec must keep:

- **Principle I:** no new syntax, no new kind of type, one way to write each thing.
- **Principle III:** the clock enters a program only through `now_ms` under `clock`. Reading an expiring
  value takes the time as an argument and reads no clock itself, so the same arguments give the same
  answer, tests need no clock, and a recording replays as before.
- **Principle V:** the smallest set of functions the four uses need; the reference stays under 15,000
  tokens.
- **Principle VI:** tokens, caches and leases are what a service that talks to other systems holds. A
  reviewer sees in a function's parameter type that what it receives can expire.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - A value that must be asked for with the time (Priority: P1)

A developer gets an access token from another system, together with how long it is good for, and wraps
the two into one expiring value. Every function that later needs the token must present a time to get
it, and gets a result: the token, or the fact that it expired and when. The developer cannot forget the
expired case, because there is no other way to the token.

**Why this priority**: it is the whole feature. The other stories are conveniences on top of it.

**Independent Test**: a program wraps a string good until time 1,000 and reads it at times 0, 999,
1,000 and 5,000; the first two give the string, the last two give the expiry with both times. A second
program that tries to use the wrapped value as a string, or to reach inside it by any means other than
the read, is refused by `vo check`.

**Acceptance Scenarios**:

1. **Given** a value wrapped with an expiry moment, **When** it is read with a time before that moment,
   **Then** the result is the value.
2. **Given** the same, **When** it is read with a time equal to or after the moment, **Then** the result
   is an error that carries the expiry moment and the time presented.
3. **Given** an expiring value, **When** a program passes it where the plain value is expected, **Then**
   `vo check` reports a type error at that place.
4. **Given** an expiring value, **When** a program reads it and ignores the result, **Then** `vo check`
   reports the unhandled result as it does for any other result.
5. **Given** an expiring value of any type (a string, a record, a list, a connection), **When** it is
   wrapped and read, **Then** the read gives back that type with no conversion.
6. **Given** a program that reads an expiring value with a time it got from `now_ms`, **When** it is
   recorded and replayed, **Then** the replay gives the same output and exit status, and the function
   that only reads the value declares no `clock` effect.

---

### User Story 2 - Good for a duration, and how long is left (Priority: P2)

Most systems say "good for 1,800 seconds", not "good until this moment". A developer wraps a value with
the time now and a duration. Later the developer asks an expiring value when it expires and how long it
has left at a given time, without reading the value: to decide whether to fetch a fresh token before a
long call, to set a cache header, to log it.

**Why this priority**: without it every program does the addition and the subtraction itself, which is
where the off-by-a-unit mistakes (seconds for milliseconds) are made. The feature works without it.

**Independent Test**: a value wrapped at time 1,000 for 500 ms expires at 1,500; at time 1,200 it has
300 ms left, at 1,500 and after it has 0.

**Acceptance Scenarios**:

1. **Given** a value wrapped with a time and a duration, **When** its expiry moment is asked for,
   **Then** it is the time plus the duration.
2. **Given** an expiring value, **When** the time left is asked for at a time before expiry, **Then** it
   is the difference; at or after expiry it is 0, never negative.
3. **Given** a token that must still be good 60 seconds from now, **When** the developer reads it with
   the time now plus 60,000, **Then** a token with less than that left is reported as expired; no
   separate function for a margin exists.
4. **Given** a duration of 0 or less, **When** a value is wrapped with it, **Then** the value is expired
   from the start and reading it at the wrapping time gives the expiry error.

---

### User Story 3 - Keeping an expiring value somewhere else (Priority: P3)

A developer wants to keep a token in a database or send a cached response to another process. The
developer reads the value at the current time, stores the value and the expiry moment as two ordinary
fields, and wraps them again on the way back. Nothing is lost on the round trip.

**Why this priority**: it is a documented pattern more than a function, but the guide must show it,
because an expiring value itself is not something to serialize.

**Independent Test**: a program wraps a value, reads it, stores value and expiry moment in a record,
rebuilds the expiring value from the record, and gets the same answers to every read as from the
original.

**Acceptance Scenarios**:

1. **Given** an expiring value, **When** its value (read at a time it is valid) and its expiry moment
   are wrapped again, **Then** the new expiring value answers every read as the original does.
2. **Given** an expiring value, **When** a program tries to turn it into JSON or a database row as a
   whole, **Then** `vo check` refuses, with the message it already gives for a type that has no such
   form.

---

### Edge Cases

- **The time presented is earlier than the time the value was wrapped** (the wall clock was set back):
  the value reads as valid. Validity is a function of the presented time and the expiry moment only;
  the wrapping time is not kept.
- **Exactly at the expiry moment**: expired. A value is valid for times strictly before its expiry
  moment.
- **Time plus duration overflows an int**: the program stops with the runtime report every int overflow
  gives; no silent wraparound to a moment in the past.
- **A negative time, or a time of 0**: allowed; times are ints as everywhere in `std.time`, and tests run
  with the clock at 0.
- **Comparing two expiring values with `==`**: answered by the plan (FR-010); it must be either defined
  or a check error, never a run-time surprise.
- **An expiring value inside an expiring value**: allowed; each read peels one.
- **An expiring value sent to a worker** (`concurrent_map`, spec 047): answered by the plan (FR-010). If
  it cannot cross, the refusal is the one vo already gives, and the way across is User Story 3's: send
  the value and the expiry moment, wrap again on the other side.
- **The caller presents a wrong time on purpose** (a constant 0): the library cannot prevent it, and
  does not try. What it guarantees is that the reading function states a time; a reviewer sees which.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The standard library MUST offer one generic type for a value of any type together with
  the moment (milliseconds since 1970-01-01T00:00:00Z, as in `std.time`) from which it is no longer valid.
- **FR-002**: A program MUST be able to make an expiring value from a value and an expiry moment, and
  from a value, a time and a duration in milliseconds.
- **FR-003**: The only way to obtain the wrapped value MUST be a read that takes a time and returns a
  result: the value when the time is before the expiry moment, otherwise an error. No field access,
  pattern match, conversion or other library function may give the value without a time.
- **FR-004**: The error of an expired read MUST carry the expiry moment and the time presented, and
  MUST have one fixed text form, so that a function can pass it on with `try` and a log line says how
  late the read was.
- **FR-005**: The read MUST NOT read the clock and MUST NOT need any effect; the same expiring value and
  time MUST always give the same result.
- **FR-006**: A program MUST be able to ask an expiring value for its expiry moment, and for the time
  left at a given time (0 when expired), without a result and without reading the value.
- **FR-007**: The feature MUST NOT add syntax, keywords, builtins, a kind of type, an effect, or a rule
  to the checker. If FR-003 cannot be met by the library alone, the plan MUST stop and say so; it MUST
  NOT fall back to a convention ("do not read this field").
- **FR-008**: The feature MUST NOT add an error or warning code unless FR-003 requires one, and then at
  most one, with a golden case.
- **FR-009**: The reference, the standard library page and the guide MUST describe the type, with one
  executed example for a token and the store-and-rebuild pattern of User Story 3; the growth of the
  reference in tokens MUST be stated in the change.
- **FR-010**: The plan MUST state what `==`, `str`, JSON and sending to a worker do with an expiring
  value, each as either defined behavior or a check error, and a golden case MUST show each.
- **FR-011**: Nothing in `std.security`, `std.web` or `std.http` changes its signatures in this spec.
  Using the type inside the library (session expiry, token `exp`) is a later decision, made when a
  service shows the need.

### Key Entities

- **Expiring value**: a value of some type and its expiry moment. Made by wrapping; read with a time;
  not changed after it is made (a fresh token is a new expiring value).
- **Expiry moment**: an int, milliseconds since 1970-01-01T00:00:00Z. The first moment at which the
  value is no longer valid.
- **Expired error**: what a late read gives: the expiry moment and the time presented.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: In a program that holds a token as an expiring value, there are 0 ways to send the token
  that `vo check` accepts and that do not state a time and handle the expired result. Shown by golden
  cases for each attempted way around (field, match, conversion, equality, text).
- **SC-002**: An agent that has only the reference and the standard library page writes a correct
  "fetch a token, reuse it until it expires, fetch again" function on the first `vo check` in at least 4
  of 5 isolated runs.
- **SC-003**: A recording of a program that uses expiring values replays to the same output and exit
  status on all three platforms.
- **SC-004**: The language reference grows by at most 150 tokens and stays under 15,000.
- **SC-005**: The toolchain's C code does not change: the diff of `src/` is empty, unless FR-008's one
  code is needed.
- **SC-006**: `make test` passes, with every example in the new documentation executed.

## Assumptions

- Times are wall-clock milliseconds as returned by `now_ms`. vo has no monotonic clock, and this spec
  adds none; a clock set back makes a value look valid for longer, as it does for every expiry a program
  computes by hand today.
- The read takes the time as an argument instead of reading the clock itself. A service usually reads
  the clock once per request and passes the time down; the read stays free of effects and tests need no
  `--clock-ms`. The cost is that a caller can present a wrong time; see Edge Cases.
- An expiring value is something a running program holds. It has no JSON or database form of its own;
  User Story 3 is the way to store one.
- Out of scope: a cache (a map of expiring values with eviction), refreshing a value automatically,
  locks or leases as such (an expiring value can hold a lock's handle; taking and releasing the lock is
  the program's), sliding expiry, and timers that fire when a value expires.
- Out of scope: the other two ideas that came with this one, complexity analysis and hardware-aware
  compilation. Both were rejected on 2026-11-28.
- No git branch is created; work continues on `main` as for the previous specs.
