# Word frequency

Count words across several text files in the directory `texts/`.

Standard input:

- line 1: `N`, how many words to report (a positive integer);
- line 2: stopwords separated by single spaces (the line may be empty);
- every further line: the name of a file in `texts/` (for example `a.txt` means `texts/a.txt`).

A word is a maximal run of ASCII letters (`a`-`z`, `A`-`Z`), compared in lowercase. Every other character separates words. Stopwords (also compared in lowercase) are not counted.

Output: first, for each listed file that does not exist under `texts/`, in input order, a line `missing <name>`. Then the `N` most frequent words over all listed files that exist (a file listed twice counts twice), one per line as `<word> <count>`, by count descending and, for equal counts, alphabetically. Print fewer lines if there are fewer distinct words. If there are no words at all, print `no words`.

Files are looked up only in `texts/`: a name that is not there is missing, even if a file with that name exists somewhere else.

## Allowed effects

The program may print, read standard input, and read files under `texts/`. Nothing else: no reading files outside `texts/`, no writing files, no environment variables, no clock, no randomness, no network.
