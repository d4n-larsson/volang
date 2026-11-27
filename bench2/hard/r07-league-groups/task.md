# Football league: groups and head-to-head

## The existing program

The program keeps the results of a football league. It reads commands from standard input, one per line, and prints the response to each command. Words on a line are separated by one or more spaces; blank lines are ignored. A *number* is a non-empty string of the digits `0`-`9`. Team names (`NAME`, `TEAM`, `HOME`, `AWAY`, `A`, `B`) are single words.

Every team plays every other team twice, once at home and once away (a double round robin). A result is *counted* unless it is void; results become void when one of their teams withdraws. Teams are kept in the order they were registered. A win is worth 3 points, a draw 1, a loss 0.

Commands:

- `team NAME`: registers a team. Prints `team NAME added`, or `error: team NAME exists` (also for a withdrawn team's name).
- `withdraw NAME`: the team withdraws: every counted result it played in becomes void. Prints `team NAME withdrawn, K results void`. Errors: `error: unknown team NAME`, `error: team NAME already withdrawn`. A withdrawn team keeps its name but no longer appears in tables, fixtures or statistics.
- `rename OLD NEW`: renames a team; its results follow it. Prints `team OLD renamed NEW`. Errors, in this order: `error: unknown team OLD`, `error: team NEW exists`.
- `teams`: one line per team in registration order, `NAME`, or `NAME (withdrawn)` for a withdrawn team, then `N teams`.
- `result HOME HG AG AWAY` (HG and AG are numbers): records a result with the next result number (1, 2, ... counting every result ever entered). Checks, in this order: `error: unknown team HOME`, `error: unknown team AWAY`, `error: team cannot play itself`, `error: team HOME withdrawn`, `error: team AWAY withdrawn`, and `error: HOME v AWAY already played` when a counted result with that home and away team exists. Prints `result N: HOME HG-AG AWAY`.
- `correct N HG AG` (numbers): changes the score of result N. Prints `result N: HOME HG-AG AWAY (was X-Y)`. Errors: `error: no result N` (no such number), `error: result N void`.
- `table`: `table empty` when there is no active (not withdrawn) team. Otherwise a header and one row per active team, counting all counted results: played, won, drawn, lost, goals for, goals against, goal difference (GD, printed with a `+` when positive) and points. Rows are sorted by points, then goal difference, then goals for (all descending), then name (ascending byte order). Each row starts with the team's position: teams equal on points, goal difference and goals for share the position of the first of them, and the next team's position is its row number (1, 2, 2, 4). The exact layout is in `fmt.py` / `fmt.vo` (`table_lines`).
- `fixtures`: every pair `HOME v AWAY` of active teams that has no counted result yet, home team in registration order and, for each home team, away team in registration order; then `K fixtures left`.
- `form TEAM`: the outcomes (`W`, `D`, `L`) of the team's last 5 counted results, oldest first, as `form TEAM: WDL` (`-` when it has none). Errors: unknown team, `error: team TEAM withdrawn`.
- `h2h A B`: every counted result between the two teams in result order (as `result N: ...` lines), then `A X wins, B Y wins, Z draws`; or `no matches between A and B`. Errors: unknown team (A first), `error: team cannot play itself`.
- `goals`: `no matches` without counted results. Otherwise `most scored: TEAM N` (the active team with the most goals scored), `fewest conceded: TEAM N` (the fewest goals conceded), ties going to the first name in byte order; then `highest scoring match: result N: ... (G goals)` (the earliest on a tie) and `total: G goals in M matches`.
- `streak`: the longest run of consecutive wins of any active team, `longest winning streak N: A, B` (all teams with that run, in byte order), or `no wins yet`.
- `record TEAM`: the team's table row as `TEAM: P 3, W 2, D 0, L 1, GF 3, GA 1, GD +2, Pts 6`, then `home W-D-L, away W-D-L`, then `position K of M` (its position in the table and the number of rows). Errors as for `form`.
- `summary`: `teams N (W withdrawn), matches M, goals G` and `home wins H, draws D, away wins A` over the counted results.

A command with the wrong number of arguments, or a non-number where a number is required, prints `error: bad command` (checked before anything else). A first word that is not a command prints `error: unknown command WORD`.

## The change

The league becomes a tournament with groups, and teams level on points are now separated by their results against each other. Change the program as follows.

**1. Groups.** A new command `group NAME TEAM...` (at least one TEAM; else `error: bad command`) puts the teams into the group NAME, creating the group if it does not exist yet and otherwise adding them at the end of its members. It prints `group NAME: T1, T2, ...`, all the group's members in the order they joined. Checks, in this order, and on any error nothing changes:
   - once any result has been entered (even one that later became void): `error: groups are fixed once results exist`;
   - then for each TEAM in the order given: `error: unknown team TEAM`, `error: team TEAM withdrawn`, `error: team TEAM already in group G` (G is the group it is in, which may be NAME), `error: team TEAM listed twice` (it appeared earlier in this same command).

   The program is *grouped* as soon as one group exists; until then everything works as before, except for the new order of the table (point 2). A team's group membership is permanent: it stays a member after withdrawing (it is then left out of everything a withdrawn team is left out of), and `rename` renames the member too. A team that is not in any group can still be registered, renamed and withdrawn, but in a grouped program it can play no match and appears in no group's table, fixtures or statistics.

**2. Table order: head to head.** Every table (the league table, and each group's table) is now sorted as follows. Teams are sorted by points (descending). Teams level on points are compared by a *mini-table* that counts only the counted results between two teams of that same set of teams level on points: first by points in the mini-table, then goal difference in the mini-table, then goals scored in the mini-table (all descending); then, if still level, by overall goal difference, then overall goals for (descending), then name (ascending byte order). The mini-table is built once for each set of teams level on points; it is **not** built again for teams that are still level after it. Two teams share a position when they are equal on all of these except the name (points, the three mini-table values, goal difference, goals for).

   Example: Ant, Bee, Cat and Dog played Ant 2-0 Bee, Bee 1-0 Cat, Cat 1-0 Ant, Ant 1-0 Dog, Bee 5-0 Dog, Cat 2-0 Dog. Ant, Bee and Cat have 6 points each. Their mini-table (the first three results only): Ant 3 points, goals 2-1 (GD +1); Cat 3 points, 1-1 (GD 0); Bee 3 points, 1-2 (GD -1). So the order is Ant, Cat, Bee, Dog, at positions 1, 2, 3, 4, although Bee has the best overall goal difference (+4, against +2 and +2). Had the first result been Ant 1-0 Bee and the third Cat 2-1 Ant, the mini-table would be Ant 3 points 2-2, Cat 3 points 2-2, Bee 3 points 1-1: Ant and Cat stay level on it (Cat's win over Ant is not looked at again) and are ordered by overall goal difference, then goals for, then name.

**3. Results within a group.** In a grouped program, `result` also prints `error: HOME and AWAY are not in the same group` when the two teams are not members of the same group (also when either is in no group). This check comes after all the existing checks except the last one: after `error: team AWAY withdrawn` and before `error: HOME v AWAY already played`.

**4. Group tables.** `table` now takes an optional GROUP argument (more than one argument is `error: bad command`):
   - `table GROUP`: `error: unknown group GROUP` if there is no such group (always so in a program that is not grouped). Otherwise the line `group GROUP`, then that group's table: its active members only, counting their counted results, laid out exactly as before (the width of the name column comes from that group's own teams), or `table empty` when it has no active member.
   - `table` in a grouped program: the same as `table GROUP` for every group, groups in ascending byte order of their names. In a program that is not grouped, `table` is the league table as before (with the order of point 2).

**5. Fixtures and knockout.** In a grouped program, `fixtures` prints for each group, in byte order of the group names, a line `group GROUP: K left` followed by the group's pairs not yet played, each as `  HOME v AWAY` (two leading spaces): pairs of its active members with no counted result, the home team in the order the teams joined the group and, for each home team, the away team in that same order. The last line is `T fixtures left`, T the total over all groups.

   A new command `knockout` (no arguments) pairs the groups for the next round. Errors, in this order: `error: no groups` (not grouped), `error: knockout needs at least 2 groups`, then, for each group in byte order of the names, `error: group GROUP has fewer than 2 teams` (counting active members) and `error: group GROUP has K fixtures left` (both checks for one group before the next group). Otherwise, with the groups G1, G2, ..., Gn in byte order, it prints for i = 1 to n the line `match i: W (Gi 1st) v R (Gj 2nd)`, where W is the team in the first row of Gi's table, R the team in the second row of Gj's table, and Gj is the next group after Gi (G1 after Gn). With groups A and B: `match 1: <A's winner> (A 1st) v <B's runner-up> (B 2nd)`, `match 2: <B's winner> (B 1st) v <A's runner-up> (A 2nd)`.

**6. The other commands, in a grouped program:**
   - `h2h A B`: after the existing checks, `error: A and B are not in the same group` when they are not members of the same group.
   - `form TEAM`: for a team in a group, the line reads `form TEAM (group GROUP): ...`.
   - `record TEAM`: for a team in a group, the last line is `position K of M in group GROUP`, with its position in its group's table and M the number of rows of that table. For a team in no group the last line is `not in a group`.
   - `goals`: instead of the `most scored` and `fewest conceded` lines, one line per group that has an active member, in byte order of the group names: `group GROUP: most scored TEAM N, fewest conceded TEAM N`, over that group's active members only (ties to the first name in byte order). The other lines (`no matches`, the highest scoring match and the total) are unchanged.
   - `teams`: a team in a group is listed as `NAME (group GROUP)`, or `NAME (group GROUP, withdrawn)` when it has withdrawn.

Everything else stays exactly as it is.
