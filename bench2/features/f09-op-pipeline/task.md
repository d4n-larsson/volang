# Operation pipeline

The first line of standard input is a pipeline: a comma-separated list of operations, applied left to right. Each operation is one of

- `add N`, `sub N`, `mul N` (N an integer): add, subtract, multiply by N;
- `neg`: negate; `abs`: absolute value; `sq`: square.

The line may be empty, meaning the identity. Each following line holds one integer; for each, print the result of running it through the pipeline. Intermediate values stay within 64-bit range.

Parse the pipeline once into a list of functions (an operation with an argument becomes a function built from that argument), then apply that list to every number.
