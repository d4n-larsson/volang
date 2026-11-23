# Quickstart: validating the removal

Prerequisites: `make` has built `bin/vo`.

```sh
git grep -n -i 'std\.html\|html_reply\|html_text\|no_html\|notes-pages\|check_html' -- . ':!specs' ':!CHANGELOG.md'
                                           # nothing (SC-002); the changelog may mention nothing of it either
bin/vo explain E0337                       # the retired entry, exit 0
printf 'import std.html.html;\nfn main() {}\n' > /tmp/x.vo && bin/vo check /tmp/x.vo
                                           # the ordinary unknown-module diagnostic
python3 tests/catalog.py                   # 0 problems; retired codes accepted
python3 tests/ref_examples.py              # fewer tokens than 14,981 (SC-005)
tests/run.sh tests/cases/web tests/cases/web_security tests/cases/generic
(cd examples/secure-notes && ../../bin/vo test main.vo)
make test                                  # once, at the end
```

Expected: every command as noted; the secure-notes tests include browser sign-in by form and JSON, a
cookie-signed note request, a refused cross-site post, and sign-out.
