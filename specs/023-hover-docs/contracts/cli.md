# Contract: CLI changes

## `vo query type-at [--json] ENTRY FILE:LINE:COL`

JSON (fields added to the `type_at` record):

```json
{"v":1,"kind":"type_at","file":"main.vo","start":{"line":7,"col":20},"end":{"line":7,"col":25},
 "text":"dist2","type":"float","name":"dist2",
 "declaration":{"file":"geo.vo","start":{"line":9,"col":11},"end":{"line":9,"col":16}},
 "doc":{"module":"geo","name":"dist2","item":"fn","signature":"fn dist2(a: Point, b: Point) -> float",
        "tparams":[],"params":[{"name":"a","type":"Point"},{"name":"b","type":"Point"}],"returns":"float",
        "effects":[],"description":"The squared distance between a and b; cheaper than the distance.",
        "example":"dist2(new Point { x: 0.0, y: 0.0 }, new Point { x: 3.0, y: 4.0 }) is 25.0"}}
```

Human:

```text
main.vo:7:20 dist2: float (declared at geo.vo:9:11)
    fn dist2(a: Point, b: Point) -> float
    The squared distance between a and b; cheaper than the distance.
```

No `doc` (and no extra lines) for locals, parameters, type parameters, and expression answers.

## `vo doc [--json] ENTRY.vo NAME`

The program's item NAME (any file of the program, exported or not), printed as library items are: the
signature, the description, `example:`. `--json`: the doc record. An unknown NAME: T0010 with a
suggestion. `vo doc` with other arguments is unchanged.
