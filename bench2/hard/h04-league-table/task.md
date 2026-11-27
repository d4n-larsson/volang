# League table

Compute a football league table with head-to-head tie-breaking.

## Input

- The first line lists the teams, separated by single spaces. A team name is a non-empty run of ASCII letters, digits and `_`; names are case-sensitive and all different. There are at most 200 teams.
- Every following non-empty line is a played match `HOME HG AG AWAY`: the home team, its goals, the away team's goals and the away team (single spaces; goals 0 to 99). Both teams are listed on the first line and are different. Two teams may meet any number of times, or never. There are at most 5000 matches.

## Points and statistics

A win gives 3 points, a draw 1, a loss 0. For a set of matches, a team's statistics are its points, its goals scored (GF), its goals conceded (GA) and its goal difference (GD = GF - GA) in those matches.

## Order

Teams are ordered by points in all matches, most first. Each group of two or more teams with equal points is ordered by this procedure:

1. Take only the matches between two teams of the group (all of them, if two teams met several times). Compare the teams of the group by, in this order: points in these matches, goal difference in these matches, goals scored in these matches (higher is better for each).
2. If this separates the group into smaller groups of equal teams, order these smaller groups accordingly, and apply the procedure again, from step 1, to each smaller group that has two or more teams. Step 1 then uses only the matches between teams of the smaller group.
3. If step 1 does not separate the group at all (all its teams are equal on all three), order the group by goal difference in all matches, then goals scored in all matches (higher first), then by name in ASCII order.

## Output

One line per team, in order: `POS NAME P W D L GF GA GD PTS`, separated by single spaces, where POS is the position (1, 2, ...), P the number of matches played, W, D and L the wins, draws and losses, and GF, GA, GD and PTS taken over all matches. GD is written with a `+` when positive (`+3`, `0`, `-2`). Every output line ends with a newline.
