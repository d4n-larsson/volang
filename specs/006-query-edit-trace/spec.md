# Feature Specification: Query, Structural Editing, Trace and Replay

**Feature Branch**: `006-query-edit-trace`

**Created**: 2026-11-14

**Status**: Draft

**Input**: User description: "query API, structural editing, trace and replay (spec 006, per docs/roadmap.md)". The roadmap entry (docs/roadmap.md, "006: query, structural editing, trace and replay") says:
- **Queries:** `vo query callers parse_line`, `vo query type-at main.vo:12:5`, and `vo impact --diff` (which functions and tests a change affects).
- **Structural edits:** `vo edit rename` and `vo edit add-field`, verified by the checker instead of done by fragile text replacement.
- **Traces and replay:** `vo run --trace` writes calls with arguments and results as JSON lines; runtime errors include local values; failing runs can be replayed exactly.
- **Success:** better results on the refactor category of the benchmark.

Clarified with the user on 2026-11-14:
- **Success is gated on a deterministic stand-in**: scripted refactorings of the refactor tasks' starter programs, not an agent benchmark run. The agent run is a follow-up.
- **One spec, prioritized**: queries and structural edits are P1, because they serve the refactor goal. Traces, local values in runtime errors, and replay are P2 stories in the same spec.

Planning note (2026-11-14): every query takes the program's entry file first (plan, research R3), so the roadmap's `vo query type-at main.vo:12:5` is written `vo query type-at main.vo main.vo:12:5`.

## User Scenarios & Testing *(mandatory)*

The users are the same as in specs 001–005:
- **AI coding agents**, which write and change vo programs from the reference and fix them from diagnostics.
- **The people who review and run those programs.**

This spec addresses the last two of the roadmap's problems with agent-written code:
- **Agents cannot see the whole codebase.** They grep, guess, and break code they never saw. A text search for `item` finds comments, strings and unrelated locals, and misses nothing that matters only by luck. A rename done by text replacement silently changes the wrong names, or misses an import in another file.
- **Debugging is mostly print statements.** When a run fails, the agent sees the error and the call trace, but not the values that led there. It adds prints, reruns, and has to reproduce the input that failed.

### User Story 1 - Ask the toolchain about the program (Priority: P1)

An agent working on a multi-file program asks exact questions instead of grepping:
- **Who calls this function?** Every call site, in every file of the program, with the calling function.
- **Where is this name used?** Every reference to a function, record, variant, field or variant case, and nothing that merely looks the same (a string, a comment, a local with the same name).
- **What is this?** The type of the expression or name at a position, and where that name is defined.
- **What does my change affect?** Given a diff, which functions changed, which functions call them (directly or not), and which tests and properties reach them.

Answers come as text and as JSON records, from the same checked program the compiler sees.

**Why this priority**: Every refactoring starts by finding what to change. Exact answers replace the grep-and-guess step, which is where agents break code they did not read. It is also the basis of the structural edits (User Story 2).

**Independent Test**: Run each query on golden programs (including multi-file programs and programs with shadowed and look-alike names) and compare the output with hand-verified expectations.

**Acceptance Scenarios**:

1. **Given** a three-file program where `parse_line` is called from `main` in `main.vo` and from `load` in `io.vo`, and passed as a function value to `map` in `report.vo`, **When** `vo query callers main.vo parse_line` runs, **Then** it lists the three sites with file, span and enclosing function, marks the third as a use as a value, and exits 0.
2. **Given** a record `Item` with a field `name`, a local variable also called `name`, and the string `"name"`, **When** `vo query refs main.vo Item.name` runs, **Then** it lists the field's declaration, every record literal and field access that uses it, and neither the local nor the string.
3. **Given** `main.vo:12:5` pointing inside a local `total` declared by `let total = sum(xs);`, **When** `vo query type-at main.vo main.vo:12:5` runs, **Then** it reports the name, its type `int`, the span of the expression at that position, and the span of its declaration.
4. **Given** a diff that changes the body of `tax` (called by `subtotal`, called by `checkout`), and tests that call `checkout` and others that do not, **When** `vo impact main.vo --diff change.diff` runs, **Then** it reports `tax` as changed, `subtotal` and `checkout` as affected, and exactly the tests and properties that reach `tax` through calls.
5. **Given** a name that does not exist, or a position outside any expression, **When** a query runs, **Then** it reports a structured error with a stable code (and, for a misspelled name, the closest existing name), and exits with a documented non-zero status.
6. **Given** a program with check errors, **When** a query runs, **Then** it still answers for the parts that check, and says that the program has errors.

---

### User Story 2 - Change the program by structure, not by text (Priority: P1)

An agent renames `item` to `product` across a program, or adds a field to a record used in a dozen places. Instead of editing text, it asks the toolchain to do the edit:
- **`vo edit rename`** renames a function, record, variant, variant case, record field, parameter or local. It changes every reference in every file of the program, including imports and the effects record, and nothing else.
- **`vo edit add-field`** adds a field with a default value to a record, and adds that field with the default to every record literal of that record.

The toolchain checks the program after the edit. If the edit would introduce any check error (a name clash, a shadowed name, a reserved word), it writes nothing and reports why. Otherwise it writes the files and prints exactly what it changed, in the same form as diagnostic fixes. A dry run prints the changes without writing.

**Why this priority**: This is the roadmap's answer to fragile text replacement. A rename that cannot break the program, and lists exactly what it touched, is both faster for the agent and easier for a reviewer to approve.

**Independent Test**: Apply edits to golden programs, then compare the resulting files byte for byte with expected files, and run the edited programs to show their behavior is unchanged. Apply edits that must be refused, and check that no file changed.

**Acceptance Scenarios**:

1. **Given** a program where `add_item` is defined in `catalog.vo`, imported and called in `main.vo`, and the string `"add_item"` appears in a `print`, **When** `vo edit rename main.vo add_item add_product` runs, **Then**:
   - the definition, the import line and every call are renamed;
   - the string is unchanged;
   - the program checks clean and its output for any input is unchanged;
   - the changes are printed (file, span, old and new text), and the exit status is 0.
2. **Given** a rename whose new name is already used by another top-level item, or would be shadowed by or shadow a local at some reference, **When** the edit runs, **Then** it is refused with a structured error naming the conflicting declaration, and no file changes.
3. **Given** a record field `price` renamed to `price_cents` with `vo edit rename main.vo Item.price price_cents`, **When** the edit runs, **Then** the declaration, every record literal and every field access are renamed, and a local variable or another record's field called `price` is not.
4. **Given** a local variable addressed by position (`vo edit rename main.vo main.vo:14:9 count`), **When** the edit runs, **Then** only that variable's declaration and uses change.
5. **Given** a function listed in the effects record (`vo.effects`) that is renamed, **When** the edit runs, **Then** the record's line for it is renamed too, and `vo check` reports no effects change.
6. **Given** `vo edit add-field main.vo Item discount: int = 0`, **When** the edit runs, **Then** the field is added at the end of the declaration, every `new Item { ... }` gets `discount: 0` at the end, and the program checks clean and behaves as before.
7. **Given** an add-field whose default does not have the field's type, whose field name already exists, or whose default uses a name that is not visible at every record literal, **When** the edit runs, **Then** it is refused with a structured error and no file changes.
8. **Given** `--dry-run`, **When** any edit runs, **Then** it prints the same changes and the same verdict, and writes nothing.
9. **Given** a program that already has check errors, **When** an edit runs, **Then** it is refused, unless the errors after the edit are exactly the errors before it (the edit introduces none and hides none).

---

### User Story 3 - See the calls that led to a result (Priority: P2)

An agent's program prints a wrong total. Instead of adding prints, it runs the program with a trace: every call of a program function, with its arguments, and every return, with its value, as JSON lines in a file. It finds the first call that returned something unexpected.

**Why this priority**: It replaces the add-prints-and-rerun loop with one run. It is not needed for refactoring, so it comes after the P1 stories.

**Independent Test**: Trace golden programs (recursion, function values, `try` returns, runtime errors, timeouts) and compare the trace files with expected files. Run the speed benchmark with tracing off.

**Acceptance Scenarios**:

1. **Given** `fn fact(n: int) -> int` called as `fact(3)` from `main`, **When** `vo run --trace t.jsonl main.vo` runs, **Then** the program's output is unchanged and `t.jsonl` holds, in order, a call and a return record for each of `main`, `fact(3)`, `fact(2)`, `fact(1)`, `fact(0)`, each with its depth, call site, arguments by name in vo syntax, and the returned value.
2. **Given** a run that stops with a runtime error, **When** traced, **Then** the calls still active at the error have no return record, and the trace ends with one record for the runtime error, then the final end record.
3. **Given** a function that returns early through `try`, **When** traced, **Then** its return record shows the propagated `err(...)` value.
4. **Given** the same program, input and options, **When** traced twice, **Then** the two trace files are byte-identical.
5. **Given** a run that would produce more records than the trace limit, **When** traced, **Then** recording stops at the limit, a final record says how many records were left out, and the program keeps running normally.
6. **Given** `vo test --trace t.jsonl`, **When** tests run, **Then** each test's calls appear under a record naming that test.

---

### User Story 4 - Runtime errors show the values (Priority: P2)

When a run stops with a runtime error, each frame of the call trace shows the values of its parameters and local variables at that point. The agent sees `i = 5, len(xs) = 5` next to the index error, instead of guessing.

**Why this priority**: It turns most failed runs into a diagnosis without a second run. It is small, and it shares value rendering with traces.

**Independent Test**: Golden runtime-error cases, in text and JSON, compared with expectations.

**Acceptance Scenarios**:

1. **Given** `xs[i]` failing with an index out of range inside `last(xs)` called from `main`, **When** the run stops, **Then** the runtime error's trace gives, for each frame, the parameters and the locals in scope at that point, each with its name, type and value in vo syntax.
2. **Given** a local that is declared later in the block than the failing statement, **When** the run stops, **Then** that local is not shown.
3. **Given** a very long list or string in a local, **When** shown, **Then** its rendering is shortened to a fixed limit and marked as shortened.
4. **Given** a test that stops with a runtime error, **When** `vo test` reports it, **Then** its `failure` carries the same per-frame values.
5. **Given** any runtime error that did not happen inside program code (for example a timeout raised inside a builtin), **When** reported, **Then** the frames that exist are shown with their values, as above.

---

### User Story 5 - Replay a failing run exactly (Priority: P2)

A program fails on an input that came from stdin, files, environment variables and the clock. The agent recorded the run. It fixes the code and replays the recording: the fixed program gets exactly the same input as the failing run, without the agent recreating the files, the environment or stdin.

**Why this priority**: vo runs are already deterministic given their inputs, so replay only has to capture what came from outside. It makes "does my fix work on the input that failed" a single command.

**Independent Test**: Record runs of golden programs that read stdin, files, environment variables, arguments and the clock, then replay them with the inputs removed or changed, and compare output and exit status byte for byte.

**Acceptance Scenarios**:

1. **Given** `vo run --record r.jsonl main.vo -- a b` on a program that reads stdin, a file, an environment variable and the clock, **When** the run ends (normally or with an error), **Then** `r.jsonl` holds everything the program received from outside, in order.
2. **Given** that recording, with the file deleted, the environment variable unset and no stdin, **When** `vo replay r.jsonl` runs, **Then** the program's stdout, stderr and exit status are byte-identical to the recorded run.
3. **Given** a recording replayed with `--trace`, **When** it runs, **Then** the trace is produced for the replayed run.
4. **Given** a program changed after recording so that it reads its inputs in the same order, **When** replayed, **Then** it runs on the recorded inputs and the report says the program differs from the one recorded.
5. **Given** a changed program that asks for an input the recording does not have at that point (a different file, more lines than were read, another variable), **When** replayed, **Then** it stops with a new runtime error naming the first input that differs.
6. **Given** a program that writes a file during the recorded run, **When** it is replayed, **Then** the write is not performed again, and the program receives the recorded result of the write.

---

### User Story 6 - Evidence on the refactor tasks (Priority: P2)

The maintainers can show that structural edits and queries do the refactor tasks' mechanical steps correctly where text replacement does not. They use the starter programs of the benchmark's refactor tasks (r01–r10) and a fixed, reproducible script of edits, not an agent run.

**Why this priority**: The roadmap gives better refactor results as the success measure. An agent run is expensive and benchmark v2 has not been run yet, so a deterministic stand-in gates this spec (clarified with the user). The agent run is reported later.

**Independent Test**: For each refactor task, apply its scripted edits with `vo edit` and with the naive text replacement, then compare each result's behavior with the starter's on every visible and hidden input.

**Acceptance Scenarios**:

1. **Given** at least 3 scripted edits per task (renames of functions, records, fields and locals, and added fields, taken from what the task statement asks for), **When** each is applied with `vo edit`, **Then** every result checks clean and produces byte-identical output to the unedited starter on every visible and hidden input of the task.
2. **Given** the same edits done by whole-word text replacement over the program's files, **When** checked and run on the same inputs, **Then** the number of edits that fail to check or change behavior is recorded per task and in total.
3. **Given** a reference list of call sites and references for at least 3 names per task, written by hand from the starter, **When** the queries run, **Then** their answers match the lists exactly. The number of wrong hits from a whole-word text search for the same names is recorded.

---

### Edge Cases

- **Name forms in queries and edits:**
  - `name` for a top-level function, record or variant;
  - `Record.field` and `Variant.case`;
  - `FILE:LINE:COL` for anything at a position, including parameters, locals and type parameters.
  - A name that matches several things (a field name used by two records) is an error that lists the candidates.
- **Shadowing:** `refs` and `rename` follow the checker's scopes. A local that shadows nothing is distinct from a same-named local in another function.
- **Generic items:** references through generic instantiations (`Pair[int, str]`, a generic function called with different types) are all found. Renaming a type parameter changes only its own item.
- **Function values:** a function passed by name (to `map`, stored in a variable) counts as a reference, and `callers` marks it as a use as a value. Calls made through the variable later are not attributed to the function.
- **Tests and properties** are callers like functions, named by their test name.
- **Builtins** cannot be renamed. `callers` of a builtin lists the program's calls of it.
- **Renaming to a reserved word or a builtin name**, or to something that is not an identifier, is refused.
- **Renaming `main`** is refused: the toolchain runs the function named `main`.
- **Files outside the program:** a `.vo` file that the entry file does not import (directly or indirectly) is not searched or edited.
- **Formatting:** an edit changes only the spans it lists. A rename that makes a line longer does not reformat it. An added field follows the canonical format at the place it is inserted.
- **Record literals with fields over several lines** get the added field on its own line, indented like the others.
- **Impact:**
  - a diff that touches only comments or blank lines inside a function still counts that function as changed (the tool does not judge meaning);
  - a diff that adds a new function lists it as changed, with no callers;
  - a diff that removes a function lists it as removed, with the callers it had in the old version, if those can be found from the diff and the current program;
  - a diff for a file that is not part of the program is reported and ignored;
  - a diff that does not apply to the current files (wrong line counts) is an error.
- **Trace volume:** recursion 10,000 deep and loops with millions of calls stay within the trace limit. Values in trace records are shortened like local values in runtime errors.
- **Trace and recording paths:** a trace or recording path that is one of the program's source files, or the same path for both, is refused before the run starts. Whether the program later writes that file itself is decided by its effect grant, as for any other file; the toolchain does not predict it.
- **Trace and effects:** writing the trace or the recording is done by the toolchain, not the program, so it needs no effect and does not appear in `vo.effects`.
- **Recording contents:** a recording holds the environment variables and file contents the program read. It can contain secrets; the reference says so.
- **Replay of a run that timed out or ran out of memory:** the replay stops the same way, with the same code. A timeout depends on the machine's speed, so the output printed before it may be shorter or longer than in the recorded run; byte-identical output is promised for runs that did not time out.
- **Replay with different options:** `--seed` and `--clock-ms` come from the recording; options that do not change what the program receives (`--trace`, `--mem-stats`, `--max-memory`, `--timeout`) may be given to `vo replay`.
- **Local values of types that cannot be rendered**, such as function values, are shown as their type only (`<fn(int) -> int>`).

## Requirements *(mandatory)*

### Functional Requirements

**Queries**

- **FR-001**: The toolchain MUST provide queries on a checked program, given by its entry file:
  - `callers NAME`: every call site and every use as a function value, in every file of the program, with file, span, enclosing function or test, and whether it is a call or a value use;
  - `refs NAME`: the declaration and every reference of a function, record, variant, field, variant case, parameter, local or type parameter;
  - `type-at FILE:LINE:COL`: the innermost expression or name at that position, its type, its span, and the span of the declaration it refers to (if it is a name).
- **FR-002**: Names MUST be given as `name`, `Record.field`, `Variant.case`, or a position `FILE:LINE:COL`. A name that does not exist MUST be a structured error with a new code, and with the closest existing name when there is one. A name that matches several declarations MUST be an error that lists them.
- **FR-003**: Query results MUST follow the checker's resolution exactly: shadowing, imports, generic instantiations and field resolution. Text that is not a reference (strings, comments, same-named but different declarations) MUST NOT appear.
- **FR-004**: `vo impact` MUST take the program's entry file and a unified diff (from a file or from stdin) and report:
  - the functions, tests and properties the diff changes, adds or removes;
  - every function that calls a changed function, directly or through other functions;
  - every test and property that reaches a changed function through calls.

  Uses as function values count as calls for impact. A diff that does not match the current files MUST be an error.
- **FR-005**: Queries MUST work on programs with check errors, answering from the parts that check, and MUST say in their output that the program has errors.
- **FR-006**: Every query MUST have text and `--json` output. In JSON, each answer MUST be a record of a new documented kind, ending with a summary record, following the existing schema rules (positions, spans, `v`).

**Structural edits**

- **FR-007**: `vo edit rename PROGRAM TARGET NEW_NAME` MUST rename the target (any name form of FR-002 except builtins and `main`) at its declaration and at every reference found by `refs`, including `import` lines and the target's lines in the effects record. Nothing else MUST change.
- **FR-008**: `vo edit add-field PROGRAM RECORD FIELD: TYPE = DEFAULT` MUST add the field at the end of the record's declaration and add `FIELD: DEFAULT` at the end of every record literal of that record, in every file of the program.
- **FR-009**: Every edit MUST be verified before any file is written: the program is checked as it would be after the edit, and the edit MUST be refused if the check reports any error that was not there before, or if a reference would resolve to a different declaration than before. A refused edit MUST write nothing and report the reason as a structured error with a new code, naming the conflicting declaration when there is one.
- **FR-010**: An edit MUST also be refused, writing nothing, when:
  - the new name is not an identifier, is reserved, or is a builtin name;
  - the field already exists, or its type or default does not check, or the default uses a name that is not visible at every record literal;
  - the program has check errors before the edit and the edit changes the set of errors.
- **FR-011**: A successful edit MUST print every change it made (file, span, old text, new text), in text and as JSON records using the existing fix shape, and exit 0. With `--dry-run` it MUST print the same changes and verdict and write nothing.
- **FR-012**: A file MUST be written only if its contents change, and the edit MUST be applied to all files or to none (if writing one file fails, files already written are restored, and the error is reported).

**Traces**

- **FR-013**: `vo run --trace FILE` and `vo test --trace FILE` MUST write a JSON-lines trace: a record for each call of a program function (function, call site, depth, arguments by parameter name in vo syntax) and for each return (function, depth, returned value). Calls ended by a runtime error get no return record; one record for the error follows them, and an end record always comes last. Builtins are not traced; calls of program functions made by builtins (such as `map`) are.
- **FR-014**: Traces MUST be deterministic: the same program, input and options give a byte-identical trace. Traces MUST NOT contain timings.
- **FR-015**: A trace MUST stop recording after a limit (100,000 records by default, set with an option), and then write one record saying how many records were left out. The program MUST keep running normally.
- **FR-016**: A run without `--trace` MUST run no slower than before this spec, measured with spec 004's speed benchmark as in spec 005.

**Local values in runtime errors**

- **FR-017**: Every frame of a runtime error's trace MUST list the parameters and the locals in scope at that frame's current position, each with name, type and value in vo syntax, in declaration order. In JSON these MUST be added fields of the existing trace entries; in text, one indented line per frame.
- **FR-018**: A rendered value (in runtime errors and in traces) MUST be shortened to a fixed length, with a marker showing it was shortened. Function values MUST be rendered as their type.

**Recording and replay**

- **FR-019**: `vo run --record FILE` MUST write a recording of everything the program received from outside, in order: arguments, stdin reads, environment variable reads, file reads and the results of file writes, clock readings and the seed. It MUST also store a fingerprint of the program's source. The recording MUST be written however the run ends (normal end, `exit`, runtime error, timeout, out of memory).
- **FR-020**: `vo replay FILE` MUST run the program named in the recording (its current source) on the recorded inputs only: it reads no real stdin, environment, files or clock, and performs no file writes. For an unchanged program whose recorded run did not time out, stdout, stderr and exit status MUST be byte-identical to the recorded run.
- **FR-021**: When the program differs from the recorded fingerprint, replay MUST say so and still run. When the program asks for an input that does not match the next recorded one (a different kind, file or variable, or more than was recorded), replay MUST stop with a new runtime error naming the first mismatch.
- **FR-022**: `vo replay` MUST accept `--trace`, `--json`, `--mem-stats`, `--max-memory` and `--timeout`. The recording's format MUST be documented and versioned like the other JSON output.

**Language, reference and catalog**

- **FR-023**: The reference MUST document every new command and option, the name forms, the new JSON record kinds and the recording format, with examples that the test suite executes. The increase in its size MUST be stated, and it MUST stay within the 15,000-token budget.
- **FR-024**: Every new code (unknown name, ambiguous name, edit refused, diff does not apply, invalid recording, an output file that cannot be written, replay mismatch, and any others) MUST have a catalog entry with `vo explain` text and at least one golden case that triggers it.
- **FR-025**: Existing commands, without the new options, MUST behave exactly as before, with one exception: runtime error reports, including those of `error` tests in `vo test` (as a `trace` added to the failure), gain the per-frame values of FR-017. The corpus's expected outputs are updated for that change only.

**Evaluation**

- **FR-026**: The project MUST include, for each refactor task r01–r10:
  - a script of at least 3 edits on the starter program, taken from what the task statement asks for;
  - hand-written reference lists of call sites and references for at least 3 names.

  A script MUST apply the edits with `vo edit` and with whole-word text replacement, run both results on every visible and hidden input, compare with the unedited starter, check the queries against the reference lists, and report the results per task and in total.

### Key Entities

In this spec, a **record** is one line of JSON output (a JSON-lines record), `vo.effects` is the **effects record**, and the saved inputs of a run are a **recording**.

- **Program**: the entry file and every file it imports, directly or indirectly, checked together. Queries and edits always work on a whole program.
- **Target**: what a query or edit is about: a top-level item, a field, a variant case, or the declaration at a position.
- **Reference**: one place in the source that the checker resolves to a target, with its file, span and enclosing function or test.
- **Edit**: an ordered set of changes (file, span, old text, new text) that the checker has verified, applied to all files or to none.
- **Impact report**: changed, added and removed items, the functions affected through calls, and the tests and properties that reach them.
- **Trace**: JSON lines of call and return records for one run or one `vo test`, with a size limit.
- **Frame values**: the parameters and locals of one call frame at the point of a runtime error, rendered in vo syntax.
- **Recording**: everything a run received from outside, in order, plus the program's fingerprint and options.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: On the refactor tasks' starters (at least 30 scripted edits over r01–r10), every edit done with `vo edit` checks clean and produces byte-identical output to the starter on every visible and hidden input. The number of the same edits that break the program when done by whole-word text replacement is reported.
- **SC-002**: For at least 30 names over r01–r10, `callers` and `refs` match the hand-written reference lists exactly (no missing and no extra entries). The number of wrong hits from a whole-word text search is reported.
- **SC-003**: For every edit that must be refused (name clash, shadowing, reserved word, bad default, existing field, program already broken), a golden case shows the refusal and that no file changed.
- **SC-004**: Golden cases show `vo impact` giving exactly the expected changed items, affected functions and tests for at least 10 diffs, including diffs that add, remove and change functions across files.
- **SC-005**: Traces, local values and replay:
  - golden traces are byte-identical over 3 runs, and every golden runtime error shows the expected frame values;
  - recordings of at least 10 golden programs that read stdin, files, environment variables, arguments and the clock replay with byte-identical stdout, stderr and exit status after those inputs are removed;
  - golden cases show a replay mismatch for a changed program.
- **SC-006**: Speed: each program in spec 004's speed benchmark, run without the new options, stays within 5% of the spec 005 binary's time (measured as in spec 005). The 703 bench2 reference runs and the whole existing corpus are byte-identical, apart from the frame values added to runtime errors.
- **SC-007**: A query or edit on the largest refactor starter finishes in under 1 second in the reference environment.
- **SC-008**: The language reference stays under 15,000 tokens, grows by at most 2,000 tokens, and every example in it passes.
- **SC-009**: `make test`, the fuzzer, the determinism check and the collector stress pass all pass, with the new golden cases included.

## Assumptions

- **The agent benchmark run is a follow-up.** Running agents on the refactor tasks with and without the new commands is reported later. It is not a gate for this spec (clarified with the user).
- **The edit scripts are written from the task statements**, before `vo edit` is run on them, and cover only the mechanical part of each task (renames and added fields). The behavior changes each task asks for are out of scope for the stand-in, which is why the edited programs are compared with the starter, not with the task's expected outputs.
- **Two edits only.** Rename and add-field are the roadmap's edits. Others (extract function, move an item to another file, change a parameter list) wait until someone needs them.
- **Impact is call-based.** A change affects what calls it, directly or not. Changes to a record's declaration affect the functions that use the record only through calls; type-level impact is out of scope.
- **The diff is a standard unified diff** (as from `git diff` or `diff -u`) against the program's current files. vo does not run version control itself.
- **Traces cover program functions only.** Builtin calls are not traced; their effects show in the arguments and results of the program functions around them. A per-function filter is left out until someone needs it; the limit keeps traces bounded.
- **Rendered values are shortened at a fixed length** (the plan picks it, around 200 characters), the same in traces and runtime errors.
- **Replay runs the current source.** The point of replay is to check a fix on the input that failed, so a changed program is allowed and reported, not refused.
- **Replay applies to `vo run` only.** Tests already run with a fixed seed and clock; recording tests is out of scope.
- **Recordings may contain secrets** (environment variables and file contents the program read). They are written only when asked for, and the reference says so.
