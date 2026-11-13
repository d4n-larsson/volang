# Contract: Benchmark v2 (`bench2/`)

## Categories (at least 10 tasks each, 40+ in total; FR-034)

| Category | What the agent gets | What is graded |
|---|---|---|
| `multi_file` | A task needing at least 3 source files; optionally a starter tree | Hidden stdin/stdout cases on the entry file |
| `refactor` | An existing codebase (at least 3 files, 150+ lines) and an instruction (rename across files, change a signature used in many places, extract a function or record) | Hidden cases for the changed behavior, plus "unchanged behavior" cases for the rest |
| `hidden_property` | A statement and visible examples | Hidden edge-case grading (overflow, empty input, unicode, large n) |
| `capability` | A statement with an explicit allowance, such as "may read `data/`, may print, nothing else" | Correct output and staying within the allowance |

## How capability tasks are graded

- **vo**: two separate observations.
  - **Declared violation:** the solution's `vo effects` grant for `main` is not covered by the allowance.
  - **Behavioral violation:** the hidden runs performed an effect outside the allowance. Since the runtime enforces the declaration, this can only come from a too-broad declaration; it is observed by running vo under the same file-access observation as Python (a temp tree with only the allowed directories writable, plus R0010 reports).
- **Python**: the solution runs through `bench2/py_sandbox.py`. It installs `sys.addaudithook` to record `open` in write modes and outside the allowed directories, `socket.*` and `subprocess.*`, and replaces `os.environ` with a recording mapping. Any recorded access outside the allowance is a behavioral violation. Python has no declarations, so it has no declared violations.

## Protocol (both languages)

- **Agent and context**: the same headless agent (`claude -p`), the same model, at most 3 attempts. For vo the context is `vo reference`; for Python, nothing.
- **Reply format**: the agent replies with one or more fenced blocks, each starting with `# file: <path>` (one block for single-file tasks).
- **Feedback after a failed attempt**: the toolchain output (check/JSON diagnostics or the traceback) and the failing **visible** cases. Hidden case contents are never shown, only "N hidden checks failed".
- **Results**: `bench2/results/<lang>.jsonl` gets one record per task: `{task, category, lang, model, attempts, solved, hidden_passed, hidden_total, declared_violation, behavioral_violation, seconds}`. A task counts as solved only with no violation of either kind.
- **Summary**: `bench2/summarize.py` reports SC-001 (ceiling check per category), SC-002 (vo against Python, using behavioral violations for both languages so the comparison is even) and SC-003 (capability detection: for vo, both kinds).

## Feature set (`bench2/features/`, SC-006)

20 single-file vo tasks that each need sort-by-key, map, filter, fold, closures or user generics. Graded by visible and hidden cases. Run for vo only.
