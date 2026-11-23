# Quickstart: validating sessions and sign-in

Prerequisites: `make` has built `bin/vo`; Python 3; a desktop browser for the last part.

## 1. Cases and tests

```sh
make quick AREA=web_security                     # golden cases of tests/cases/web_security/
python3 tests/web_security.py --group cookies    # read_cookie against Python's http.cookies
python3 tests/web_security.py --group origin     # the same-origin rule against a Python model
python3 tests/web_security.py --group timing     # unknown name vs wrong password, form route (SC-006)
python3 tests/web_security.py --group secrets    # no password or session value in events (SC-008)
(cd examples/notes-pages && ../../bin/vo test main.vo)
make test                                        # once, at the end
```

Expected: every case passes; the example's tests cover sign-in, sign-out, a refused cross-site post,
and one user not seeing another's notes.

## 2. The flow without a browser

```sh
cd examples/notes-pages
printf 'correct horse\n' | ../../bin/vo run main.vo add-user ada
../../bin/vo run main.vo &
curl -si -H 'Accept: text/html' localhost:8080/notes           # 303, Location: /sign-in?next=%2Fnotes
curl -si -H 'Origin: http://localhost:8080' \
     -d 'name=ada&password=correct+horse&next=/notes' localhost:8080/sign-in
                                                                 # 303 to /notes, Set-Cookie: __Host-session=...
curl -si -H 'Origin: https://evil.example' -H 'Cookie: __Host-session=VALUE' \
     -d '' localhost:8080/notes/1/done                           # 403
curl -si -H 'Accept: application/json' localhost:8080/notes     # 401 problem
```

## 3. In a browser (SC-002, SC-003, R11)

1. Open `http://localhost:8080/notes` in Chrome or Firefox: the sign-in page appears.
2. Sign in as `ada`: back on `/notes`; add a note, mark it done with the script and with JavaScript
   turned off.
3. In the developer tools, the session cookie is `HttpOnly`, `Secure`, `SameSite=Lax`; `document.cookie`
   does not show it.
4. Copy the cookie's value, sign out, send a request with the copied value (curl as above): it is
   sent to the sign-in page.
5. Serve a page with a form posting to `http://localhost:8080/notes` from another port and submit it
   while signed in: 403.
