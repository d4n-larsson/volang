# Word wrap

Re-wrap text to a maximum line width.

Input: the first line holds the width `W` (an integer from 1 to 1000). The rest of standard input is the text. The only whitespace in the text is spaces and newlines.

- A word is a maximal run of non-space characters. Lines that are empty or contain only spaces are blank lines. One or more blank lines separate paragraphs; blank lines at the start or end of the text, and runs of several blank lines, do not create empty paragraphs.
- Within a paragraph, words are packed greedily: each output line takes as many of the following words as fit, separated by single spaces, with the line at most `W` characters long. No output line has leading or trailing spaces.
- A word longer than `W` always starts on a new output line (the current line, if it has any words, is ended first). It is cut into pieces of exactly `W` characters, each on its own line, and the final piece (1 to `W` characters) becomes the current line, so the following words may continue on it if they fit.
- Paragraphs are printed in order, separated by exactly one empty line.

If the text has no words, print nothing. Every output line ends with a newline.
