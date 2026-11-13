# Competition ranking

Rank players by score and print the top of the table.

Input:
- The first line holds an integer `K` (0 or more), possibly with leading and trailing spaces. If the input is completely empty, print nothing.
- Every following non-blank line holds `name score`, separated by one or more spaces, possibly with leading and trailing spaces. Blank lines (empty or only spaces) are ignored.
- A name is a non-empty run of ASCII letters and digits; names are case-sensitive (`Ann` and `ann` are different players). A score is an integer, an optional `-` followed by digits, between -1000000000000000 and 1000000000000000 inclusive.
- A name may appear on several lines. The player's score is then the highest of their scores, and their position in the input (used for ties, below) is the line where the name first appears.

Ranking:
- Players are ordered by score, highest first. Players with equal scores keep the order in which they first appear in the input.
- Ranks use standard competition ranking ("1224"): a player's rank is 1 plus the number of players with a strictly higher score. Equal scores share a rank and the next rank skips accordingly, e.g. scores 90, 80, 80, 70 get ranks 1, 2, 2, 4.

Output: one line `rank name score` (single spaces) for every player whose rank is at most `K`, in the order above. Because of ties this can be more than `K` lines; with `K` = 0, or no players, print nothing. Every output line ends with a newline.
