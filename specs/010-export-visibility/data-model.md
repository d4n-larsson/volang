# Data Model: Visibility with `export`

## Item visibility

| Item | Flag | Span |
|---|---|---|
| `export fn`, `export record`, `export variant` | `NF_EXPORT` (512) on the item node | starts at `export` |
| `fn`, `record`, `variant` | none: private to its file | starts at the item keyword |
| `test`, `prop`, `import`, comments | never exported | unchanged |

Visibility is decided by the declaration alone; an import never changes it (no re-export).

## Interface of a file

The exported items of a file: what another file may import, and what `vo doc` lists for a library or
package module. For `std/`, exported = documented.

## Diagnostics state (one check)

A list of (declaration, diagnostic) pairs: the first E0209/E0210 for a declaration; later reports for it
become related notes on that diagnostic.
