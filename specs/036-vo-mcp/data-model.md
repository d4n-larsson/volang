# Data Model: `vo mcp`

## Tool (static table in `src/mcp.c`)

| Field | Meaning |
|---|---|
| `name` | the command's name |
| `description` | one sentence |
| `hint` | reads, writes or runs (→ `annotations`) |
| `args` | the arguments, in the order their words are emitted |

## Argument

| Field | Meaning |
|---|---|
| `name` | the JSON name (`max_memory`) |
| `type` | string, integer, boolean, list of strings |
| `required` | must be present |
| `kind` | operand, flag, option, program arguments, stdin text, choice |
| `word` | the option's word (`--max-memory`) for flags and options |
| `choices` | the allowed words, for a choice |
| `description` | one phrase, for the schema |

Rules: every operand is emitted after the options, in table order; program arguments last after `--`;
an operand may not begin with `--`; a list operand needs at least one element when required.

## Call (one at a time)

| Field | Meaning |
|---|---|
| `id` | the request's id, kept as its raw JSON text (number or string), echoed in the answer |
| `pid` | the child; its process group is its own |
| `out`, `err` | up to 1 MiB each, with the count of bytes beyond |
| `status` | the child's exit status; 125 when it was ended by a signal |

States: running → answered, or running → cancelled (no answer). Nothing of a call remains afterwards.

## Session

`initialized` (false until `initialize` is answered); the input buffer and how far it has been read;
whether end of input was seen.
