# Validators

The first line of standard input is a comma-separated list of rules (possibly empty):

- `min N`: the value is at least N; `max N`: at most N;
- `even`, `odd`;
- `div N`: the value is a multiple of N (N is not 0);
- `not N`: the value is not N.

Each following line holds one integer. For each, print `X ok` if it satisfies every rule, otherwise `X fails RULES` where `RULES` are the failed rules, written exactly as in the input, in input order, separated by `,`.

Turn each rule into a record holding its text and a predicate function, then check every value against the list of rules.
