# Research: Small Language and Toolchain Fixes

## R1: `get_or` is a builtin, typed like `has`

- **Decision:** builtin id 55 (`B_COUNT` 55, `B_LAST` 55), `get_or(m: map[K, T], key: K, default: T) -> T`,
  pure (no bit in `builtin_effects`). Typing reuses `map_arg`/`check_arg` as `has` does: the key is
  checked against `K`, the default against `T`, the result is `T`. At run time it finds the key with the
  existing map lookup and returns the stored value or the default; it never inserts.
- **Rationale:** a library function in vo would need a new `std` module and an import for one line, and a
  generic `map[K, T]` parameter in vo needs nothing new, but agents would not find it among the builtins
  they already use (`has`, `keys`, `remove`). A builtin also gets the existing "cannot be redefined"
  error (E0202) for free, which is the one compatibility cost (spec FR-010).
- **Alternatives:** `std.maps.get_or` (an import per use; rejected for discoverability); a lazy default
  (no other vo call is lazy; rejected).

## R2: Replay matches the request line

- **Decision:** `io_http_exchange` takes the request's first line up to ` HTTP/` (`GET /status?x=1`) as
  the exchange's `request`; recording writes `"request":"GET /status"` in the `http` event, after `port`;
  replay matches host, port and, when the recorded event has one, the request; any difference is R0014
  naming `GET /v2/status on localhost:8000` (asked) and the recorded one. Events without `request`
  (recordings made by spec 007's toolchain) match by host and port only.
- **Rationale:** `std.http` builds the request line from the method and the URL's target, so the line is
  exactly "what was asked for"; headers and body are left out (spec Assumptions).
- **Event layout:** `Event` gains `request` (str) at offset 48; `EV_SIZE` 56.

## R3: R0010 names the function that granted the effects

- **Decision:** `rt_denied` names the *grant frame*: walking up from the running frame while the frame's
  function inherits its caller's grant. A frame inherits exactly when `vm_push` makes it inherit: not a
  test, and no declared effect set at run time (`decl->a.n->b.fx->declared == 0`), which covers anonymous
  functions and library functions (whose declarations are cleared before running, spec 007 R6). The walk
  stops at `main` (no parent).
- **Rationale:** the same condition as `vm_push`, so the named function is the one whose `uses` clause
  produced the effect set that was checked. Comparing effect-set pointers would stop early at generic
  library functions (`rt_es_subst` makes a new set).

## R4: `case ok()` and the one form for `result[void]`

- **Finding during planning:** besides `case ok(_)`, the checker already accepts `case ok` (no
  parentheses) for any result, because a case's binding is optional; the reference documents
  `case name(_)` for ignoring a value and `case name` for a case without a value. So a `result[void]`
  has two accepted spellings today, and `case ok()` would be a third.
- **Decision:** the parser accepts `case NAME()` (flag `NF_EMPTY_ARM`, 1024). The checker:
  - `case ok()` on a `result[void]`: the success arm, no binding.
  - `case NAME()` anywhere else (a result with a value, a variant case): **E0411** `empty_case_parens`
    with a fix: `NAME(_)` when the case carries a value, `NAME` when it does not.
  - `case ok(_)` and `case ok` on a `result[void]`: **W0402** `void_ok_pattern`, with a fix rewriting the
    pattern to `case ok()`. Warnings never change an exit status, so existing programs keep working.
  - `vo fmt` keeps the pattern as written (it does not know types); the fix is how programs converge.
- **Rationale:** the user's decision (spec, User Story 4) is that `case ok()` is the one form for
  `result[void]`, mirroring the `ok()` constructor; treating `case ok` like `case ok(_)` keeps that
  decision complete. Other results and variants keep their documented forms.
- **Codes:** E0411 (next free E04xx), W0402 (next free W04xx).

## R5: `align_left` and `align_right` in vo

- **Decision:** `std/text.vo` gains `align_left(s, width)` = `pad_right(s, width, " ")` and
  `align_right(s, width)` = `pad_left(s, width, " ")`, each with a doc comment and an example.
- **Rationale:** the user's decision (no default arguments, Principle I); two lines of vo, documented by
  `vo doc` and covered by `tests/doc_conformance.py` automatically.

## R6: The generator's own `get_or`

- **Decision:** `tests/gen_programs.py`'s prelude renames its helper to `lookup_or`. Generated programs are
  compared between two binaries running the same source, so the rename changes no comparison.
