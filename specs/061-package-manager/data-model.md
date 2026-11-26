# Data Model: Package Manager

## Manifest entry (`Pkg`, `src/packages.c`)

| Field | Today | New |
|---|---|---|
| name | identifier, unique (E0704) | unchanged; one name per source (E0708) |
| dir / raw | absolute and as written | unchanged |
| pin | SHA-256 of the `.vo` files | unchanged |
| url | archive URL with `#PATH`, or none | for a versioned entry the source: repository URL with optional `#FOLDER` |
| version | — | `major, minor, patch` (each 0 to 999,999), or none |
| indirect | — | 1 when the fifth field is `indirect` |

Validation (E0703): the version is three decimal numbers without leading zeros; `indirect` only on a
versioned entry; nothing after it.

## Version

Ordered by (major, minor, patch). `compatible(a, b)`: `a.major == b.major`, and when the major is 0 also
`a.minor == b.minor`. Tags with anything else (`1.2`, `1.2.0-rc1`, `release-3`) are not versions.

## Source

`https://HOST/PATH` plus optional `#FOLDER`; compared as text after dropping one trailing `/` and one
trailing `.git` from the path. Identity of a package across manifests is its source; its name must agree.

## Requirement

`(name, source, version, hash, required_by)`, read from a versioned line of the program's manifest
(`required_by` = the program) or of a selected package's manifest (`required_by` = that package).

## Selection

`name → (source, version, hash, direct, requirers)`; computed by research R4; written back as the
program's manifest. State changes of an entry reported by commands: `added`, `updated` (old → new
version and hash), `unchanged`, `removed`, `restored`, `present`, `local` (a version-less entry kept
where a version was required).

## Git objects (transient, never stored)

Ref list: `tag → commit id`. Pack: objects by SHA-1 id with type (commit, tree, blob, tag) and bytes;
only blobs reached from the package root with names ending `.vo`, and the root `vo.packages`, leave
memory.
