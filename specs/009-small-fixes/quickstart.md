# Quickstart: Validating Spec 009

```sh
make && make test                          # the suite with the new cases
tests/run.sh tests/cases/run/get_or* tests/cases/check/case_ok*   # the new golden cases
python3 tests/steps.py tests/cases/http    # replay matching (request line) against the test server
python3 tests/catalog.py && python3 tests/ref_examples.py && python3 tests/doc_conformance.py
make gate                                  # the full gate (x86-64)
make ARCH=aarch64 test && make ARCH=riscv64 test   # the other platforms, under qemu
make diff-asm COUNT=10000                  # programs without the new features: 0 disagreements
```

Expected: everything passes; `examples/` use `get_or` and `align_*` and still pass `make test`.
