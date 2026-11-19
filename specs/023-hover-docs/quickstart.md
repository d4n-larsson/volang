# Quickstart: validating documentation on hover

1. `make quick AREA=query,docs`: the `hover_*` and `program_*` cases, and the existing `query` and `doc`
   cases.
2. In a two-file program (`geo.vo` with a documented `dist2`, `main.vo` calling it):

       build/vo query type-at main.vo main.vo:7:20            # the line, the signature, the description
       build/vo query type-at --json main.vo main.vo:7:20     # ... "doc":{...}
       build/vo doc main.vo dist2                             # as vo doc prints std items

3. At `sqrt(...)`: the `doc` equals `vo doc --json std.math.sqrt` (without its head).
4. `python3 tests/hover_speed.py` (SC-003), then `make test` once.
