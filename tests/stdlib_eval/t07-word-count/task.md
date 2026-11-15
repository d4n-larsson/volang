Read text from stdin. Split it into words on whitespace (std.text's word splitting), lowercase each word (ASCII letters only), and remove these characters from both ends of each word: `.,;:!?"()`. Words that become empty are dropped.

Print the 5 most frequent words as `WORD: COUNT` (fewer if there are fewer distinct words), most frequent first, equal counts in ascending order of the word. Then print `distinct: N`, the number of distinct words.

Effects: `console`, `input`.
