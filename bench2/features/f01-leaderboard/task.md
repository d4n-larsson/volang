# Leaderboard

Each line of standard input is `NAME SCORE`: a name of lowercase letters and an integer score (between -1000000000 and 1000000000), separated by one space. The input may be empty.

Print the players sorted by score, highest first; players with equal scores are ordered by name (ascending byte order); entries with the same name and score keep their input order. Use sort-by-key on a list of records.

Each output line is `RANK NAME SCORE`. The rank is the 1-based position of the first player with that score (competition ranking: scores 9, 7, 7, 5 get ranks 1, 2, 2, 4).
