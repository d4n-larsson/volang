A game leaderboard. Each non-blank line of stdin is one command; process them in order:

- `score NAME POINTS`: add POINTS (an integer from 1 to 100) to NAME's total (a player who has not scored yet starts at 0). NAME is lowercase letters and digits. Print nothing.
- `top K`: print the K best players (1 <= K <= 10) on one line, best first, each as `NAME:TOTAL`, separated by single spaces. Players are ranked by total, highest first; equal totals rank by name in ascending (byte) order. When fewer than K players have scored, print all of them; when none has, print `empty`.

Example: after `score bo 5`, `score ana 3`, `score ana 2`, `score cy 9`, the command `top 2` prints `cy:9 ana:5` and `top 5` prints `cy:9 ana:5 bo:5`.

Effects: `console`, `input`.

The largest inputs have 100,000 commands, 30% of them `top`, and 20,000 players: sorting all players for each `top` is too slow.
