# Contract: Site Map

The published site's sections and addresses (under `https://d4n-larsson.github.io/volang/`).

| Section | Addresses | Kind | Source |
|---|---|---|---|
| Home | `/` | guide | `site/docs/index.md` |
| Getting started | `/start/install/`, `/start/first-program/`, `/start/tests-and-effects/` | guide | `site/docs/start/` |
| Language guide | `/guide/<chapter>/` | guide | `site/docs/guide/` |
| Toolchain guide | `/tools/<command>/`, `/tools/json-output/`, `/tools/exit-statuses/` | guide | `site/docs/tools/` |
| Reference | `/reference/`, `/reference/<section>/` | generated | `docs/reference.md` |
| Error catalog | `/errors/`, `/errors/<CODE>/` (e.g. `/errors/E0503/`) | generated | `docs/errors.md` |
| Standard library | `/std/`, `/std/<module>/#<item>` (e.g. `/std/text/#pad_left`) | generated | `vo doc --json` |
| Examples | `/examples/`, `/examples/<name>/` | generated | `examples/<name>/` |
| Security | `/security/`, `/security/review-1/` | generated | `docs/security.md`, `docs/security-review-1.md` |
| Roadmap | `/roadmap/` | generated | `docs/roadmap.md` |
| Specs | `/specs/` (one line per spec: number, title, status, link to the repository) | generated | `specs/*/spec.md`, `results.md` |
| Agents | `/llms.txt`, `/llms-full.txt`, every page's `.md` copy | generated | all pages |

## Language guide chapters

Each chapter: the problem the feature solves, how it works, the rules (linked to the reference section),
worked examples, edge cases and limits, the error codes involved (linked to their pages).

| Chapter | Reference sections |
|---|---|
| `basics` (values, `let`/`var`/`set`, control flow) | Lexical rules, Statements, Expressions and operators |
| `types` (built-in types, conversions, `str`) | Types |
| `records-and-variants` (`record`, `variant`, `match`) | Declarations |
| `functions` (functions as values, lambdas, closures) | Functions as values |
| `generics` (type parameters, `ordered`, inference, effect parameters) | Generics |
| `results-and-errors` (`result`, `try`, `ok`/`err`, runtime errors) | Results and errors |
| `lists-and-maps` | Lists and maps, Builtins |
| `collections` (`ordered_map`, `ordered_set`, `priority_queue`, `deque`, `std.collections`) | Types, Builtins |
| `effects` (`uses`, scopes, the root grant, `vo.effects`, R0010) | Effects |
| `tests` (`test`, `expect`, `@spec`) | Tests |
| `properties` (`prop`, generation, shrinking, `assume`) | Tests |
| `contracts` (`requires`, `ensures`) | Tests |
| `holes` (`todo`) | Holes |
| `modules-and-packages` (imports, `export`, `vo.packages`) | Programs in several files, Standard library and packages |
| `standard-library` (overview of the modules, `vo doc`) | Standard library and packages |
| `determinism` (seeds, clock, record and replay) | Traces and replay, Command line |

## Toolchain guide pages

One page per command: `check`, `run` (with `--trace`, `--record`, `--timeout`, `--max-memory`, `--seed`,
`--clock-ms`), `test`, `fmt`, `effects`, `explain`, `doc`, `reference`, `query`, `impact`, `edit`,
`replay`, `package`, `version`; plus `json-output` (every record kind, the schema version rules) and
`exit-statuses`. Each command page: synopsis, an Options table (checked against the toolchain), the
output in text and JSON, exit statuses, a worked session (a checked `console` block).
