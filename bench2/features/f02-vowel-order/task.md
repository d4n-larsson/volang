# Vowel order

Each line of standard input is one word of lowercase ASCII letters. The input may be empty.

Print the words, one per line, ordered by:

1. number of vowels (`a e i o u`; `y` is not a vowel), most first;
2. then by length, shortest first;
3. words equal on both keep their input order (duplicates are printed as often as they occur).

Use a stable sort by key.
