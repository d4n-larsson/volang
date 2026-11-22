# Quickstart: Security (spec 045)

## What it looks like

```vo
fn rules() -> list[Rule] {
    return [
        rule("/health", Access.anyone),
        rule_for(["POST"], "/sign-in", Access.anyone),
        rule("/admin", Access.any_role(["admin"])),
        rule("/notes", Access.signed_in)
    ];
}

record Remove {
    id: int;
    @header("X-Identity")
    @default("")
    identity: str;
}

fn remove(db: Db, input: Remove) -> result[Response] uses sqlite {
    let user = try require_user(input.identity);
    let note = try find_note(db, input.id);
    if note.owner != user.name and not has_role(user, "admin") {
        return err("forbidden: not your note");
    }
    ...
}

test "a user cannot delete another's note" {
    let r = as_user(request("DELETE", "/notes/1", ""), new User { name: "bob", roles: new list[str] });
    expect_eq(handle_request(test_app(), db(), r).status, 403);
}
```

## Validate

```sh
make quick AREA=web_security            # golden cases, doc examples, tests/web_security.py
tests/run.sh tests/cases/web_security   # stories 1 to 5 and the contract stop
python3 tests/web_security.py --group tokens   # SC-002, SC-003
python3 tests/web_security.py --group timing   # SC-004
python3 tests/web_security.py --group speed    # SC-005
(cd examples/secure-notes && ../../build/vo test)   # SC-001, SC-006, SC-007
python3 tests/ref_examples.py           # reference under 15,000 tokens
make site-check
python3 tools/progress.py run test      # once; then read build/progress/test.log
```

Expected: every case passes; the example's review is empty and its description equals the text in its
test; token checking adds about 10 microseconds a request.
