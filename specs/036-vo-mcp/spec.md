# Feature Specification: The Toolchain as an Agent Server (`vo mcp`)

**Feature Branch**: `036-vo-mcp`

**Created**: 2026-11-21

**Status**: Implemented (gate not run)

**Input**: User description: "`vo mcp`: an MCP (Model Context Protocol) server inside the vo binary, so
any agent host can use the toolchain with no prompt engineering. `vo mcp` speaks JSON-RPC 2.0 over
stdin/stdout (the stdio transport, newline-delimited messages) and stays running for the session. It
offers the existing commands as tools: `check`, `test`, `run`, `fmt`, `doc`, `query`, `impact`, `edit`,
`explain`, `effects`; each tool takes the arguments its command takes and returns the command's schema-1
JSON output and exit status, unchanged, so there is one output format and not a second one. It offers
`vo reference` as a resource. Source: docs/review-2026-11/ecosystem.md, Tooling, 'An MCP server for
agents' (roadmap step 3). Must keep: a tool call gives exactly what the command gives, each call
isolated from the others, the working directory rules and the effect grant of the command line, no
libc, three architectures, the reference under 15,000 tokens (about 14,755 now). Out of scope: the HTTP
transport, prompts, sampling, a language server, `build`/`package`/`replay` as tools, a published agent
skill."

Background: vo is written for agents, and everything an agent needs is in the toolchain: diagnostics
with fixes, tests, documentation, queries, safe edits, the reference. But the agent has to be told. A
person writes "this project uses vo; run `vo check --json`; read `vo reference` first" into a prompt, for
every project and every agent host, and an agent that was not told falls back to guessing a language it
has never seen. Agent hosts have a standard way for a program to say "these are my tools and my
documents": the Model Context Protocol. A host that is given the one line `vo mcp` then knows every
command, what it takes and what it gives back, and can read the reference, without a word of prompt.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - An agent finds and uses the toolchain by itself (Priority: P1)

A developer adds `vo mcp` to their agent host's server list, in the project's directory. The agent now
sees the tools `check`, `test`, `run`, `fmt`, `effects`, `explain`, `doc`, `query`, `impact` and `edit`,
each with a description and named arguments. It calls `check` on a file and gets the same diagnostics,
codes and fixes that `vo check --json` prints.

**Why this priority**: it is the feature.

**Independent Test**: a scripted client starts `vo mcp`, performs the opening exchange, lists the tools,
and calls each of them on a small project; each result's output and exit status equal those of the same
command run directly with `--json`.

**Acceptance Scenarios**:

1. **Given** a host that starts `vo mcp`, **When** it performs the protocol's opening exchange, **Then**
   the server answers with its name, the toolchain version and what it offers (tools and resources),
   and stays running until its input ends.
2. **Given** the list of tools, **When** read, **Then** every tool has a one-sentence description, a
   schema naming each argument with its type and whether it is required, and a mark saying whether it
   only reads or may change files (`fmt`, `edit`, `effects` with update) or run a program (`run`,
   `test`).
3. **Given** a tool call, **When** it finishes, **Then** the result holds the command's standard output,
   its standard error and its exit status, byte for byte what the command gives with `--json` in the
   server's directory.
4. **Given** a call whose command reports problems in the user's program (check errors, a failing test,
   a runtime error), **When** it finishes, **Then** the result is a normal result carrying those
   reports; it is marked as an error only when the command itself could not be carried out (a usage
   error or an internal error).
5. **Given** a call with a missing, unknown or wrongly typed argument, or an unknown tool, **When**
   made, **Then** the answer is the protocol's error for invalid parameters, naming the argument; no
   command runs.
6. **Given** `run`, **When** called, **Then** the program's own arguments and its standard input can be
   given as arguments of the call, and the program never reads the server's input.

---

### User Story 2 - One call cannot hurt the next (Priority: P1)

An agent's session is long and its programs are often wrong. A program that loops forever, exhausts
memory, exits, or trips an internal error ends that call with the report the command line would give;
the server answers the next call as if nothing had happened.

**Why this priority**: a server that dies with the first bad program is worse than the command line it
replaces.

**Independent Test**: in one session, call `run` on a program that loops (timeout), one that allocates
without bound (memory limit), one that calls `exit(3)`, and one with a runtime error; then call `check`
on a good file. Every call gets its answer, and the last equals a fresh `vo check --json`.

**Acceptance Scenarios**:

1. **Given** a `run` or `test` call with no limits given, **When** the program loops or allocates
   without bound, **Then** the command line's default time and memory limits end it, the result carries
   R0009 or R0006 with the usual exit status, and the server continues.
2. **Given** any sequence of calls, **When** the same call is repeated later in the session, **Then** it
   gives the same result as the first time and as in a fresh session (nothing leaks between calls: no
   memory, seeds, open files or settings).
3. **Given** a call that produces very much output, **When** it finishes, **Then** the result holds the
   first part up to a documented limit and says how much was left out; the server's memory does not
   grow with the output.
4. **Given** a host that cancels a call in progress, **When** the cancellation arrives, **Then** the work
   stops promptly and the server is ready for the next call.
5. **Given** the server's input ends, **When** requests already received are unanswered, **Then** they
   are answered and the server exits with status 0. **Given** the host goes away (the answers can no
   longer be delivered, or the server is ended), **Then** the work in progress stops at once and no
   process is left behind.

---

### User Story 3 - The reference is one request away (Priority: P2)

The agent host lists the server's resources and finds the language reference. Reading it gives exactly
what `vo reference` prints. An agent can also ask `explain` for any code and `doc` for any module or
item, as tools.

**Why this priority**: the reference is what makes an agent write correct vo the first time; tools
alone let it only find its mistakes.

**Independent Test**: list the resources, read the reference, and compare with `vo reference`.

**Acceptance Scenarios**:

1. **Given** the list of resources, **When** read, **Then** it holds the reference with a name, a
   one-sentence description and its type (Markdown).
2. **Given** a request for the reference, **When** answered, **Then** the text is byte for byte what
   `vo reference` prints; a request for anything else is the protocol's error for an unknown resource.

---

### User Story 4 - A protocol that does not break (Priority: P2)

Hosts differ and some are wrong. A malformed line, an unknown method, a request before the opening
exchange or a message that is too large gets the protocol's proper error (or is ignored where the
protocol says so), and the server keeps running.

**Why this priority**: an agent cannot debug a server that hangs or dies silently.

**Independent Test**: send a line that is not JSON, a request with an unknown method, a notification
with an unknown method, a call before the opening exchange and a line over the size limit; each gets
the documented answer and a following valid call succeeds.

**Acceptance Scenarios**:

1. **Given** a line that is not a valid message, **When** received, **Then** the server answers with the
   protocol's parse or invalid-request error and continues.
2. **Given** an unknown method, **When** received as a request, **Then** the answer is the protocol's
   method-not-found error; as a notification it is ignored.
3. **Given** a liveness request (ping), **When** received at any time, **Then** it is answered.
4. **Given** a host that asks for another revision of the protocol than the server knows, **When** the
   opening exchange happens, **Then** the server answers with a revision it supports, as the protocol
   prescribes.
5. **Given** the server's standard output, **When** observed for a whole session, **Then** it holds
   nothing but protocol messages, one per line.

---

### User Story 5 - Documented where hosts and agents read (Priority: P3)

The reference's command-line section lists `vo mcp` in a line or two; the site has a page showing how
to add the server to an agent host, what the tools are, and the limits.

**Why this priority**: the server removes the need to prompt; a developer still has to find the one
line to add.

**Independent Test**: the reference lists `vo mcp` and stays under 15,000 tokens; the site page's
configuration example starts a server that answers the opening exchange.

**Acceptance Scenarios**:

1. **Given** the reference, **When** read, **Then** `vo mcp` is in the command list with one sentence on
   what it offers.
2. **Given** the site, **When** read, **Then** a page shows the configuration line, the list of tools
   and the reference resource, and says what the server does not do.

---

### Edge Cases

- `vo mcp` with any argument, or `--json`: a usage error (T0001); the server takes no options.
- A terminal as input (a person typed `vo mcp`): it waits for messages like any server; end of input
  ends it.
- Paths in arguments: relative to the directory the server was started in, exactly as on the command
  line; the server adds no restriction and no permission the command line does not have.
- `run` of a program that reads input when no input was given: it sees end of input at once.
- `run` of a program that prints bytes that are not valid text: the result stays a valid message (the
  bytes are replaced in a documented way) and says so.
- `edit` and `fmt` change files, as on the command line; `edit` has its dry-run argument.
- An operand that begins with `--` (a file named like an option): refused as an invalid argument, so
  an argument can never be read as an option the caller did not name.
- `impact` and `test` take the text of a diff as an argument, not the name of a file holding it.
- Two calls sent without waiting: answered one after the other, in order.
- A call whose identifier repeats an earlier one: answered; the server does not track old identifiers.
- A message larger than the documented limit: the protocol's error, and the server continues.
- Limits given in a call (`timeout`, `max_memory`): the same ranges and errors as the command-line
  options.
- A file changed on disk between two calls: the second call sees the new content (no caching visible
  to the user).
- The server is started in a directory with no vo files: tools answer as the commands would (T0002).
- A built executable (`vo build`) is never a server: `./app mcp` passes `mcp` to the program.

## Requirements *(mandatory)*

### Functional Requirements

**The server**

- **FR-001**: `vo mcp` MUST serve the Model Context Protocol over its standard input and output, one
  message per line, until its input ends, and then exit with status 0.
- **FR-002**: The opening exchange MUST state the server's name, the toolchain version, and that it
  offers tools and resources; requests other than the opening exchange and ping made before it MUST be
  refused with a protocol error.
- **FR-003**: The server's standard output MUST hold only protocol messages. Malformed, unknown or
  oversized messages MUST get the protocol's error (or be ignored, for notifications) and MUST NOT end
  the server.

**Tools**

- **FR-004**: The tools MUST be `check`, `run`, `test`, `fmt`, `effects`, `explain`, `doc`, `query`,
  `impact` and `edit`. Each MUST have a description, a schema with one named argument for each operand
  and option of its command (except `--json`, always on, and options that write a second file where a
  call has no use for them), and marks for read-only and for changing files.
- **FR-005**: A tool call's result MUST hold the standard output, standard error and exit status that
  the same command gives with `--json` in the same directory with the same arguments, unchanged.
- **FR-006**: A result MUST be marked as an error only for a usage error or an internal error of the
  command; reports about the user's program are normal results.
- **FR-007**: Arguments MUST be validated against the schema before anything runs; a violation is the
  protocol's invalid-parameters error naming the argument.
- **FR-008**: `run` MUST accept the program's arguments and its standard input as arguments of the call.

**Isolation and limits**

- **FR-009**: Each call MUST be independent: its result does not depend on earlier calls, and a runtime
  error, exit, timeout, memory exhaustion or internal error in one call MUST NOT end the server or
  change a later result.
- **FR-010**: `run` and `test` MUST apply the command line's default time and memory limits unless the
  call gives others, within the command line's ranges.
- **FR-011**: Output beyond a documented limit per call MUST be cut, with the result saying how many
  bytes were left out; the server's own memory use MUST stay bounded whatever a call prints.
- **FR-012**: A cancellation from the host MUST stop the call in progress. At end of input the server
  MUST answer what it has received and exit; when the host is gone or the server is ended, work in
  progress MUST stop and no process may be left behind.
- **FR-013**: The server MUST grant nothing the command line does not: the same working directory, the
  same effect grant from `main`'s declaration and `vo.effects`, the same environment.

**Resources**

- **FR-014**: The server MUST offer one resource, the language reference, whose content equals the
  output of `vo reference`.

**Documentation and tests**

- **FR-015**: The reference MUST list `vo mcp` and stay under 15,000 tokens; the site MUST have a page
  with a host configuration example, the tools and the limits.
- **FR-016**: Every new mistake on the command line MUST reuse an existing code or have a stable code
  with a catalog entry; protocol mistakes use the protocol's own error codes.
- **FR-017**: The server MUST behave identically on the three architectures.

### Key Entities

- **Session**: one run of `vo mcp`: an opening exchange, then any number of requests, until input ends.
- **Tool**: one toolchain command offered to the agent: name, description, argument schema, marks.
- **Call result**: standard output, standard error, exit status, and whether output was cut.
- **Resource**: the language reference, readable by its address.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: For every tool, on a set of calls covering each argument, the result's output and exit
  status equal the direct command's, with no differences.
- **SC-002**: A session of 1,000 calls mixing good programs, check errors, runtime errors, timeouts and
  memory exhaustion answers every call, and the server's memory at the end is no higher than after the
  first ten calls.
- **SC-003**: A call adds no more than 5 ms to the time of the same command run directly.
- **SC-004**: Every malformed-message case in the test set gets the documented answer, and the call
  after it succeeds.
- **SC-005**: A general agent host, given only the server (no prompt about vo), lists the tools, reads
  the reference and gets a check result; verified once by hand with one host.
- **SC-006**: The reference stays under 15,000 tokens, and every existing test case is unchanged.

## Assumptions

- Only the standard-input transport. A host that needs HTTP can wrap it; nothing here prevents adding
  it later.
- Only tools and one resource. No prompts, no sampling, no logging messages, no progress messages, no
  subscriptions, no completion of arguments.
- One call at a time; calls sent together are answered in order. Agents wait for each answer, and
  programs under test share the directory.
- `build`, `package`, `replay` and `version` are not tools: an agent that writes and tests a program
  does not need them, and the version is in the opening exchange. They can be added without breaking
  anything. `--record` and `--trace` are likewise left out of `run` and `test` in this version.
- Limits: 1 MiB of standard output and 1 MiB of standard error per call, and 4 MiB per incoming
  message. A result's text is the command's output as is; the agent already reads schema-1 JSON lines.
- The server answers in the newest protocol revision it knows and falls back as the protocol
  prescribes; revisions are named in the plan.
- The site does not gain a published "agent skill"; that is a separate item of the review.
