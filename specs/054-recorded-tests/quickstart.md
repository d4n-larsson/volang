# Quickstart: validating recorded tests

Prerequisites: `make` has built `bin/vo`; Python 3.

```sh
make quick AREA=recorded                     # golden and step cases of recorded tests
python3 tests/recorded_tests.py              # identical re-records, no network, timing, redaction, mutate
cd examples/rates
../../bin/vo test --replay recordings/tests main.vo    # offline, every test passes, "(replayed)"
../../bin/vo test --replay recordings/tests --mutate main.vo
../../bin/vo test --record recordings/tests --test 'rates.vo::converts with the day rate' main.vo
                                             # needs the network; re-records one test only
git diff recordings/tests                    # empty when the API's answer did not change
```

Expected: replayed tests pass with no network; a change to the request in `rates.vo` makes the replayed
test `error` with R0014 naming the new and the recorded request; `grep` for a key passed with
`--redact X-Api-Key` finds nothing in `recordings/tests`.
