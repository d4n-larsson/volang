# Version ranges

Validate and sort semantic version numbers, then answer range queries.

## Input

Lines of text (each ends with `\n`; the last line may lack it). The lines before the first line that is exactly `%%` are candidate versions; empty ones are ignored. The lines after it are queries (there may be none; empty query lines are ignored). There is always a `%%` line.

## Versions

A valid version is `MAJOR.MINOR.PATCH`, optionally followed by `-PRERELEASE`, optionally followed by `+BUILD`, with nothing else on the line (no spaces):

- MAJOR, MINOR and PATCH are numbers: one or more ASCII digits, without leading zeros (`0` is fine, `01` is not), at most 18 digits.
- PRERELEASE and BUILD are one or more identifiers separated by `.`. An identifier is a non-empty run of ASCII letters, digits and `-`.
- A prerelease identifier made only of digits is numeric: it must not have leading zeros and has at most 18 digits. Other identifiers (such as `0a` or `-`) are alphanumeric. Build identifiers have no further rules (`001` is fine).

Every other non-empty line is invalid.

## Precedence

Versions are compared by precedence:

1. Compare MAJOR, MINOR and PATCH numerically, in that order.
2. If they are equal, a version without a prerelease is greater than one with a prerelease.
3. Two prereleases are compared identifier by identifier from the left: numeric identifiers numerically, alphanumeric ones by ASCII order, and a numeric identifier is less than an alphanumeric one. If all identifiers of the shorter prerelease are equal to the first ones of the longer, the longer one is greater.
4. BUILD is ignored: `1.0.0+a` and `1.0.0+b` have equal precedence.

## Queries

A query is one or more comparators separated by single spaces. A comparator is an operator directly followed by a valid version `V`; each query is well-formed. A version `X` satisfies:

- `=V`, `<V`, `<=V`, `>V`, `>=V`: when X compares that way with V by precedence;
- `^V`: when X ≥ V and, if MAJOR of V is not 0, X has the same MAJOR as V; if MAJOR of V is 0 and MINOR is not, X has the same MAJOR and MINOR; if both are 0, X has the same MAJOR, MINOR and PATCH;
- `~V`: when X ≥ V and X has the same MAJOR and MINOR as V.

A version satisfies a query if it satisfies all its comparators and, in case it has a prerelease, at least one comparator of the query has a version with a prerelease and the same MAJOR, MINOR and PATCH as it.

## Output

1. Every valid version as written, in ascending order of precedence; versions of equal precedence in input order.
2. Then `invalid: LINE` for every invalid line, in input order.
3. Then for each query, `QUERY -> ANSWER`, where ANSWER is the valid version with the highest precedence that satisfies it (of several with equal precedence, the one first in input order), or `none`.

Every output line ends with a newline.
